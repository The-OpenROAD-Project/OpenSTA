// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#pragma once

#include <stdio.h>
#include <cstdarg>
#include <string>
#include <mutex>

#include "Machine.hh" // __attribute__

struct Tcl_Interp;

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

  // Print line with return.
  virtual void reportLine(const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));
  virtual void reportLineString(const char *line);
  virtual void reportLineString(const string &line);
  virtual void reportBlankLine();

  ////////////////////////////////////////////////////////////////

  // Report warning.
  virtual void warn(int id,
                    const char *fmt, ...)
    __attribute__((format (printf, 3, 4)));
  virtual void vwarn(int id,
                     const char *fmt,
                     va_list args);
  // Report warning in a file.
  virtual void fileWarn(int id,
                        const char *filename,
                        int line,
                        const char *fmt, ...)
    __attribute__((format (printf, 5, 6)));
  virtual void vfileWarn(int id,
                         const char *filename,
                         int line,
                         const char *fmt,
                         va_list args);

  virtual void error(int id,
                     const char *fmt, ...)
    __attribute__((format (printf, 3, 4)));
  virtual void verror(int id,
                      const char *fmt,
                      va_list args);
  // Report error in a file.
  virtual void fileError(int id,
                         const char *filename,
                         int line,
                         const char *fmt, ...)
    __attribute__((format (printf, 5, 6)));
  virtual void vfileError(int id,
                          const char *filename,
                          int line,
                          const char *fmt,
                          va_list args);

  // Critical. 
  // Report error condition that should not be possible or that prevents execution.
  // The default handler prints msg to stderr and exits.
  virtual void critical(int id,
                        const char *fmt,
                        ...)
    __attribute__((format (printf, 3, 4)));
  virtual void fileCritical(int id,
                            const char *filename,
                            int line,
                            const char *fmt,
                            ...)
    __attribute__((format (printf, 5, 6)));

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

  // Primitive to print output.
  // Return the number of characters written.
  // public for use by ReportTcl encapsulated channel functions.
  virtual size_t printString(const char *buffer,
                             size_t length);
  static Report *defaultReport() { return default_; }

protected:
  // All sta print functions have an implicit return printed by this function.
  virtual void printLine(const char *line,
                         size_t length);
  // Primitive to print output on the console.
  // Return the number of characters written.
  virtual size_t printConsole(const char *buffer,
                              size_t length);
  void printToBuffer(const char *fmt,
                     ...)
    __attribute__((format (printf, 2, 3)));

  void printToBuffer(const char *fmt,
                     va_list args);
  void printToBufferAppend(const char *fmt,
                           ...);
  void printToBufferAppend(const char *fmt,
                           va_list args);
  void printBufferLine();
  void redirectStringPrint(const char *buffer,
                           size_t length);

  FILE *log_stream_;
  FILE *redirect_stream_;
  bool redirect_to_string_;
  string redirect_string_;
  // Buffer to support printf style arguments.
  size_t buffer_size_;
  char *buffer_;
  // Length of string in buffer.
  size_t buffer_length_;
  std::mutex buffer_lock_;
  static Report *default_;

  friend class Debug;
};

} // namespace
