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

#include <tcl.h>
#include <stdlib.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "Vector.hh"
#include "Sta.hh"
#include "StaMain.hh"

namespace sta {

typedef sta::Vector<SwigInitFunc> SwigInitFuncSeq;

// "Arguments" passed to staTclAppInit.
static int sta_argc;
static char **sta_argv;
static const char **sta_tcl_inits;
static SwigInitFunc sta_swig_init;

static const char *init_filename = "[file join $env(HOME) .sta]";

void
staMain(Sta *sta,
	int argc,
	char *argv[],
	SwigInitFunc swig_init,
	const char *tcl_inits[])
{
  initSta();

  Sta::setSta(sta);
  sta->makeComponents();

  int thread_count = 1;
  bool threads_exists = false;
  parseThreadsArg(argc, argv, thread_count, threads_exists);
  if (threads_exists)
    sta->setThreadCount(thread_count);

  staSetupAppInit(argc, argv, swig_init, tcl_inits);
  // Set argc to 1 so Tcl_Main doesn't source any files.
  // Tcl_Main never returns.
  Tcl_Main(1, argv, staTclAppInit);
}

void
parseThreadsArg(int argc,
		char **argv,
		int &thread_count,
		bool &exists)
{
  char *thread_arg = findCmdLineKey(argc, argv, "-threads");
  if (thread_arg) {
    if (stringEqual(thread_arg, "max")) {
      thread_count = processorCount();
      exists = true;
    }
    else if (isDigits(thread_arg)) {
      thread_count = atoi(thread_arg);
      exists = true;
    }
    else
      fprintf(stderr,"Warning: -threads must be max or a positive integer.\n");
  }
}

// Set globals to pass to staTclAppInit.
void
staSetupAppInit(int argc,
		char *argv[],
		SwigInitFunc swig_init,
		const char *tcl_inits[])
{
  sta_argc = argc;
  sta_argv = argv;
  sta_tcl_inits = tcl_inits;
  sta_swig_init = swig_init;
}

// Tcl init executed inside Tcl_Main.
int
staTclAppInit(Tcl_Interp *interp)
{
  int argc = sta_argc;
  char **argv = sta_argv;

  // source init.tcl
  Tcl_Init(interp);

  // Define swig commands.
  sta_swig_init(interp);

  Sta *sta = Sta::sta();
  sta->setTclInterp(interp);

  // Eval encoded sta TCL sources.
  evalTclInit(interp, sta_tcl_inits);

  if (!findCmdLineFlag(argc, argv, "-no_splash"))
    Tcl_Eval(interp, "sta::show_splash");

  // Import exported commands from sta namespace to global namespace.
  Tcl_Eval(interp, "sta::define_sta_cmds");
  Tcl_Eval(interp, "namespace import sta::*");

  if (!findCmdLineFlag(argc, argv, "-no_init"))
    sourceTclFile(init_filename, true, true, interp);

  bool exit_after_cmd_file = findCmdLineFlag(argc, argv, "-exit");

  if (argc > 2 ||
      (argc > 1 && argv[1][0] == '-'))
    showUsage(argv[0]);
  else {
    if (argc == 2) {
      char *cmd_file = argv[1];
      if (cmd_file) {
	sourceTclFile(cmd_file, false, false, interp);
	if (exit_after_cmd_file)
	  exit(EXIT_SUCCESS);
      }
    }
  }
  return TCL_OK;
}

bool
findCmdLineFlag(int &argc,
		char *argv[],
		const char *flag)
{
  bool found = false;
  int j = 1;
  int argc1 = argc;
  for (int i = 1; i < argc1; i++) {
    char *arg = argv[i];
    if (stringEq(arg, flag)) {
      found = true;
      // remove flag from argv.
      argc--;
    }
    else
      argv[j++] = argv[i];
  }
  return found;
}

char *
findCmdLineKey(int &argc,
	       char *argv[],
	       const char *key)
{
  char *value = nullptr;
  int j = 1;
  int argc1 = argc;
  for (int i = 1; i < argc1; i++) {
    char *arg = argv[i];
    if (stringEq(arg, key) && i + 1 < argc) {
      value = argv[i + 1];
      // remove key and value from argv.
      i++;
      argc -= 2;
    }
    else
      argv[j++] = argv[i];
  }
  return value;
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

void
showUsage(const char * prog) 
{
  printf("Usage: %s [-help] [-version] [-no_init] [-exit] cmd_file\n", prog);
  printf("  -help              show help and exit\n");
  printf("  -version           show version and exit\n");
  printf("  -no_init           do not read .sta init file\n");
  printf("  -threads count|max use count threads\n");
  printf("  -no_splash         do not show the license splash at startup\n");
  printf("  -exit              exit after reading cmd_file\n");
  printf("  cmd_file           source cmd_file\n");
}

} // namespace
