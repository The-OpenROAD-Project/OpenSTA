%module sdf

%{

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

#include "SdfReader.hh"
#include "ReportAnnotation.hh"
#include "SdfWriter.hh"
#include "Search.hh"
#include "Sta.hh"

using sta::Sta;
using sta::cmdLinkedNetwork;
using sta::AnalysisType;
using sta::MinMax;
using sta::MinMaxAllNull;
using sta::stringEq;
using sta::readSdfSingle;
using sta::readSdfMinMax;
using sta::reportAnnotatedDelay;
using sta::reportAnnotatedCheck;

%}

%inline %{

// min/max index is:
//  sdf_min = 0
//  sdf_typ = 1
//  sdf_max = 2

// If unescaped_dividers is true path names do not have to escape
// hierarchy dividers when the path name is quoted.
// For example verilog "\mod1/mod2 " can be referenced as "mod1/mod2"
// instead of the correct "mod1\/mod2".

// Read sdf_index value from sdf triples.
// Return true if successful.
bool
read_sdf_file_single(const char *filename,
		     const char *path,
		     Corner *corner,
		     int sdf_index,
		     AnalysisType analysis_type,
		     bool unescaped_dividers,
		     bool incremental_only,
		     MinMaxAllNull *cond_use)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  sta->ensureGraph();
  if (stringEq(path, ""))
    path = NULL;
  bool success = readSdfSingle(filename, path, corner, sdf_index,
			       analysis_type, unescaped_dividers,
			       incremental_only, cond_use, sta);
  sta->search()->arrivalsInvalid();
  return success;
}

// Read sdf_min_index and sdf_max_index values from sdf triples.
// Return true if successful.
bool
read_sdf_file_min_max(const char *filename,
		      const char *path,
		      Corner *corner,
		      int sdf_min_index,
		      int sdf_max_index,
		      AnalysisType analysis_type,
		      bool unescaped_dividers,
		      bool incremental_only,
		      MinMaxAllNull *cond_use)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  sta->ensureGraph();
  if (stringEq(path, ""))
    path = NULL;
  bool success = readSdfMinMax(filename, path, corner, sdf_min_index,
			       sdf_max_index, analysis_type,
			       unescaped_dividers, incremental_only,
			       cond_use, sta);
  sta->search()->arrivalsInvalid();
  return success;
}

void
report_annotated_delay_cmd(bool report_cells,
			   bool report_nets,
			   bool report_in_ports,
			   bool report_out_ports,
			   unsigned max_lines,
			   bool list_annotated,
			   bool list_not_annotated,
			   bool report_constant_arcs)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  sta->ensureGraph();
  reportAnnotatedDelay(report_cells, report_nets,
		       report_in_ports, report_out_ports,
		       max_lines, list_annotated, list_not_annotated,
		       report_constant_arcs, sta);
}

void
report_annotated_check_cmd(bool report_setup,
			   bool report_hold,
			   bool report_recovery,
			   bool report_removal,
			   bool report_nochange,
			   bool report_width,
			   bool report_period,
			   bool report_max_skew,
			   unsigned max_lines,
			   bool list_annotated,
			   bool list_not_annotated,
			   bool report_constant_arcs)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  sta->ensureGraph();
  reportAnnotatedCheck(report_setup, report_hold,
		       report_recovery, report_removal,
		       report_nochange, report_width,
		       report_period,  report_max_skew,
		       max_lines, list_annotated, list_not_annotated,
		       report_constant_arcs, sta);
}

void
write_sdf_cmd(char *filename,
	      Corner *corner,
	      char sdf_divider,
	      int digits,
	      bool gzip,
	      bool no_timestamp,
	      bool no_version)
{
  cmdLinkedNetwork();
  Sta *sta = Sta::sta();
  sta->writeSdf(filename, corner, sdf_divider, digits, gzip,
		no_timestamp, no_version);
}

%} // inline
