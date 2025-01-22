// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "Error.hh"

#include <cstdlib>
#include <cstdio>

#include "StringUtil.hh"

namespace sta {

Exception::Exception() :
  std::exception()
{
}

ExceptionMsg::ExceptionMsg(const char *msg,
			   const bool suppressed) :
  Exception(),
  msg_(msg),
  suppressed_(suppressed)
{
}

const char *
ExceptionMsg::what() const noexcept
{
  return msg_.c_str();
}

ExceptionLine::ExceptionLine(const char *filename,
			     int line) :
  Exception(),
  filename_(filename),
  line_(line)
{
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
