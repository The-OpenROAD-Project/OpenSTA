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
#include "DisallowCopyAssign.hh"
#include "Report.hh"
#include "ReportStd.hh"

namespace sta {

// Output streams that talk to stdout and stderr streams.
class ReportStd : public Report
{
public:
  ReportStd();

protected:
  virtual size_t printConsole(const char *buffer, size_t length);
  virtual size_t printErrorConsole(const char *buffer, size_t length);

private:
  DISALLOW_COPY_AND_ASSIGN(ReportStd);
};

Report *
makeReportStd()
{
  return new ReportStd;
}

ReportStd::ReportStd() :
  Report()
{
}

size_t
ReportStd::printConsole(const char *buffer, size_t length)
{
  return fwrite(buffer, sizeof(char), length, stdout);
}

size_t
ReportStd::printErrorConsole(const char *buffer, size_t length)
{
  return fwrite(buffer, sizeof(char), length, stderr);
}

} // namespace
