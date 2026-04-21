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

#include "sdf/SdfWriter.hh"

#include <cstdio>
#include <ctime>
#include <string>
#include <string_view>

#include "Format.hh"
#include "Fuzzy.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Liberty.hh"
#include "MinMaxValues.hh"
#include "Network.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "StaConfig.hh"  // STA_VERSION
#include "StaState.hh"
#include "StringUtil.hh"
#include "TimingArc.hh"
#include "TimingRole.hh"
#include "Units.hh"
#include "Zlib.hh"

namespace sta {

class SdfWriter : public StaState
{
public:
  SdfWriter(StaState *sta);
  void write(std::string_view filename,
             const Scene *scene,
             char sdf_divider,
             bool include_typ,
             int digits,
             bool gzip,
             bool no_timestamp,
             bool no_version);

protected:
  void writeHeader(LibertyLibrary *default_lib,
                   bool no_timestamp,
                   bool no_version);
  void writeTrailer();
  void writeInterconnects();
  void writeInstInterconnects(Instance *inst);
  void writeInterconnectFromPin(Pin *drvr_pin);

  void writeInstances();
  void writeInstHeader(const Instance *inst);
  void writeInstTrailer();
  void writeIopaths(const Instance *inst,
                    bool &inst_header);
  void writeIopathHeader();
  void writeIopathTrailer();
  void writeTimingChecks(const Instance *inst,
                         bool &inst_header);
  void ensureTimingCheckheaders(bool &check_header,
                                const Instance *inst,
                                bool &inst_header);
  void writeCheck(Edge *edge,
                  std::string_view sdf_check);
  void writeCheck(Edge *edge,
                  TimingArc *arc,
                  std::string_view sdf_check,
                  bool use_data_edge,
                  bool use_clk_edge);
  void writeEdgeCheck(Edge *edge,
                      std::string_view sdf_check,
                      int clk_rf_index,
                      TimingArc *arcs[RiseFall::index_count][RiseFall::index_count]);
  void writeTimingCheckHeader();
  void writeTimingCheckTrailer();
  void writeWidthCheck(const Pin *pin,
                       const RiseFall *hi_low,
                       float min_width,
                       float max_width);
  void writePeriodCheck(const Pin *pin,
                        float min_period);
  std::string_view sdfEdge(const Transition *tr);
  void writeArcDelays(Edge *edge);
  void writeSdfTriple(RiseFallMinMax &delays,
                      const RiseFall *rf);
  void writeSdfTriple(float min,
                      float max);
  void writeSdfDelay(double delay);
  std::string sdfPortName(const Pin *pin);
  std::string sdfPathName(const Pin *pin);
  std::string sdfPathName(const Instance *inst);
  std::string sdfName(const Instance *inst);

private:
  char sdf_divider_;
  bool include_typ_;
  float timescale_;

  char sdf_escape_{'\\'};
  char network_escape_;
  int digits_;

