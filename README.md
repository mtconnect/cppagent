
MTConnect C++ Agent Version 2.5
--------
[![Build MTConnect C++ Agent](https://github.com/mtconnect/cppagent/actions/workflows/build.yml/badge.svg)](https://github.com/mtconnect/cppagent/actions/workflows/build.yml)

The C++ Agent provides the a complete implementation of the HTTP
server required by the MTConnect standard. The agent provides the
protocol and data collection framework that will work as a standalone
server. Once built, you only need to specify the XML description of
the devices and the location of the adapter.

**NOTE: This version cannot currently be built on Windows XP since there is currently no support for the XP toolchain and C++ 17.**

Pre-built binary releases for Windows are available from [Releases](https://github.com/mtconnect/cppagent/releases) for those who do not want to build the agent themselves. For *NIX users, you will need libxml2, cppunit, and cmake as well as build essentials.

Version 2.5.0 Support for version 2.5. Added validation of observations in the stream and WebSockets support.

Version 2.4.0 Added support for version 2.4

Version 2.3.0 Support for all Version 2.3 standard changes and JSON ingress to MQTT adapter.

Version 2.2.0 Support for all Version 2.2 standard changes and dynamic configuration from adapters. Upgrade to conan 2.

Version 2.1.0 Added MQTT Sink, Agent Restart and new JSON format (version 2)

Version 2.0.0 Rearchitecture of the agent with TLS, MQTT Adapter, Ruby Interpreter, Agent Adaptr, and much more

Version 1.7.0 added kinematics, solid models, and new specifications types.

Version 1.6.0 added coordinate systems, specifications, and tabular data.

Version 1.5.0 added Data Set capabilities and has been updated to use C++ 14.

Version 1.4.0 added time period filter constraint, compositions, initial values, and reset triggers.

Version 1.3.0 added the filter constraints, references, cutting tool archetypes, and formatting styles.

Version 1.2.0 added the capability to support assets.

Version 1.1.0.8 add the ability to run the C++ Agent as a Windows
service and support for a configuration file instead of command line
arguments. The agent can accept input from a socket in a pipe (`|`)
delimited stream according to the descriptions given in the adapter
guide.

The current win32 binary is built statically and requires no
additional dlls to run. This allows for a single exe distributable.

Usage
------

    agent [help|install|debug|run] [configuration_file]
       help           Prints this message
       install        Installs the service
       remove         Remove the service
       debug          Runs the agent on the command line with verbose logging -
                      sets logging_level to debug
       run            Runs the agent on the command line
       config_file    The configuration file to load
                      Default: agent.cfg in current directory

When the agent is started without any arguments it is assumed it will
be running as a service and will begin the service initialization
sequence. The full path to the configuration file is stored in the
registry in the following location:

    \\HKEY_LOCAL_MACHINE\SOFTWARE\MTConnect\MTConnect Agent\ConfigurationFile

Directories
-----

* `agent/`        - This contains the application source files and CMake file.

* `assets/`       - Some example Cutting Tool asset files.

* `lib/`          - Third party library source. Contains source for cppunit, dlib, and libxml++.

* `samples/`      - Sample XML configuration files mainly used for tests.

* `simulator/`    - Ruby scripts to execute an adapter simulator, both command-line and log-replay.

* `test/`         - Various unit tests.

* `tools/`        - Ruby scripts to dump the agent and the adapter in SHDR format. Includes a sequence
                    test script.

* `unix/`         - Unix init.d script

* `win32/`        - Libraries required only for the win32 build and the win32 solution.

Windows Binary Release
-----

The windows binary releases come with a prebuilt exe that is
statically linked with the Microsoft Runtime libraries. Aside from the
standard system libraries, the agent only requires winsock
libraries. The agent has been test with version of Windows 2000 and
later.

* `bin/`         - Win32 binary (no dependencies required).

Building
-------

Platform specific instructions are at the end of the README.

Configuration
------

The configuration file is using the standard Boost C++ file
format. The configuration file format is flexible and allows for both
many adapters to be served from one agent and an adapter to feed
multiple agents. The format is:

    Key = Value

The key can only occur once within a section and the value can be any
sequence of characters followed by a `<CR>`. There no significance to
the order of the keys, so the file can be specified in free form. We
will go over some configurations from a minimal configuration to more
complex multi-adapter configurations.

### Serving Static Content ###

Using a Files Configuration section, individual files or directories can be inserted
into the request file space from the agent. To do this, we use the Files top level
configuration declaration as follows:

    Files {
        schemas {
            Path = ../schemas
            Location = /schemas/
        }
        styles {
            Path = ../styles
            Location = /styles/
        }
    }

Each set of files must be declared using a named file description, like schema or
styles and the local `Path` and the `Location` the files will be mapped to in the
HTTP server namespace. For example:

    http://example.com:5000/schemas/MTConnectStreams_2.0.xsd will map to ../schemas/MTConnectStreams_2.0.xsd

All files will be mapped and the directory names do not need to be the same. These files can be either served directly or can be used to extend the schema or add XSLT stylesheets for formatting the XML in browsers.

### Specifying the Extended Schemas ###

To specify the new schema for the documents, use the following declaration:

    StreamsNamespaces {
      e {
        Urn = urn:example.com:ExampleStreams:2.0
        Location = /schemas/ExampleStreams_2.0.xsd
      }
    }

This will use the ExampleStreams_2.0.xsd schema in the document. The `e` is the alias that will be
used to reference the extended schema. The `Location` is the location of the xsd file relative in
the agent namespace. The `Location` must be mapped in the `Files` section.

An optional `Path` can be added to the `...Namespaces` declaration instead of declaring the
`Files` section. The `Files` makes it easier to include multiple files from a directory and will
automatically include all the default MTConnect schema files for the correct version.
(See `SchemaVersion` option below)

You can do this for any one of the other documents:

    StreamsNamespaces
    DevicesNamespaces
    AssetsNamespaces
    ErrorNamespaces

### Specifying the XML Document Style ###

The same can be done with style sheets, but only the Location is required.

    StreamsStyle {
      Location = /styles/Streams.xsl
    }

An optional `Path` can also be used to reference the xsl file directly. This will not
include other files in the path like css or included xsl transforms. It is advised to
use the `Files` declaration.

The following can also be declared:

    DevicesStyle
    StreamsStyle
    AssetsStyle
    ErrorStyle

### Example 1: ###

Here’s an example configuration file. The `#` character can be used to
comment sections of the file. Everything on the line after the `#` is
ignored. We will start with an extremely simple configuration file.

    # A very simple file...
    Devices = VMC-3Axis.xml

This is a one line configuration that specifies the XML file to load
for the devices. Since all the values are defaulted, an empty
configuration file can be specified. This configuration file will load
the `VMC-3Axis.xml` file and try to connect to an adapter located on the
localhost at port 7878. `VMC-3Axis.xml` must only contain the definition
for one device; if more devices exist, an error will be raised and the
process will exit.

### Example 2: ###

Most configuration files will specify at least one adapter. The
adapters are contained within a block. The Boost configuration file
format allows for nested configurations and block associations. There
are a number of configurations that can be given for each
adapter. Multiple adapters can be specified for one device as well.

    Devices = VMC-3Axis.xml

    Adapters
    {
        VMC-3Axis
        {
            Host = 192.168.10.22
            Port = 7878 # *Default* value...
        }
    }

This example loads the devices file as before, but specifies the list
of adapters. The device is taken from the name `VMC-3Axis` that starts
the nested block and connects to the adapter on `192.168.10.22` and port
`7878`. Another way of specifying the equivalent configuration is:

    Devices = VMC-3Axis.xml

    Adapters
    {
        Adapter_1
        {
            Device = VMC-3Axis
            Host = 192.168.10.22
            Port = 7878 # *Default* value...
        }
    }

Line 7 specifies the Device name associated with this adapter
explicitly. We will show how this is used in the next example.

### Example 3: ###

Multiple adapters can supply data to the same device. This is done by
creating multiple adapter entries and specifying the same Device for
each.

    Devices = VMC-3Axis.xml

    Adapters
    {
        Adapter_1
        {
            Device = VMC-3Axis
            Host = 192.168.10.22
            Port = 7878 # *Default* value...
        }

        # Energy sensor
        Adapter_2
        {
            Device = VMC-3Axis
            Host = 192.168.10.2
            Port = 7878 # *Default* value...
        }
    }

Both `Adapter_1` and `Adapter_2` will feed the `VMC-3Axis` device with
different data items.  The `Adapter_1` name is arbitrary and could just
as well be named `EnergySensor` if desired as illustrated below.

    Devices = VMC-3Axis.xml

    Adapters
    {
        Controller
        {
            Device = VMC-3Axis
            Host = 192.168.10.22
            Port = 7878 # *Default* value...
        }

        EnergySensor
        {
            Device = VMC-3Axis
            Host = 192.168.10.2
            Port = 7878 # *Default* value...
        }
    }

### Example 4: ###

In this example we change the port to 80 which is the default http port.
This also allows HTTP PUT from the local machine and 10.211.55.2.

    Devices = MyDevices.xml
    Port = 80
    AllowPutFrom = localhost, 10.211.55.2

    Adapters
    {
        ...

For browsers you will no longer need to specify the port to connect to.

### Example 5: ###

If multiple devices are specified in the XML file, there must be an
adapter feeding each device.

    Devices = MyDevices.xml

    Adapters
    {
        VMC-3Axis
        {
            Host = 192.168.10.22
        }

        HMC-5Axis
        {
            Host = 192.168.10.24
        }
    }

This will associate the adapters for these two machines to the VMC
and HMC devices in `MyDevices.xml` file. The ports are defaulted to
7878, so we are not required to specify them.

### Example 6: ###

In this example we  demonstrate how to change the service name of the agent. This
allows a single machine to run multiple agents and/or customize the name of the service.
Multiple configuration files can be created for each service, each with a different
ServiceName. The configuration file must be referenced as follows:

    C:> agent install myagent.cfg

If myagent.cfg contains the following statements:

    Devices = MyDevices.xml
    ServiceName = MTC Agent 1

    Adapters
    {
        ...


The service will now be displayed as "MTC Agent 1" as opposed to "MTConnect Agent"
and it will automatically load the contents of myagent.cfg with it starts. You can now
use the following command to start this from a command prompt:

    C:> net start "MTC Agent 1"

To remove the service, do the following:

    C:> agent remove myagent.cfg

### Example 7: ###

Logging configuration is specified using the `logger_config` block. You
can change the `logging_level` to specify the verbosity of the logging
as well as the destination of the logging output.

    logger_config
    {
        logging_level = debug
        output = file debug.log
    }

This will log everything from debug to fatal to the file
debug.log. For only fatal errors you can specify the following:

    logger_config
    {
        logging_level = fatal
    }

The default file is agent.log in the same directory as the agent.exe
file resides. The default logging level is `info`. To have the agent log
to the command window:

    logger_config
    {
        logging_level = debug
        output = cout
    }

This will log debug level messages to the current console window. When
the agent is run with debug, it sets the logging configuration to
debug and outputs to the standard output as specified above.

### Example: 8 ###

The MTConnect C++ Agent supports extensions by allowing you to specify your
own XSD schema files. These files must include the MTConnect schema and the top
level node is required to be MTConnect. The "x" in this case is the namespace. You
MUST NOT use the namespace "m" since it is reserved for MTConnect. See example 9
for an example of changing the MTConnect schema file location.

There are four namespaces in MTConnect: Devices, Streams, Assets, and Error. In
this example we will replace the Streams and Devices namespace with our own namespace
so we can have validatable XML documents.

	StreamsNamespaces {
	  x {
	    Urn = urn:example.com:ExampleStreams:1.2
	    Location = /schemas/ExampleStreams_1.2.xsd
	    Path = ./ExampleStreams_1.2.xsd
	  }
	}

	DevicesNamespaces {
	  x {
	    Urn = urn:example.com:ExampleDevices:1.2
	    Location = /schemas/ExampleDevices_1.2.xsd
	    Path = ./ExampleDevices_1.2.xsd
	  }
	}

For each schema file we have three options we need to specify. The Urn
is the urn in the schema file that will be used in the header. The Location
is the path specified in the URL when requesting the schema file from the
HTTP client and the Path is the path on the local file system.

### Example: 9 ###

If you only want to change the schema location of the MTConnect schema files and
serve them from your local agent and not from the default internet location, you
can use the namespace "m" and give the new schema file location. This MUST be the
MTConnect schema files and the urn will always be the MTConnect urn for the "m"
namespace -- you cannot change it.

	StreamsNamespaces {
	  m {
	    Location = /schemas/MTConnectStreams_2.0.xsd
	    Path = ./MTConnectStreams_2.0.xsd
	  }
	}

	DevicesNamespaces {
	  m {
	    Location = /schemas/MTConnectDevices_2.0.xsd
	    Path = ./MTConnectDevices_2.0.xsd
	  }
	}

The MTConnect agent will now serve the standard MTConnect schema files
from the local directory using the schema path /schemas/MTConnectDevices_2.0.xsd.


### Example: 10 ###

We can also serve files from the MTConnect Agent as well. In this example
we can assume we don't have access to the public internet and we would still
like to provide the MTConnect streams and devices files but have the MTConnect
Agent serve them up locally.

	DevicesNamespaces {
	  x {
	    Urn = urn:example.com:ExampleDevices:2.0
	    Location = /schemas/ExampleDevices_2.0.xsd
	    Path = ./ExampleDevices_2.0.xsd
	  }

	Files {
	  stream {
	    Location = /schemas/MTConnectStreams_2.0.xsd
	    Path = ./MTConnectStreams_2.0.xsd
	  }
	  device {
	    Location = /schemas/MTConnectDevices_2.0.xsd
	    Path = ./MTConnectDevices_2.0.xsd
	  }
	}

Or use the short form for all files:

        Files {
          schemas {
            Location = /schemas/MTConnectStreams_2.0.xsd
            Path = ./MTConnectStreams_2.0.xsd
          }
        }

If you have specified in your xs:include schemaLocation inside the
ExampleDevices_2.0.xsd file the location "/schemas/MTConnectStreams_2.0.xsd",
this will allow it to be served properly. This can also be done using the
Devices namespace:

	DevicesNamespaces {
	  m {
	    Location = /schemas/MTConnectDevices_2.0.xsd
	    Path = ./MTConnectDevices_2.0.xsd
	  }
	}

The MTConnect agent will allow you to serve any other files you wish as well. You
can specify a new static file you would like to deliver:

	Files {
	  myfile {
	    Location = /files/xxx.txt
	    Path = ./files/xxx.txt
	  }

The agent will not serve all files from a directory and will not provide an index
function as this is insecure and not the intended function of the agent.

Ruby
---------

If the "-o with_ruby=True" build is selected, then use to following configuration:

    Ruby {
      module = path/to/module.rb
    }

The module specified at the path given will be loaded. There are examples in the test/Resources/ruby directory in
github: [Ruby Tests](https://github.com/mtconnect/cppagent/tree/master/test/resources/ruby).

The current functionality is limited to the pipeline transformations from the adapters. Future changes will include adding sources and sinks.

The following is a complete example for fixing the Execution of a machine:

```ruby
class AlertTransform < MTConnect::RubyTransform
  def initialize(name, filter)
    @cache = Hash.new
    super(name, filter)
  end

  @@count = 0
  def transform(obs)
    @@count += 1
    if @@count % 10000 == 0
      puts "---------------------------"
      puts ">  #{ObjectSpace.count_objects}"
      puts "---------------------------"
    end

    dataItemId = obs.properties[:dataItemId]
    if dataItemId == 'servotemp1' or dataItemId == 'Xfrt' or dataItemId == 'Xload'
      @cache[dataItemId] = obs.value
      device = MTConnect.agent.default_device

      di = device.data_item('xaxisstate')
      if @cache['servotemp1'].to_f > 10.0 or @cache['Xfrt'].to_f > 10.0 or @cache['Xload'].to_f > 10
        newobs = MTConnect::Observation.new(di, "ERROR")
      else
        newobs = MTConnect::Observation.new(di, "OK")
      end
      forward(newobs)
    end
    forward(obs)
  end
end

MTConnect.agent.sources.each do |s|
  pipe = s.pipeline
  puts "Splicing the pipeline"
  trans = AlertTransform.new('AlertTransform', :Sample)
  puts trans
  pipe.splice_before('DeliverObservation', trans)
end
```

Configuration Parameters
---------

### Top level configuration items ####

* `AgentDeviceUUID` - Set the UUID of the agent device

    *Default*: UUID derived from the IP address and port of the agent

* `BufferSize` - The 2^X number of slots available in the circular
  buffer for samples, events, and conditions.

    *Default*: 17 -> 2^17 = 131,072 slots.

* `CheckpointFrequency` - The frequency checkpoints are created in the
  stream. This is used for current with the at argument. This is an
  advanced configuration item and should not be changed unless you
  understand the internal workings of the agent.

    *Default*: 1000

* `CreateUniqueIds`: Changes all the ids in each element to a UUID that will be unique across devices. This is used for merging devices from multiple sources.

    *Default*: `false`

* `Devices` - The XML file to load that specifies the devices and is
  supplied as the result of a probe request. If the key is not found
  the defaults are tried.

    *Defaults*: probe.xml or Devices.xml

* `DisableAgentDevice` - When the schema version is >= 1.7, disable the
  creation of the Agent device.

    *Default*: false

* `JsonVersion`     - JSON Printer format. Old format: 1, new format: 2

    *Default*: 2

* `LogStreams` - Debugging flag to log the streamed data to a file. Logs to a file named: `Stream_` + timestamp + `.log` in the current working directory. This is only for the Rest Sink.

    *Default*: `false`

* `MaxAssets` - The maximum number of assets the agent can hold in its buffer. The
  number is the actual count, not an exponent.

    *Default*: 1024

* `MaxCachedFileSize` - The maximum size of a raw file to cache in memory.

    *Default*: 20 kb

* `MinCompressFileSize` - The file size where we begin compressing raw files sent to the client.

    *Default*: 100 kb

* `MinimumConfigReloadAge` - The minimum age of a config file before an agent reload is triggered (seconds).

    *Default*: 15 seconds

* `MonitorConfigFiles` - Monitor agent.cfg and Devices.xml files and restart agent if they change.

    *Default*: false

* `MonitorInterval` - The interval between checks if the agent.cfg or Device.xml files have changed.

    *Default*: 10 seconds

* `Pretty` - Pretty print the output with indententation

    *Default*: false

* `PidFile` - UNIX only. The full path of the file that contains the
  process id of the daemon. This is not supported in Windows.

    *Default*: agent.pid

* `SchemaVersion` - Change the schema version to a different version number.

    *Default*: 2.0

* `Sender` - The value for the sender header attribute.

    *Default*: Local machine name

* `ServiceName` - Changes the service name when installing or removing
  the service. This allows multiple agents to run as services on the same machine.

    *Default*: MTConnect Agent

* `SuppressIPAddress` - Suppress the Adapter IP Address and port when creating the Agent Device ids and names. This applies to all adapters.

    *Default*: `false`

* `VersionDeviceXml` - Create a new versioned file every time the Device.xml file changes from an external source.

    *Default*: `false`

* `CorrectTimestamps` - Verify time is always progressing forward for each data item and correct if not.

    *Default*: `false`

* `Validation` - Turns on validation of model components and observations

    *Default*: `false`

* `WorkerThreads` - The number of operating system threads dedicated to the Agent

    *Default*: 1

#### Adapter General Configuration

These can be overridden on a per-adapter basis

* `Protocol` –Specify protocol. Options: [`shdr`|`mqtt`]

    *Default*: shdr

* `ConversionRequired` - Global default for data item units conversion in the agent.
  Assumes the adapter has already done unit conversion.

    *Default*: true

* `EnableSourceDeviceModels` - Allow adapters and data sources to supply Device
  configuration

    *Default*: false

* `Heartbeat` – Overrides the heartbeat interval sent back from the adapter in the
   `* PONG <hb>`. The heartbeat will always be this value in milliseconds.

    *Default*: _None_

* `IgnoreTimestamps` - Overwrite timestamps with the agent time. This will correct
  clock drift but will not give as accurate relative time since it will not take into
  consideration network latencies. This can be overridden on a per adapter basis.

    *Default*: false

* `LegacyTimeout`	- The default length of time an adapter can be silent before it
  is disconnected. This is only for legacy adapters that do not support heartbeats.

    *Default*: 600

* `PreserveUUID` - Do not overwrite the UUID with the UUID from the adapter, preserve
  the UUID in the Devices.xml file. This can be overridden on a per adapter basis.

    *Default*: true

* `ReconnectInterval` - The amount of time between adapter reconnection attempts.
  This is useful for implementation of high performance adapters where availability
  needs to be tracked in near-real-time. Time is specified in milliseconds (ms).

    *Default*: 10000

* `ShdrVersion` - Specifies the SHDR protocol version used by the adapter. When greater than one (1),
  allows multiple complex observations, like `Condition` and `Message` on the same line. If it equials one (1),
  then any observation requiring more than a key/value pair need to be on separate lines. This is the default for all adapters.

    *Default*: 1

* `UpcaseDataItemValue` - Always converts the value of the data items to upper case.

    *Default*: true

#### REST Service Configuration

* `AllowPut`	- Allow HTTP PUT or POST of data item values or assets.

    *Default*: false

* `AllowPutFrom`	- Allow HTTP PUT or POST from a specific host or
  list of hosts. Lists are comma (,) separated and the host names will
  be validated by translating them into IP addresses.

    *Default*: none

* `HttpHeaders`     - Additional headers to add to the HTTP Response for CORS Security

    > Example: ```
    > HttpHeaders {
    >   Access-Control-Allow-Origin = *
    >   Access-Control-Allow-Methods = GET
    >   Access-Control-Allow-Headers = Content-Type
    > }```

* `Port`	- The port number the agent binds to for requests.

    *Default*: 5000

* `ServerIp` - The server IP Address to bind to. Can be used to select the interface in IPV4 or IPV6.

    *Default*: 0.0.0.0


#### Configuration Pameters for TLS (https) Support ####

The following parameters must be present to enable https requests. If there is no password on the certificate, `TlsCertificatePassword` may be omitted.

* `TlsCertificateChain` - The name of the file containing the certificate chain created from signing authority

    *Default*: *NULL*

* `TlsCertificatePassword` -  The password used when creating the certificate. If none was supplied, do not use.

    *Default*: *NULL*

* `TlsClientCAs` - For `TlsVerifyClientCertificate`, specifies a file that contains additional certificate authorities for verification

    *Default*: *NULL*

* `TlsDHKey` -  The name of the file containing the Diffie–Hellman key

    *Default*: *NULL*

* `TlsOnly` -  Only allow secure connections, http requests will be rejected

    *Default*: `false`

* `TlsPrivateKey` -  The name of the file containing the private key for the certificate

    *Default*: *NULL*

* `TlsVerifyClientCertificate` - Request and verify the client certificate against root authorities

    *Default*: false

### MQTT Configuration

* `MqttCaCert` - CA Certificate for MQTT TLS connection to the MTT Broker

    *Default*: *NULL*

* `MqttHost` - IP Address or name of the MQTT Broker

    *Default*: 127.0.0.1

* `MqttPort` - Port number of MQTT Broker

    *Default*: 1883

* `MqttTls` - TLS Certificate for secure connection to the MQTT Broker

    *Default*: `false`

* `MqttWs` - Instructs MQTT to connect using web sockets

    *Default*: `false`

#### MQTT Sink

> **⚠️ Deprecation Notice:** The origional `MqttService` has been deprecated. `MqttService` is now an alias for `Mqtt2Service`. Please use `MqttService` for new configurations. `Mqtt2Service` will continue to work for backward compatibility but may be removed in a future version.

**Deprecated Configuration (still supported):**

Enabled in `agent.cfg` by specifying:

```
Sinks {
  MqttService {
    # Configuration Options below...
  }
}
```

> **Note:** Both `MqttService` and `Mqtt2Service` use the same underlying implementation and support the same configuration options listed below.

#### MQTT Sink Configuration

Enabled in `agent.cfg` by specifying:

```
Sinks {
  Mqtt2Service {
    # Configuration Options...
  }
}
```

* `AssetTopic` - Prefix for the Assets

    *Default*: `MTConnect/Asset/[device]`

* `CurrentTopic` - Prefix for the Current

    *Default*: `MTConnect/Current/[device]`

* `ProbeTopic` or `DeviceTopic` - Prefix for the Device Model topic

    > Note: `[device]` will be replaced with the uuid of each device. Other patterns can be created,
    > for example: `MTConnect/[device]/Probe` will group by device instead of operation.
    > `DeviceTopic` will also work.

    *Default*: `MTConnect/Probe/[device]`

* `SampleTopic` - Prefix for the Sample

    *Default*: `MTConnect/Sample/[device]`

* `MqttLastWillTopic` - The topic used for the last will and testement for an agent

    > Note: The value will be `AVAILABLE` when the Agent is publishing and connected and will
    > publish `UNAVAILABLE` when the agent disconnects from the broker.

    *Default*: `MTConnect/Probe/[device]/Availability"`

* `MqttCurrentInterval` - The frequency to publish currents. Acts like a keyframe in a video stream.

    *Default*: 10000ms

* `MqttSampleInterval` - The frequency to publish samples. Works the same way as the `interval` in the rest call. Groups observations up and publishes with the minimum interval given. If nothing is availble, will wait until an observation arrives to publish.

    *Default*: 500ms

* `MqttSampleCount` - The maxmimum number of observations to publish at one time.

    *Default*: 1000

* `MqttRetain` - For the MQTT Sinks, sets the retain flag for publishing.

    *Default*: True

* `MqttQOS`: - For the MQTT Sinks, sets the Quality of Service. Must be one of `at_least_once`, `at_most_once`, `exactly_once`.

    *Default*: `at_least_once`

* `MqttXPath`: - The xpath filter to apply to all current and samples published to MQTT. If the XPath is invalid, it will fall back to publishing all data items.

    *Default*: All data items

### Adapter Configuration Items ###

* `Adapters` - Contains a list of device blocks. If there are no Adapters
  specified and the Devices file contains one device, the Agent defaults
  to an adapter located on the localhost at port 7878.  Data passed from
  the Adapter is associated with the default device.

    *Default*: localhost 7878, associated with the default device

    * `Device` - The name of the device that corresponds to the name of
      the device in the Devices file. Each adapter can map to one
      device. Specifying a "*" will map to the default device.

        *Default*: The name of the block for this adapter or if that is
        not found the default device if only one device is specified
        in the devices file.

    * `Host` - The host the adapter is located on.

        *Default*: localhost

    * `Port` - The port to connect to the adapter.

        *Default*: 7878

    * `Manufacturer` - Replaces the manufacturer attribute in the device XML.

        *Default*: Current value in device XML.

    * `Station` - Replaces the Station attribute in the device XML.

        *Default*: Current value in device XML.

    * `SerialNumber` - Replaces the SerialNumber attribute in the device XML.

        *Default*: Current value in device XML.

    * `UUID` - Replaces the UUID attribute in the device XML.

        *Default*: Current value in device XML.

    * `AutoAvailable` - For devices that do not have the ability to provide available events, if `yes`, this sets the `Availability` to AVAILABLE upon connection.

        *Default*: no (new in 1.2, if AVAILABILITY is not provided for device it will be automatically added and this will default to yes)

    * `AdditionalDevices` - Comma separated list of additional devices connected to this adapter. This provides availability support when one adapter feeds multiple devices.

        *Default*: nothing

    * `FilterDuplicates` - If value is `yes`, filters all duplicate values for data items. This is to support adapters that are not doing proper duplicate filtering.

        *Default*: no

    * `LegacyTimeout` - length of time an adapter can be silent before it
        is disconnected. This is only for legacy adapters that do not support
        heartbeats. If heartbeats are present, this will be ignored.

        *Default*: 600

    * `ReconnectInterval` - The amount of time between adapter reconnection attempts.
       This is useful for implementation of high performance adapters where availability
       needs to be tracked in near-real-time. Time is specified in milliseconds (ms).
       Defaults to the top level ReconnectInterval.

        *Default*: 10000

    * `IgnoreTimestamps` - Overwrite timestamps with the agent time. This will correct
      clock drift but will not give as accurate relative time since it will not take into
      consideration network latencies. This can be overridden on a per adapter basis.

        *Default*: Top Level Setting

    * `PreserveUUID` - Do not overwrite the UUID with the UUID from the adapter, preserve
		  the UUID in the Devices.xml file. This can be overridden on a per adapter basis.

		    *Default*: false

    * `RealTime` - Boost the thread priority of this adapter so that events are handled faster.

        *Default*: false

    * `RelativeTime` - The timestamps will be given as relative offsets represented as a floating
      point number of milliseconds. The offset will be added to the arrival time of the first
      recorded event. If the timestamp is given as a time, the difference between the agent time
      and the fitst incoming timestamp will be used as a constant clock adjustment.

        *Default*: false

    * `ConversionRequired` - Adapter setting for data item units conversion in the agent.
      Assumes the adapter has already done unit conversion. Defaults to global.

        *Default*: Top Level Setting

    * `UpcaseDataItemValue` - Always converts the value of the data items to upper case.

        *Default*: Top Level Setting

    * `ShdrVersion` - Specifies the SHDR protocol version used by the adapter. When greater than one
      (1), allows  multiple complex observations, like `Condition` and `Message` on the same line.
      If it equials one (1), then any observation requiring more than a key/value pair need to be on
      separate lines. Applies to only this adapter.

	    *Default*: 1

    * `SuppressIPAddress` - Suppress the Adapter IP Address and port when creating the Agent Device ids and names.

        *Default*: false

	* `AdapterIdentity` - Adapter Identity name used to prefix dataitems within the Agent device ids and names.

        *Default*:
		* If `SuppressIPAddress` == false:\
		`AdapterIdentity` = ```_ {IP}_{PORT}```\
		example:`_localhost_7878`

		* If `SuppressIPAddress` == true:\
		`AdapterIdentity` = ```_ sha1digest({IP}_{PORT})```\
		example: `__71020ed1ed`

#### MQTT Adapter/Source

* `MqttHost` - IP Address or name of the MQTT Broker

    *Default*: 127.0.0.1

* `MqttPort` - Port number of MQTT Broker

    *Default*: 1883

* `topics` - list of topics to subscribe to. Note : Only raw SHDR strings supported at this time

    *Required*

* `MqttClientId` - Port number of MQTT Broker

    *Default*: Auto-generated

	> **⚠️Note:** Mqtt Sinks and Mqtt Adapters create separate connections to their respective brokers, but currently use the same client ID by default. Because of this, when using a single broker for source and sink, best practice is to explicitly specify their respective `MqttClientId`
	>

	Example mqtt adapter block:
	```json
	mydevice {
			Protocol = mqtt
			MqttHost = localhost
			MqttPort = 1883
			MqttClientId = myUniqueID
			Topics = /ingest
		}
	```

* `AdapterIdentity` - Adapter Identity name used to prefix dataitems within the Agent device ids and names.

	*Default*:
	* If `SuppressIPAddress` == false:\
	`AdapterIdentity` = ```_ {IP}_{PORT}```\
	example:`_localhost_7878`

	* If `SuppressIPAddress` == true:\
	`AdapterIdentity` = ```_ sha1digest({IP}_{PORT})```\
	example: `__71020ed1ed`

### Agent Adapter Configuration

* `Url` - The URL of the source agent. `http:` or `https:` are accepted for the protocol.
* `SourceDevice` – The Device name or UUID for the source of the data
* `Count` – the number of items request during a single sample

    *Default*: 1000

* `Polling Interval` – The interval used for streaming or polling in milliseconds

    *Default*: 500ms

* `Reconnect Interval` – The interval between reconnection attampts in milliseconds

    *Default*: 10000ms

* `Use Polling` – Force the adapter to use polling instead of streaming. Only set to `true` if x-multipart-replace blocked.

    *Default*: false

* `Heartbeat` – The heartbeat interval from the server

    *Default*: 10000ms

logger_config configuration items
-----

* `logger_config` - The logging configuration section.

    * `logging_level` - The logging level: `trace`, `debug`, `info`, `warn`,
        `error`, or `fatal`.

        *Default*: `info`

        Note: when running Agent with `agent debug`, `logging_level` will be set to `debug`.

    * `output` - The output file or stream. If using a file, specify
      as: `"file <filename>"`. cout and cerr can be used to specify the
      standard output and standard error streams. *Default*s to the same
      directory as the executable.

        *Default*: file `adapter.log`

    * `max_size` - The maximum log file size. Suffix can be K for kilobytes, M for megabytes, or
      G for gigabytes. No suffix will default to bytes (B). Case is ignored.

        *Default*: 10M

    * `max_index` - The maximum number of log files to keep.

        *Default*: 9

    * `schedule` - The scheduled time to start a new file. Can be DAILY, WEEKLY, or NEVER.

        *Default*: NEVER

# Agent-Adapter Protocols
## SHDR Version 2.0

The principle adapter data format is a simple plain text stream separated by the pipe character `|`. Every line except for commands starts with an optional timestamp in UTC. If the timestamp is not supplied the agent will supply a timestamp of its own taken at the arrival time of the data to the agent. The remainder of the line is a key followed by data – depending on the type of data item is being written to.

A simple set of events and samples will look something like this:

	2009-06-15T00:00:00.000000|power|ON|execution|ACTIVE|line|412|Xact|-1.1761875153|Yact|1766618937

A line is a sequence of fields separated by `|`. The simplest requires one key/value pair. The agent will discard any lines where the data is malformed. The end must end with a LF (ASCII 10) or CR-LF (ASCII 15 followed by ASCII 10) (UNIX or Windows conventions respectively). The key will map to the data item using the following items: the `id` attribute, the `name` attribute, and the `CDATA` of the `Source` element. If the key does not match it will be rejected and the agent will log the first time it fails. Different data items categories and types require a different number of tokens. The following rules specify the requirements for the adapter tokens:

If the value itself contains a pipe character `|` the pipe must be escaped using a leading backslash `\`. In addition the entire value has to be wrapped in quotes:

        2009-06-15T00:00:00.000000|description|"Text with \| (pipe) character."

Conditions require six (6) fields as follows:

	<timestamp>|<data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>

	Condition id and native code are set to the same value given as <native_code>

	<timestamp>|<data_item_name>|<level>|<native_code>:<condition_id>|<native_severity>|<qualifier>|<message>

	Condition id is set to condition_id and native code is set to native_code

	<timestamp>|<data_item_name>|<level>|<condition_id>|<native_severity>|<qualifier>|<message>

	Condition id is set to condition_id and native code is not set


For a complete description of these fields, see the standard. An example line will look like this:

	2014-09-29T23:59:33.460470Z|htemp|WARNING|HTEMP-1-HIGH|HTEMP|1|HIGH|Oil Temperature High

The next special format is the Message. There is one additional field, native_code, which needs to be included:

	2014-09-29T23:59:33.460470Z|message|CHG_INSRT|Change Inserts

Time series data also gets special treatment, the count and optional frequency are specified. In the following example we have 10 items at a frequency of 100hz:

	2014-09-29T23:59:33.460470Z|current|10|100|1 2 3 4 5 6 7 8 9 10

The data item name can also be prefixed with the device name if this adapter is supplying data to multiple devices. The following is an example of a power meter for three devices named `device1`, `device2`, and `device3`:

	2014-09-29T23:59:33.460470Z|device1:current|12|device2:current|11|device3:current|10

All data items follow the formatting requirements in the MTConnect standard for the vocabulary and coordinates like PathPosition.

A new feature introduced in version 1.4 is the ability to announce a reset has been triggered. If we have a part count named `pcount` that gets reset daily, the new protocol is as follows:

	2014-09-29T23:59:33.460470Z|pcount|0:DAY

To specify the duration of the static, indicate it with an `@` sign after the timestamp as follows:

	2014-09-29T23:59:33.460470Z@100.0|pcount|0:DAY

### `DATA_SET` Representation ###

A new feature in version 1.5 is the `DATA_SET` representation which allows for key value pairs to be given. The protocol is similar to time series where each pair is space delimited. The agent automatically removes duplicate values from the stream and allows for addition, deletion and resetting of the values. The format is as follows:

	2014-09-29T23:59:33.460470Z|vars|v1=10 v2=20 v3=30

This will create a set of three values. To remove a value

	2014-09-29T23:59:33.460470Z|vars|v2 v3=

This will remove v2 and v3. If text after the equal `=` is empty or the `=` is not given, the value is deleted. To clear the set, specify a `resetTriggered` value such as `MANUAL` or `DAY` by preceeding it with a colon `:` at the beginning of the line.

	2014-09-29T23:59:33.460470Z|vars|:MANUAL

This will remove all the values from the current set. The set can also be reset to a specific set of values:

	2014-09-29T23:59:33.460470Z|vars|:MANUAL v5=1 v6=2

This will remove all the old values from the set and set the current set. Values will accumulate when addition pairs are given as in:

	2014-09-29T23:59:33.460470Z|vars|v8=1 v9=2 v5=10

This will add values for v8 and v9 and update the value for v5 to 10. If the values are duplcated they will be removed from the stream unless a `resetTriggered` value is given.

	2014-09-29T23:59:33.460470Z|vars|v8=1 v9=2 v5=0

Will be detected as a duplicate with respect to the previous values and will be removed. If a partial update is given and the other values are duplicates, then will be stripped:

	2014-09-29T23:59:33.460470Z|vars|v8=1 v9=3 v5=10

Will be effectively the same as specifying:

	2014-09-29T23:59:33.460470Z|vars|v9=2

And the streams will only have the one value when a sample is request is made at that point in the stream.

When the `discrete` flag is set to `true` in the data item, all change tracking is ignored and each set is treated as if it is new.

One can quote values using the following methods: `"<text>"`, `'<text>'`, and `{<text>}`. Spaces and other characters can be included in the text and the matching character can be escaped with a `\` if it is required as follows: `"hello \"there\""` will yield the value: `hellow "there"`.

### `TABLE` Representation ###

A `TABLE` representation is similar to the `DATA_SET` that has a key value pair as the value. Using the encoding mentioned above, the following representations are allowed for tables:

    <timestamp>|wpo|G53.1={X=1.0 Y=2.0 Z=3.0 s='string with space'} G53.2={X=4.0 Y=5.0 Z=6.0} G53.3={X=7.0 Y=8.0 Z=9 U=10.0}

Using the quoting conventions explained in the previous section, the inner content can contain quoted text and exscaped values. The value is interpreted as a key/value pair in the same way as the `DATA_SET`. A table can be thought of as a data set of data sets.

All the reset rules of data set apply to tables and the values are treated as a unit.

## Assets

Assets are associated with a device but do not have a data item they are mapping to. They therefore get the special data item name `@ASSET@`. Assets can be sent either on one line or multiple lines depending on the adapter requirements. The single line form is as follows:

	2012-02-21T23:59:33.460470Z|@ASSET@|KSSP300R.1|CuttingTool|<CuttingTool>...

This form updates the asset id KSSP300R.1 for a cutting tool with the text at the end. For multiline assets, use the keyword `--multiline--` with a following unique string as follows:

		2012-02-21T23:59:33.460470Z|@ASSET@|KSSP300R.1|CuttingTool|--multiline--0FED07ACED
		<CuttingTool>
		...
		</CuttingTool>
		--multiline--0FED07ACED

The terminal text must appear on the first position after the last line of text. The adapter can also remove assets (1.3) by sending a @REMOVE_ASSET@ with an asset id:

	2012-02-21T23:59:33.460470Z|@REMOVE_ASSET@|KSSP300R.1

Or all assets can be removed in one shot for a certain asset type:

	2012-02-21T23:59:33.460470Z|@REMOVE_ALL_ASSETS@|CuttingTool

Partial updates to assets is also possible by using the @UPDATE_ASSET@ key, but this will only work for cutting tools. The asset id needs to be given and then one of the properties or measurements with the new value for that entity. For example to update the overall tool length and the overall diameter max, you would provide the following:

	2012-02-21T23:59:33.460470Z|@UPDATE_ASSET@|KSSP300R.1|OverallToolLength|323.64|CuttingDiameterMax|76.211

## MQTT JSON Ingress Protocol Version 2.0

In general the data format is {"timestamp": "YYYY-MM-DDThh:mm:ssZ","dataItemId":"value", "dataItemId":{"key1":"value1", ..., "keyn":"valuen}}

**NOTE**: See the standard for the complete description of the fields for the data item representations below.

A simple set of events and samples will look something like this:

```json
{
	"timestamp": "2023-11-06T12:12:44Z",			//Time Stamp
	"tempId": 22.6,								//Temperature
	"positionId": 1002.345,						//X axis position
	"executionId": "ACTIVE"						//Execution state
}
```

A `CONDITION` requires the key to be the dataItemId and requires the 6 fields as shown in the example below

```json
{
	"timestamp": "2023-11-06T12:12:44Z",
	"dataItemId": {
		"level": "fault",
		"conditionId":"ac324",
		"nativeSeverity": "1000",
		"qualifier": "HIGH",
		"nativeCode": "ABC",
		"message": "something went wrong"
	}
}
```
A `MESSAGE` requires the key to be the dataItemId and requires the nativeCode field as shown in the example below

```json
{
	"timestamp": "2023-11-06T12:12:44Z",
	"messsageId": {
		"nativeCode": "ABC",
		"message": "something went wrong"
	}
}
```

The `TimeSeries` `REPRESENTATION` requires the key to be the dataItemId and requires 2 fields "count" and "values" and 1 to n comma delimited values.
>**NOTE**: The "frequency" field is optional.

```json
{
	"timestamp": "2023-11-06T12:12:44Z",
	"timeSeries1": {
		"count": 10,
		"frequency": 100,
		"values": [1,2,3,4,5,6,7,8,9,10]
	}
}
```
The `DataSet` `REPRESENTATION` requires the the dataItemId as the key and the "values" field.   It may also have the optional "resetTriggered" field.

```json
{
{
	"timestamp": "2023-11-09T11:20:00Z",
	"dataSetId": {
		"key1": 123,
		"key2": 456,
		"key3": 789
	}
}
```

Example with the optional "resetTriggered" filed:

```json
{
	"timestamp": "2023-11-09T11:20:00Z",
	"cncregisterset1": {
		"resetTriggered": "NEW",
		"value": {"r1":"v1", "r2":"v2", "r3":"v3" }
	}
}
```

The `Table` `REPRESENTATION` requires the the dataItemId as the key and the "values" field.   It may also have the optional "resetTriggered" field.

```json

{
	"timestamp":"2023-11-06T12:12:44Z",
	"tableId":{
		"row1":{
		"cell1":"Some Text",
		"cell2":3243
		},
		"row2": {
		"cell1":"Some Other Text",
		"cell2":243
		}
	}
}
```

Example with the optional resetTriggered field:

```json
{
	"timestamp": "2023-11-09T11:20:00Z",
	"a1": {
		"resetTriggered": "NEW",
		"value": {
			"r1": {
				"k1": 123.45,
				"k3": 6789
			},
			"r2": null
		}
	}
}
```

## Adapter Commands
There are a number of commands that can be sent as part of the adapter stream over the SHDR port connection. These change some dynamic elements of the device information, the interpretation of the data, or the associated default device. Commands are given on a single line starting with an asterisk `* ` as the first character of the line and followed by a <key>: <value>. They are as follows:

* Specify the Adapter Software Version the adapter supports:

	`* adapterVersion: <version>`

* Set the calibration in the device component of the associated device:

	`* calibration: XXX`

* Tell the agent that the data coming from this adapter requires conversion:

	`* conversionRequired: <yes|no>`

* Set the description in the device header of the associated device:

	`* description: XXX`

* Specify the default device for this adapter. The device can be specified as either the device name or UUID:

	`* device: <uuid|name>`

* Tell the agent to load a new device XML model:

	`* devicemodel: <deviceXML>`

* Set the manufacturer in the device header of the associated device:

	`* manufacturer: XXX`

* Specify the MTConnect Version the adapter supports:

	`* mtconnectVersion: <version>`

* Set the nativeName in the device component of the associated device:

	`* nativeName: XXX`

* Tell the agent that the data coming from this adapter would like real-time priority:

	`* realTime: <yes|no>`

* Tell the agent that the data coming from this adapter is specified in relative time:

	`* relativeTime: <yes|no>`

* Set the serialNumber in the device header of the associated device:

	`* serialNumber: XXX`

* Specify the version of the SHDR protocol delivered by the adapter. See `ShdrVersion` above:

	`* shdrVersion: <version>`

* Set the station in the device header of the associated device:

	`* station: XXX`

* Set the uuid in the device header of the associated device if preserveUuid = false:

	`* uuid: XXX`

Any other command will be logged as a warning.

## Heartbeat Protocol

The agent and the adapter have a heartbeat that makes sure each is responsive to properly handle disconnects in a timely manner. The Heartbeat frequency is set by the adapter and honored by the agent. When the agent connects to the adapter, it first sends a `* PING` and then expects the response `* PONG <timeout>` where `<timeout>` is specified in milliseconds. So if the following communications are given:

Agent:

	* PING

Adapter:

	* PONG 10000

This indicates that the adapter is expecting a `PING` every 10 seconds and if there is no `PING`, in 2x the frequency, then the adapter should close the connection. At the same time, if the agent does not receive a `PONG` within 2x frequency, then it will close the connection. If no `PONG` response is received, the agent assumes the adapter is incapable of participating in heartbeat protocol and uses the legacy time specified above.

Just as with the SHDR protocol, these messages must end with an LF (ASCII 10) or CR-LF (ASCII 15 followed by ASCII 10).

HTTP PUT/POST Method of Uploading Data
-----

There are two configuration settings mentioned above: `AllowPut` and `AllowPutFrom`. `AllowPut` alone will allow any process to use `HTTP` `POST` or `PUT` to send data to the agent and modify values. To restrict this to a limited number of
machines, you can list the IP Addresses that are allowed to `POST` data to the agent.

An example would be:

    AllowPut = yes
    AllowPutFrom = 192.168.1.72, 192.168.1.73

This will allow the two machines to post data to the MTConnect agent. The data can be either data item values or assets. The primary use of this capability is uploading assets from a process or even the command line using utilities like curl. I'll be using curl for these examples.

For example, with curl you can use the -d option to send data to the server. The data will be in standard form data format, so all you need to do is to pass the `<data_item_name>=<data_item_value>` to set the values, as follows:

    curl -d 'avail=AVAILABLE&program_1=XXX' 'http://localhost:5000/ExampleDevice'

By specifying the device at the end of the URL, you tell the agent which device to use for the POST. This will set the availability tag to AVAILABLE and the program to XXX:

    <Availability dataItemId="dtop_3" timestamp="2015-05-18T18:20:12.278236Z" name="avail" sequence="65">AVAILABLE</Availability>
    ...
    <Program dataItemId="path_51" timestamp="2015-05-18T18:20:12.278236Z" name="program_1" sequence="66">XXX</Program>

The full raw data being passed over looks like this:

    => Send header, 161 bytes (0xa1)
    0000: POST /ExampleDevice HTTP/1.1
    001e: User-Agent: curl/7.37.1
    0037: Host: localhost:5000
    004d: Accept: */*
    005a: Content-Length: 29
    006e: Content-Type: application/x-www-form-urlencoded
    009f:
    => Send data, 29 bytes (0x1d)
    0000: avail=AVAILABLE&program_1=XXX
    == Info: upload completely sent off: 29 out of 29 bytes
    == Info: HTTP 1.0, assume close after body
    <= Recv header, 17 bytes (0x11)
    0000: HTTP/1.0 200 OK
    <= Recv header, 20 bytes (0x14)
    0000: Content-Length: 10
    <= Recv header, 24 bytes (0x18)
    0000: Content-Type: text/xml
    <= Recv header, 2 bytes (0x2)
    0000:
    <= Recv data, 10 bytes (0xa)
    0000: <success/>
    >== Info: Closing connection 0

This is using the --trace - to dump the internal data. The response will be a simple `<success/>` or `<fail/>`.

Any data item can be set in this fashion. Similarly conditions are set using the following syntax:

    curl -d 'system=fault|XXX|1|LOW|Feeling%20low' 'http://localhost:5000/ExampleDevice'

One thing to note, the data and values are URL encoded, so the space needs to be encoded as a %20 to appear correctly.

    <Fault dataItemId="controller_46" timestamp="2015-05-18T18:24:48.407898Z" name="system" sequence="67" nativeCode="XXX" nativeSeverity="1" qualifier="LOW" type="SYSTEM">Feeling Low</Fault>

Assets are posted in a similar fashion. The data will be taken from a file containing the XML for the content. The syntax is very similar to the other requests:

    curl -d @B732A08500HP.xml 'http://localhost:5000/asset/B732A08500HP.1?device=ExampleDevice&type=CuttingTool'

The @... uses the named file to pass the data and the URL must contain the asset id and the device name as well as the asset type. If the type is CuttingTool or CuttingToolArchetype, the data will be parsed and corrected if properties are out of order as with the adapter. If the device is not specified and there are more than one device in this adapter, it will cause an error to be returned.

Programmatically, send the data as the body of the POST or PUT request as follows. If we look at the raw data, you will see the data is sent over verbatim as follows:

    => Send header, 230 bytes (0xe6)
    0000: POST /asset/B732A08500HP.1?device=ExampleDevice&type=CuttingTool
    0040:  HTTP/1.1
    004b: User-Agent: curl/7.37.1
    0064: Host: localhost:5000
    007a: Accept: */*
    0087: Content-Length: 2057
    009d: Content-Type: application/x-www-form-urlencoded
    00ce: Expect: 100-continue
    00e4:
    == Info: Done waiting for 100-continue
    => Send data, 2057 bytes (0x809)
    0000: (file data sent here, see below...)
    == Info: HTTP 1.0, assume close after body
    <= Recv header, 17 bytes (0x11)
    0000: HTTP/1.0 200 OK
    <= Recv header, 20 bytes (0x14)
    0000: Content-Length: 10
    <= Recv header, 24 bytes (0x18)
    0000: Content-Type: text/xml
    <= Recv header, 2 bytes (0x2)
    0000:
    <= Recv data, 10 bytes (0xa)
    0000: <success/>
    <success/>== Info: Closing connection 0

The file that was included looks like this:

    <CuttingTool serialNumber="1 " toolId="B732A08500HP" timestamp="2011-05-11T13:55:22" assetId="B732A08500HP.1" manufacturers="KMT">
    	<Description>
    		Step Drill KMT, B732A08500HP Grade KC7315
    		Adapter KMT CV50BHPVTT12M375
    	</Description>
    	<CuttingToolLifeCycle>
    		<CutterStatus><Status>NEW</Status></CutterStatus>
    		<ProcessSpindleSpeed nominal="5893">5893</ProcessSpindleSpeed>
    		<ProcessFeedRate nominal="2.5">2.5</ProcessFeedRate>
    		<ConnectionCodeMachineSide>CV50 Taper</ConnectionCodeMachineSide>
    		<Measurements>
    			<BodyDiameterMax code="BDX">31.8</BodyDiameterMax>
    			<BodyLengthMax code="LBX" nominal="120.825" maximum="126.325" minimum="115.325">120.825</BodyLengthMax>
    			<ProtrudingLength code="LPR" nominal="155.75" maximum="161.25" minimum="150.26">158.965</ProtrudingLength>
    			<FlangeDiameterMax code="DF" nominal="98.425">98.425</FlangeDiameterMax>
    			<OverallToolLength nominal="257.35" minimum="251.85" maximum="262.85" code="OAL">257.35</OverallToolLength>
    		</Measurements>
    		<CuttingItems count="2">
    			<CuttingItem indices="1" manufacturers="KMT" grade="KC7315">
    				<Measurements>
    					<CuttingDiameter code="DC1" nominal="8.5" maximum="8.521" minimum="8.506">8.513</CuttingDiameter>
    					<StepIncludedAngle code="STA1" nominal="90" maximum="91" minimum="89">89.8551</StepIncludedAngle>
    					<FunctionalLength code="LF1" nominal="154.286" minimum="148.786" maximum="159.786">157.259</FunctionalLength>
    					<StepDiameterLength code="SDL1" nominal="9">9</StepDiameterLength>
    					<PointAngle code="SIG" nominal="135" minimum="133" maximum="137">135.1540</PointAngle>
    				</Measurements>
    			</CuttingItem>
    			<CuttingItem indices="2" manufacturers="KMT" grade="KC7315">
    				<Measurements>
    					<CuttingDiameter code="DC2" nominal="12" maximum="12.011" minimum="12">11.999</CuttingDiameter>
    					<FunctionalLength code="LF2" nominal="122.493" maximum="127.993" minimum="116.993">125.500</FunctionalLength>
    					<StepDiameterLength code="SDL2" nominal="9">9</StepDiameterLength>
    				</Measurements>
    			</CuttingItem>
    		</CuttingItems>
    	</CuttingToolLifeCycle>
    </CuttingTool>

An example in ruby is as follows:

    > require 'net/http'
    => true
    > h = Net::HTTP.new('localhost', 5000)
    => #<Net::HTTP localhost:5000 open=false>
    > r = h.post('/asset/B732A08500HP.1?type=CuttingTool&device=ExampleDevice', File.read('B732A08500HP.xml'))
    => #<Net::HTTPOK 200 OK readbody=true>
    > r.body
    => "<success/>"

# Building the agent

## Overview

The agent build is dependent on the following utilities:

* C++ Compiler compliant with C++ 17
* git is optional but suggested to download source and update when changes occur
* cmake for build generator and testing
* python 3 and pip to support conan for dependency and package management
* ruby and rake for mruby to support building the embedded scripting engine [not required if -o with_ruby=False]

## Conan MTConnect Options (set using `-o <option>=<value>`)

* `agent_prefix`: Prefix the agent and agent_lib executable. For example `-o agent_prefix=mtc` create mtcagent.exe and mtcagent_lib.lib

    *default*: ''

* `cpack`: At the end of the build, run cpack to create a deployable package. By default it will go in the build direct, see `cpack_destination` to change the default location. Values: `True` or `False`.

    *default*: False

* `development`: Create a build environment suitable for developemnt. Changes the test_package subdirectory by including it as part of the library build for easier debugging on IDEs and integrated build debug environments. Values: `True` or `False`.

    *default*: False

* `fPIC`: Enables Position Independent Code on *NIX platforms allowing the machine code to be dynamically relocate on load. Values: `True` or `False`.

    *default*: True

* `shared`: Specifies if the build will create shared libraries (.dll/.so/.dylib) or static libraries. Also makes dependent libraries dynamic as well. Packaging picks up all dependent libraries when creating the ZIP. This is useful when creating plugins since there is less duplication of dependent code. Values: `True` or `False`.

    *default*: False

* `winver`: For windows, version of the minimum target operating system version. Defaults to Windows Vista.

    *default*: 0x600

* `with_docs`: Enable generation of MTConnect Agent library documentation using doxygen. If true, will build the use conan to build doxygen if it is not installed. Values: `True` or `False`.

    *default*: False

* `with_ruby`: Enable mruby extensions for dynamic scripting. Values: `True` or `False`.

    *default*: True

* `without_ipv6`: Disable IPV6 support when building on operating systems that disable IPV6 services. Some docker images disable IPV6. Values: `True` or `False`.

    *default*: False

* `cpack_destination`: The destination directory for the package

   *default*: _Package build directory_

* `cpack_name`: The name of the package generated by cpack

   *default*: _Default package name: agent_{version}_{OS}_

* `cpack_generator`:

    *default*: `ZIP` for windows and `TGZ` for *NIX


### Conan useful settings (set using `-s <setting>=<value>`)

* `build_type`: Changes the debug or release build type. Values: `Release`, `Debug`, `RelWithDebInfo`, and `MinSizeRel`. See CMake documentation for explanations.

    *default*: `Release`

### Conan useful configurations (set using `-c <config>=<value>`)

* `tools.build:skip_test`: Stops conan from running the tests. Test package will still build. To disable tests from building, use `-tf ""` or `--testfolder=` to skip the building of tests.

    *default*: `False`

* `tools.build:jobs`: Sets the number of concurrent processes used in the build. If the machine runs out of resources, reduce the number of concurrent processes.

    *default*: _number of processes_

## Building on Windows

The MTConnect Agent uses the conan package manager to install dependencies:

  [python 3](https://www.python.org/downloads/) and [Conan Package Manager Downloads](https://conan.io/downloads.html)

Download the Windows installer for your platform and run the installer.

You also need [git](https://git-scm.com/download/win) and [ruby](https://rubyinstaller.org) if you want to embed mruby.

CMake is installed as part of Visual Studio. If you are using Visual Studio for the build, use the bundled version. Otherwise download from CMake [CMake](https://cmake.org/download/).

### Setting up build

Install dependencies from the downloads above. Make sure python, ruby, and cmake are in your path.

	pip install --upgrade pip
	pip install conan

Clone the agent to another directory:

	git clone https://github.com/mtconnect/cppagent.git

####  To build for 64 bit Windows

The following 64 and 32 bit builds will create the zip package in the directory where the repository was cloned. The build will occur in the .conan2 directory of the user's home directory.

Make sure to setup the environment:

    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"

or

    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
   	conan create cppagent -pr cppagent/conan/profiles/vs64 --build=missing -o cpack=True -o zip_destination=.

#### To build for 32 bit Windows

    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars32.bat"

or

    "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
   	conan create cppagent -pr cppagent/conan/profiles/vs32 --build=missing -o cpack=True -o zip_destination=.

## *NIX Builds

The minimum memory (main + swap) when building with on CPU is 3GB. Less than that will likely cause the build to fail.

If the build runs out of resources, there are two options, you can add swap or set the following command line option:

    -c tools.build:jobs=1

to instruct conan to not parallelize the builds. Some of the modules that include boost beast require significant resources to build.

## Building on Ubuntu on 20.04 LTS

### Setup the build

    sudo apt install -y build-essential cmake gcc-11 g++-11 python3 python3-pip autoconf automake ruby ruby rake
    python3 -m pip install conan
    echo 'export PATH=$HOME/.local/bin:$PATH' >> .bashrc

### Download the source

	git clone https://github.com/mtconnect/cppagent.git

### Build the agent

	conan create cppagent -pr cppagent/conan/profiles/gcc --build=missing

## Building on Mac OS

Install brew and xcode command line tools

	/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
	xcode-select —install

### Setup the build

    brew install git
    brew install cmake
	pip3 install conan

### Download the source

	git clone https://github.com/mtconnect/cppagent.git

### Build the agent

	conan create cppagent -pr cppagent/conan/profiles/macos --build=missing

### Generate an xcode project for debugging

	conan build . -pr conan/profiles/xcode -s build_type=Debug --build=missing -o development=True

## Building on Fedora Alpine

### As Root

	apk add g++ python3 cmake git linux-headers make perl ruby
	gem install rake

	python3 -m ensurepip
	python3 -m pip install --upgrade pip

### As the user

	export PATH=~/.local/bin:$PATH
	pip3 install conan
	git clone https://github.com/mtconnect/cppagent.git

### Build the agent

	conan create cppagent -pr cppagent/conan/profiles/gcc --build=missing

## For some examples, see the CI/CD workflows in `.github/workflows/build.yml`

# Creating Test Certifications (see resources gen_certs shell script)

This section assumes you have installed openssl and can use the command line. The subject of the certificate is only for testing and should not be used in production. This section is provided to support testing and verification of the functionality. A certificate provided by a real certificate authority should be used in a production process.

NOTE: The certificates must be generated with OpenSSL version 1.1.1 or later. LibreSSL 2.8.3 is not compatible with
      more recent version of SSL on raspian (Debian).

## Server Creating self-signed certificate chain

### Create Signing authority key and certificate

	openssl req -x509 -nodes -sha256 -days 3650 -newkey rsa:3072 -keyout rootca.key -out rootca.crt -subj "/C=US/ST=State/L=City/O=Your Company, Inc./OU=IT/CN=serverca.org"

### User Key

    openssl genrsa -out user.key 3072

#### Signing Request

    openssl req -new -sha256 -key user.key -out user.csr -subj "/C=US/ST=State/L=City/O=Your Company, Inc./OU=IT/CN=user.org"

#### User Certificate using root signing certificate

    openssl x509 -req -in user.csr -CA rootca.crt -CAkey rootca.key -CAcreateserial -out user.crt -days 3650

### Create DH Parameters

    openssl dhparam -out dh2048.pem 3072

### Verify

    openssl verify -CAfile rootca.crt rootca.crt
    openssl verify -CAfile rootca.crt user.crt

## Client Certificate

### Create Signing authority key

    openssl req -x509 -nodes -sha256 -days 3650 -newkey rsa:3072 -keyout clientca.key -out clientca.crt -subj "/C=US/ST=State/L=City/O=Your Company, Inc./OU=IT/CN=clientca.org"

### Create client key

    openssl genrsa -out client.key 3072

### Create client signing request

    openssl req -new -key client.key -out client.csr -subj "/C=US/ST=State/L=City/O=Your Company, Inc./OU=IT/CN=client.org"

### Create Client Certificate

For client.cnf

    basicConstraints = CA:FALSE
    nsCertType = client, email
    nsComment = "OpenSSL Generated Client Certificate"
    subjectKeyIdentifier = hash
    authorityKeyIdentifier = keyid,issuer
    keyUsage = critical, nonRepudiation, digitalSignature, keyEncipherment
    extendedKeyUsage = clientAuth, emailProtection

### Create the cert

    openssl x509 -req -in client.csr -CA clientca.crt -CAkey clientca.key -out client.crt -CAcreateserial -days 3650 -sha256 -extfile client.cnf

### Verify

    openssl verify -CAfile clientca.crt clientca.crt
    openssl verify -CAfile clientca.crt client.crt

# Docker

See [demo ](demo) or [docker](docker)
