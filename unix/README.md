Instructions for MTConnect Ubuntu Support
------

The MTConnect Agent can be easily built and run on any modern linux environment. The steps to build it and run it as a service are given in the following tutorial. These instructions have been tested on a clean Ubuntu LTE 18.04 instance, so if you are using another version of flavor of linux your milage may vary.

The instructions also cover running the agent as a service using systemd. For older versions of *NIX, we have also included upstart and init.d scripts.

The agent is built on AppVeyor after every push, so if the AppVeyor status is green, the version should be able to build.

Build Dependencies
-----

    $ sudo apt-get update
    $ sudo apt-get -y install git build-essential cmake libxml2-dev ruby curl


Clone from github
-----

	$ mkdir agent; cd agent
	$ git clone git@github.com:mtconnect/cppagent.git
	$ cd cppagent
	$ git submodule init
	$ git submodule update

Buiid the agent
----

	$ mkdir build; cd build
	$ cmake --config Release ..
	$ cmake --build . --config Release --target agent

The agent is now in the `agent` directory.

Install the agent to /usr/local/bin:

    $ sudo make install 

Optional Tests
-----

If you want to run the tests, do the following:

    $ cmake --build . --target agent_test
    $ ./tests/agent_test


> Note: make will by default build both targets

    $ make 
	

Install the agent service
-----

Create a user for the mtconnect service to run under

    $ sudo useradd -r -s /bin/false mtconnect

Make a logging directory:

    $ sudo mkdir /var/log/mtconnect
    $ sudo chown mtconnect:mtconnect /var/log/mtconnect

Create a directory for the agent

    $ sudo mkdir -p /etc/mtconnect/cfg 
    $ sudo cp ../unix/agent.cfg /etc/mtconnect/cfg/
	$ sudo cp agent/agent /usr/local/bin/
    $ sudo cp -r ../styles ../schemas ../simulator /etc/mtconnect/
    $ sudo chown -R mtconnect:mtconnect /etc/mtconnect

Install the service for systemd

    $ sudo cp ../unix/mtcagent.service ../unix/mtcsimulator.service /etc/systemd/system/
    $ sudo systemctl enable mtcagent
    $ sudo systemctl start mtcagent
    $ sudo systemctl enable mtcsimulator
    $ sudo systemctl start mtcsimulator

Verifying the agent is working correctly
-----

Run curl to do a probe and a current

    $ curl http://localhost:5000/probe
    $ curl http://localhost:5000/current

Verify the correct content is being delivered