  gzFile stream_;
  const Scene *scene_;
  int arc_delay_min_index_;
  int arc_delay_max_index_;
};

void
writeSdf(std::string_view filename,
         const Scene *scene,
         char sdf_divider,
         bool include_typ,
         int digits,
         bool gzip,
         bool no_timestamp,
         bool no_version,
         StaState *sta)
{
  SdfWriter writer(sta);
  writer.write(filename, scene, sdf_divider, include_typ, digits, gzip,
               no_timestamp, no_version);
}

SdfWriter::SdfWriter(StaState *sta) :
  StaState(sta),
  network_escape_(network_->pathEscape())
{
}

void
SdfWriter::write(std::string_view filename,
                 const Scene *scene,
                 char sdf_divider,
                 bool include_typ,
                 int digits,
                 bool gzip,
                 bool no_timestamp,
                 bool no_version)
{
  sdf_divider_ = sdf_divider;
  include_typ_ = include_typ;
  digits_ = digits;

  LibertyLibrary *default_lib = network_->defaultLibertyLibrary();
  timescale_ = default_lib->units()->timeUnit()->scale();

  scene_ = scene;
  arc_delay_min_index_ = scene->dcalcAnalysisPtIndex(MinMax::min());
  arc_delay_max_index_ = scene->dcalcAnalysisPtIndex(MinMax::max());

  stream_ = gzopen(std::string(filename).c_str(), gzip ? "wb" : "wT");
  if (stream_ == nullptr)
    throw FileNotWritable(filename);

  writeHeader(default_lib, no_timestamp, no_version);
  writeInterconnects();
  writeInstances();
  writeTrailer();

  gzclose(stream_);
  stream_ = nullptr;
}

void
SdfWriter::writeHeader(LibertyLibrary *default_lib,
                       bool no_timestamp,
                       bool no_version)
{
  sta::print(stream_, "(DELAYFILE\n");
  sta::print(stream_, " (SDFVERSION \"3.0\")\n");
  sta::print(stream_, " (DESIGN \"{}\")\n",
             network_->cellName(network_->topInstance()));

  if (!no_timestamp) {
    time_t now;
    time(&now);
    char *time_str = ctime(&now);
    // Remove trailing \n.
    time_str[strlen(time_str) - 1] = '\0';
    sta::print(stream_, " (DATE \"{}\")\n", time_str);
  }

  sta::print(stream_, " (VENDOR \"Parallax\")\n");
  sta::print(stream_, " (PROGRAM \"STA\")\n");
  if (!no_version)
    sta::print(stream_, " (VERSION \"{}\")\n", STA_VERSION);
  sta::print(stream_, " (DIVIDER {:c})\n", sdf_divider_);

  LibertyLibrary *lib_min = default_lib;
  const LibertySeq &libs_min = scene_->libertyLibraries(MinMax::min());
  if (!libs_min.empty())
    lib_min = libs_min[0];
  LibertyLibrary *lib_max = default_lib;
  const LibertySeq &libs_max = scene_->libertyLibraries(MinMax::max());
  if (!libs_max.empty())
    lib_max = libs_max[0];

  OperatingConditions *cond_min = lib_min->defaultOperatingConditions();
  OperatingConditions *cond_max = lib_max->defaultOperatingConditions();
  if (cond_min && cond_max) {
    sta::print(stream_, " (VOLTAGE {:.3f}::{:.3f})\n",
               cond_min->voltage(),
               cond_max->voltage());
    sta::print(stream_, " (PROCESS \"{:.3f}::{:.3f}\")\n",
               cond_min->process(),
               cond_max->process());
    sta::print(stream_, " (TEMPERATURE {:.3f}::{:.3f})\n",
               cond_min->temperature(),
               cond_max->temperature());
  }

  const char *sdf_timescale = nullptr;
  if (fuzzyEqual(timescale_, 1e-6))
    sdf_timescale = "1us";
  else if (fuzzyEqual(timescale_, 10e-6))
    sdf_timescale = "10us";
  else if (fuzzyEqual(timescale_, 100e-6))
    sdf_timescale = "100us";
  else if (fuzzyEqual(timescale_, 1e-9))
    sdf_timescale = "1ns";
  else if (fuzzyEqual(timescale_, 10e-9))
    sdf_timescale = "10ns";
  else if (fuzzyEqual(timescale_, 100e-9))
    sdf_timescale = "100ns";
  else if (fuzzyEqual(timescale_, 1e-12))
    sdf_timescale = "1ps";
  else if (fuzzyEqual(timescale_, 10e-12))
    sdf_timescale = "10ps";
  else if (fuzzyEqual(timescale_, 100e-12))
    sdf_timescale = "100ps";
  if (sdf_timescale)
    sta::print(stream_, " (TIMESCALE {})\n", sdf_timescale);
}

void
SdfWriter::writeTrailer()
{
  sta::print(stream_, ")\n");
}

void
SdfWriter::writeInterconnects()
{
  sta::print(stream_, " (CELL\n");
  sta::print(stream_, "  (CELLTYPE \"{}\")\n",
             network_->cellName(network_->topInstance()));
  sta::print(stream_, "  (INSTANCE)\n");
  sta::print(stream_, "  (DELAY\n");
  sta::print(stream_, "   (ABSOLUTE\n");

  writeInstInterconnects(network_->topInstance());

  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    writeInstInterconnects(inst);
  }
  delete inst_iter;

  sta::print(stream_, "   )\n");
  sta::print(stream_, "  )\n");
  sta::print(stream_, " )\n");
}

void
SdfWriter::writeInstInterconnects(Instance *inst)
{
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    if (network_->isDriver(pin))
      writeInterconnectFromPin(pin);
  }
  delete pin_iter;
}

