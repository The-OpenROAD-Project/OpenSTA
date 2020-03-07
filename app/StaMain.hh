// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

struct Tcl_Interp;

namespace sta {

class Sta;

// Parse command line argument
int
staTclAppInit(int argc,
	      char *argv[],
	      const char *init_filename,
	      Tcl_Interp *interp);

// Sta initialization.
// Makes the Sta object and registers TCL commands.
void
initSta(int argc,
	char *argv[],
	Tcl_Interp *interp);

// TCL init files are encoded into the string init using the three
// digit decimal equivalent for each ascii character.  This function
// unencodes the string and evals it.  This packages the TCL init
// files as part of the executable so they don't have to be shipped as
// separate files that have to be located and loaded at run time.
void
evalTclInit(Tcl_Interp *interp,
	    const char *inits[]);

bool
findCmdLineFlag(int &argc,
		char *argv[],
		const char *flag);
char *
findCmdLineKey(int &argc,
	       char *argv[],
	       const char *key);

int
parseThreadsArg(int &argc,
		char *argv[]);
void
sourceTclFile(const char *filename,
	      bool echo,
	      bool verbose,
	      Tcl_Interp *interp);

} // namespace
