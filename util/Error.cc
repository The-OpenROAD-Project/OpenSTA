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

#include <stdlib.h>
#include <stdio.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "Error.hh"

namespace sta {

StaException::StaException() :
  std::exception()
{
}

StaExceptionLine::StaExceptionLine(const char *filename,
				   int line) :
  StaException(),
  filename_(filename),
  line_(line)
{
}

InternalError::InternalError(const char *filename,
			     int line,
			     const char *msg) :
  StaExceptionLine(filename, line),
  msg_(msg)
{
}

const char *
InternalError::what() const throw()
{
  return stringPrintTmp("Internal error in %s:%d %s.",
			filename_, line_, msg_);
}

FileNotReadable::FileNotReadable(const char *filename) :
  filename_(filename)
{
}

const char *
FileNotReadable::what() const throw()
{
  return stringPrintTmp("Error: cannot read file %s.", filename_);
}

FileNotWritable::FileNotWritable(const char *filename) :
  filename_(filename)
{
}

const char *
FileNotWritable::what() const throw()
{
  return stringPrintTmp("Error: cannot write file %s.", filename_);
}

} // namespace