void
SdfWriter::writeInterconnectFromPin(Pin *drvr_pin)
{
  Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
  if (drvr_vertex) {
    VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->isWire()) {
        Pin *load_pin = edge->to(graph_)->pin();
        std::string drvr_pin_name = sdfPathName(drvr_pin);
        std::string load_pin_name = sdfPathName(load_pin);
        sta::print(stream_, "    (INTERCONNECT {} {} ",
                   drvr_pin_name,
                   load_pin_name);
        writeArcDelays(edge);
        sta::print(stream_, ")\n");
      }
    }
  }
}

void
SdfWriter::writeInstances()
{
  LeafInstanceIterator *leaf_iter = network_->leafInstanceIterator();
  while (leaf_iter->hasNext()) {
    const Instance *inst = leaf_iter->next();
    bool inst_header = false;
    writeIopaths(inst, inst_header);
    writeTimingChecks(inst, inst_header);
    if (inst_header)
      writeInstTrailer();
  }
  delete leaf_iter;
}

void
SdfWriter::writeInstHeader(const Instance *inst)
{
  sta::print(stream_, " (CELL\n");
  sta::print(stream_, "  (CELLTYPE \"{}\")\n", network_->cellName(inst));
  std::string inst_name = sdfPathName(inst);
  sta::print(stream_, "  (INSTANCE {})\n", inst_name);
}

void
SdfWriter::writeInstTrailer()
{
  sta::print(stream_, " )\n");
}

void
SdfWriter::writeIopaths(const Instance *inst,
                        bool &inst_header)
{
  bool iopath_header = false;
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *from_pin = pin_iter->next();
    if (network_->isLoad(from_pin)) {
      Vertex *from_vertex = graph_->pinLoadVertex(from_pin);      
      VertexOutEdgeIterator edge_iter(from_vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        const TimingRole *role = edge->role();
        if (role == TimingRole::combinational()
            || role == TimingRole::tristateEnable()
            || role == TimingRole::regClkToQ()
            || role == TimingRole::regSetClr()
            || role == TimingRole::latchEnToQ()
            || role == TimingRole::latchDtoQ()) {
          Vertex *to_vertex = edge->to(graph_);
          Pin *to_pin = to_vertex->pin();
          if (!inst_header) {
            writeInstHeader(inst);
            inst_header = true;
          }
          if (!iopath_header) {
            writeIopathHeader();
            iopath_header = true;
          }
          const std::string &sdf_cond = edge->timingArcSet()->sdfCond();
          if (!sdf_cond.empty()) {
            sta::print(stream_, "    (COND {}\n", sdf_cond);
            sta::print(stream_, " ");
          }
          std::string from_pin_name = sdfPortName(from_pin);
          std::string to_pin_name = sdfPortName(to_pin);
          sta::print(stream_, "    (IOPATH {} {} ",
                     from_pin_name,
                     to_pin_name);
          writeArcDelays(edge);
          if (!sdf_cond.empty())
            sta::print(stream_, ")");
          sta::print(stream_, ")\n");
        }
      }
    }
  }
  delete pin_iter;

  if (iopath_header)
    writeIopathTrailer();
}

void
SdfWriter::writeIopathHeader()
{
  sta::print(stream_, "  (DELAY\n");
  sta::print(stream_, "   (ABSOLUTE\n");
}

void
SdfWriter::writeIopathTrailer()
{
  sta::print(stream_, "   )\n");
  sta::print(stream_, "  )\n");
}

void
SdfWriter::writeArcDelays(Edge *edge)
{
  RiseFallMinMax delays;
  TimingArcSet *arc_set = edge->timingArcSet();
  for (TimingArc *arc : arc_set->arcs()) {
    const RiseFall *rf = arc->toEdge()->asRiseFall();
    const ArcDelay &min_delay = graph_->arcDelay(edge, arc, arc_delay_min_index_);
    delays.setValue(rf, MinMax::min(), delayAsFloat(min_delay, MinMax::min(), this));

    const ArcDelay &max_delay = graph_->arcDelay(edge, arc, arc_delay_max_index_);
    delays.setValue(rf, MinMax::max(), delayAsFloat(max_delay, MinMax::max(), this));
  }

  if (delays.hasValue(RiseFall::rise(), MinMax::min())
      && delays.hasValue(RiseFall::fall(), MinMax::min())) {
    // Rise and fall.
    writeSdfTriple(delays, RiseFall::rise());
    // Merge rise/fall values if they are the same.
    if (!(fuzzyEqual(delays.value(RiseFall::rise(), MinMax::min()),
                     delays.value(RiseFall::fall(), MinMax::min()))
          && fuzzyEqual(delays.value(RiseFall::rise(), MinMax::max()),
                        delays.value(RiseFall::fall(),MinMax::max())))) {
      sta::print(stream_, " ");
      writeSdfTriple(delays, RiseFall::fall());
    }
  }
  else if (delays.hasValue(RiseFall::rise(), MinMax::min()))
    // Rise only.
    writeSdfTriple(delays, RiseFall::rise());
  else if (delays.hasValue(RiseFall::fall(), MinMax::min())) {
    // Fall only.
    sta::print(stream_, "() ");
    writeSdfTriple(delays, RiseFall::fall());
  }
}

