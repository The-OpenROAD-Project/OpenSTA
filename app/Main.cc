// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "StaMain.hh"
#include "StaConfig.hh"  // STA_VERSION

#include <stdio.h>
#include <cstdlib>              // exit
#include <tcl.h>
#if TCL_READLINE
  #include <tclreadline.h>
#endif

#include "Sta.hh"

namespace sta {
extern const char *tcl_inits[];
}

using std::string;
using sta::stringEq;
using sta::findCmdLineFlag;
using sta::Sta;
using sta::initSta;
using sta::evalTclInit;
using sta::sourceTclFile;
using sta::parseThreadsArg;
using sta::tcl_inits;
using sta::is_regular_file;

// Swig uses C linkage for init functions.
extern "C" {
extern int Sta_Init(Tcl_Interp *interp);
}

static int cmd_argc;
static char **cmd_argv;
static const char *init_filename = ".sta";

static void
showUsage(const char *prog,
	  const char *init_filename);
static int
tclAppInit(Tcl_Interp *interp);
static int
staTclAppInit(int argc,
	      char *argv[],
	      const char *init_filename,
	      Tcl_Interp *interp);
static void
initStaApp(int &argc,
	   char *argv[],
	   Tcl_Interp *interp);

int
main(int argc,
     char *argv[])
{
  if (argc == 2 && stringEq(argv[1], "-help")) {
    showUsage(argv[0], init_filename);
    return 0;
  }
  else if (argc == 2 && stringEq(argv[1], "-version")) {
    printf("%s\n", STA_VERSION);
    return 0;
  }
  else {
    // Set argc to 1 so Tcl_Main doesn't source any files.
    // Tcl_Main never returns.
#if 0
    // It should be possible to pass argc/argv to staTclAppInit with
    // a closure but I couldn't get the signature to match Tcl_AppInitProc.
    Tcl_Main(1, argv, [=](Tcl_Interp *interp)
		      { sta::staTclAppInit(argc, argv, interp);
			return 1;
		      });
#else
    // Workaround.
    cmd_argc = argc;
    cmd_argv = argv;
    Tcl_Main(1, argv, tclAppInit);
#endif
    return 0;
  }
}

static int
tclAppInit(Tcl_Interp *interp)
{
  return staTclAppInit(cmd_argc, cmd_argv, init_filename, interp);
}

// Tcl init executed inside Tcl_Main.
static int
staTclAppInit(int argc,
	      char *argv[],
	      const char *init_filename,
	      Tcl_Interp *interp)
{
  // source init.tcl
  if (Tcl_Init(interp) == TCL_ERROR)
    return TCL_ERROR;

#if TCL_READLINE
  if (Tclreadline_Init(interp) == TCL_ERROR)
    return TCL_ERROR;
  Tcl_StaticPackage(interp, "tclreadline", Tclreadline_Init, Tclreadline_SafeInit);
  if (Tcl_EvalFile(interp, TCLRL_LIBRARY "/tclreadlineInit.tcl") != TCL_OK)
    printf("Failed to load tclreadline.tcl\n");
#endif

  initStaApp(argc, argv, interp);

  if (!findCmdLineFlag(argc, argv, "-no_splash"))
    Tcl_Eval(interp, "sta::show_splash");

  if (!findCmdLineFlag(argc, argv, "-no_init")) {
    const char *home = getenv("HOME");
    if (home) {
      string init_path = home;
      init_path += "/";
      init_path += init_filename;
      if (is_regular_file(init_path.c_str()))
        sourceTclFile(init_path.c_str(), true, true, interp);
    }
  }

  bool exit_after_cmd_file = findCmdLineFlag(argc, argv, "-exit");

  if (argc > 2
      || (argc > 1 && argv[1][0] == '-')) {
    showUsage(argv[0], init_filename);
    exit(1);
  }
  else {
    if (argc == 2) {
      char *cmd_file = argv[1];
      if (cmd_file) {
	int result = sourceTclFile(cmd_file, false, false, interp);
        if (exit_after_cmd_file) {
          int exit_code = (result == TCL_OK) ? EXIT_SUCCESS : EXIT_FAILURE;
          exit(exit_code);
        }
      }
    }
  }
#if TCL_READLINE
  return Tcl_Eval(interp, "::tclreadline::Loop");
#else
  return TCL_OK;
#endif
}

static void
initStaApp(int &argc,
	   char *argv[],
	   Tcl_Interp *interp)
{
  initSta();
  Sta *sta = new Sta;
  Sta::setSta(sta);
  sta->makeComponents();
  sta->setTclInterp(interp);
  int thread_count = parseThreadsArg(argc, argv);
  sta->setThreadCount(thread_count);

  // Define swig TCL commands.
  Sta_Init(interp);
  // Eval encoded sta TCL sources.
  evalTclInit(interp, tcl_inits);
  Tcl_Eval(interp, "init_sta_cmds");
}

static void
showUsage(const char *prog,
	  const char *init_filename)
{
  printf("Usage: %s [-help] [-version] [-no_init] [-exit] cmd_file\n", prog);
  printf("  -help              show help and exit\n");
  printf("  -version           show version and exit\n");
  printf("  -no_init           do not read %s init file\n", init_filename);
  printf("  -threads count|max use count threads\n");
  printf("  -no_splash         do not show the license splash at startup\n");
  printf("  -exit              exit after reading cmd_file\n");
  printf("  cmd_file           source cmd_file\n");
}
