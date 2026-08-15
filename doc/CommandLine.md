# Command line arguments

The command line arguments for `sta` are shown below.

```
Usage: sta [-help] [-version] [-no_init] [-no_splash] [-threads count|max] [-exit] cmd_file
  -help              show help and exit
  -version           show version and exit
  -no_init           do not read .sta init file
  -threads count|max use count threads
  -no_splash         do not show the license splash at startup
  -exit              exit after reading cmd_file
  cmd_file           source cmd_file
```

When OpenSTA starts up, commands are first read from the user
initialization file `~/.sta` if it exists. If a Tcl command file
`cmd_file` is specified on the command line, commands are read from
the file and executed before entering an interactive Tcl command
interpreter. If `-exit` is specified the application exits after
reading `cmd_file`. Use the Tcl `exit` command to exit the
application. The `-threads` option specifies how many parallel
threads to use. Use `-threads max` to use one thread per processor.

