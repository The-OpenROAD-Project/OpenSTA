# Coding guidelines

## Naming conventions

| Kind | Convention | Example |
| --- | --- | --- |
| directory | lowercase | `directory` |
| filename | corresponding class name without prefix | `Filename.cc` |
| class | upper camel case | `ClassName` |
| member function | lower camel case | `memberFunction` |
| member variable | snake case with trailing underscore | `member_variable_` |
| function | lower camel case | `functionName` |
| variable | snake case | `variable` |
| comments | capitalized sentences that end with periods | |

The trailing underscore on member variables prevents a conflict with
the accessor member function name.

C++ source files should use a `.cc` file extension.
C++ header files should use a `.hh` file extension.

Use pragmas to protect headers from being read more than once instead of
`#ifdef` / `#define`.

```cpp
#pragma once
```

In general it is better for class variables to use pointers to
objects of other classes rather than embedding the instance directly.
This only requires that the class be declared rather than defined,
many times breaking a dependency on another header file.

Header files that define the classes of a sub-directory allow other
headers to have pointers to the objects without pulling in the details
of the class definitions. These headers are named `DirectoryClass.hh`
where `Directory` is the capitalized name of the sub-directory.

Place comments describing public functions and classes in header files
rather than code files because a consumer is more likely to have
access to the header and that is the first place they will look.

The return type of a function should be on the line before the
function name. Arguments should be on separate lines to make it easier
to remove or add them without having to reformat the lines as they
change length.

```cpp
return_type
function(type1 arg1,
         type2 arg2)
{
}
```

Functions should be less than one screen long. Break long functions
up into smaller ones. Lines should be less than 90 characters long.

Avoid assignments inside `if` conditions. For example, don't write
this:

```cpp
if ((foo = (char *) malloc (sizeof *foo)) == 0)
  fatal ("virtual memory exhausted");
```

instead, write this:

```cpp
foo = (char *) malloc (sizeof *foo);
if (foo == nullptr)
  fatal ("virtual memory exhausted");
```

Do not use braces around `if`/`for` that are one line.

```cpp
if (pred)
  bar = 1;
else
  bar = 3;
```

Use braces around `if`/`for` bodies that are more than one line.

```cpp
if (pred) {
  for (int i = 0; i < len; i++) {
     ...
  }
}
```

Add a default clause to all switches calling `switchCaseNotHandled`:

```cpp
switch (type) {
case edge_interconnect:
  ...
default:
  switchCaseNotHandled();
}
```

Put return types for functions on the line before the function name:

```cpp
Cell *
Library::findCell(char *name)
{
  ...
}
```

Class member functions should be grouped in public, protected and then
private order.

```cpp
class Frob
{
public:
protected:
private:

  friend class Frobulator;
};
```

Class member functions should not be defined inside the class unless they
are simple accessors that return a member variable.

Avoid using `[]` to lookup a map value because it creates a key/null value
pair if the lookup fails. Use `map::find` or `sta::findKey` instead.

Avoid nested classes/enums because SWIG has trouble with them.

Avoid all use of global variables as "caches", even if they are thread local.
OpenSTA goes to great lengths to minimize global state variables that prevent
multiple instances of the `Sta` class from coexisting.

Do not use `thread_local` variables. They are essentially global
variables so they prevent multiple instances of an `Sta` object from
existing concurrently, so they should also be avoided. Use stack state
in each thread instead.

## Regression tests

Most regression tests live in the private `pvt/test/` tree. A smaller
public subset ships in `test/` at the OpenSTA repo root.

Tests are run with the Tcl script `test/regression` (or `pvt/test/regression`):

```
Usage: regression [-help] [-threads threads] [-valgrind] [-report_stats] tests...
  -threads max|integer - number of threads to use
  -valgrind - run valgrind (linux memory checker)
  -report_stats - report run time and memory
  Wildcarding for test names is supported (enclose in "'s)
```

Test log files and results are in `test/results`. The `test/results/<test>.log`
is compared to `test/<test>.ok` to determine if a test passes.

Test scripts are written in Tcl and live in `pvt/test/` or `test/`.
Compress large Liberty, Verilog, and SPEF files. Use small or
existing Verilog and Liberty files to prevent repository bloat.

The test script should use a one line comment at the beginning of the
file so `head -1` can show what it is for. Use file names to roughly
group regressions and use numeric suffixes to distinguish them.

The script `test/save_ok` saves a `test/results/<test>.log` to
`test/<test>.ok`.

To add a new regression:

- add `<test>.tcl` to `test/`
- add the `<test>` name to `test/regression_vars.tcl`
- run the test with `test/regression <test>`
- use `save_ok <test>` to save the log file to `test/<test>.ok`
