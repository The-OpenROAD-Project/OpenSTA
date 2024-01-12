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

#include "Report.hh"

#include <algorithm> // min
#include <cstdlib>   // exit
#include <cstring>   // strlen

#include "Machine.hh"
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
      ret = min(ret, fwrite(buffer, sizeof(char), length, redirect_stream_));
    else
      ret = min(ret, printConsole(buffer, length));
    if (log_stream_)
      ret = min(ret, fwrite(buffer, sizeof(char), length, log_stream_));
  }
  return ret;
}

void
Report::reportLine(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  std::unique_lock<std::mutex> lock(buffer_lock_);
  printToBuffer(fmt, args);
  printBufferLine();
  va_end(args);
}

void
Report::reportBlankLine()
{
  printLine("", 0);
}

void
Report::reportLineString(const char *line)
{
  printLine(line, strlen(line));
}

void
Report::reportLineString(const string &line)
{
  printLine(line.c_str(), line.length());
}

////////////////////////////////////////////////////////////////

void
Report::printToBuffer(const char *fmt,
                      ...)
{
  va_list args;
  va_start(args, fmt);
  printToBuffer(fmt, args);
  va_end(args);
}

void
Report::printToBuffer(const char *fmt,
                      va_list args)
{
  buffer_length_ = 0;
  printToBufferAppend(fmt, args);
}

void
Report::printToBufferAppend(const char *fmt,
                            ...)
{
  va_list args;
  va_start(args, fmt);
  printToBufferAppend(fmt, args);
  va_end(args);
}

void
Report::printToBufferAppend(const char *fmt,
                            va_list args)
{
  // Copy args in case we need to grow the buffer.
  va_list args_copy;
  va_copy(args_copy, args);
  size_t length = vsnprint(buffer_ + buffer_length_, buffer_size_- buffer_length_,
                           fmt, args);
  if (length >= buffer_size_) {
    buffer_size_ = buffer_length_ + length * 2;
    char *new_buffer = new char[buffer_size_];
    strncpy(new_buffer, buffer_, buffer_length_);
    delete [] buffer_;
    buffer_ = new_buffer;
    length = vsnprint(buffer_ + buffer_length_, buffer_size_ - buffer_length_,
                      fmt, args_copy);
  }
  buffer_length_ += length;
  va_end(args_copy);
}

void
Report::printBufferLine()
{
  printLine(buffer_, buffer_length_);
}

////////////////////////////////////////////////////////////////

void
Report::warn(int /* id */,
             const char *fmt,
             ...)
{
  va_list args;
  va_start(args, fmt);
  printToBuffer("Warning: ");
  printToBufferAppend(fmt, args);
  printBufferLine();
  va_end(args);
}

void
Report::vwarn(int /* id */,
              const char *fmt,
              va_list args)
{
  printToBuffer("Warning: ");
  printToBufferAppend(fmt, args);
  printBufferLine();
}

void
Report::fileWarn(int /* id */,
                 const char *filename,
                 int line,
                 const char *fmt,
                 ...)
{
  va_list args;
  va_start(args, fmt);
  printToBuffer("Warning: %s line %d, ", filename, line);
  printToBufferAppend(fmt, args);
  printBufferLine();
  va_end(args);
}

void
Report::vfileWarn(int /* id */,
                  const char *filename,
                  int line,
                  const char *fmt,
                  va_list args)
{
  printToBuffer("Warning: %s line %d, ", filename, line);
  printToBufferAppend(fmt, args);
  printBufferLine();
}

////////////////////////////////////////////////////////////////

void
Report::error(int /* id */,
              const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  // No prefix msg, no \n.
  printToBuffer(fmt, args);
  va_end(args);
  throw ExceptionMsg(buffer_);
}

void
Report::verror(int /* id */,
               const char *fmt,
               va_list args)
{
  // No prefix msg, no \n.
  printToBuffer(fmt, args);
  throw ExceptionMsg(buffer_);
}

void
Report::fileError(int /* id */,
                  const char *filename,
                  int line,
                  const char *fmt,
                  ...)
{
  va_list args;
  va_start(args, fmt);
  // No prefix msg, no \n.
  printToBuffer("%s line %d, ", filename, line);
  printToBufferAppend(fmt, args);
  va_end(args);
  throw ExceptionMsg(buffer_);
}

void
Report::vfileError(int /* id */,
                   const char *filename,
                   int line,
                   const char *fmt,
                   va_list args)
{
  // No prefix msg, no \n.
  printToBuffer("%s line %d, ", filename, line);
  printToBufferAppend(fmt, args);
  throw ExceptionMsg(buffer_);
} 

////////////////////////////////////////////////////////////////

void
Report::critical(int /* id */,
                 const char *fmt,
                 ...)
{
  va_list args;
  va_start(args, fmt);
  printToBuffer("Critical: ");
  printToBufferAppend(fmt, args);
  printBufferLine();
  va_end(args);
}

void
Report::fileCritical(int /* id */,
                     const char *filename,
                     int line,
                     const char *fmt,
                     ...)
{
  va_list args;
  va_start(args, fmt);
  printToBuffer("Critical: %s line %d, ", filename, line);
  printToBufferAppend(fmt, args);
  printBufferLine();
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
