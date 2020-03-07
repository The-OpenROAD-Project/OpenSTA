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

#include <stdio.h>
#include <stdarg.h>
#include <string>
#include <tcl.h>
#include "DisallowCopyAssign.hh"

namespace sta {

using std::string;

// Output streams used for printing.
// This is a wrapper for all printing.  It supports logging output to
// a file and redirection of command output to a file.
class Report
{
public:
  Report();
  virtual ~Report();

  // Primitives to print output.
  // Return the number of characters written.
  virtual size_t printString(const char *buffer, size_t length);
  virtual void print(const char *fmt, ...);
  virtual void vprint(const char *fmt, va_list args);
  void print(const string *str);
  void print(const string &str);

  // Print to debug stream (same as output stream).
  virtual void printDebug(const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));
  virtual void vprintDebug(const char *fmt, va_list args);

  // Print to error stream.
  // Return the number of characters written.
  virtual size_t printError(const char *buffer, size_t length);
  virtual void printError(const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));
  virtual void vprintError(const char *fmt, va_list args);

  // Report error.
  virtual void error(const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));
  virtual void verror(const char *fmt, va_list args);
  // Report error in a file.
  virtual void fileError(const char *filename, int line, const char *fmt, ...)
    __attribute__((format (printf, 4, 5)));
  virtual void vfileError(const char *filename, int line, const char *fmt,
			  va_list args);

  // Print to warning stream (same as error stream).
  virtual void printWarn(const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));
  virtual void vprintWarn(const char *fmt, va_list args);
  // Report warning.
  virtual void warn(const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));
  virtual void vwarn(const char *fmt, va_list args);
  // Report warning in a file.
  virtual void fileWarn(const char *filename, int line, const char *fmt, ...)
    __attribute__((format (printf, 4, 5)));
  virtual void vfileWarn(const char *filename, int line, const char *fmt,
			 va_list args);

  // Log output to filename until logEnd is called.
  virtual void logBegin(const char *filename);
  virtual void logEnd();

  // Redirect output to filename until redirectFileEnd is called.
  virtual void redirectFileBegin(const char *filename);
  // Redirect append output to filename until redirectFileEnd is called.
  virtual void redirectFileAppendBegin(const char *filename);
  virtual void redirectFileEnd();
  // Redirect output to a string until redirectStringEnd is called.
  virtual void redirectStringBegin();
  virtual const char *redirectStringEnd();
  virtual void setTclInterp(Tcl_Interp *) {}

protected:
  // Primitive to print output on the console.
  // Return the number of characters written.
  virtual size_t printConsole(const char *buffer, size_t length) = 0;
  // Primitive to print error, warning and debug output.
  // Return the number of characters written.
  virtual size_t printErrorConsole(const char *buffer, size_t length) = 0;
  void printToBuffer(const char *fmt, va_list args);
  void redirectStringPrint(const char *buffer, size_t length);

  FILE *log_stream_;
  FILE *redirect_stream_;
  bool redirect_to_string_;
  string redirect_string_;
  // Buffer to support printf style arguments.
  size_t buffer_size_;
  char *buffer_;
  // Length of string in buffer.
  size_t buffer_length_;

private:
  DISALLOW_COPY_AND_ASSIGN(Report);
};

} // namespace
