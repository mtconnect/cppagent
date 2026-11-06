# 🚀 MTConnect C++ Agent

A modern and extensible C++ implementation of the MTConnect standard, designed to reliably collect machine data from diverse industrial devices and expose it in a consistent, structured format. The agent normalizes raw device signals, ensures compliance with the MTConnect standard, and makes the data accessible to client applications such as MES systems, monitoring dashboards, analytics platforms, or custom integrations.

The agent supports multiple industrial communication models (SHDR, JSON, MQTT), multiple devices, secure network transport, and pluggable data transformation logic, making it suitable for both small installations and large-scale production environments.

---

# 📑 Table of Contents

- [Overview](#overview)
- [Key Features](#key-features)
- [Architecture](#architecture)
- [Quick Start](#quick-start)
- [Building From Source](#building-from-source)
- [Basic Configuration](#basic-configuration)
- [Advanced Configuration](#advanced-configuration)
  - [Multiple Adapters](#multiple-adapters)
  - [HTTP & TLS](#http--tls)
  - [MQTT](#mqtt)
  - [Ruby Extensions](#ruby-extensions)
- [Sample Outputs](#sample-outputs)
- [Directory Structure](#directory-structure)
- [Troubleshooting](#troubleshooting)
- [Contributing](#contributing)
- [Version History](#version-history)
- [Useful Links](#useful-links)
- [License](#license)

---

# 🧭 Overview

The MTConnect Agent acts as the intermediary between raw machine data and higher-level software systems that require structured, contextualized insights. It transforms messy, vendor-specific machine signals into clean, semantic MTConnect XML/JSON outputs. The agent follows a one-way, read-only information model, meaning it never sends instructions back to machines—making it safe and easy to integrate into existing manufacturing setups.

By handling multiple adapters, buffering high-frequency data, supporting secure communication, and offering real-time streaming capabilities, the agent is suitable for both small machine shops and enterprise-level Industry 4.0 deployments.

---

# ⭐ Key Features

### 🔌 Wide Input Support

- SHDR adapters for CNC interoperability
- MQTT ingestion for IoT environments
  - Native JSON input for modern sensors and custom connectors
  - Customizable JSON handling for propriatary models

### 🌐 Flexible Output Options

- REST HTTP server for `/probe`, `/current`, `/sample`, and `/assets` APIs
- WebSocket streaming for real-time dashboards
- TLS support for secure communication
- MQTT output using flattended or hierarchial topics

### 🧠 Extensibility

- Ruby-based scripting for custom transformation pipelines
- Modular components allowing custom inputs and outputs
- XML Namespace support for extended functionality

### ⚙️ Industrial-Grade Infrastructure

- High-performance store-and-forward buffering of data
- Cross-platform support (Windows, Linux, and MacOS)
- Low-overhead runtime for embedded or resource-constrained machines

---

# 🧱 Architecture

The agent transforms heterogeneous raw machine signals into structured, queryable MTConnect data streams. This architecture ensures consistent data modeling regardless of vendor or device type.

```
              ┌─────────────────────────┐
              │      Client Layer       │
              │  MES, Dashboards, Apps  │
              └──────────┬──────────────┘
                         │
            HTTP / WebSockets / TLS / MQTT
                         │
               ┌─────────┴───────────┐
               │   MTConnect Agent   │
               │   (C++ Reference)   │
               └─────┬────────┬──────┘
                     │        │
        Normalization &    Buffering
                     │
      ┌──────────────┴─────────────────┐
      │                                │
   SHDR Adapters                     MQTT Broker
      │                                │
Raw Machine Signals         Sensor/Device Messages
```

This architecture is designed to be modular, allowing organizations to gradually expand their MTConnect ecosystem without reconfiguring existing systems.

---

# ⚡ Quick Start

There are 2 ways to start using the CPP agent:

## Using Pre-Built Binaries

This section helps developers get up and running with the agent in minutes.

### 1. Downloading Pre-Built Binaries

Pre-built binaries for Windows and Linux platforms are available on the [Releases](https://github.com/mtconnect/cppagent/releases) page.

### 2. Instructions for using Windows Binaries

1. Download the latest `.zip` release for Windows.
2. Extract the contents to a desired location.

### 3. Create a Minimal Configuration

```txt
Devices = Devices.xml
Port = 5000

Adapters {
  MyCNC {
    Host = localhost
    Port = 7878
  }
}
```

### 4. Run the Agent

```powershell
C> agent.exe run agent.cfg
```

### 5. Verify Output

- http://localhost:5000/probe
- http://localhost:5000/current
- http://localhost:5000/sample

---

## Building From Source

Building from source requires cloning the repo and then using Docker commands.

To see the full process, check out the [Wiki Page for Building From Source](https://github.com/mtconnect/cppagent/wiki/Building-From-Source).

---

# 🔧 Basic Configuration

The agent is controlled via two primary files:

### **1. Devices.xml**

Defines the machine structure, including components and DataItems.
This file gives meaning to raw data received by adapters.

### **2. agent.cfg**

Sets up communication ports, adapters, buffer sizes, logging, security, and optional features.

A minimal configuration looks like:

Example:

```
Devices = Devices.xml
Port = 5000
AllowPut = true
BufferSize = 17
```

**⚠️ Warning**: The `BufferSize` is specified in powers of 2. A value of 17 means $2^{17}$ = 131,072 slots.

# 🔧 Advanced Configuration

## **Overview of Advanced Features**

Some capabilities of the agent require additional setup or are only needed in more complex environments. These include:

- connecting multiple adapters to one or more devices
- securing communication through TLS
- ingesting MQTT-based data sources
- applying custom logic using Ruby
- enabling optional services like asset ingestion, data transforms, or extended namespaces

The following sections explain when and why you would use these features, along with short examples.

## Multiple Adapters

The agent supports gathering data from multiple devices at the same time. Each device communicates using its own adapter defined by a host and port.

Use this when:

- you want one agent instance to serve multiple machines,
- you want to reduce infrastructure overhead,
- machines are located on the same subnet or facility network.

Example:

```
Adapters {
  Mill1 {
    Host = 10.0.0.10
    Port = 7878
  }
  Lathe1 {
    Host = 10.0.0.20
    Port = 7879
  }
  Router {
    Host = router.local
    Port = 9000
  }
}
```

Each entry represents one physical machine producing SHDR output.

---

## HTTP & TLS

HTTP is the standard protocol for MTConnect clients to retrieve data.
TLS (HTTPS) is optional but recommended when:

- data crosses network boundaries,
- sensitive operation data must be encrypted,
- compliance or security policies require secure transport.

TLS enables the agent to serve MTConnect data securely using certificates.

Example configuration:

```
TlsDefaults {
  PrivateKeyFile = server.key
  CertificateFile = server.crt
  VerifyClientCertificate = true
}
```

After configuration, the agent serves encrypted data over `https://`.

---

## MQTT

The agent supports MQTT ingestion for devices or sensors that publish data to a broker instead of using SHDR.

Use MQTT when:

- you have applications that already using MQTT
- you want to publish data from a local broker to the cloud
- you want a lightweight protocol for high-frequency data

The agent requires the broker address, topics, and connection details:

```
MqttService {
  Host = tcp://broker:1883
  Topics = factory/mtconnect/#
  ClientId = agent01
}
```

Additional message mapping rules may be needed depending on your topic structure and payload format.

### Embedded MQTT Broker Adapter

The agent can also act as the MQTT broker itself (`Protocol = mqtt_broker`). Devices connect and publish directly to the agent; every publication is forwarded to any other subscribers, and publications matching `Topics` are ingested with the same topic and payload mapping as the MQTT client adapter (`Protocol = mqtt`).

```
Adapters {
  Cell {
    Protocol = mqtt_broker
    Device = Mill
    Topics = Mill/#
    MqttHost = 0.0.0.0          # bind address, default 0.0.0.0
    MqttPort = 8883             # default 1883, or 8883 with TLS
    MqttTls = true
    TlsCertificateChain = server.crt
    TlsPrivateKey = server.key
    MqttUserName = device
    MqttPassword = secret
  }
}
```

* Listener, transport, and credential settings (`MqttHost`, `MqttPort`, `MqttTls`, `MqttWs`, `MqttUserName`, `MqttPassword`, `MqttBrokerSecurity`) are only read from the adapter block, so they do not conflict with the MQTT sink settings. TLS certificate settings fall back to the top level values.
* `TlsVerifyClientCertificate = true` with `TlsClientCAs` rejects TLS clients without a valid client certificate. `TlsCertificatePassword` and `TlsDHKey` are optional.
* `MqttBrokerSecurity` is the path to an mqtt_cpp broker security JSON file for multiple users, salted `sha256` passwords, groups, and per-topic publish/subscribe ACLs. It replaces `MqttUserName`/`MqttPassword`. For example:

  ```
  {
    "authentication": [
      { "name": "device", "method": "sha256", "salt": "abc", "digest": "<sha256(salt + password) in hex>" },
      { "name": "dashboard", "method": "plain_password", "password": "viewer" }
    ],
    "authorization": [
      { "topic": "Mill/#", "allow": { "pub": ["device"], "sub": ["device", "dashboard"] } }
    ]
  }
  ```
* With neither credentials nor a security file, clients connect anonymously and a warning is logged.

---

## Ruby Extensions

Ruby extensions allow you to customize how data is processed by the agent before it is exposed to clients. These scripts can:

- normalize vendor-specific data formats,
- apply mathematical transformations (e.g., unit conversions),
- filter or enrich incoming data,
- modify assets or samples before buffering,
- implement custom logic not present in the MTConnect standard.

The agent loads Ruby scripts from the current directory:

```
Ruby {
  module = mymodule.rb
}
```

You can then write Ruby code that provides tranformation of the data in the pipeline.

## SHDR (Simple Hierarchical Data Representation)

### What SHDR Is

SHDR is the original MTConnect text-based protocol for transmitting machine state and data items. It streams information as timestamped key-value pairs over a plain TCP connection. It is simple, deterministic, and extremely reliable — which is why it continues to be the standard for CNC machine adapters.

Example SHDR line:

```
2025-05-01T10:24:31.123Z|Xpos|12.345
```

### Why SHDR Exists

- CNC controls and legacy equipment often cannot support modern protocols like MQTT or HTTP
- SHDR provides a low-overhead, real-time stream that adapters can implement with minimal dependencies
- It was intentionally created by MTConnect to reduce the complexity of machine data collection

### When to Use SHDR

Choose SHDR if:

- you are connecting to Fanuc, Mazak, Okuma, Haas, etc.
- you need real-time telemetry with minimal latency and low overhead
- you are using existing MTConnect adapters shipped with machine vendors

### Alternatives

| Protocol | Best for                                 | Notes                               |
| -------- | ---------------------------------------- | ----------------------------------- |
| SHDR     | CNC & legacy equipment                   | Most common MTConnect adapter input |
| MQTT     | IoT sensors, PLC networks, mixed devices | Requires broker & topic mapping     |

### Documentation & Reference

- MTConnect Adapters: [https://github.com/mtconnect/adapter](https://github.com/mtconnect/adapter)

### Why SHDR Is Only for MTConnect

Unlike MQTT, SHDR is not a general IoT messaging protocol. It only exists in MTConnect ecosystems and is purpose-built for CNC-level real-time telemetry. It remains widely deployed but many modern environments now run blended architectures:

```
CNC → SHDR → Agent
IoT Sensor → MQTT → Agent
```

---

# 📤 Sample Outputs

Understanding what the agent returns helps developers build clients effectively.

### /probe

Shows device structure:

```xml
<MTConnectDevices ...>
...
  <Device id="dev1" name="CNC1">
    <Components>
      <Axes>...</Axes>
    </Components>
  </Device>
</MTConnectDevices>
```

### /current

Shows the latest sample for every data item.

```xml
<Position dataItemId="X" sequence="12345" timestamp="..." ...>12.345</Position>
```

### /sample

Shows buffered time series data.

```xml
<CuttingSpeed dataItemId="X" sequence="12345" timestamp="..." ...>3500</CuttingSpeed>
```

These outputs comply with the MTConnect schema and can be validated using standard tools.

---

# Configuration Parameters

This section provides an overview of all the necessary configurable parameters.

### Top level configuration items

- `Devices` - Path to the Devices.xml file defining the machine structure.

  _Default_: `Devices.xml`

- `Port` - The TCP port number the agent listens on for HTTP requests.

  _Default_: 5000

- `BufferSize` - The 2^X number of slots available in the circular
  buffer for samples, events, and conditions.

  _Default_: 17 -> 2^17 = 131,072 slots.

- `CheckpointFrequency` - The frequency checkpoints are created in the
  stream. This is used for current with the at argument. This is an
  advanced configuration item and should not be changed unless you
  understand the internal workings of the agent.

  _Default_: 1000

* `DisconnectRemovesAllAssets` - When an adapter disconnects, remove all assets for the
  device it is the primary adapter for (the adapter's `Device`). Assets of devices listed in
  `AdditionalDevices` are not removed. This can be overridden on a per adapter basis.

  _Default_: false

* `IgnoreTimestamps` - Overwrite timestamps with the agent time. This will correct
  clock drift but will not give as accurate relative time since it will not take into
  consideration network latencies. This can be overridden on a per adapter basis.

    *Default*: false

- `MaxAssets` - The maximum number of assets that can be stored in memory.

  _Default_: 1024

- `SchemaVersion` - The MTConnect Schema version to use for output.

  _Default_: _Current supported version_

* `Validation` - Turns on validation of model components and observations

    *Default*: `false`

* `WorkerThreads` - The number of operating system threads dedicated to the Agent

    *Default*: 1


Make sure to checkout all [Configuration Parameters Wiki Page](https://github.com/mtconnect/cppagent/wiki/Configuration-Parameters)

---

# 🩺 Troubleshooting

| Issue            | Cause                 | Solution                |
| ---------------- | --------------------- | ----------------------- |
| Missing data     | Adapter not connected | Check host/port         |
| Device not found | Wrong Devices.xml     | Fix file path           |
| MQTT ignored     | Wrong topic           | Check configuration     |
| TLS errors       | Bad certs             | Regenerate certificates |

---

# 🤝 Contributing

We welcome all contributions to the agent, from bug fixes to documentation improvements.

This repository typically follows GitHub Flow:

1. Fork the repository
2. Create a feature branch
3. Add/update tests
4. Ensure builds pass
5. Submit PR

---

> Note: We need to have links to the configuration docs for all the options. We can reference the wiki.

# 📝 Version History

### Version 2.7

This release adds the following assets:
- Part Archetype
- Part
- Process Archetype
- Process
- Task Archetype
- Task

The asset models allow us to communicate information about parts and processes, and they are manufactured on the shop floor. Tasks are used to coordinate activities involving multiple machines.

### Version 2.6

* Added changes to support version 2.6 REST error response documents. 
* Support for  `AssetAdded` event that adds to `AssetChanged` and `AssetRemoved`. 
* Fixed WebSocket error reporting and handling. 
* Added support for the new Error Response types in REST API.
* Validation for samples and other data types.
* Improved handling of ID conflicts in Devices.xml when dyn dynamically adding devices.

### Version 2.5

* Support for validation of the MTConnect Streams document
  * In the configuration file set: `Validation = true`
  * At present, it only validates controlled vocabulary (enumerations). In future releases, we will validate all types.
* Added support for new asset models: Pallet and Fixture
* Supports WebSockets communication using the REST interface
  * See Wiki for more information (https://github.com/mtconnect/cppagent/wiki)
* Deprecated the old MQTT Server, topics now mirror probe, current, and streams.
  * See wiki for more information (https://github.com/mtconnect/cppagent/wiki)
* Added support for DataSet represetation of geometric transformations in Coordnate Systems, Solid Models, and Motion.

### Complete Version History

Please see the Releases page: [MTConnect Agent Releases](https://github.com/mtconnect/cppagent/releases)

---

# 🔗 Useful Links

- https://mtconnect.org
- https://github.com/mtconnect
- https://github.com/mtconnect/adapter

---

# 📄 License

[Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)
