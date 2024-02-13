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

#pragma once

#include <exception>

#include "Report.hh"

namespace sta {

// Abstract base class for sta exceptions.
class Exception : public std::exception
{
public:
  Exception();
  virtual ~Exception() {}
  virtual const char *what() const noexcept = 0;
};

class ExceptionMsg : public Exception
{
public:
  ExceptionMsg(const char *msg);
  virtual const char *what() const noexcept;

private:
  string msg_;
};

class ExceptionLine : public Exception
{
public:
  ExceptionLine(const char *filename,
		int line);

protected:
  const char *filename_;
  int line_;
};

// Failure opening filename for reading.
class FileNotReadable : public Exception
{
public:
  explicit FileNotReadable(const char *filename);
  virtual const char *what() const noexcept;

protected:
  const char *filename_;
};

// Failure opening filename for writing.
class FileNotWritable : public Exception
{
public:
  explicit FileNotWritable(const char *filename);
  virtual const char *what() const noexcept;

protected:
  const char *filename_;
};

// Report an error condition that should not be possible.
// The default handler prints msg to stderr and exits.
// The msg should NOT include a period or return.
// Only for use in those cases where a Report object is not available. 
#define criticalError(id,msg) \
  Report::defaultReport()->fileCritical(id, __FILE__, __LINE__, msg)

} // namespace
