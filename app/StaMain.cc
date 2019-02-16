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
static SwigInitFunc sta_swig_init;

static const char *init_filename = "[file join $env(HOME) .sta]";
extern const char *tcl_inits[];

static void
sourceTclFileEchoVerbose(const char *filename,
			 Tcl_Interp *interp);

void
staMain(Sta *sta,
	int argc,
	char **argv,
	SwigInitFunc swig_init)
{
  initSta();

  Sta::setSta(sta);
  sta->makeComponents();

  int thread_count = 1;
  bool threads_exists = false;
  parseThreadsArg(argc, argv, thread_count, threads_exists);
  if (threads_exists)
    sta->setThreadCount(thread_count);

  staSetupAppInit(argc, argv, swig_init);
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
		char **argv,
		SwigInitFunc swig_init)
{
  sta_argc = argc;
  sta_argv = argv;
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
  evalTclInit(interp, tcl_inits);

  if (!findCmdLineFlag(argc, argv, "-no_splash"))
    Tcl_Eval(interp, "sta::show_splash");

  // Import exported commands from sta namespace to global namespace.
  Tcl_Eval(interp, "sta::define_sta_cmds");
  const char *export_cmds = "namespace import sta::*";
  Tcl_Eval(interp, export_cmds);

  if (!findCmdLineFlag(argc, argv, "-no_init"))
    sourceTclFileEchoVerbose(init_filename, interp);

  // "-x cmd" is evaled before -f file is sourced.
  char *cmd = findCmdLineKey(argc, argv, "-x");
  if (cmd)
    Tcl_Eval(interp, cmd);

  // "-f cmd_file" is evaled as "source -echo -verbose file".
  char *file = findCmdLineKey(argc, argv, "-f");
  if (file)
    sourceTclFileEchoVerbose(file, interp);

  return TCL_OK;
}

bool
findCmdLineFlag(int argc,
		char **argv,
		const char *flag)
{
  for (int argi = 1; argi < argc; argi++) {
    char *arg = argv[argi];
    if (stringEq(arg, flag))
      return true;
  }
  return false;
}

char *
findCmdLineKey(int argc,
	       char **argv,
	       const char *key)
{
  for (int argi = 1; argi < argc; argi++) {
    char *arg = argv[argi];
    if (stringEq(arg, key) && argi + 1 < argc)
      return argv[argi + 1];
  }
  return 0;
}

// Use overridden version of source to echo cmds and results.
static void
sourceTclFileEchoVerbose(const char *filename,
			 Tcl_Interp *interp)
{
  string cmd;
  stringPrint(cmd, "source -echo -verbose %s", filename);
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
showUseage(char *prog)
{
  printf("Usage: %s [-help] [-version] [-no_init] [-f cmd_file]\n", prog);
  printf("  -help              show help and exit\n");
  printf("  -version           show version and exit\n");
  printf("  -no_init           do not read .sta init file\n");
  printf("  -x cmd             evaluate cmd\n");
  printf("  -f cmd_file        source cmd_file\n");
  printf("  -threads count|max use count threads\n");
}

} // namespace