void
SdfWriter::writeSdfTriple(RiseFallMinMax &delays,
                          const RiseFall *rf)
{
  float min = delays.value(rf, MinMax::min());
  float max = delays.value(rf, MinMax::max());
  writeSdfTriple(min, max);
}

void
SdfWriter::writeSdfTriple(float min,
                          float max)
{
  sta::print(stream_, "(");
  writeSdfDelay(min);
  if (include_typ_) {
    sta::print(stream_, ":");
    writeSdfDelay((min + max) / 2.0);
    sta::print(stream_, ":");
  }
  else
    sta::print(stream_, "::");
  writeSdfDelay(max);
  sta::print(stream_, ")");
}

void
SdfWriter::writeSdfDelay(double delay)
{
  std::string str = sta::formatRuntime("{:.{}f}", delay / timescale_, digits_);
  sta::print(stream_, "{}", str);
}

void
SdfWriter::writeTimingChecks(const Instance *inst,
                             bool &inst_header)
{
  bool check_header = false;

  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    Pin *pin = pin_iter->next();
    Vertex *vertex = graph_->pinLoadVertex(pin);
    if (vertex) {
      VertexOutEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        const TimingRole *role = edge->role();
        const char *sdf_check = nullptr;
        if (role == TimingRole::setup())
          sdf_check = "SETUP";
        else if (role == TimingRole::hold())
          sdf_check = "HOLD";
        else if (role == TimingRole::recovery())
          sdf_check = "RECOVERY";
        else if (role == TimingRole::removal())
          sdf_check = "REMOVAL";
        if (sdf_check) {
          ensureTimingCheckheaders(check_header, inst, inst_header);
          writeCheck(edge, sdf_check);
        }
      }
      for (auto hi_low : RiseFall::range()) {
        float min_width, max_width;
        Edge *edge;
        TimingArc *arc;
        graph_->minPulseWidthArc(vertex, hi_low, edge, arc);
        if (edge) {
          min_width = delayAsFloat(graph_->arcDelay(edge, arc, arc_delay_min_index_),
                                   MinMax::min(), this);
          max_width = delayAsFloat(graph_->arcDelay(edge, arc, arc_delay_max_index_),
                                   MinMax::max(), this);
          ensureTimingCheckheaders(check_header, inst, inst_header);
          writeWidthCheck(pin, hi_low, min_width, max_width);
        }
      }
      float min_period;
      bool exists;
      graph_delay_calc_->minPeriod(pin, scene_, min_period, exists);
      if (exists) {
        ensureTimingCheckheaders(check_header, inst, inst_header);
        writePeriodCheck(pin, min_period);
      }
    }
  }
  delete pin_iter;

  if (check_header)
    writeTimingCheckTrailer();
}

void
SdfWriter::ensureTimingCheckheaders(bool &check_header,
                                    const Instance *inst,
                                    bool &inst_header)
{
  if (!inst_header) {
    writeInstHeader(inst);
    inst_header = true;
  }
  if (!check_header) {
    writeTimingCheckHeader();
    check_header = true;
  }
}

void
SdfWriter::writeTimingCheckHeader()
{
  sta::print(stream_, "  (TIMINGCHECK\n");
}

void
SdfWriter::writeTimingCheckTrailer()
{
  sta::print(stream_, "  )\n");
}

