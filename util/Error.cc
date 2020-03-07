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

#include <stdlib.h>
#include <stdio.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "Error.hh"

namespace sta {

Exception::Exception() :
  std::exception()
{
}

ExceptionLine::ExceptionLine(const char *filename,
			     int line) :
  Exception(),
  filename_(filename),
  line_(line)
{
}

InternalError::InternalError(const char *filename,
			     int line,
			     const char *msg) :
  ExceptionLine(filename, line),
  msg_(msg)
{
}

const char *
InternalError::what() const noexcept
{
  return stringPrintTmp("Internal error in %s:%d %s.",
			filename_, line_, msg_);
}

FileNotReadable::FileNotReadable(const char *filename) :
  filename_(filename)
{
}

const char *
FileNotReadable::what() const noexcept
{
  return stringPrintTmp("cannot read file %s.", filename_);
}

FileNotWritable::FileNotWritable(const char *filename) :
  filename_(filename)
{
}

const char *
FileNotWritable::what() const noexcept
{
  return stringPrintTmp("cannot write file %s.", filename_);
}

} // namespace
