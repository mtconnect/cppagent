
MTConnect C++ Agent Version 1.3.0.0
--------

The C++ Agent provides the a complete implementation of the HTTP
server required by the MTConnect standard. The agent provides the
protocol and data collection framework that will work as a standalone
server. Once built, you only need to specify the XML description of
the devices and the location of the adapter.

Pre-built binary releases for Windows are available from [Releases](https://github.com/mtconnect/cppagent/releases) for those who do not want to build the agent themselves. For *NIX users, you will need libxml2, cppunit, and cmake as well as build essentials.

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

* `tools/`        - Ruby scripts to dump the agent and and the adapter in SHDR format. Includes a sequence 
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

###Serving Static Content###

Using a Files Configuration section, indvidual files or directories can be 
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

###Specifying the Extended Schemas###

To specify the new schema for the documents, use the following declaration:

    StreamsNamespaces {
      e {
        Urn = urn:example.com:ExampleStreams:1.3
        Location = /schemas/ExampleStreams_1.3.xsd
      }
    }

This will use the ExampleStreams_1.3.xsd schema in the document. The `e` is the alias that will be
used the reference the extended schema. The `Location` is the location of the xsd file relative in 
the agent namespace. The `Location` must be mapped in the `Files` section.

An optional `Path` can be added to the `...Namespaces` declaration instead of declaring the 
`Files` section. The `Files` makes it easier to include multiple files from a directory and will
automatically include all the default MTConnect schema files for the correct version. 
(See `SchameVersion` option below)

You can do this for any one of the other documents: 

    StreamsNamespaces
    DevicesNamespaces
    AssetsNamespaces
    ErrorNamespaces

###Specifying the XML Document Style###

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

###Example 1:###

Here’s an example configuration file. The `#` character can be used to
comment sections of the file. Everything on the line after the `#` is
ignored. We will start with a extremely simple configuration file.

    # A very simple file...
    Devices = VMC-3Axis.xml

This is a one line configuration that specifies the XML file to load
for the devices. Since all the values are defaulted, an empty
configuration configuration file can be specified. This configuration
file will load the `VMC-3Axis.xml` file and try to connect to an adapter
located on the localhost at port 7878. `VMC-3Axis.xml` must only contain
the definition for one device, if more devices exist, and error will
be raised and the process will exit.

###Example 2:###

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
7878. Another way of specifying the equivalent configuration is:

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

###Example 3:###

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

###Example 4:###

In this example we change the port to 80 which is the default http port. 
This also allows HTTP PUT from the local machine and 10.211.55.2. 

    Devices = MyDevices.xml
    Port = 80
    AllowPutFrom = localhost, 10.211.55.2

    Adapters
    {
        ...

For browsers you will no longer need to specify the port to connect to.

###Example 5:###

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

This will map associate the adapters for these two machines to the VMC
and HMC devices in `MyDevices.xml` file. The ports are defaulted to
7878, so we are not required to specify them.

###EXample 6:###

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

###Example 7:###

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
the agent is run with debug, it is sets the logging configuration to
debug and outputs to the standard output as specified above.

###Example: 8###

The MTConnect C++ Agent supports extensions by allowing you to specify your
own XSD schema files. These files must include the mtconnect schema and the top
level node is required to be MTConnect. The "x" in this case is the namespace. You
MUST NOT use the namespace "m" since it is reserved for MTConnect. See example 9
for an example of changing the MTConnect schema file location.

There are four namespaces in MTConnect, Devices, Streams, Assets, and Error. In 
this example we will replace the Streams and devices namespace with our own namespace
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
is the path specified in the URL when to request the schema file from the 
HTTP client and the Path is the path on the local file system.

###Example: 9###

If you only want to change the schema location of the MTConnect schema files and 
serve them from your local agent and not from the default internet location, you
can use the namespace "m" and give the new schema file location. This MUST be the 
MTConnect schema files and the urn will always be the MTConnect urn for the "m" 
namespace -- you cannot change it.

	StreamsNamespaces {
	  m {
	    Location = /schemas/MTConnectStreams_1.2.xsd
	    Path = ./MTConnectStreams_1.2.xsd
	  }
	}

	DevicesNamespaces {
	  m {
	    Location = /schemas/MTConnectDevices_1.2.xsd
	    Path = ./MTConnectDevices_1.2.xsd
	  }
	}
    
The MTConnect agent will now serve the standard MTConnect schema files
from the local directory using the schema path /schemas/MTConnectDevices_1.2.xsd.


###Example: 10###

We can also serve files from the MTConnect Agent as well. In this example
we can assume we don't have access to the public internet and we would still 
like to provide the MTConnect streams and devices files but have the MTConnect
Agent serve them up locally. 

	DevicesNamespaces {
	  x {
	    Urn = urn:example.com:ExampleDevices:1.2
	    Location = /schemas/ExampleDevices_1.2.xsd
	    Path = ./ExampleDevices_1.2.xsd
	  }

	Files {
	  stream { 
	    Location = /schemas/MTConnectStreams_1.2.xsd
	    Path = ./MTConnectStreams_1.2.xsd
	  }
	  device { 
	    Location = /schemas/MTConnectDevices_1.2.xsd
	    Path = ./MTConnectDevices_1.2.xsd
	  }
	}
	
Or use the short form for all files:

  Files {
    schemas { 
      Location = /schemas/MTConnectStreams_1.2.xsd
      Path = ./MTConnectStreams_1.2.xsd
    }
  }
    
If you have specified in your xs:include schemaLocation inside the 
ExampleDevices_1.2.xsd file the location "/schemas/MTConnectStreams_1.2.xsd",
this will allow it to be served properly. This will can also be done using the 
devices namespace:

	DevicesNamespaces {
	  m {
	    Location = /schemas/MTConnectDevices_1.2.xsd
	    Path = ./MTConnectDevices_1.2.xsd
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

###Top level configuration items####

* `BufferSize` - The 2^X number of slots available in the circular
  buffer for samples, events, and conditions.

    *Default*: 17 -> 2^17 = 131,072 slots.

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
  be validated by translating them into ip addresses.

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
  consideration network latencies. This can be overriden on a per adapter basis.
  
    *Default*: false

* `PreserveUUID` - Do not overwrite the UUID with the UUID from the adapter, preserve
  the UUID in the Devices.xml file. This can be overridden on a per adapter basis.

    *Default*: true
    
* `SchemaVersion` - Change the schema version to a different version number.

    *Default*: 1.3

* `ConversionRequired` - Global default for data item units conversion in the agent. 
  Assumes the adapter has already done unit conversion.

    *Default*: true
    
* `UpcaseDataItemValue` - Always converts the value of the data items to upper case.

    *Default*: true

* `MonitorConfigFiles` - Monitor agent.cfg and Devices.xml files and restart agent if they change.

    *Default*: false

###Adapter configuration items###

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

    * `AdditionalDevices` - Comma separated list of additional devices connected to this adapter. This provides availability support when one addapter feeds multiple devices.

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
       Defaults to the top level ReconnetInterval.
      
        *Default*: 10000
        
    * `IgnoreTimestamps` - Overwrite timestamps with the agent time. This will correct
      clock drift but will not give as accurate relative time since it will not take into
      consideration network latencies. This can be overriden on a per adapter basis.

        *Default*: Top Level Setting
        
    * `PreserveUUID` - Do not overwrite the UUID with the UUID from the adapter, preserve
		  the UUID in the Devices.xml file. This can be overridden on a per adapter basis.

		    *Default*: false
		
    * `RealTime` - Boost the thread priority of this adapter so that events are handled faster.

        *Default*: false
    
    * `RelativeTime` - The timestamps will be given as relative offests represented as a floating 
      point number of milliseconds. The offset will be added to the arrival time of the first 
      recorded event.

        *Default*: false

    * `ConversionRequired` - Adapter setting for data item units conversion in the agent. 
      Assumes the adapter has already done unit conversion. Defaults to global.

        *Default*: Top Level Setting
        
    * `UpcaseDataItemValue` - Always converts the value of the data items to upper case.

        *Default*: Top Level Setting


logger_config configuration items
-----

* `logger_config` - The logging configuration section.

    * `logging_level` - The logging level: `trace`, `debug`, `info`, `warn`,
        `error`, or `fatal`.

        *Default*: `info`

    * `output` - The output file or stream. If a file is specified specify
      as:
 "file <filename>". cout and cerr can be used to specify the
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
    
Adapter Agent Protocol Version 1.3
=======

The principle adapter data format is a simple plain text stream separated by the pipe character `|`. Every line except for commands starts with an optional timestamp in UTC. If the timestamp is not supplied the agent will supply a timestamp of its own taken at the arrival time of the data to the agent. The remainder of the line is a key followed by data – depending on the type of data item is being written to.

A simple set of events and samples will look something like this:

	2009-06-15T00:00:00.000000|power|ON|execution|ACTIVE|line|412|Xact|-1.1761875153|Yact|1766618937

For simple events and samples, the data is pipe delimited key value pairs with multiple pairs on one line. Each line must have at least one k/v on it or else it has no meaning. The agent will discard any lines where the data is malformed. The end must end with a LF (ASCII 10) or CR-LF (ASCII 15 followed by ASCII 10) (UNIX or Windows conventions respectively).

Conditions are a little more complex since there are multiple fields and must appear on one line. The fields are as follows:

	<timestamp>|<data_item_name>|<level>|<native_code>|<native_severity>|<qualifier>|<message>
	
For a completed description of these fields, see the standard. An example line will look like this:

	2014-09-29T23:59:33.460470Z|htemp|WARNING|HTEMP|1|HIGH|Oil Temperature High
	
The next special format is the Message. There is one additional field, native_code, which needs to be included:

	2014-09-29T23:59:33.460470Z|message|CHG_INSRT|Change Inserts
	
Time series data also gets special treatment, the count and optional frequency are specified. In the following example we have 10 items at a frequency of 100hz:

	2014-09-29T23:59:33.460470Z|current|10|100|1 2 3 4 5 6 7 8 9 10

The data item name can also be prefixed with the device name if this adapter is supplying data to multiple devices. The following is an example of a power meter for three devices named `device1`, `device2`, and `device3`:

	2014-09-29T23:59:33.460470Z|device1:current|12|device2:current|11|device3:current|10

All data items follow the formatting requirements in the MTConnect standard for the vocabulary and coordinates like PathPosition. 

Assets
-----

Assets are associated with a device but do not have a data item they are mapping to. They therefor get the special data item name `@ASSET@`. Assets can be sent either on one line or multiple lines depending on the adapter requirements. The single line form is as follows:

	2012-02-21T23:59:33.460470Z|@ASSET@|KSSP300R.1|CuttingTool|<CuttingTool>...

This form associates updates the asset id KSSP300R.1 for a cutting tool with the text at the end. For multiline assets, use the keyword `--multiline--` with a following unique string as follows:

		2012-02-21T23:59:33.460470Z|@ASSET@|KSSP300R.1|CuttingTool|--multiline--
		<CuttingTool>
		...
		</CuttingTool>
		--multiline--0FED07ACED
		
The terminal text must appear on the first position after the last line of text. The adapter can also remove assets (1.3) by sending a @REMOVE_ASSET@ with an asset id:

	2012-02-21T23:59:33.460470Z|@REMOVE_ASSET@|KSSP300R.1

Or all assets can be removed in one shot for a certain asset type:

	2012-02-21T23:59:33.460470Z|@REMOVE_ALL_ASSETS@|CuttingTool

Partial updates to assets is also possible by using the @UPDATE_ASSET@ key, but this will only work for cutting tools. The asset id need to be given and then one of the properties or measurements with the new value for that entity. For example to update the overall tool length and the overall diameter max, you would provide the following:

	2012-02-21T23:59:33.460470Z|@UPDATE_ASSET@|KSSP300R.1|OverallToolLength|323.64|CuttingDiameterMax|76.211

Commands
-----

There are a number of commands that can be sent as part of the adapter stream. These change some dynamic elements of the device informaiton, the interpretation of the data, or the associated default device. Commands are given on a single line starting with an asterix `* ` as the first character of the line and followed by a <key>: <value>. They are as follows:

* Set the manufacturer in the devcie header of the associated device:
 
	`* manufacturer: XXX`

* Set the station in the devcie header of the associated device:
 
	`* station: XXX`

* Set the sderialNumber in the devcie header of the associated device:
 
	`* sderialNumber: XXX`

* Set the description in the devcie header of the associated device:
 
	`* description: XXX`

* Set the nativeName in the devcie component of the associated device:
 
	`* nativeName: XXX`

* Set the calibration in the devcie component of the associated device:
 
	`* calibration: XXX`

* Tell the agent that the data coming from this adapter requires conversion:
 
	`* conversionRequired: <yes|no>`

* Tell the agent that the data coming from this adapter is specified in relative time:
 
	`* relativeTime: <yes|no>`

* Tell the agent that the data coming from this adapter would like real-time priority:
 
	`* realTime: <yes|no>`

* Specify the default device for this adapter. The device can be specified as either the device name or UUID:

	`* device: <uuid|name>`

Any other command will be logged as a warning.

Protocol
----

The agent and the adapter have a heartbeat that makes sure each is responsive to properly handle disconnects in a timely manor. The Heartbeat frequency is set by the adapter and honored by the agent. When the agent connects to the adapter, it first sends a `* PING` and then expects the response `* PONG: <timeout>` where `<timeout>` is specified in milliseconds. So if the following communications are given:

Agent:

	* PING

Adapter:

	* PONG: 10000

This indicates that the adapter is expecting a `PING` every 10 seconds and if there is no `PING`, in 2x the frequency, then the adapter should close the connection. At the same time, if the agent does not receive a `PONG` within 2x frequency, then it will close the connection. If no `PONG` response is received, the agent assumes the adapter is incapable of participating in heartbeat protocol and uses the legacy time specified above.