void
SdfWriter::writeCheck(Edge *edge,
                      std::string_view sdf_check)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  // Examine the arcs to see if the check requires clk or data edge specifiers.
  TimingArc *arcs[RiseFall::index_count][RiseFall::index_count] = 
    {{nullptr, nullptr}, {nullptr, nullptr}};
  for (TimingArc *arc : arc_set->arcs()) {
    const RiseFall *clk_rf = arc->fromEdge()->asRiseFall();
    const RiseFall *data_rf = arc->toEdge()->asRiseFall();;
    arcs[clk_rf->index()][data_rf->index()] = arc;
  }

  if (arcs[RiseFall::fallIndex()][RiseFall::riseIndex()] == nullptr
      && arcs[RiseFall::fallIndex()][RiseFall::fallIndex()] == nullptr)
    writeEdgeCheck(edge, sdf_check, RiseFall::riseIndex(), arcs);
  else if (arcs[RiseFall::riseIndex()][RiseFall::riseIndex()] == nullptr
           && arcs[RiseFall::riseIndex()][RiseFall::fallIndex()] == nullptr)
    writeEdgeCheck(edge, sdf_check, RiseFall::fallIndex(), arcs);
  else {
    // No special case; write all the checks with data and clock edge specifiers.
    for (TimingArc *arc : arc_set->arcs())
      writeCheck(edge, arc, sdf_check, true, true);
  }
}

void
SdfWriter::writeEdgeCheck(Edge *edge,
                          std::string_view sdf_check,
                          int clk_rf_index,
                          TimingArc *arcs[RiseFall::index_count][RiseFall::index_count])
{
  // SDF requires edge specifiers on the data port to define separate
  // rise/fall check values.
  // Check the rise/fall margins to see if they are the same to avoid adding
  // data port edge specifiers if they aren't necessary.
  if (arcs[clk_rf_index][RiseFall::riseIndex()]
      && arcs[clk_rf_index][RiseFall::fallIndex()]
      && arcs[clk_rf_index][RiseFall::riseIndex()]
      && arcs[clk_rf_index][RiseFall::fallIndex()]) {
    float rise_min=delayAsFloat(graph_->arcDelay(edge, 
                                                 arcs[clk_rf_index][RiseFall::riseIndex()],
                                                 arc_delay_min_index_),
                                MinMax::min(), this);
    float fall_min=delayAsFloat(graph_->arcDelay(edge,
                                                 arcs[clk_rf_index][RiseFall::fallIndex()],
                                                 arc_delay_min_index_),
                                MinMax::min(), this);
    float rise_max=delayAsFloat(graph_->arcDelay(edge,
                                                 arcs[clk_rf_index][RiseFall::riseIndex()],
                                                 arc_delay_max_index_),
                                MinMax::max(), this);
    float fall_max=delayAsFloat(graph_->arcDelay(edge,
                                                 arcs[clk_rf_index][RiseFall::fallIndex()],
                                                 arc_delay_max_index_),
                                MinMax::max(), this);
    if (fuzzyEqual(rise_min, fall_min)
        && fuzzyEqual(rise_max, fall_max)) {
      // Rise/fall margins are the same, so no data edge specifier is required.
      writeCheck(edge, arcs[clk_rf_index][RiseFall::riseIndex()],
                 sdf_check, false, true);
      return;
    }
  }
  if (arcs[clk_rf_index][RiseFall::riseIndex()])
    writeCheck(edge, arcs[clk_rf_index][RiseFall::riseIndex()], 
               sdf_check, true, true);
  if (arcs[clk_rf_index][RiseFall::fallIndex()])
    writeCheck(edge, arcs[clk_rf_index][RiseFall::fallIndex()],
               sdf_check, true, true);
}

