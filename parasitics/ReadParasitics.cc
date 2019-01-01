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

#include <string.h>
#include <ctype.h>
#include "Machine.hh"
#include "Zlib.hh"
#include "Error.hh"
#include "Report.hh"
#include "StringUtil.hh"
#include "Parasitics.hh"
#include "SpfReader.hh"
#include "SpefReader.hh"
#include "ReadParasitics.hh"

namespace sta {

typedef enum {
  parasitics_file_dspf,
  parasitics_file_rspf,
  parasitics_file_spef,
  parasitics_file_unknown
} ParasiticsFileType;

static void
parsiticsFileType(gzFile stream,
		  Report *report,
		  ParasiticsFileType &file_type,
		  int &line_num);
static bool
isSpfComment(const char *line,
	     bool &in_multi_line_comment,
	     bool &in_single_line_comment);

bool
readParasiticsFile(const char *filename,
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
		   bool save, bool quiet,
		   Report *report,
		   Network *network,
		   Parasitics *parasitics)
{
  bool success = false;
  // Use zlib to uncompress gzip'd files automagically.
  gzFile stream = gzopen(filename, "rb");
  if (stream) {
    ParasiticsFileType file_type;
    int line_num;
    parsiticsFileType(stream, report, file_type, line_num);
    switch (file_type) {
    case parasitics_file_spef:
      success = readSpefFile(filename, stream, line_num,
			     instance, ap, increment, pin_cap_included,
			     keep_coupling_caps, coupling_cap_factor,
			     reduce_to, delete_after_reduce,
			     op_cond, corner, cnst_min_max,
			     save, quiet, report, network, parasitics);

      break;
    case parasitics_file_rspf:
      success = readSpfFile(filename, stream, line_num, true, instance, ap,
			    increment, pin_cap_included,
			    keep_coupling_caps, coupling_cap_factor,
			    reduce_to, delete_after_reduce,
			    op_cond, corner, cnst_min_max,
			    save, quiet, report, network, parasitics);
      break;
    case parasitics_file_dspf:
      success = readSpfFile(filename, stream, line_num, false, instance, ap,
			    increment, pin_cap_included,
			    keep_coupling_caps, coupling_cap_factor,
			    reduce_to, delete_after_reduce,
			    op_cond, corner, cnst_min_max,
			    save, quiet, report,
			    network, parasitics);
      break;
    case parasitics_file_unknown:
      report->error("unknown parasitics file type.\n");
      break;
    }
    gzclose(stream);
  }
  else
    throw FileNotReadable(filename);
  return success;
}

// Read the first line of a parasitics file to find its type.
static void
parsiticsFileType(gzFile stream,
		  Report *report,
		  ParasiticsFileType &file_type,
		  int &line_num)
{
  file_type = parasitics_file_unknown;
  line_num = 1;
  const int line_len = 100;
  char line[line_len];
  char *err;
  bool in_multi_line_comment = false;
  bool in_single_line_comment = false;
  // Skip comment lines before looking for file type.
  do {
    err = gzgets(stream, line, line_len);
    if (err == Z_NULL) {
      report->error("SPEF/RSPF/DSPF header not found.\n");
      file_type = parasitics_file_unknown;
    }
    if (line[strlen(line) - 1] == '\n')
      line_num++;
  } while (isSpfComment(line, in_multi_line_comment, in_single_line_comment)
	   || in_multi_line_comment
	   || in_single_line_comment);

  if (stringEq(line, "*SPEF", 5))
    file_type = parasitics_file_spef;
  else if (stringEq(line, "*|RSPF", 6))
    file_type = parasitics_file_rspf;
  else if (stringEq(line, "*|DSPF", 6))
    file_type = parasitics_file_dspf;
}

static bool
isSpfComment(const char *line,
	     bool &in_multi_line_comment,
	     bool &in_single_line_comment)
{
  const char *s = line;
  while (isspace(*s) && *s)
    s++;
  if (in_multi_line_comment) {
    in_multi_line_comment = (strstr(s, "*/") == NULL);
    return true;
  }
  else if (in_single_line_comment) {
    in_single_line_comment = (line[strlen(line) - 1] != '\n');
    return true;
  }
  else if (*s && stringEq(s, "/*", 2)) {
    in_multi_line_comment = strstr(s, "*/") == NULL;
    return true;
  }
  else if (*s && stringEq(s, "//", 2)) {
    in_single_line_comment = (line[strlen(line) - 1] != '\n');
    return true;
  }
  else if (*s && stringEq(s, "*", 1)
	   && !stringEq(s, "*|", 2)
	   && !stringEq(s, "*SPEF", 5)) {
    in_single_line_comment = false;
    // DSPF or RSPF comment.
    return true;
  }
  else
    return false;
}

} // namespace
