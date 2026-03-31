// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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
//
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
//
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <stdio.h>
#include <cstdarg>
#include <string>
#include <string_view>
#include <mutex>
#include <set>

#include "Machine.hh"  // __attribute__
#include "Format.hh"

struct Tcl_Interp;

namespace sta {

// Throws ExceptionMsg - implemented in Report.cc to avoid circular include with
// Error.hh
void
reportThrowExceptionMsg(const std::string &msg,
                        bool suppressed);

// Output streams used for printing.
// This is a wrapper for all printing.  It supports logging output to
// a file and redirection of command output to a file.
class Report
{
public:
  Report();
  virtual ~Report();

  virtual void reportLine(const std::string &line);
  virtual void reportBlankLine();

  // Print formatted line using std::format (C++20).
  template <typename... Args>
  void report(std::string_view fmt,
              Args &&...args)
  {
    reportMsg(sta::vformat(fmt, sta::make_format_args(args...)));
  }
  virtual void reportMsg(const std::string &formatted_msg)
  {
    reportLine(formatted_msg);
  }

  ////////////////////////////////////////////////////////////////

  // Report warning.
  template <typename... Args>
  void warn(int id,
            std::string_view fmt,
            Args &&...args)
  {
    if (!isSuppressed(id))
      warnMsg(id, sta::vformat(fmt, sta::make_format_args(args...)));
  }
  virtual void warnMsg(int id,
                       const std::string &formatted_msg) {
    reportLine(sta::format("Warning {}: {}", id, formatted_msg));
  }

  // Report warning in a file.
  template <typename... Args>
  void fileWarn(int id,
                std::string_view filename,
                int line,
                std::string_view fmt,
                Args &&...args)
  {
    if (!isSuppressed(id)) {
      fileWarnMsg(id, filename, line, 
                  sta::vformat(fmt, sta::make_format_args(args...)));
    }
  }
  virtual void
  fileWarnMsg(int id,
              std::string_view filename,
              int line,
              const std::string &formatted_msg) {
    reportLine(sta::format("Warning {}: {} line {}, {}",
                           id, filename, line, formatted_msg));
  }

  template <typename... Args>
  void error(int id,
             std::string_view fmt,
             Args &&...args)
  {
    errorMsg(id, sta::vformat(fmt, sta::make_format_args(args...)));
  }
  virtual void errorMsg(int id,
                        const std::string &formatted_msg)
  {
    reportThrowExceptionMsg(sta::format("{} {}", id, formatted_msg), isSuppressed(id));
  }
  // Report error in a file.
  template <typename... Args>
  void fileError(int id,
                 std::string_view filename,
                 int line,
                 std::string_view fmt,
                 Args &&...args)
  {
    fileErrorMsg(id, filename, line,
                 sta::vformat(fmt, sta::make_format_args(args...)));
  }
  virtual void fileErrorMsg(int id,
                            std::string_view filename,
                            int line,
                            const std::string &formatted_msg)
  {
    reportThrowExceptionMsg(sta::format("{} {} line {}, {}",
                                        id, filename, line, formatted_msg),
                            isSuppressed(id));
  }

  // Critical.
  // Report error condition that should not be possible or that prevents execution.
  // The default handler prints msg to stderr and exits.
  template <typename... Args>
  void critical(int id,
                std::string_view fmt,
                Args &&...args)
  {
    criticalMsg(id, sta::vformat(fmt, sta::make_format_args(args...)));
  }
  virtual void criticalMsg(int id,
                           const std::string &formatted_msg)
  {
    reportLine(sta::format("Critical {}: {}", id, formatted_msg));
    exit(1);
  }

  template <typename... Args>
  void fileCritical(int id,
                    std::string_view filename,
                    int line,
                    std::string_view fmt,
                    Args &&...args)
  {
    fileCriticalMsg(id, filename, line,
                    sta::vformat(fmt, sta::make_format_args(args...)));
  }
  virtual void fileCriticalMsg(int id,
                               std::string_view filename,
                               int line,
                               const std::string &formatted_msg)
  {
    reportLine(sta::format("Critical {}: {} line {}, {}", id, filename, line,
                           formatted_msg));
    exit(1);
  }

  // Log output to filename until logEnd is called.
  virtual void logBegin(std::string_view filename);
  virtual void logEnd();

  // Redirect output to filename until redirectFileEnd is called.
  virtual void redirectFileBegin(std::string_view filename);
  // Redirect append output to filename until redirectFileEnd is called.
  virtual void redirectFileAppendBegin(std::string_view filename);
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

  // Suppress message by id.
  void suppressMsgId(int id);
  void unsuppressMsgId(int id);
  [[nodiscard]] bool isSuppressed(int id);

protected:
  // All sta print functions have an implicit return printed by this function.
  virtual void printLine(const char *line,
                         size_t length);
  // Primitive to print output on the console.
  // Return the number of characters written.
  virtual size_t printConsole(const char *buffer,
                              size_t length);
  void printToBuffer(const char *fmt, ...) __attribute__((format(printf, 2, 3)));

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
  std::string redirect_string_;
  // Buffer to support printf style arguments.
  size_t buffer_size_;
  char *buffer_;
  // Length of string in buffer.
  size_t buffer_length_;
  std::mutex buffer_lock_;
  static Report *default_;
  std::set<int> suppressed_msg_ids_;

  friend class Debug;
};

}  // namespace sta