void
SdfWriter::writeCheck(Edge *edge,
                      TimingArc *arc,
                      std::string_view sdf_check,
                      bool use_data_edge,
                      bool use_clk_edge)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  Pin *from_pin = edge->from(graph_)->pin();
  Pin *to_pin = edge->to(graph_)->pin();
  const std::string &sdf_cond_start = arc_set->sdfCondStart();
  const std::string &sdf_cond_end = arc_set->sdfCondEnd();

  sta::print(stream_, "    ({} ", sdf_check);

  if (!sdf_cond_start.empty())
    sta::print(stream_, "(COND {} ", sdf_cond_start);

  std::string to_pin_name = sdfPortName(to_pin);
  if (use_data_edge) {
    sta::print(stream_, "({} {})",
               sdfEdge(arc->toEdge()),
               to_pin_name);
  }
  else
    sta::print(stream_, "{}", to_pin_name);

  if (!sdf_cond_start.empty())
    sta::print(stream_, ")");

  sta::print(stream_, " ");

  if (!sdf_cond_end.empty())
    sta::print(stream_, "(COND {} ", sdf_cond_end);

  std::string from_pin_name = sdfPortName(from_pin);
  if (use_clk_edge)
    sta::print(stream_, "({} {})",
               sdfEdge(arc->fromEdge()),
               from_pin_name);
  else
    sta::print(stream_, "{}", from_pin_name);

  if (!sdf_cond_end.empty())
    sta::print(stream_, ")");

  sta::print(stream_, " ");

  float min_delay = delayAsFloat(graph_->arcDelay(edge, arc, arc_delay_min_index_),
                                 MinMax::min(), this);
  float max_delay = delayAsFloat(graph_->arcDelay(edge, arc, arc_delay_max_index_),
                                 MinMax::max(), this);
  writeSdfTriple(min_delay, max_delay);

  sta::print(stream_, ")\n");
}

void
SdfWriter::writeWidthCheck(const Pin *pin,
                           const RiseFall *hi_low,
                           float min_width,
                           float max_width)
{
  std::string pin_name = sdfPortName(pin);
  sta::print(stream_, "    (WIDTH ({} {}) ",
             sdfEdge(hi_low->asTransition()),
             pin_name);
  writeSdfTriple(min_width, max_width);
  sta::print(stream_, ")\n");
}

void
SdfWriter::writePeriodCheck(const Pin *pin,
                            float min_period)
{
  std::string pin_name = sdfPortName(pin);
  sta::print(stream_, "    (PERIOD {} ", pin_name);
  writeSdfTriple(min_period, min_period);
  sta::print(stream_, ")\n");
}

std::string_view
SdfWriter::sdfEdge(const Transition *tr)
{
  if (tr == Transition::rise())
    return "posedge";
  else if (tr == Transition::fall())
    return "negedge";
  return {};
}

////////////////////////////////////////////////////////////////

std::string
SdfWriter::sdfPathName(const Pin *pin)
{
  Instance *inst = network_->instance(pin);
  if (network_->isTopInstance(inst))
    return sdfPortName(pin);
  else {
    std::string inst_path = sdfPathName(inst);
    std::string port_name = sdfPortName(pin);
    std::string sdf_name = inst_path;
    sdf_name += sdf_divider_;
    sdf_name += port_name;
    return sdf_name;
  }
}

// Based on Network::pathName.
std::string
SdfWriter::sdfPathName(const Instance *instance)
{
  InstanceSeq inst_path;
  network_->path(instance, inst_path);
  std::string path_name;
  while (!inst_path.empty()) {
    const Instance *inst = inst_path.back();
    path_name += sdfName(inst);
    inst_path.pop_back();
    if (!inst_path.empty())
      path_name += sdf_divider_;
  }
  return path_name;
}

// Escape for non-alpha numeric characters.
std::string
SdfWriter::sdfName(const Instance *inst)
{
  const std::string &name = network_->name(inst);
  std::string sdf_name;
  for (char ch : name) {
    // Ignore sta escapes.
    if (ch != network_escape_) {
      if (!(isalnum(ch) || ch == '_'))
        // Insert escape.
        sdf_name += sdf_escape_;
      sdf_name += ch;
    }
  }
  return sdf_name;
}

std::string
SdfWriter::sdfPortName(const Pin *pin)
{
  const std::string &name = network_->portName(pin);
  size_t name_length = name.size();
  std::string sdf_name;

  constexpr char bus_brkt_left = '[';
  constexpr char bus_brkt_right = ']';
  size_t bus_index = name_length;
  if (name_length >= 4
      && name[name_length - 1] == bus_brkt_right) {
    size_t left = name.rfind(bus_brkt_left);
    if (left != std::string_view::npos)
      bus_index = left;
  }
  for (size_t i = 0; i < name_length; i++) {
    char ch = name[i];
    if (ch == network_escape_) {
      // Copy escape and escaped char.
      sdf_name += sdf_escape_;
      sdf_name += name[++i];
    }
    else {
      if (!(isalnum(ch) || ch == '_')
          && !(i >= bus_index && (ch == bus_brkt_right || ch == bus_brkt_left)))
        // Insert escape.
        sdf_name += sdf_escape_;
      sdf_name += ch;
    }
  }
  return sdf_name;
}

} // namespace sta
