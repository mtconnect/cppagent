
MTConnect C++ Agent Version 1.2.0.18
--------

The C++ Agent provides the a complete implementation of the HTTP
server required by the MTConnect standard. The agent provides the
protocol and data collection framework that will work as a standalone
server. Once built, you only need to specify the XML description of
the devices and the location of the adapter.

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

    *Default*: 1.2

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

        *Default*: false
        
		* `PreserveUUID` - Do not overwrite the UUID with the UUID from the adapter, preserve
		  the UUID in the Devices.xml file. This can be overridden on a per adapter basis.

		    *Default*: false



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

Adapter Agent Protocol Version 1.2
-----

