import { Device } from './twin.js';
import * as JP from "../jsonpath/jsonpath.js";
import { JsonParserV1 } from "./json_parser_v1.js";
import { JsonParserV2 } from "./json_parser_v2.js";

class ProtocolError extends Error {  
}

class RestProtocol {
  constructor(url, onupdate, interval = 100, usePolling = false) {
    this.url = url;
    this.interval = interval;
    this.pollInterval = 250;
    this.usePolling = usePolling;
    this.streamingTimeout = 11000;

    this.onload = onload;
    this.onupdate = onupdate;
    this.version = 1;
    this.parser = null;
  }

  async probe() {
    try {
      const result = await fetch(`${this.url}/probe`, {
        headers: {
          'Accept': 'application/json'
        }
      });
      const text = await result.text();
      const probe = JSON.parse(text);
      this.version = probe["MTConnectDevices"]["jsonVersion"];
      if (this.version == 2) {
        this.parser = new JsonParserV2();
      }
      else {
        this.parser = new JsonParserV1();
      }

      const devices = this.parser.devices(probe, device => { return new Device(device, this.parser); });
      this.device = devices[0];
      console.debug(this.device);
    } catch (error) {
      console.log(error);
      throw new ProtocolError("Cannot probe");
    }
    return this.device;
  }

  async current() {
    const result = await fetch(`${this.url}/current?${this.path}`, {
      headers: {
        'Accept': 'application/json'
      }
    });
    const text = await result.text();
    this.handleData(text);
  }

  sleep(delay) {
    return new Promise(resolve => setTimeout(resolve, delay));
  }

  async pollSample() {
    while (true) {
      try {
        const result = await fetch(`${this.url}/sample?${this.path}&from=${this.nextSequence}&count=1000`, {
          headers: {
            'Accept': 'application/json'
          }
        });
        const text = await result.text();
        this.handleData(text);
    
        await this.sleep(this.interval);
      } catch (error) {
        console.log(error);
        if (error instanceof ProtocolError) {
          throw error;
        }
        await this.sleep(this.pollInterval);
      }
    }
  }

  async streamSample() {
    while (true) {
      try {
        const controller = new AbortController();
        let id = setTimeout(() => controller.abort(), this.streamingTimeout);
        const uri = `${this.url}/sample?${this.path}&interval=${this.interval}&from=${this.nextSequence}&count=1000`;
        console.debug(uri);
        const response = await fetch(uri, {
          headers: {
            'Accept': 'application/json'
          },
          signal: controller.signal
        });

        clearTimeout(id);

        const content = response.headers.get('Content-Type'); 
        const pos = content.indexOf("boundary=");
        if (pos < 0) {
          throw Error("Cound not find boundary in content type");
        }

        this.boundary = `--${content.substr(pos + 9)}`;
        console.debug(this.boundary);

        const reader = response.body.getReader();

        while (true) {
          const { value, done } = await reader.read();

          if (done) break;

          const text = new TextDecoder().decode(value);
          console.log("Read " + text.length + " bytes");

          if (!this.buffer) {
            this.buffer = text;
          } else {
            this.buffer += text;
          }
          let more = this.processChunk();
          while (more)
          {
            more = false;
            const ind = this.buffer.indexOf(this.boundary);
            if (ind >= 0)
            {
              const body = this.buffer.indexOf("\r\n\r\n", ind);
              if (body > ind) {
                // Parse header fields
                const headers = this.parseHeader(this.buffer.substring(ind + this.boundary.length + 2, body));
                if (headers['content-length']) {
                  this.length = Number(headers['content-length']);
                  this.buffer = this.buffer.substr(body + 4)
                  more = this.processChunk();
                } else {
                  this.usePolling = true;
                  throw new ProtocolError("Missing content length in frame header, switching to polling");
                }
              }
            }
          }
        }
      } catch(error) {
        console.log(error);
        if (error instanceof ProtocolError) {
          throw error;
        }
        if (error.name == 'AbortError') {
          this.usePolling = true;
          throw new ProtocolError('Switching to polling');
        }
        await this.sleep(this.interval);
      }
    }
  }

  parseHeader(header) {
    return Object.fromEntries(header.split("\r\n").
            map(s => s.split(/:[ ]*/)).map(v => [v[0].toLowerCase(), v[1]]));
  }

  processChunk() {
    if (this.length && this.buffer.length >= this.length)
    {
      this.handleData(this.buffer.substr(0, this.length));

      if (this.buffer.length > this.length) {
        this.buffer = this.buffer.substr(this.length);
      } else {
        this.buffer = undefined;
      }
      this.length = undefined;
    }

    return this.buffer && this.buffer.length > 0;
  }

  handleData(text) {
    const data = JSON.parse(text);

    if (this.instanceId && data.MTConnectStreams.Header.instanceId != this.instanceId) {
      this.instanceId = undefined;
      this.nextSequence = undefined;
      throw new ProtocolError("Restart stream");
    } else if (!this.instanceId) {
      this.instanceId = data.MTConnectStreams.Header.instanceId;
    }

    this.nextSequence = data.MTConnectStreams.Header.nextSequence;
    this.transform(data);
  }

  transform(data) {
    const updates = [];
    this.parser.observations(data, (key, obs) => {
      const di = this.dataItems[obs.dataItemId];
      if (di) {
        di.apply(key, obs);
        updates.push([di, obs]);
      }      
    });
    
    this.onupdate(updates);
  }

  pascalize(str) {
    return str.split('_').map(s => s[0] + s.substr(1).toLowerCase()).join('');
  }

  async streamData(dataItems) {
    this.dataItems = Object.fromEntries(dataItems.map(di => [di.id, di]));
    this.path = `path=//DataItem[${Object.keys(this.dataItems).map(id => "@id='" + id + "'").join(" or ")}]`;

    while (true) {
      try {
        await this.current();

        if (this.usePolling)
          await this.pollSample();
        else
          await this.streamSample();
        
      } catch (error) {
        console.log(error);
        if (error instanceof ProtocolError) {
          await this.sleep(1000);
        }
      }
    }
  }

  async probeMachine() {
    while (true) {
      try {
        await this.probe();
        return this.device;
      } catch (error) {
        console.log(error);
        if (error instanceof ProtocolError) {
          await this.sleep(1000);
        } else {
          return undefined;
        }  
      }
    }
  }

  async loadModel(onload) { 
    await this.device.load(onload);
    return this;
  };
}

export { RestProtocol };
