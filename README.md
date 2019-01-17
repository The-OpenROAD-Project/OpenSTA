# OpenSTA, Static Timing Analyzer
Copyright (c) 2019, Parallax Software, Inc.

> This program is free software: you can redistribute it and/or modify
> it under the terms of the GNU General Public License as published by
> the Free Software Foundation, either version 3 of the License, or
> (at your option) any later version.

> This program is distributed in the hope that it will be useful,
> but WITHOUT ANY WARRANTY; without even the implied warranty of
> MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
> GNU General Public License for more details.

> You should have received a copy of the GNU General Public License
> along with this program.  If not, see <https://www.gnu.org/licenses/>.

Parallax Gate Level Static Timing Analyzer

## Install

See INSTALL for installation and build instructions. Alternatively, run using Docker as described in the next section

## Run using Docker
1. Install Docker on [Windows](https://docs.docker.com/docker-for-windows/), [Mac](https://docs.docker.com/docker-for-mac/) or [Linux](https://docs.docker.com/install/).
2. Navigate to the directory where you have the input files.
3. Run OpenSTA as a binary using `docker run -it -v $(pwd):/data openroad/opensta`
4. From the interactive terminal, use OpenSTA commands. You can read input files from `/data` directory inside the docker container (e.g. `read_liberty /data/liberty.lib`). You can use OpenSTA in non-interactive mode by passing a command file using `-f` flag as follows `docker run -it -v $(pwd):/data openroad/opensta -f /data/cmd_file`. Note that the path after `-f` is the path inside container, not on the guest machine. 

## Standard file formats

* Verilog
* Liberty
* SDC
* SDF
* RSPF/DSPF/SPEF

## Exception path support
* False path
* Multicycle path
* Min/Max delay
* Exception points
*  -from clock/pin/instance -through pin/net -to clock/pin/instance
*  Edge specific exception points
*   -rise_from/-fall_from, -rise_through/-fall_through, -rise_to/-fall_to

## Clocks
* Generated
* Latency
* Source latency (insertion delay)
* Uncertainty
* Propagated/Ideal
* Gated clock checks
* Multiple frequency clocks

## Delay calculation
* Integrated Dartu/Menezes/Pileggi RC effective capacitance algorithm
* External delay calculator API

## Analysis
* Report timing checks -from, -through, -to, multiple paths to endpoint
* Report delay calculation
* Check timing setup

## Search Engine
* Query based incremental update of delays, arrival and required times
* Simulator to propagate constants from constraints and netlist tie high/low

## Timing engine library
* Network adapter uses external netlist database without duplicating any data
