
MTConnect C++ Agent Version 1.7
--------
[![Build status](https://ci.appveyor.com/api/projects/status/8ijbprd7rtwohv7c?svg=true)](https://ci.appveyor.com/project/WilliamSobel/cppagent-dev)

The C++ Agent provides the a complete implementation of the HTTP
server required by the MTConnect standard. The agent provides the
protocol and data collection framework that will work as a standalone
server. Once built, you only need to specify the XML description of
the devices and the location of the adapter.

Pre-built binary releases for Windows are available from [Releases](https://github.com/mtconnect/cppagent/releases) for those who do not want to build the agent themselves. For *NIX users, you will need libxml2, cppunit, and cmake as well as build essentials.

Version 1.7.0.0 added kinematics, solid models, and new specifications types.

Version 1.6.0.0 added coordinate systems, specifications, and tabular data.

Version 1.5.0.0 added Data Set capabilities and has been updated to use C++ 14.

Version 1.4.0.0 added time period filter constraint, compositions, initial values, and reset triggers.

Version 1.3.0.0 added the filter constraints, references, cutting tool archetypes, and formatting styles.

Version 1.2.0.0 added the capability to support assets.

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
       debug          Runs the agent on the command line with verbose logging
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

Download cmake from [cmake](http://www.cmake.org/cmake/resources/software.html)

Make sure to initialize submodules:

    git submodule init
    git submodule update

Configure cmake using the `CMakeLists.txt` file in the agent
directory. This will generate a project file for the target
platform. See CMake documentation for more information.



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

    http://example.com:5000/schemas/MTConnectStreams_1.3.xsd will map to ../schemas/MTConnectStreams_1.3.xsd

All files will be mapped and the directory names do not need to be the same. These files can be either served directly or can be used to extend the schema or add XSLT stylesheets for formatting the XML in browsers.

### Specifying the Extended Schemas ###

To specify the new schema for the documents, use the following declaration:

    StreamsNamespaces {
      e {
        Urn = urn:example.com:ExampleStreams:1.3
        Location = /schemas/ExampleStreams_1.3.xsd
      }
    }

This will use the ExampleStreams_1.3.xsd schema in the document. The `e` is the alias that will be
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
	    Location = /schemas/MTConnectStreams_1.4.xsd
	    Path = ./MTConnectStreams_1.4.xsd
	  }
	}

	DevicesNamespaces {
	  m {
	    Location = /schemas/MTConnectDevices_1.4.xsd
	    Path = ./MTConnectDevices_1.4.xsd
	  }
	}
    
The MTConnect agent will now serve the standard MTConnect schema files
from the local directory using the schema path /schemas/MTConnectDevices_1.4.xsd.


### Example: 10 ###

We can also serve files from the MTConnect Agent as well. In this example
we can assume we don't have access to the public internet and we would still 
like to provide the MTConnect streams and devices files but have the MTConnect
Agent serve them up locally. 

	DevicesNamespaces {
	  x {
	    Urn = urn:example.com:ExampleDevices:1.4
	    Location = /schemas/ExampleDevices_1.4.xsd
	    Path = ./ExampleDevices_1.4.xsd
	  }

	Files {
	  stream { 
	    Location = /schemas/MTConnectStreams_1.4.xsd
	    Path = ./MTConnectStreams_1.4.xsd
	  }
	  device { 
	    Location = /schemas/MTConnectDevices_1.4.xsd
	    Path = ./MTConnectDevices_1.4.xsd
	  }
	}
	
Or use the short form for all files:

        Files {
          schemas { 
            Location = /schemas/MTConnectStreams_1.4.xsd
            Path = ./MTConnectStreams_1.4.xsd
          }
        }
    
If you have specified in your xs:include schemaLocation inside the 
ExampleDevices_1.4.xsd file the location "/schemas/MTConnectStreams_1.4.xsd",
this will allow it to be served properly. This can also be done using the 
Devices namespace:

	DevicesNamespaces {
	  m {
	    Location = /schemas/MTConnectDevices_1.4.xsd
	    Path = ./MTConnectDevices_1.4.xsd
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

Configuration Parameters
---------

### Top level configuration items ####

* `BufferSize` - The 2^X number of slots available in the circular
  buffer for samples, events, and conditions.

    *Default*: 17 -> 2^17 = 131,072 slots.

* `MaxAssets` - The maximum number of assets the agent can hold in its buffer. The
  number is the actual count, not an exponent.

    *Default*: 1024

* `CheckpointFrequency` - The frequency checkpoints are created in the
  stream. This is used for current with the at argument. This is an
  advanced configuration item and should not be changed unless you
  understand the internal workings of the agent.

    *Default*: 1000

* `Devices` - The XML file to load that specifies the devices and is
  supplied as the result of a probe request. If the key is not found
  the defaults are tried.

    *Defaults*: probe.xml or Devices.xml 

* `PidFile` - UNIX only. The full path of the file that contains the
  process id of the daemon. This is not supported in Windows.

    *Default*: agent.pid

* `ServiceName` - Changes the service name when installing or removing 
  the service. This allows multiple agents to run as services on the same machine.

    *Default*: MTConnect Agent

* `Port`	- The port number the agent binds to for requests.

    *Default*: 5000
    
* `ServerIp` - The server IP Address to bind to. Can be used to select the interface in IPV4 or IPV6.

    *Default*: 0.0.0.0

* `AllowPut`	- Allow HTTP PUT or POST of data item values or assets.

    *Default*: false

* `AllowPutFrom`	- Allow HTTP PUT or POST from a specific host or 
  list of hosts. Lists are comma (,) separated and the host names will
  be validated by translating them into IP addresses.

    *Default*: none

* `LegacyTimeout`	- The default length of time an adapter can be silent before it
  is disconnected. This is only for legacy adapters that do not support heartbeats.

    *Default*: 600

* `ReconnectInterval` - The amount of time between adapter reconnection attempts. 
  This is useful for implementation of high performance adapters where availability
  needs to be tracked in near-real-time. Time is specified in milliseconds (ms).
      
    *Default*: 10000
      
* `IgnoreTimestamps` - Overwrite timestamps with the agent time. This will correct
  clock drift but will not give as accurate relative time since it will not take into
  consideration network latencies. This can be overridden on a per adapter basis.
  
    *Default*: false

* `PreserveUUID` - Do not overwrite the UUID with the UUID from the adapter, preserve
  the UUID in the Devices.xml file. This can be overridden on a per adapter basis.

    *Default*: true
    
* `SchemaVersion` - Change the schema version to a different version number.

    *Default*: 1.7

* `ConversionRequired` - Global default for data item units conversion in the agent. 
  Assumes the adapter has already done unit conversion.

    *Default*: true
    
* `UpcaseDataItemValue` - Always converts the value of the data items to upper case.

    *Default*: true

* `MonitorConfigFiles` - Monitor agent.cfg and Devices.xml files and restart agent if they change.

    *Default*: false

* `MinimumConfigReloadAge` - The minimum age of a config file before an agent reload is triggered (seconds).

    *Default*: 15

* `Pretty` - Pretty print the output with indententation

    *Default*: false

* `ShdrVersion` - Specifies the SHDR protocol version used by the adapter. When greater than one (1), allows multiple complex observations, like `Condition` and `Message` on the same line. If it equials one (1), then any observation requiring more than a key/value pair need to be on separate lines. This is the default for all adapters.

    *Default*: 1


### Adapter configuration items ###

* `Adapters` - Adapters begins a list of device blocks. If the Adapters
  are not specified and the Devices file only contains one device, a
  default device entry will be created with an adapter located on the
  localhost and port 7878 associated with the device in the devices
  file.

    *Default*: localhost 5000 associated with the default device

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
      recorded event.

        *Default*: false

    * `ConversionRequired` - Adapter setting for data item units conversion in the agent. 
      Assumes the adapter has already done unit conversion. Defaults to global.

        *Default*: Top Level Setting
        
    * `UpcaseDataItemValue` - Always converts the value of the data items to upper case.

        *Default*: Top Level Setting

	* `ShdrVersion` - Specifies the SHDR protocol version used by the adapter. When greater than one (1), allows multiple complex observations, like `Condition` and `Message` on the same line. If it equials one (1), then any observation requiring more than a key/value pair need to be on separate lines. Applies to only this adapter.

	    *Default*: 1


logger_config configuration items
-----

* `logger_config` - The logging configuration section.

    * `logging_level` - The logging level: `trace`, `debug`, `info`, `warn`,
        `error`, or `fatal`.

        *Default*: `info`

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
    
Adapter Agent Protocol Version 1.4
=======

The principle adapter data format is a simple plain text stream separated by the pipe character `|`. Every line except for commands starts with an optional timestamp in UTC. If the timestamp is not supplied the agent will supply a timestamp of its own taken at the arrival time of the data to the agent. The remainder of the line is a key followed by data – depending on the type of data item is being written to.

A simple set of events and samples will look something like this:

	2009-06-15T00:00:00.000000|power|ON|execution|ACTIVE|line|412|Xact|-1.1761875153|Yact|1766618937

A line is a sequence of fields separated by `|`. The simplest requires one key/value pair. The agent will discard any lines where the data is malformed. The end must end with a LF (ASCII 10) or CR-LF (ASCII 15 followed by ASCII 10) (UNIX or Windows conventions respectively). The key will map to the data item using the following items: the `id` attribute, the `name` attribute, and the `CDATA` of the `Source` element. If the key does not match it will be rejected and the agent will log the first time it fails. Different data items categories and types require a different number of tokens. The following rules specify the requirements for the adapter tokens:

If the value itself contains a pipe character `|` the pipe must be escaped using a leading backslash `\`. In addition the entire value has to be wrapped in quotes:

        2009-06-15T00:00:00.000000|description|"Text with \| (pipe) character."

Conditions require six (6) fields as follows:

	<timestamp>|<data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>
	
For a complete description of these fields, see the standard. An example line will look like this:

	2014-09-29T23:59:33.460470Z|htemp|WARNING|HTEMP|1|HIGH|Oil Temperature High
	
The next special format is the Message. There is one additional field, native_code, which needs to be included:

	2014-09-29T23:59:33.460470Z|message|CHG_INSRT|Change Inserts
	
Time series data also gets special treatment, the count and optional frequency are specified. In the following example we have 10 items at a frequency of 100hz:

	2014-09-29T23:59:33.460470Z|current|10|100|1 2 3 4 5 6 7 8 9 10

The data item name can also be prefixed with the device name if this adapter is supplying data to multiple devices. The following is an example of a power meter for three devices named `device1`, `device2`, and `device3`:

	2014-09-29T23:59:33.460470Z|device1:current|12|device2:current|11|device3:current|10

All data items follow the formatting requirements in the MTConnect standard for the vocabulary and coordinates like PathPosition.

A new feature in version 1.4 is the ability to announce a reset has been triggered. If we have a part count named `pcount` that gets reset daily, the new protocol is as follows:

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

Assets
-----

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

Commands
-----

There are a number of commands that can be sent as part of the adapter stream. These change some dynamic elements of the device information, the interpretation of the data, or the associated default device. Commands are given on a single line starting with an asterisk `* ` as the first character of the line and followed by a <key>: <value>. They are as follows:

* Specify the Adapter Software Version the adapter supports:

	`* adapterVersion: <version>`

* Set the calibration in the device component of the associated device:
 
	`* calibration: XXX`

* Tell the agent that the data coming from this adapter requires conversion:
 
	`* conversionRequired: <yes|no>`

* Specify the default device for this adapter. The device can be specified as either the device name or UUID:

	`* device: <uuid|name>`

* Set the description in the device header of the associated device:
 
	`* description: XXX`

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

Any other command will be logged as a warning.

Protocol
----

The agent and the adapter have a heartbeat that makes sure each is responsive to properly handle disconnects in a timely manner. The Heartbeat frequency is set by the adapter and honored by the agent. When the agent connects to the adapter, it first sends a `* PING` and then expects the response `* PONG <timeout>` where `<timeout>` is specified in milliseconds. So if the following communications are given:

Agent:

	* PING

Adapter:

	* PONG 10000

This indicates that the adapter is expecting a `PING` every 10 seconds and if there is no `PING`, in 2x the frequency, then the adapter should close the connection. At the same time, if the agent does not receive a `PONG` within 2x frequency, then it will close the connection. If no `PONG` response is received, the agent assumes the adapter is incapable of participating in heartbeat protocol and uses the legacy time specified above.

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
	
# Building the agent on Windows and Ubuntu

## Building on Windows

The MTConnect Agent uses the conan package manager to install dependencies:

  [Conan Package Manager Downloads](https://conan.io/downloads.html)

Download the Windows installer for your platform and run the installer.

You also need CMake [CMake](https://cmake.org/download/) and git [git](https://git-scm.com/download/win)

### Setting up build

Clone the agent to another directory:

    cd [home-directory]
    git clone git@github.com:/mtconnect/cppagent_dev.git

Make a build subdirectory of `cppagent_dev`

    mkdir cppagent_dev\build	
    cd cppagent_dev\build
	
####  For the 64 bit build
	
    conan install .. -s build_type=Release --build=missing
    cmake -G "Visual Studio 16 2019" -A x64 ..
	
#### For the Win32 build for XP

The windows XP 140 XP toolchain needs to be installed under individual component in the Visual Studio 2019 installer.
	
    conan install ..\cppagent_dev -s build_type=Release --build=missing -s compiler.toolset=v140_xp -e define=WINVER=0x0501 -s arch=x86
    cmake -G "Visual Studio 16 2019" -A Win32 -T v141_xp -D AGENT_ENABLE_UNITTESTS=false -D WINVER=0x0501 ..

### Build from the command line

#### For the x64 build

    cmake --build . --config Release --target ALL_BUILD
	
#### Test and package the release

    ctest -C Release
    cpack -G ZIP

#### For the Win32 build

    cmake --build . --config Release --target ALL_BUILD

#### Package the release

    cpack -G ZIP

## Building on Ubuntu on 20.04 LTS

### Setup the build

    sudo apt-get install build-essential boost python3.9 git

### Download the source

	git clone git@github.com:/mtconnect/cppagent_dev.git
	
### Install the dependencies

	pip3 install conan
	mkdir build
	conan profile new default --detect
	conan profile update settings.compiler.libcxx=libstdc++11 default
	conan install .. -s build_type=Release --build=missing
	
### Build the agent
	
	cmake -D CMAKE_BUILD_TYPE=Release ../
	cmake --build .
	
### Test the release

	ctest -C Release

## Building on Mac OS

Install brew and xcode command line tools

### Setup the build

    brew install git

### Download the source

	git clone git@github.com:/mtconnect/cppagent_dev.git
	
### Install the dependencies

	pip3 install conan
	mkdir build
	conan install .. -s build_type=Release --build=missing
	
### Build the agent
	
	cmake -D CMAKE_BUILD_TYPE=Release ../
	cmake --build .
	
### Test the release

	ctest -C Release

### For XCode
   
    cmake -G Xcode ..
