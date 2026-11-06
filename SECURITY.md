# Security Policy

## Supported Versions

Security fixes are made for every 2.x release line of the MTConnect C++ Agent, from 2.0 on, when the line contains the affected code. A fix ships as a new patch release of each affected line. If you run a 1.x version, upgrade to a 2.x release to get them.

| Version | Supported |
| ------- | --------- |
| 2.x     | Yes       |
| < 2.0   | No        |

## Reporting a Vulnerability

Please do not report security vulnerabilities in public issues, pull requests or discussions.

Report them privately through GitHub instead:

1. Go to the [Security tab](https://github.com/mtconnect/cppagent/security) of this repository.
2. Select **Report a vulnerability**.
3. Fill in the form.

Please include:

- The agent version, and whether you built it yourself or used a published binary or Docker image.
- The relevant parts of your `agent.cfg`, such as `BrowserView`, `AllowPut`, the sinks and the sources.
- Steps to reproduce the problem, with a request, adapter data or file that shows it if you have one.
- What an attacker can do with it, and what they need first (for example network access to the agent, `AllowPut`, or control of an adapter).
- Any mitigation you have found.

## What to Expect

We will acknowledge your report, look into it, and keep you updated through the advisory on GitHub. When a fix is ready we will publish a release and a security advisory. We are glad to credit you in the advisory; tell us if you would rather not be named.

## Scope

The agent is usually deployed on a plant network and often has no authentication in front of it. Reports are in scope when they affect the agent's own code, for example:

- The REST sink (HTTP and WebSocket), including the browser viewer in `styles/`.
- The MQTT sinks and sources.
- Parsing of adapter (SHDR and MQTT) data, assets, `Devices.xml` and `agent.cfg`.
- The mruby scripting integration.

Problems in a dependency (Boost, libxml2, mruby, and others) should be reported to that project. Let us know as well if the agent needs to update the dependency or work around it.
