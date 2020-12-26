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

#include "Report.hh"

#include <algorithm> // min
#include <cstdlib>

#include "Error.hh"

namespace sta {

using std::min;

Report *Report::default_ = nullptr;

Report::Report() :
  log_stream_(nullptr),
  redirect_stream_(nullptr),
  redirect_to_string_(false),
  buffer_size_(1000),
  buffer_(new char[buffer_size_]),
  buffer_length_(0)
{
  default_ = this;
}

Report::~Report()
{
  delete [] buffer_;
}

void
Report::printToBuffer(const char *fmt,
                      va_list args)
{
  // Copy args in case we need to grow the buffer.
  va_list args_copy;
  va_copy(args_copy, args);
  buffer_length_ = vsnprint(buffer_, buffer_size_, fmt, args);
  if (buffer_length_ >= buffer_size_) {
    delete [] buffer_;
    buffer_size_ = buffer_length_ * 2;
    buffer_ = new char[buffer_size_];
    buffer_length_ = vsnprint(buffer_, buffer_size_, fmt, args_copy);
  }
  va_end(args_copy);
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
      ret = min(ret, fwrite(buffer, sizeof(char), length, redirect_stream_));
    else
      ret = min(ret, printConsole(buffer, length));
    if (log_stream_)
      ret = min(ret, fwrite(buffer, sizeof(char), length, log_stream_));
  }
  return ret;
}

void
Report::print(const string *str)
{
  printString(str->c_str(), str->size());
}

void
Report::print(const string &str)
{
  printString(str.c_str(), str.size());
}

void
Report::vprint(const char *fmt,
               va_list args)
{
  std::unique_lock<std::mutex> lock(buffer_lock_);
  printToBuffer(fmt, args);
  printString(buffer_, buffer_length_);
}

void
Report::print(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprint(fmt, args);
  va_end(args);
}

size_t
Report::printError(const char *buffer,
                   size_t length)
{
  size_t ret = length;
  if (redirect_to_string_)
    redirectStringPrint(buffer, length);
  else {
    if (redirect_stream_)
      ret = min(ret, fwrite(buffer, sizeof(char), length, redirect_stream_));
    else
      ret = min(ret, printErrorConsole(buffer, length));
    if (log_stream_)
      ret = min(ret, fwrite(buffer, sizeof(char), length, log_stream_));
  }
  return ret;
}

void
Report::vprintError(const char *fmt,
                    va_list args)
{
  std::unique_lock<std::mutex> lock(buffer_lock_);
  printToBuffer(fmt, args);
  printError(buffer_, buffer_length_);
}

void
Report::printError(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintError(fmt, args);
  va_end(args);
}

void
Report::printDebug(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprint(fmt, args);
  va_end(args);
}

void
Report::vprintDebug(const char *fmt,
                    va_list args)
{
  vprint(fmt, args);
}

////////////////////////////////////////////////////////////////

void
Report::printWarn(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  vprintError(fmt, args);
  va_end(args);
}

void
Report::vprintWarn(const char *fmt,
                   va_list args)
{
  vprintError(fmt, args);
}

void
Report::warn(int /* id */,
             const char *fmt,
             ...)
{
  printWarn("Warning: ");
  va_list args;
  va_start(args, fmt);
  vprintWarn(fmt, args);
  printWarn("\n");
  va_end(args);
}

void
Report::fileWarn(int /* id */,
                 const char *filename,
                 int line,
                 const char *fmt,
                 ...)
{
  printWarn("Warning: %s, line %d ", filename, line);
  va_list args;
  va_start(args, fmt);
  vprintWarn(fmt, args);
  printWarn("\n");
  va_end(args);
}

void
Report::vfileWarn(int /* id */,
                  const char *filename,
                  int line,
                  const char *fmt,
		  va_list args)
{
  printWarn("Warning: %s, line %d ", filename, line);
  vprintWarn(fmt, args);
  printWarn("\n");
}

////////////////////////////////////////////////////////////////

void
Report::error(int /* id */,
              const char *fmt, ...)
{
  printError("Error: ");
  va_list args;
  va_start(args, fmt);
  vprintError(fmt, args);
  printWarn("\n");
  va_end(args);
}

void
Report::fileError(int /* id */,
                  const char *filename,
                  int line,
                  const char *fmt,
                  ...)
{
  printError("Error: %s, line %d ", filename, line);
  va_list args;
  va_start(args, fmt);
  vprintError(fmt, args);
  printWarn("\n");
  va_end(args);
}

void
Report::vfileError(int /* id */,
                   const char *filename,
                   int line,
                   const char *fmt,
		    va_list args)
{
  printError("Error: %s, line %d ", filename, line);
  vprintError(fmt, args);
  printWarn("\n");
} 

////////////////////////////////////////////////////////////////

void
Report::critical(int /* id */,
                 const char *fmt,
                 ...)
{
  printError("Critical: ");
  va_list args;
  va_start(args, fmt);
  vprintError(fmt, args);
  printWarn("\n");
  va_end(args);
  exit(1);
}

void
Report::fileCritical(int /* id */,
                     const char *filename,
                     int line,
                     const char *fmt,
                     ...)
{
  printError("Critical: %s, line %d ", filename, line);
  va_list args;
  va_start(args, fmt);
  vprintError(fmt, args);
  printWarn("\n");
  va_end(args);
  exit(1);
}

////////////////////////////////////////////////////////////////

void
Report::logBegin(const char *filename)
{
  log_stream_ = fopen(filename, "w");
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
Report::redirectFileBegin(const char *filename)
{
  redirect_stream_ = fopen(filename, "w");
  if (redirect_stream_ == nullptr)
    throw FileNotWritable(filename);
}

void
Report::redirectFileAppendBegin(const char *filename)
{
  redirect_stream_ = fopen(filename, "a");
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

} // namespace
