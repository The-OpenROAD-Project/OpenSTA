# This is effecively a fork of [parallaxsw/OpenSTA](https://github.com/parallaxsw/OpenSTA).  All issues and PRs should be filed there.

# Parallax Static Timing Analyzer

OpenSTA is a gate level static timing verifier. As a stand-alone
executable it can be used to verify the timing of a design using
standard file formats.

* Verilog netlist
* Liberty library
* SDC timing constraints
* SDF delay annotation
* SPEF parasitics
* VCD power acitivies
* SAIF power acitivies

OpenSTA uses a TCL command interpreter to read the design, specify
timing constraints and print timing reports.

##### Clocks
* Generated
* Latency
* Source latency (insertion delay)
* Uncertainty
* Propagated/Ideal
* Gated clock checks
* Multiple frequency clocks

##### Exception paths
* False path
* Multicycle path
* Min/Max path delay
* Exception points
*  -from clock/pin/instance -through pin/net -to clock/pin/instance
*  Edge specific exception points
*   -rise_from/-fall_from, -rise_through/-fall_through, -rise_to/-fall_to

##### Delay calculation
* Integrated Dartu/Menezes/Pileggi RC effective capacitance algorithm
* External delay calculator API

##### Analysis
* Report timing checks -from, -through, -to, multiple paths to endpoint
* Report delay calculation
* Check timing setup

##### Timing Engine
OpenSTA is architected to be easily bolted on to other tools as a
timing engine.  By using a network adapter, OpenSTA can access the host
netlist data structures without duplicating them.

* Query based incremental update of delays, arrival and required times
* Simulator to propagate constants from constraints and netlist tie high/low

See doc/OpenSTA.pdf for command documentation.
See doc/ChangeLog.txt for changes to commands.
See doc/StaApi.txt for timing engine API documentation.

OpenSTA is dual licensed. It is released under GPL v3 as OpenSTA and
is also licensed for commerical applications by Parallax Software without
the GPL's requirements.

OpenSTA is open source, meaning the sources are published and can be
compiled locally.  Derivative works are supported as long as they
adhere to the GPL license requirements.  However, OpenSTA is not
supported by a public community of developers as many other open
source projects are. The copyright and develpment are exclusive to
Parallax Software. Contributors must signing the Contributor License
Agreement (doc/CLA.txt) when submitting pull requests.

Removing copyright and license notices from OpenSTA sources (or any
other open source project for that matter) is illegal. This should be
obvious, but the author of OpenSTA has discovered two different cases
where the copyright and license were removed from source files that
were copied.

The official git repository is located at
https://github.com/parallaxsw/OpenSTA.git. Any forks from this code
base have not passed extensive regression testing which is not
publicly available.

## Build from source

OpenSTA is built with CMake.

### Prerequisites

The build dependency versions are show below.  Other versions may
work, but these are the versions used for development.

```
         Ubuntu   Macos
        22.04.2   14.5
cmake    3.24.2    3.29.2
clang             15.0.0
gcc      11.4.0
tcl       8.6      8.6.6
swig      4.1.0    4.1.1
bison     3.8.2    3.8.2
flex      2.6.4    2.6.4
```

Note that flex versions before 2.6.4 contain 'register' declarations that
are illegal in c++17.

External library dependencies:
```
           Ubuntu   Macos license
eigen       3.4.0   3.4.0   MPL2  required
cudd        3.0.0   3.0.0   BSD   required
tclreadline 2.3.8   2.3.8   BSD   optional
zLib        1.2.5   1.2.8   zlib  optional
```

The [TCL readline library](https://tclreadline.sourceforge.net/tclreadline.html)
links the GNU readline library to the TCL interpreter for command line editing 
On OSX, Homebrew does not support tclreadline, but the macports system does
(see https://www.macports.org). To enable TCL readline support use the following
Cmake option: See (https://tclreadline.sourceforge.net/) for TCL readline
documentation. To change the overly verbose default prompt, add something this
to your ~/.sta init file:

```
if { ![catch {package require tclreadline}] } {
  proc tclreadline::prompt1 {} {
    return "> "
  }
}
```

The Zlib library is an optional.  If CMake finds libz, OpenSTA can
read Liberty, Verilog, SDF, SPF, and SPEF files compressed with gzip.

CUDD is a binary decision diageram (BDD) package that is used to
improve conditional timing arc handling, constant propagation, power
activity propagation and spice netlist generation.

CUDD is available
[here](https://github.com/davidkebo/cudd/blob/main/cudd_versions/cudd-3.0.0.tar.gz).

Unpack and build CUDD.

```
tar xvfz cudd-3.0.0.tar.gz
cd cudd-3.0.0
./configure
make
```

You can use the "configure --prefix" option and "make install" to install CUDD
in a different directory.

### Installing with CMake

Use the following commands to checkout the git repository and build the
OpenSTA library and excutable.

```
git clone https://github.com/parallaxsw/OpenSTA.git
cd OpenSTA
mkdir build
cd build
cmake -DCUDD_DIR=<CUDD_INSTALL_DIR> ,.
make
```
The default build type is release to compile optimized code.
The resulting executable is in `app/sta`.
The library without a `main()` procedure is `app/libSTA.a`.

Optional CMake variables passed as -D<var>=<value> arguments to CMake are show below.

```
CMAKE_BUILD_TYPE DEBUG|RELEASE
CMAKE_CXX_FLAGS - additional compiler flags
TCL_LIBRARY - path to tcl library
TCL_HEADER - path to tcl.h
CUDD_DIR - path to cudd installation
ZLIB_ROOT - path to zlib
CMAKE_INSTALL_PREFIX
```

If `TCL_LIBRARY` is specified the CMake script will attempt to locate
the header from the library path.

The default install directory is `/usr/local`.
To install in a different directory with CMake use the CMAKE_INSTALL_PREFIX option.

If you make changes to `CMakeLists.txt` you may need to clean out
existing CMake cached variable values by deleting all of the
files in the build directory.

## Build with Docker

An alternative way to build and run OpenSTA is with
[Docker](https://www.docker.com).  After installing Docker, the
following command builds a Docker image.

```
cd OpenSTA
docker build --file Dockerfile.ubuntu_22.04 --tag OpenSTA .
```

To run a docker container using the OpenSTA image, use the -v option
to docker to mount direcories with data to use and -i to run
interactively.

```
docker run -i -v $HOME:/data OpenSTA
```

## Bug Reports

Use the Issues tab on the github repository to report bugs.

Each issue/bug should be a separate issue. The subject of the issue
should be a short description of the problem. Attach a test case to
reproduce the issue as described below. Issues without test cases are
unlikely to get a response.

The files in the test case should be collected into a directory named
YYYYMMDD where YYYY is the year, MM is the month, and DD is the
day (this format allows "ls" to report them in chronological order).
The contents of the directory should be collected into a compressed
tarfile named YYYYMMDD.tgz.

The test case should have a tcl command file recreates the issue named
run.tcl. If there are more than one command file using the same data
files, there should be separate command files, run1.tcl, run2.tcl
etc. The bug report can refer to these command files by name.

Command files should not have absolute filenames like
"/home/cho/OpenSTA_Request/write_path_spice/dump_spice" in them.
These obviously are not portable. Use filenames relative to the test
case directory.

## Authors

* James Cherry

* William Scott authored the arnoldi delay calculator at Blaze, Inc which was subsequently licensed to Nefelus, Inc that has graciously contributed it to OpenSTA.

## License

OpenSTA, Static Timing Analyzer
Copyright (c) 2023, Parallax Software, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
