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

#include <tcl.h>
#include <stdlib.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "Vector.hh"
#include "Sta.hh"
#include "StaMain.hh"

namespace sta {

static char *
unencode(const char *inits[]);

int
parseThreadsArg(int &argc,
		char *argv[])
{
  char *thread_arg = findCmdLineKey(argc, argv, "-threads");
  if (thread_arg) {
    if (stringEqual(thread_arg, "max"))
      return processorCount();
    else if (isDigits(thread_arg))
      return atoi(thread_arg);
    else
      fprintf(stderr,"Warning: -threads must be max or a positive integer.\n");
  }
  return 1;
}

bool
findCmdLineFlag(int &argc,
		char *argv[],
		const char *flag)
{
  for (int i = 1; i < argc; i++) {
    char *arg = argv[i];
    if (stringEq(arg, flag)) {
      // remove flag from argv.
      for (int j = i + 1; j < argc; j++, i++)
	argv[i] = argv[j];
      argc--;
      return true;
    }
  }
  return false;
}

char *
findCmdLineKey(int &argc,
	       char *argv[],
	       const char *key)
{
  for (int i = 1; i < argc; i++) {
    char *arg = argv[i];
    if (stringEq(arg, key) && i + 1 < argc) {
      char *value = argv[i + 1];
      // remove key and value from argv.
      for (int j = i + 2; j < argc; j++, i++)
	argv[i] = argv[j];
      argc -= 2;
      return value;
    }
  }
  return nullptr;
}

// Use overridden version of source to echo cmds and results.
void
sourceTclFile(const char *filename,
	      bool echo,
	      bool verbose,
	      Tcl_Interp *interp)
{
  string cmd;
  stringPrint(cmd, "source %s%s%s",
	      echo ? "-echo " : "",
	      verbose ? "-verbose " : "",
	      filename);
  Tcl_Eval(interp, cmd.c_str());
}

void
evalTclInit(Tcl_Interp *interp,
	    const char *inits[])
{
  char *unencoded = unencode(inits);
  if (Tcl_Eval(interp, unencoded) != TCL_OK) {
    // Get a backtrace for the error.
    Tcl_Eval(interp, "$errorInfo");
    const char *tcl_err = Tcl_GetStringResult(interp);
    fprintf(stderr, "Error: TCL init script: %s.\n", tcl_err);
    fprintf(stderr, "       Try deleting app/TclInitVar.cc and rebuilding.\n");
    exit(0);
  }
  delete [] unencoded;
}

static char *
unencode(const char *inits[])
{
  size_t length = 0;
  for (const char **e = inits; *e; e++) {
    const char *init = *e;
    length += strlen(init);
  }
  char *unencoded = new char[length / 3 + 1];
  char *u = unencoded;
  for (const char **e = inits; *e; e++) {
    const char *init = *e;
    size_t init_length = strlen(init);
    for (const char *s = init; s < &init[init_length]; s += 3) {
      char code[4] = {s[0], s[1], s[2], '\0'};
      char ch = atoi(code);
      *u++ = ch;
    }
  }
  *u = '\0';
  return unencoded;
}

} // namespace
