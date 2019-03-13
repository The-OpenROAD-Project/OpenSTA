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

See doc/OpenSTA.pdf for complete documentiaton.

## Getting Started

OpenSTA can be run as a [Docker](https://www.docker.com/) container
or built as local executable with CMake.

### Run using Docker
* Install Docker on [Windows](https://docs.docker.com/docker-for-windows/), [Mac](https://docs.docker.com/docker-for-mac/) or [Linux](https://docs.docker.com/install/).
* Navigate to the directory where you have the input files.
* Run OpenSTA as a binary using
````
docker run -it -v $(pwd):/data openroad/opensta
````

4. From the interactive terminal, use OpenSTA commands. You can read input files from `/data` directory inside the docker container (e.g. `read_liberty /data/liberty.lib`). You can use OpenSTA in non-interactive mode by passing a command file using the `-f` flag as follows.
```
docker run -it -v $(pwd):/data openroad/opensta -f /data/cmd_file
```
Note that the path after `-f` is the path inside container, not on the guest machine. 

### Prerequisites

The build dependency versions are show below.  Other versions may
work, but these are the versions used for development.

```
         from   Ubuntu   Xcode
                18.04.1  10.1
cmake    3.9
clang    9.1.0           10.0.0
gcc      3.3.2   7.3.0   
tcl      8.2     8.6     8.6.6
swig     1.3.28  3.0.12  3.0.12
bison    1.35    3.0.4   2.3
flex     2.5.4   2.6.4   2.5.35
```

These packages are optional:

```
libz     1.1.4   1.2.5     1.2.8
cudd             2.4.1     3.0.0
```

CUDD is a binary decision diageram (BDD) package that is used to improve conditional timing arc handling. It is available [here](https://www.davidkebo.com/source/cudd_versions/cudd-3.0.0.tar.gz) or [here](https://sourceforge.net/projects/cudd-mirror/).

Note that the file hierarchy of the CUDD installation changed with version 3.0.
Some changes to the CMake are required to support older versions.

You may use the `--prefix ` option to `configure` to install in a location other than
the default (`/usr/local/lib`).
```
cd $HOME/cudd-3.0.0
mkdir $HOME/cudd
./configure --prefix $HOME/cudd
make
make install
```

The Zlib library is an optional.  If CMake finds libz, OpenSTA can
read Verilog, SDF, SPF, and SPEF files compressed with gzip.

### Installing with CMake

Use the following commands to checkout the git repository and build the
OpenSTA library and excutable.

```
git clone https://xp-dev.com/git/opensta
cd opensta
mkdir build
cd build
cmake .. -DCUDD=$HOME/cudd
make
```
The default build type is release to compile optimized code.
The resulting executable is in `app/sta`.
The library without a `main()` procedure is `app/libSTA.a`.

Optional CMake variables passed as -D<var>=<value> arguments to CMake are show below.

```
CMAKE_BUILD_TYPE DEBUG|RELEASE
CMAKE_CXX_FLAGS - additional compiler flags
TCL_LIB - path to tcl library
TCL_HEADER - path to tcl.h
CUDD - path to cudd installation ($HOME/cudd if following install shown above)
ZLIB_ROOT - path to zlib
CMAKE_INSTALL_PREFIX
```

If `TCL_LIB` is specified the CMake script will attempt to locate the
header from the library path.

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

### Installing on Windoz

Use a .bat file to start a cygwin shell that has its path set to
support the Microcruft cl compiler by calling the vsvars32.bat script
from the Visual C++ installation.
```
tcsh-startup.bat
  @echo off
  call "c:\Microsoft Visual Studio 9.0\Common7\Tools\vsvars.bat"
  set path=c:\cygwin\bin;%PATH%
  c:\cygwin\bin\tcsh
```
CMake is supposedly more compatible with the windoz environment
so you may have better luck wih it.

Cmake and build from the shell. Note that tcl and zlib must be
built with the Visual C++ compiler to link to the sta libraries.

  mkdir build
  cd build
  cmakd ..
  make
...

Good luck and don't bother me with windoz specific issues.
I am happy to say I haven't owned a windoz machine in 20 years.

## Authors

* James Cherry

* William Scott authored the arnoldi delay calculator at Blaze, Inc which was subsequently licensed to Nefelus, Inc that has graciously contributed it to OpenSTA.

## License

Copyright (c) 2019, Parallax Software, Inc.
All rights reserved.

No part of this document may be copied, transmitted or
disclosed in any form or fashion without the express
written consent of Parallax Software, Inc.
