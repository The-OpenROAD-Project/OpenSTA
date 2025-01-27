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

#pragma once

#include "Vector.hh"
#include "TimingRole.hh"
#include "Transition.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "SdcClass.hh"
#include "StaState.hh"

namespace sta {

class Report;
class SdfTriple;
class SdfPortSpec;
class SdfScanner;

typedef Vector<SdfTriple*> SdfTripleSeq;

class SdfReader : public StaState
{
public:
  SdfReader(const char *filename,
	    const char *path,
	    int arc_min_index,
	    int arc_max_index,
	    AnalysisType analysis_type,
	    bool unescaped_dividers,
	    bool is_incremental_only,
            MinMaxAll *cond_use,
	    StaState *sta);
  ~SdfReader();
  bool read();

  void setDivider(char divider);
  void setTimescale(float multiplier,
                    const string *units);
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
  void setInstance(const string *instance_name);
  void setInstanceWildcard();
  void cellFinish();
  void setCell(const string *cell_name);
  void interconnect(const string *from_pin_name,
		    const string *to_pin_name,
		    SdfTripleSeq *triples);
  void iopath(SdfPortSpec *from_edge,
	      const string *to_port_name,
	      SdfTripleSeq *triples,
	      const string *cond,
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
  void timingCheckSetupHold1(SdfPortSpec *data_edge,
                             SdfPortSpec *clk_edge,
                             SdfTriple *setup_triple,
                             SdfTriple *hold_triple,
                             TimingRole *setup_role,
                             TimingRole *hold_role);
  void timingCheckNochange(SdfPortSpec *data_edge,
			   SdfPortSpec *clk_edge,
			   SdfTriple *before_triple,
			   SdfTriple *after_triple);
  void port(const string *to_pin_name,
	    SdfTripleSeq *triples);
  void device(SdfTripleSeq *triples);
  void device(const string *to_pin_name,
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
			    const string *port,
			    const string *cond);
  SdfPortSpec *makeCondPortSpec(const string *cond_port);
  string *unescaped(const string *token);
  string *makePath(const string *head,
                   const string *tail);
  // Parser state used to control lexer for COND handling.
  bool inTimingCheck() { return in_timing_check_; }
  void setInTimingCheck(bool in);
  bool inIncremental() const { return in_incremental_; }
  void setInIncremental(bool incr);
  string *makeBusName(string *bus_name,
                      int index);
  const string &filename() const { return filename_; }
  void sdfWarn(int id,
               const char *fmt, ...);
  void sdfError(int id,
                const char *fmt,
                ...);
  void notSupported(const char *feature);

private:
  int readSdfFile1(Network *network,
		   Graph *graph,
		   const char *filename);
  Edge *findCheckEdge(Pin *from_pin,
		      Pin *to_pin,
		      TimingRole *sdf_role,
		      const string *cond_start,
		      const string *cond_end);
  Edge *findWireEdge(Pin *from_pin,
		     Pin *to_pin);
  bool condMatch(const string *sdf_cond,
		 const char *lib_cond);
  void timingCheck1(TimingRole *role,
                    Port *data_port,
                    SdfPortSpec *data_edge,
                    Port *clk_port,
                    SdfPortSpec *clk_edge,
                    SdfTriple *triple);
  bool annotateCheckEdges(Pin *data_pin,
			  SdfPortSpec *data_edge,
			  Pin *clk_pin,
			  SdfPortSpec *clk_edge,
			  TimingRole *sdf_role,
			  SdfTriple *triple,
			  bool match_generic);
  Pin *findPin(const string *name);
  Instance *findInstance(const string *name);
  void setEdgeDelays(Edge *edge,
		     SdfTripleSeq *triples,
		     const char *sdf_cmd);
  void setDevicePinDelays(Pin *to_pin,
			  SdfTripleSeq *triples);
  Port *findPort(const Cell *cell,
                 const string *port_name);

  string filename_;
  SdfScanner *scanner_;
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

  char divider_;
  char escape_;
  Instance *instance_;
  const string *cell_name_;
  bool in_timing_check_;
  bool in_incremental_;
  float timescale_;

  static const int null_index_ = -1;
};

} // namespace
