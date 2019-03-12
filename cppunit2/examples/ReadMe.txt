simple/simple.dsp: A simple CppUnit's example. Basic TextTestRunner and a single 'standard' 
	TestFixture. A good starting point

simple/simple_plugin.dsp: Like 'simple', but creates a test plug-in. The test plug-in can
	be run with DllPlugInRunner.

hierarchy/: A simple example that demonstrate the use of helper macros with template
	and how to subclass TestFixture.

cppunittest/: CppUnit's unit tests. Contains CppUnitTestMain which build an application
	with CppUnit's tests, and  which wrap the same unit tests into a test plug-in.

ClockerPlugIn/: a 'TestListener' plug-in. Demonstrates the use of the test plug-in to
extends DllPlugInRunner. The test plug-in tracks the time each test and suite takes to run
and includes the timing in the XML output.

DumperPlugIn/: a 'TestListener' plug-in that dumps the test hierarchy as a tree or in
a flattened format (using TestPath).
