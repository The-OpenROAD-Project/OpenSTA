// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include <string>
#include <string_view>
#include <vector>

#include "GraphClass.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "Report.hh"
#include "SdcClass.hh"
#include "StaState.hh"
#include "TimingRole.hh"
#include "Transition.hh"

namespace sta {

class Report;
class SdfTriple;
class SdfPortSpec;
class SdfScanner;

using SdfTripleSeq = std::vector<SdfTriple*>;

class SdfReader : public StaState
{
public:
  SdfReader(std::string_view filename,
            std::string_view path,
            int arc_min_index,
            int arc_max_index,
            AnalysisType analysis_type,
            bool unescaped_dividers,
            bool is_incremental_only,
            MinMaxAll *cond_use,
            StaState *sta);
  ~SdfReader() override;
  bool read();

  void setDivider(char divider);
  void setTimescale(float multiplier,
                    std::string_view units);
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
                               const float *value,
                               int triple_index,
                               int arc_delay_index,
                               const MinMax *min_max);
  void setInstance();
  void setInstance(std::string_view instance_name);
  void setInstanceWildcard();
  void cellFinish();
  void setCell(std::string_view cell_name);
  void interconnect(std::string_view from_pin_name,
                    std::string_view to_pin_name,
                    SdfTripleSeq *triples);
  void iopath(SdfPortSpec *from_edge,
              std::string_view to_port_name,
              SdfTripleSeq *triples,
              std::string_view cond,
              bool condelse);
  void timingCheck(const TimingRole *role,
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
                             const TimingRole *setup_role,
                             const TimingRole *hold_role);
  void timingCheckNochange(SdfPortSpec *data_edge,
                           SdfPortSpec *clk_edge,
                           SdfTriple *before_triple,
                           SdfTriple *after_triple);
  void port(std::string_view o_pin_name,
            SdfTripleSeq *triples);
  void device(SdfTripleSeq *triples);
  void device(std::string_view to_port_name,
              SdfTripleSeq *triples);

  SdfTriple *makeTriple();
  SdfTriple *makeTriple(float value);
  SdfTriple *makeTriple(float *min,
                        float *typ,
                        float *max);
  void deleteTriple(SdfTriple *triple);
  SdfTripleSeq *makeTripleSeq();
  void deleteTripleSeq(SdfTripleSeq *triples);
  SdfPortSpec *makePortSpec(const Transition *tr,
                            std::string_view port);
  SdfPortSpec *makePortSpec(const Transition *tr,
                            std::string_view port,
                            std::string_view cond);
  SdfPortSpec *makeCondPortSpec(std::string_view cond_port);
  std::string unescaped(std::string_view token);
  std::string makePath(std::string_view head,
                       std::string_view tail);
  // Parser state used to control lexer for COND handling.
  bool inTimingCheck() { return in_timing_check_; }
  void setInTimingCheck(bool in);
  bool inIncremental() const { return in_incremental_; }
  void setInIncremental(bool incr);
  std::string makeBusName(std::string_view base_name,
                          int index);
  std::string_view filename() const { return filename_; }
  int sdfLine() const;
  template <typename... Args>
  void warn(int id,
               std::string_view fmt,
               Args &&...args)
  {
    report_->fileWarn(id, filename_, sdfLine(), fmt,
                     std::forward<Args>(args)...);
  }
  template <typename... Args>
  void error(int id,
                std::string_view fmt,
                Args &&...args)
  {
    report_->fileError(id, filename_, sdfLine(), fmt,
                      std::forward<Args>(args)...);
  }

private:
  Edge *findCheckEdge(Pin *from_pin,
                      Pin *to_pin,
                      const TimingRole *sdf_role,
                      const std::string *cond_start,
                      const std::string *cond_end);
  Edge *findWireEdge(Pin *from_pin,
                     Pin *to_pin);
  bool condMatch(std::string_view sdf_cond,
                 std::string_view lib_cond);
  void timingCheck1(const TimingRole *role,
                    Port *data_port,
                    SdfPortSpec *data_edge,
                    Port *clk_port,
                    SdfPortSpec *clk_edge,
                    SdfTriple *triple);
  bool annotateCheckEdges(Pin *data_pin,
                          SdfPortSpec *data_edge,
                          Pin *clk_pin,
                          SdfPortSpec *clk_edge,
                          const TimingRole *sdf_role,
                          SdfTriple *triple,
                          bool match_generic);
  Pin *findPin(std::string_view name);
  Instance *findInstance(std::string_view name);
  void setEdgeDelays(Edge *edge,
                     SdfTripleSeq *triples,
                     std::string_view sdf_cmd);
  void setDevicePinDelays(Pin *to_pin,
                          SdfTripleSeq *triples);
  Port *findPort(const Cell *cell,
                 std::string_view port_name);

  std::string_view filename_;
  SdfScanner *scanner_;
  std::string_view path_;
  // Which values to pull out of the sdf triples.
  int triple_min_index_{0};
  int triple_max_index_{2};
  // Which arc delay value to deposit the sdf values into.
  int arc_delay_min_index_;
  int arc_delay_max_index_;
  AnalysisType analysis_type_;
  bool unescaped_dividers_;
  bool is_incremental_only_;
  MinMaxAll *cond_use_;

  char divider_{'/'};
  char escape_{'\\'};
  Instance *instance_{nullptr};
  std::string cell_name_;
  bool in_timing_check_{false};
  bool in_incremental_{false};
  float timescale_{1.0E-9F};  // default units of ns

  static const int null_index_ = -1;
};

} // namespace sta
