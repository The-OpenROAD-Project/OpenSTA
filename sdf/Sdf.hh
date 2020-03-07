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

#pragma once

#include "DisallowCopyAssign.hh"
#include "Zlib.hh"
#include "Vector.hh"
#include "TimingRole.hh"
#include "Transition.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SdcClass.hh"
#include "StaState.hh"

// Header for ReadSdf.cc to communicate with SdfLex.cc, SdfParse.cc

// global namespace

#define YY_INPUT(buf,result,max_size) \
  sta::sdf_reader->getChars(buf, result, max_size)
int
SdfParse_error(const char *msg);

namespace sta {

class Report;
class SdfTriple;
class SdfPortSpec;

typedef Vector<SdfTriple*> SdfTripleSeq;

class SdfReader : public StaState
{
public:
  SdfReader(const char *filename,
	    const char *path,
	    int arc_min_index,
	    int triple_min_index,
	    int arc_max_index,
	    int triple_max_index,
	    AnalysisType analysis_type,
	    bool unescaped_dividers,
	    bool is_incremental_only,
            MinMaxAll *cond_use,
	    StaState *sta);
  ~SdfReader();
  bool read();
  // Arc/Triple index passed to read() to ignore arg.
  static int nullIndex() { return null_index_; }

  void setDivider(char divider);
  void setTimescale(float multiplier, const char *units);
  void setPortDeviceDelay(Edge *edge,
			  SdfTripleSeq *triples,
			  bool from_trans);
  void setEdgeArcDelays(Edge *edge,
			TimingArc *arc,
			SdfTriple *triple);
  void setEdgeArcDelays(Edge *edge,
			TimingArc *arc,
			SdfTriple *triple,
			int triple_index,
			int arc_delay_index);
  void setEdgeArcDelaysCondUse(Edge *edge,
			       TimingArc *arc,
			       SdfTriple *triple);
  void setEdgeArcDelaysCondUse(Edge *edge,
			       TimingArc *arc,
			       float *value,
			       int triple_index,
			       int arc_delay_index,
			       const MinMax *min_max);
  void setInstance(const char *instance_name);
  void setInstanceWildcard();
  void cellFinish();
  void setCell(const char *cell_name);
  void interconnect(const char *from_pin_name,
		    const char *to_pin_name,
		    SdfTripleSeq *triples);
  void iopath(SdfPortSpec *from_edge,
	      const char *to_port_name,
	      SdfTripleSeq *triples,
	      const char *cond,
	      bool condelse);
  void timingCheck(TimingRole *role,
		   SdfPortSpec *data_edge,
		   SdfPortSpec *clk_edge,
		   SdfTriple *triple);
  void timingCheckWidth(SdfPortSpec *edge,
			SdfTriple *triple);
  void timingCheckPeriod(SdfPortSpec *edge,
			 SdfTriple *triple);
  void timingCheckSetupHold(SdfPortSpec *data_edge,
			    SdfPortSpec *clk_edge,
			    SdfTriple *setup_triple,
			    SdfTriple *hold_triple);
  void timingCheckRecRem(SdfPortSpec *data_edge,
			 SdfPortSpec *clk_edge,
			 SdfTriple *rec_triple,
			 SdfTriple *rem_triple);
  void timingCheckNochange(SdfPortSpec *data_edge,
			   SdfPortSpec *clk_edge,
			   SdfTriple *before_triple,
			   SdfTriple *after_triple);
  void port(const char *to_pin_name,
	    SdfTripleSeq *triples);
  void device(SdfTripleSeq *triples);
  void device(const char *to_pin_name,
	      SdfTripleSeq *triples);

  SdfTriple *makeTriple();
  SdfTriple *makeTriple(float value);
  SdfTriple *makeTriple(float *min,
			float *typ,
			float *max);
  void deleteTriple(SdfTriple *triple);
  SdfTripleSeq *makeTripleSeq();
  void deleteTripleSeq(SdfTripleSeq *triples);
  SdfPortSpec *makePortSpec(Transition *tr,
			    const char *port,
			    const char *cond);
  SdfPortSpec *makeCondPortSpec(char *cond_port);
  const char *unescaped(const char *s);
  // Parser state used to control lexer for COND handling.
  bool inTimingCheck() { return in_timing_check_; }
  void setInTimingCheck(bool in);
  bool inIncremental() const { return in_incremental_; }
  void setInIncremental(bool incr);

  // flex YY_INPUT yy_n_chars arg changed definition from int to size_t,
  // so provide both forms.
  void getChars(char *buf,
		size_t &result,
		size_t max_size);
  void getChars(char *buf,
		int &result,
		size_t max_size);
  void incrLine();
  const char *filename() { return filename_; }
  int line() { return line_; }
  void sdfError(const char *fmt, ...);
  void notSupported(const char *feature);

private:
  DISALLOW_COPY_AND_ASSIGN(SdfReader);
  int readSdfFile1(Network *network,
		   Graph *graph,
		   const char *filename);
  Edge *findCheckEdge(Pin *from_pin,
		      Pin *to_pin,
		      TimingRole *sdf_role,
		      const char *cond_start,
		      const char *cond_end);
  Edge *findWireEdge(Pin *from_pin,
		     Pin *to_pin);
  bool condMatch(const char *sdf_cond,
		 const char *lib_cond);
  void timingCheck1(TimingRole *role,
		    SdfPortSpec *data_edge,
		    SdfPortSpec *clk_edge,
		    SdfTriple *triple,
		    bool warn);
  bool annotateCheckEdges(Pin *data_pin,
			  SdfPortSpec *data_edge,
			  Pin *clk_pin,
			  SdfPortSpec *clk_edge,
			  TimingRole *sdf_role,
			  SdfTriple *triple,
			  bool match_generic);
  void deletePortSpec(SdfPortSpec *edge);
  void portNotFound(const char *port_name);
  Pin *findPin(const char *name);
  Instance *findInstance(const char *name);
  void setEdgeDelays(Edge *edge,
		     SdfTripleSeq *triples,
		     const char *sdf_cmd);
  void setDevicePinDelays(Pin *to_pin,
			  SdfTripleSeq *triples);

  const char *filename_;
  const char *path_;
  // Which values to pull out of the sdf triples.
  int triple_min_index_;
  int triple_max_index_;
  // Which arc delay value to deposit the sdf values into.
  int arc_delay_min_index_;
  int arc_delay_max_index_;
  AnalysisType analysis_type_;
  bool unescaped_dividers_;
  bool is_incremental_only_;
  MinMaxAll *cond_use_;

  int line_;
  gzFile stream_;
  char divider_;
  char escape_;
  Instance *instance_;
  const char *cell_name_;
  bool in_timing_check_;
  bool in_incremental_;
  float timescale_;

  static const int null_index_ = -1;
};

extern SdfReader *sdf_reader;

} // namespace
