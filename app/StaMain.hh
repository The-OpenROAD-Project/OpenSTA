// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#ifndef STA_APP_H
#define STA_APP_H

struct Tcl_Interp;

namespace sta {

typedef int (*SwigInitFunc)(Tcl_Interp *);

// The swig_init function is called to define the swig interface
// functions to the tcl interpreter.
void
staMain(Sta *sta,
	int argc,
	char **argv,
	SwigInitFunc swig_init);

// Set arguments passed to staTclAppInit inside the tcl interpreter.
void
staSetupAppInit(int argc,
		char **argv,
		SwigInitFunc swig_init);

// The variable tcl_init is an implicit argument to this function that
// provides the definitions for builtin tcl commands encoded by
// etc/TclEncode.tcl.
int
staTclAppInit(Tcl_Interp *interp);

// TCL init files are encoded into the string init using the three
// digit decimal equivalent for each ascii character.  This function
// unencodes the string and evals it.  This packages the TCL init
// files as part of the executable so they don't have to be shipped as
// separate files that have to be located and loaded at run time.
void
evalTclInit(Tcl_Interp *interp,
	    const char *inits[]);

bool
findCmdLineFlag(int argc,
		char **argv,
		const char *flag);
char *
findCmdLineKey(int argc,
	       char **argv,
	       const char *key);

void
showUseage(char *prog);
void
parseThreadsArg(int argc,
		char **argv,
		int &threads,
		bool &exists);
void
parseCmdsArg(int argc,
	     char **argv,
	     bool &native_cmds,
	     bool &compatibility_cmds);

} // namespace
#endif
