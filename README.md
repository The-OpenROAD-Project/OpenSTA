# Parallax Static Timing Analyzer

OpenSTA is a gate level static timing verifier. As a stand-alone
executable it can be used to verify the timing of a design using
standard file formats.

* Verilog netlist
* Liberty library
* SDC timing constraints
* SDF delay annotation
* SPEF parasitics

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

See doc/OpenSTA.pdf for command documentiaton.
See doc/StaApi.txt for timing engine API documentiaton.
See doc/ChangeLog.txt for changes to commands.

## Build

OpenSTA is built with CMake.

### Prerequisites

The build dependency versions are show below.  Other versions may
work, but these are the versions used for development.

```
         from   Ubuntu   Xcode
                18.04.1  11.3
cmake    3.10.2 3.10.2   3.16.2
clang    9.1.0           11.0.0
gcc      3.3.2   7.3.0   
tcl      8.2     8.6     8.6.6
swig     1.3.28  3.0.12  4.0.1
bison    1.35    3.0.4   3.5
flex     2.5.4   2.6.4   2.5.35
```

These packages are **optional**:

```
libz     1.1.4   1.2.5     1.2.8
cudd             2.4.1     3.0.0
```

CUDD is a binary decision diageram (BDD) package that is used to improve conditional timing arc handling. OpenSTA does not require it to be installed. It is available [here](https://www.davidkebo.com/source/cudd_versions/cudd-3.0.0.tar.gz) or [here](https://sourceforge.net/projects/cudd-mirror/).

Note that the file hierarchy of the CUDD installation changed with version 3.0.
Some changes to CMakeLists.txt are required to support older versions.

When building CUDD you may use the `--prefix ` option to `configure` to
install in a location other than the default (`/usr/local/lib`).
```
cd $HOME/cudd-3.0.0
mkdir $HOME/cudd
./configure --prefix $HOME/cudd
make
make install
```
To not use CUDD specify `CUDD=0`.
Cmake looks for the CUDD library in `CUDD/lib, CUDD/cudd/lib`
and for the header in `CUDD/include, CUDD/cudd/include`.
```
# equivalent to -DCUDD=0
cmake ..                     
or
cmake .. -DCUDD=0
or
# look in ~/cudd/lib, ~/cudd/include
cmake .. -DCUDD=$HOME/cudd
or
# look in /usr/local/lib/cudd, /usr/local/include/cudd
cmake .. -DCUDD=/usr/local
```

The Zlib library is an optional.  If CMake finds libz, OpenSTA can
read Verilog, SDF, SPF, and SPEF files compressed with gzip.

### Installing with CMake

Use the following commands to checkout the git repository and build the
OpenSTA library and excutable.

```
git clone https://github.com/The-OpenROAD-Project/OpenSTA.git
cd OpenSTA
mkdir build
cd build
cmake ..
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
CUDD - path to cudd installation
ZLIB_ROOT - path to zlib
CMAKE_INSTALL_PREFIX
```

If `TCL_LIBRARY` is specified the CMake script will attempt to locate
the header from the library path.

The default install directory is `/usr/local`.
To install in a different directory with CMake use:

```
cmake .. -DCMAKE_INSTALL_PREFIX=<prefix_path>
```

Alternatively, you can use the `DESTDIR` variable with make.

```
make DESTDIR=<prefix_path> install
```

If you make changes to `CMakeLists.txt` you may need to clean out
existing CMake cached variable values by deleting all of the
files in the build directory.

### Run using Docker

OpenSTA can be run as a [Docker](https://www.docker.com/) container.

* Install Docker on [Windows](https://docs.docker.com/docker-for-windows/), [Mac](https://docs.docker.com/docker-for-mac/) or [Linux](https://docs.docker.com/install/).
* Navigate to the directory where you have the input files.
* Run OpenSTA as a binary using
````
docker run -it -v $(pwd):/data openroad/opensta
````

From the interactive terminal, use OpenSTA commands. You can read input files from `/data` directory inside the docker container (e.g. `read_liberty /data/liberty.lib`). You can use OpenSTA in non-interactive mode by passing a command file using the `-f` flag as follows.
```
docker run -it -v $(pwd):/data openroad/opensta /data/cmd_file
```
Note that the path after `-f` is the path inside container, not on the guest machine. 

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

OpenSTA is dual licensed. It is released under GPL v3 as OpenSTA and
is also licensed for commerical applications by Parallax Software without
the GPL's requirements.

OpenSTA, Static Timing Analyzer
Copyright (c) 2020, Parallax Software, Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.


