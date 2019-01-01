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

#include <stdarg.h>
#include "Machine.hh"
#include "Zlib.hh"
#include "Report.hh"
#include "MinMax.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "Sdc.hh"
#include "SpefNamespace.hh"
#include "SpfSpefReader.hh"
#include "Parasitics.hh"

namespace sta {

SpfSpefReader::SpfSpefReader(const char *filename,
			     gzFile stream,
			     int line,
			     Instance *instance,
			     ParasiticAnalysisPt *ap,
			     bool increment,
			     bool pin_cap_included,
			     bool keep_coupling_caps,
			     float coupling_cap_factor,
			     ReduceParasiticsTo reduce_to,
			     bool delete_after_reduce,
			     const OperatingConditions *op_cond,
			     const Corner *corner,
			     const MinMax *cnst_min_max,
			     bool quiet,
			     Report *report,
			     Network *network,
			     Parasitics *parasitics) :
  filename_(filename),
  instance_(instance),
  ap_(ap),
  increment_(increment),
  pin_cap_included_(pin_cap_included),
  keep_coupling_caps_(keep_coupling_caps),
  reduce_to_(reduce_to),
  delete_after_reduce_(delete_after_reduce),
  op_cond_(op_cond),
  corner_(corner),
  cnst_min_max_(cnst_min_max),
  keep_device_names_(false),
  quiet_(quiet),
  stream_(stream),
  line_(line),
  // defaults
  divider_('\0'),
  delimiter_('\0'),
  bus_brkt_left_('\0'),
  bus_brkt_right_('\0'),
  net_(NULL),
  report_(report),
  network_(network),
  parasitics_(parasitics)
{
  ap->setCouplingCapFactor(coupling_cap_factor);
}

SpfSpefReader::~SpfSpefReader()
{
}

void
SpfSpefReader::setDivider(char divider)
{
  divider_ = divider;
}

void
SpfSpefReader::setDelimiter(char delimiter)
{
  delimiter_ = delimiter;
}

void
SpfSpefReader::setBusBrackets(char left, char right)
{
  bus_brkt_left_ = left;
  bus_brkt_right_ = right;
}

Instance *
SpfSpefReader::findInstanceRelative(const char *name)
{
  return network_->findInstanceRelative(instance_, name);
}

Net *
SpfSpefReader::findNetRelative(const char *name)
{
  return network_->findNetRelative(instance_, name);
}

Pin *
SpfSpefReader::findPinRelative(const char *name)
{
  return network_->findPinRelative(instance_, name);
}

Pin *
SpfSpefReader::findPortPinRelative(const char *name)
{
  return network_->findPin(instance_, name);
}

void
SpfSpefReader::getChars(char *buf,
			int &result,
			size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_NULL
  else
    result = static_cast<int>(strlen(buf));
}

void
SpfSpefReader::getChars(char *buf,
			size_t &result,
			size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_NULL
  else
    result = strlen(buf);
}

char *
SpfSpefReader::translated(const char *token)
{
  return spefToSta(token, divider_, network_->pathDivider(),
		   network_->pathEscape());
}

void
SpfSpefReader::incrLine()
{
  line_++;
}

void
SpfSpefReader::warn(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(filename_, line_, fmt, args);
  va_end(args);
}

} // namespace
