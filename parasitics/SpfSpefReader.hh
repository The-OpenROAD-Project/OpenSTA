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

#ifndef STA_SPF_SPEF_READER_H
#define STA_SPF_SPEF_READER_H

#include "Zlib.hh"
#include "NetworkClass.hh"
#include "ParasiticsClass.hh"

namespace sta {

class Report;
class OperatingConditions;
class MinMax;
class Corner;

// Common to Spf and Spef readers.
class SpfSpefReader
{
public:
  // Constraint min/max cnst_min_max and operating condition op_cond
  // are used for parasitic network reduction.
  SpfSpefReader(const char *filename,
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
		Parasitics *parasitics);
  virtual ~SpfSpefReader();
  char divider() const { return divider_; }
  void setDivider(char divider);
  char delimiter() const { return delimiter_; }
  void setDelimiter(char delimiter);
  void setBusBrackets(char left, char right);
  void incrLine();
  int line() const { return line_; }
  const char *filename() const { return filename_; }
  // flex YY_INPUT yy_n_chars arg changed definition from int to size_t,
  // so provide both forms.
  void getChars(char *buf,
		int &result,
		size_t max_size);
  void getChars(char *buf,
		size_t &result,
		size_t max_size);
  // Translate from spf/spef namespace to sta namespace.
  char *translated(const char *token);
  void warn(const char *fmt, ...)
    __attribute__((format (printf, 2, 3)));

protected:
  Pin *findPinRelative(const char *name);
  Pin *findPortPinRelative(const char *name);
  Net *findNetRelative(const char *name);
  Instance *findInstanceRelative(const char *name);

  const char *filename_;
  Instance *instance_;
  const ParasiticAnalysisPt *ap_;
  bool increment_;
  bool pin_cap_included_;
  bool keep_coupling_caps_;
  ReduceParasiticsTo reduce_to_;
  bool delete_after_reduce_;
  const OperatingConditions *op_cond_;
  const Corner *corner_;
  const MinMax *cnst_min_max_;
  // Normally no need to keep device names.
  bool keep_device_names_;
  bool quiet_;
  gzFile stream_;
  int line_;
  char divider_;
  char delimiter_;
  char bus_brkt_left_;
  char bus_brkt_right_;
  Net *net_;
  Report *report_;
  Network *network_;
  Parasitics *parasitics_;
};

} // namespace
#endif
