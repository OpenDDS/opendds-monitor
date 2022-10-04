# OpenDDS Monitor

A Qt application for monitoring OpenDDS domain participants and topics.

## Requirements

* OpenDDS (http://www.opendds.org) and its dependencies (ACE/TAO, and possibly openssl or xerces3)
* Qt 5
* Qwt
* CMake
* A compiler and tool chain capable of C++17

## Tested Platforms

* Ubuntu 22.04 (g++ 11.2.0)
* Ubuntu 20.04 (g++ 9.4.0)
* macOS 12 (Apple clang XXX)
* macOS 11 (Apple clang 13.0.0)
* Windows Server 2022 (VS2022)
* Windows Server 2019 (VS2019)

## Building

Assuming a valid development environment, OpenDDS environment variables are set, and Qt / Qwt installs are discoverable:
```
$ mkdir bulid
$ cd build
$ cmake ..
```
And then build with the appropriate tool chain.

On Linux/macOS:
```
$ make
```
On Windows:
```
$ msbuild -p:Configuration:Debug,Platform=x64 opendds-monitor.sln
```

See `.github/workflows/build.yml` for explicit list of steps for building on several supported platforms listed above.

## Running

An OpenDDS configuration file is expected in the same directory as the monitor executable for governing the local domain
participant, otherwise the application will quietly exit after the initial domain-chooser dialogue. Be sure to configure
the local OpenDDS domain participant for the domain you intend to monitor.
