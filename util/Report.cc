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

#include "Report.hh"

#include <algorithm>  // min
#include <cstdlib>    // exit
#include <cstring>    // strlen

#include "Error.hh"
#include "Format.hh"
#include "Machine.hh"

namespace sta {

Report *Report::default_ = nullptr;

Report::Report()
{
  default_ = this;
}

size_t
Report::printConsole(const char *buffer,
                     size_t length)
{
  printf("%s", buffer);
  return length;
}

void
Report::printLine(const char *line,
                  size_t length)
{
  printString(line, length);
  printString("\n", 1);
}

size_t
Report::printString(const char *buffer,
                    size_t length)
{
  size_t ret = length;
  if (redirect_to_string_)
    redirectStringPrint(buffer, length);
  else {
    if (redirect_stream_)
      ret = std::min(ret, fwrite(buffer, sizeof(char), length, redirect_stream_));
    else
      ret = std::min(ret, printConsole(buffer, length));
    if (log_stream_)
      ret = std::min(ret, fwrite(buffer, sizeof(char), length, log_stream_));
  }
  return ret;
}

void
Report::reportBlankLine()
{
  printLine("", 0);
}

void
Report::reportLine(const std::string &line)
{
  printLine(line.c_str(), line.length());
}

////////////////////////////////////////////////////////////////

void
reportThrowExceptionMsg(const std::string &msg,
                        bool suppressed)
{
  throw ExceptionMsg(msg, suppressed);
}

////////////////////////////////////////////////////////////////

void
Report::suppressMsgId(int id)
{
  suppressed_msg_ids_.insert(id);
}

void
Report::unsuppressMsgId(int id)
{
  suppressed_msg_ids_.erase(id);
}

bool
Report::isSuppressed(int id)
{
  return suppressed_msg_ids_.contains(id);
}

////////////////////////////////////////////////////////////////

void
Report::logBegin(std::string_view filename)
{
  log_stream_ = fopen(std::string(filename).c_str(), "w");
  if (log_stream_ == nullptr)
    throw FileNotWritable(filename);
}

void
Report::logEnd()
{
  if (log_stream_)
    fclose(log_stream_);
  log_stream_ = nullptr;
}

void
Report::redirectFileBegin(std::string_view filename)
{
  redirect_stream_ = fopen(std::string(filename).c_str(), "w");
  if (redirect_stream_ == nullptr)
    throw FileNotWritable(filename);
}

void
Report::redirectFileAppendBegin(std::string_view filename)
{
  redirect_stream_ = fopen(std::string(filename).c_str(), "a");
  if (redirect_stream_ == nullptr)
    throw FileNotWritable(filename);
}

void
Report::redirectFileEnd()
{
  if (redirect_stream_)
    fclose(redirect_stream_);
  redirect_stream_ = nullptr;
}

void
Report::redirectStringBegin()
{
  redirect_to_string_ = true;
  redirect_string_.clear();
}

const char *
Report::redirectStringEnd()
{
  redirect_to_string_ = false;
  return redirect_string_.c_str();
}

void
Report::redirectStringPrint(const char *buffer,
                            size_t length)
{
  redirect_string_.append(buffer, length);
}

}  // namespace sta
