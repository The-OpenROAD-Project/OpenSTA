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
#include <readline/readline.h>
#include <readline/history.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "Vector.hh"
#include "Sta.hh"
#include "StaMain.hh"

namespace sta {

typedef sta::Vector<SwigInitFunc> SwigInitFuncSeq;

char **extra_command_completion (const char *, int, int);
char *extra_command_generator (const char *, int);
int command_exit (ClientData, Tcl_Interp *, int, Tcl_Obj *const []);

// "Arguments" passed to staTclAppInit.
static int sta_argc;
static char **sta_argv;
static const char **sta_tcl_inits;
static SwigInitFunc sta_swig_init;

static const char *init_filename = "[file join $env(HOME) .sta]";

static bool ended = 0;

void
staMain(Sta *sta,
	int argc,
	char *argv[],
	SwigInitFunc swig_init,
	const char *tcl_inits[])
{
  char *buffer;
  Tcl_Interp *myInterp;
  int status;

  initSta();

  Sta::setSta(sta);
  sta->makeComponents();

  int thread_count = parseThreadsArg(argc, argv);
  sta->setThreadCount(thread_count);

  staSetupAppInit(argc, argv, swig_init, tcl_inits);

  myInterp = Tcl_CreateInterp();
  Tcl_CreateObjCommand(myInterp, "exit", (Tcl_ObjCmdProc*)command_exit, 0, 0);

  rl_attempted_completion_function = extra_command_completion;

  staTclAppInit(myInterp);

  while((!ended) && (buffer = readline("OpenSTA> ")) != NULL) {
      status = Tcl_Eval(myInterp, buffer);
      if(status != TCL_OK) {
          fprintf(stderr, "%s\n", Tcl_GetStringResult(myInterp));
      }
      if (buffer[0] != 0)
          add_history(buffer);
      free(buffer);
      if(ended) break;
  }

  Tcl_DeleteInterp(myInterp);
  Tcl_Finalize();
}

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

int command_exit(ClientData, Tcl_Interp *, int, Tcl_Obj *const [])
{
  ended = 1;
  return 0;
}

char const *extra_commands[] = {
    "all_clocks",
    "all_inputs",
    "all_outputs",
    "all_registers",
    "check_setup",
    "create_clock",
    "create_generated_clock",
    "create_voltage_area",
    "current_design",
    "current_instance",
    "define_corners",

    "get_clocks",
    "get_fanin",
    "get_fanout",

    "get_nets",
    "get_pins",
    "get_ports",
    "read_liberty",
    "read_parasitics",
    "read_sdc",
    "read_sdf",
    "read_spef",
    "read_verilog",
    "report_annotated_delay",
    "report_cell",
    "report_checks",
    "report_path",
    "report_slack",
    "set_input_delay",
    "write_sdc",
    "write_sdf",
    NULL
};

char **extra_command_completion(const char *text, int, int)
{
  rl_attempted_completion_over = 0;
  return rl_completion_matches(text, extra_command_generator);
}

char *extra_command_generator(const char *text, int state)
{
  static int list_index, len;
  const char *name;

  if (!state) {
    list_index = 0;
    len = strlen(text);
  }

  while ((name = extra_commands[list_index++])) {
    if(strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }

  return NULL;
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
