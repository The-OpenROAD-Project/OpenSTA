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

#include "sdf/SdfReader.hh"

#include <cctype>
#include <cstdarg>
#include <string>
#include <utility>

#include "ContainerHelpers.hh"
#include "Debug.hh"
#include "Error.hh"
#include "Graph.hh"
#include "MinMax.hh"
#include "Network.hh"
#include "Report.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "SdcNetwork.hh"
#include "Stats.hh"
#include "TimingArc.hh"
#include "Zlib.hh"
#include "sdf/SdfReaderPvt.hh"
#include "sdf/SdfScanner.hh"

namespace sta {

class SdfTriple
{
public:
  SdfTriple(float *min,
            float *typ,
            float *max);
  ~SdfTriple();
  float **values() { return values_; }
  bool hasValue() const;

private:
  float *values_[3];
};

class SdfPortSpec
{
public:
  SdfPortSpec(const Transition *tr,
              std::string_view port,
              std::string_view cond);
  std::string_view port() const { return port_; }
  const Transition *transition() const { return tr_; }
  std::string_view cond() const { return cond_; }

private:
  const Transition *tr_;
  const std::string port_;
  const std::string cond_;  // timing checks only
};

bool
readSdf(std::string_view filename,
        std::string_view path,
        Scene *scene,
        bool unescaped_dividers,
        bool incremental_only,
        MinMaxAll *cond_use,
        StaState *sta)
{
  int arc_min_index = scene->dcalcAnalysisPtIndex(MinMax::min());
  int arc_max_index = scene->dcalcAnalysisPtIndex(MinMax::max());
  SdfReader reader(filename, path, arc_min_index, arc_max_index,
                   scene->sdc()->analysisType(), unescaped_dividers,
                   incremental_only, cond_use, sta);
  bool success = reader.read();
  return success;
}

SdfReader::SdfReader(std::string_view filename,
                     std::string_view path,
                     int arc_min_index,
                     int arc_max_index,
                     AnalysisType analysis_type,
                     bool unescaped_dividers,
                     bool is_incremental_only,
                     MinMaxAll *cond_use,
                     StaState *sta) :
  StaState(sta),
  filename_(filename),
  path_(path),
  arc_delay_min_index_(arc_min_index),
  arc_delay_max_index_(arc_max_index),
  analysis_type_(analysis_type),
  unescaped_dividers_(unescaped_dividers),
  is_incremental_only_(is_incremental_only),
  cond_use_(cond_use)
{
  if (unescaped_dividers)
    network_ = makeSdcNetwork(network_);
}

SdfReader::~SdfReader()
{
  if (unescaped_dividers_)
    delete network_;
}

bool
SdfReader::read()
{
  gzstream::igzstream stream(std::string(filename_).c_str());
  if (stream.is_open()) {
    Stats stats(debug_, report_);
    SdfScanner scanner(&stream, filename_, this, report_);
    scanner_ = &scanner;
    SdfParse parser(&scanner, this);
    bool success = (parser.parse() == 0);
    stats.report("Read sdf");
    return success;
  }
  else
    throw FileNotReadable(filename_);
}

void
SdfReader::setDivider(char divider)
{
  divider_ = divider;
}

void
SdfReader::setTimescale(float multiplier,
                        std::string_view units)
{
  if (multiplier == 1.0 || multiplier == 10.0 || multiplier == 100.0) {
    if (units == "us")
      timescale_ = multiplier * 1E-6F;
    else if (units == "ns")
      timescale_ = multiplier * 1E-9F;
    else if (units == "ps")
      timescale_ = multiplier * 1E-12F;
    else
      error(180, "TIMESCALE units not us, ns, or ps.");
  }
  else
    error(181, "TIMESCALE multiplier not 1, 10, or 100.");
}

void
SdfReader::interconnect(std::string_view from_pin_name,
                        std::string_view to_pin_name,
                        SdfTripleSeq *triples)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)) {
    Pin *from_pin = findPin(from_pin_name);
    Pin *to_pin = findPin(to_pin_name);
    if (from_pin && to_pin) {
      // Assume the pins are non-hierarchical and on the same net.
      Edge *edge = findWireEdge(from_pin, to_pin);
      if (edge)
        setEdgeDelays(edge, triples, "INTERCONNECT");
      else {
        bool from_is_hier = network_->isHierarchical(from_pin);
        bool to_is_hier = network_->isHierarchical(to_pin);
        if (from_is_hier || to_is_hier) {
          if (from_is_hier)
            error(182, "pin {} is a hierarchical pin.", from_pin_name);
          if (to_is_hier)
            error(183, "pin {} is a hierarchical pin.", to_pin_name);
        }
        else
          warn(184, "INTERCONNECT from {} to {} not found.",
                  from_pin_name, to_pin_name);
      }
    }
    else {
      if (from_pin == nullptr)
        warn(185, "pin {} not found.", from_pin_name);
      if (to_pin == nullptr)
        warn(186, "pin {} not found.", to_pin_name);
    }
  }
  deleteTripleSeq(triples);
}

void
SdfReader::port(std::string_view to_pin_name,
                SdfTripleSeq *triples)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)) {
    Pin *to_pin = (instance_)
        ? network_->findPinRelative(instance_, to_pin_name)
        : network_->findPin(to_pin_name);
    if (to_pin == nullptr)
      warn(187, "pin {} not found.", to_pin_name);
    else {
      Vertex *vertex = graph_->pinLoadVertex(to_pin);
      VertexInEdgeIterator edge_iter(vertex, graph_);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        if (edge->role()->sdfRole()->isWire())
          setEdgeDelays(edge, triples, "PORT");
      }
    }
  }
  deleteTripleSeq(triples);
}

Edge *
SdfReader::findWireEdge(Pin *from_pin,
                        Pin *to_pin)
{
  Vertex *to_vertex, *to_vertex_bidirect_drvr;
  graph_->pinVertices(to_pin, to_vertex, to_vertex_bidirect_drvr);
  if (to_vertex) {
    // Fanin < fanout, so search for driver from load.
    VertexInEdgeIterator edge_iter(to_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      const TimingRole *edge_role = edge->role();
      if (edge->from(graph_)->pin() == from_pin && edge_role->sdfRole()->isWire())
        return edge;
    }
  }
  return nullptr;
}

void
SdfReader::setEdgeDelays(Edge *edge,
                         SdfTripleSeq *triples,
                         std::string_view sdf_cmd)
{
  // Rise/fall triples.
  size_t triple_count = triples->size();
  if (triple_count == 1 || triple_count == 2) {
    TimingArcSet *arc_set = edge->timingArcSet();
    for (TimingArc *arc : arc_set->arcs()) {
      size_t triple_index;
      if (triple_count == 1)
        triple_index = 0;
      else
        triple_index = arc->toEdge()->sdfTripleIndex();
      SdfTriple *triple = (*triples)[triple_index];
      setEdgeArcDelays(edge, arc, triple);
    }
  }
  else if (triple_count == 0)
    error(188, "{} with no triples.", sdf_cmd);
  else
    error(189, "{} with more than 2 triples.", sdf_cmd);
}

void
SdfReader::setCell(std::string_view cell_name)
{
  cell_name_ = cell_name;
}

void
SdfReader::setInstance()
{
  instance_ = nullptr;
}

void
SdfReader::setInstance(std::string_view instance_name)
{
  if (instance_name == "*") {
    warn(193, "INSTANCE wildcards not supported.");
    instance_ = nullptr;
  }
  else {
    instance_ = findInstance(instance_name);
    if (instance_) {
      Cell *inst_cell = network_->cell(instance_);
      std::string inst_cell_name(network_->name(inst_cell));
      if (inst_cell_name != cell_name_)
        warn(190, "instance {} cell {} does not match enclosing cell {}.",
             instance_name,
             inst_cell_name,
             cell_name_);
    }
  }
}

void
SdfReader::setInstanceWildcard()
{
  warn(172, "INSTANCE wildcards not supported.");
  instance_ = nullptr;
}

void
SdfReader::cellFinish()
{
  cell_name_.clear();
  instance_ = nullptr;
}

void
SdfReader::iopath(SdfPortSpec *from_edge,
                  std::string_view to_port_name,
                  SdfTripleSeq *triples,
                  std::string_view cond,
                  bool condelse)
{
  if (instance_) {
    std::string_view from_port_name = from_edge->port();
    Cell *cell = network_->cell(instance_);
    Port *from_port = findPort(cell, from_port_name);
    Port *to_port = findPort(cell, to_port_name);
    if (from_port && to_port) {
      Pin *from_pin = network_->findPin(instance_, from_port);
      Pin *to_pin = network_->findPin(instance_, to_port);
      // Do not report an error if the pin is not found because the
      // instance may not have the pin.
      if (from_pin && to_pin) {
        Vertex *to_vertex = graph_->pinDrvrVertex(to_pin);
        if (to_vertex) {
          size_t triple_count = triples->size();
          bool matched = false;
          // Fanin < fanout, so search for driver from load.
          // Search for multiple matching edges because of
          // tristate enable/disable.
          VertexInEdgeIterator edge_iter(to_vertex, graph_);
          while (edge_iter.hasNext()) {
            Edge *edge = edge_iter.next();
            TimingArcSet *arc_set = edge->timingArcSet();
            const std::string &lib_cond = arc_set->sdfCond();
            const TimingRole *edge_role = arc_set->role();
            bool cond_use_flag = cond_use_ && !cond.empty() && lib_cond.empty()
              && !(!is_incremental_only_ && in_incremental_);
            if (edge->from(graph_)->pin() == from_pin
                && edge_role->sdfRole() == TimingRole::sdfIopath()
                && (cond_use_flag
                    || (!condelse && condMatch(cond, lib_cond))
                    // condelse matches the default (unconditional) arc.
                    || (condelse && lib_cond.empty()))) {
              matched = true;
              for (TimingArc *arc : arc_set->arcs()) {
                if ((from_edge->transition() == Transition::riseFall())
                    || (arc->fromEdge() == from_edge->transition())) {
                  size_t triple_index = arc->toEdge()->sdfTripleIndex();
                  SdfTriple *triple = nullptr;
                  if (triple_index < triple_count)
                    triple = (*triples)[triple_index];
                  if (triple_count == 1)
                    triple = (*triples)[0];
                  // Rules for matching when triple is missing not implemented.
                  // See SDF pg 3-17.
                  if (triple) {
                    if (cond_use_flag)
                      setEdgeArcDelaysCondUse(edge, arc, triple);
                    else
                      setEdgeArcDelays(edge, arc, triple);
                  }
                }
              }
            }
          }
          if (!matched)
            warn(191, "cell {} IOPATH {} -> {} not found.",
                 network_->cellName(instance_),
                 from_port_name,
                 to_port_name);
        }
      }
    }
  }
  delete from_edge;
  deleteTripleSeq(triples);
}

Port *
SdfReader::findPort(const Cell *cell,
                    std::string_view port_name)
{
  Port *port = network_->findPort(cell, port_name);
  if (port == nullptr)
    warn(194, "instance {} port {} not found.", network_->pathName(instance_),
            port_name);
  return port;
}

void
SdfReader::timingCheck(const TimingRole *role,
                       SdfPortSpec *data_edge,
                       SdfPortSpec *clk_edge,
                       SdfTriple *triple)
{
  if (instance_) {
    std::string_view data_port_name = data_edge->port();
    std::string_view clk_port_name = clk_edge->port();
    Cell *cell = network_->cell(instance_);
    Port *data_port = findPort(cell, data_port_name);
    Port *clk_port = findPort(cell, clk_port_name);
    if (data_port && clk_port)
      timingCheck1(role, data_port, data_edge, clk_port, clk_edge, triple);
  }
  delete data_edge;
  delete clk_edge;
  deleteTriple(triple);
}

void
SdfReader::timingCheck1(const TimingRole *role,
                        Port *data_port,
                        SdfPortSpec *data_edge,
                        Port *clk_port,
                        SdfPortSpec *clk_edge,
                        SdfTriple *triple)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_) && instance_) {
    Pin *data_pin = network_->findPin(instance_, data_port);
    Pin *clk_pin = network_->findPin(instance_, clk_port);
    if (data_pin && clk_pin) {
      // Hack: always use triple max value for check.
      float **values = triple->values();
      float *value_min = values[triple_min_index_];
      float *value_max = values[triple_max_index_];
      if (value_min && value_max) {
        switch (analysis_type_) {
          case AnalysisType::single:
            break;
          case AnalysisType::bc_wc:
            if (role->genericRole() == TimingRole::setup())
              *value_min = *value_max;
            else
              *value_max = *value_min;
            break;
          case AnalysisType::ocv:
            *value_min = *value_max;
            break;
        }
      }
      bool matched = annotateCheckEdges(data_pin, data_edge, clk_pin, clk_edge, role,
                                        triple, false);
      // Liberty setup/hold checks on preset/clear pins can be translated
      // into recovery/removal checks, so be flexible about matching.
      if (!matched)
        matched = annotateCheckEdges(data_pin, data_edge, clk_pin, clk_edge, role,
                                     triple, true);
      if (!matched
          // Only warn when non-null values are present.
          && triple->hasValue())
        warn(192, "cell {} {} -> {} {} check not found.",
                network_->cellName(instance_), network_->name(data_port),
                network_->name(clk_port), role->to_string());
    }
  }
}

// Return true if matched.
bool
SdfReader::annotateCheckEdges(Pin *data_pin,
                              SdfPortSpec *data_edge,
                              Pin *clk_pin,
                              SdfPortSpec *clk_edge,
                              const TimingRole *sdf_role,
                              SdfTriple *triple,
                              bool match_generic)
{
  bool matched = false;
  std::string_view cond_start = data_edge->cond();
  std::string_view cond_end = clk_edge->cond();
  // Timing check graph edges from clk to data.
  Vertex *to_vertex = graph_->pinLoadVertex(data_pin);
  // Fanin < fanout, so search for driver from load.
  VertexInEdgeIterator edge_iter(to_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->from(graph_)->pin() == clk_pin) {
      TimingArcSet *arc_set = edge->timingArcSet();
      const TimingRole *edge_role = arc_set->role();
      std::string_view lib_cond_start = arc_set->sdfCondStart();
      std::string_view lib_cond_end = arc_set->sdfCondEnd();
      bool cond_matches =
        condMatch(cond_start, lib_cond_start) && condMatch(cond_end, lib_cond_end);
      if (((!match_generic && edge_role->sdfRole() == sdf_role)
           || (match_generic && edge_role->genericRole() == sdf_role->genericRole()))
          && cond_matches) {
        TimingArcSet *arc_set = edge->timingArcSet();
        for (TimingArc *arc : arc_set->arcs()) {
          if (((data_edge->transition() == Transition::riseFall())
               || (arc->toEdge() == data_edge->transition()))
              && ((clk_edge->transition() == Transition::riseFall())
                  || (arc->fromEdge() == clk_edge->transition()))) {
            setEdgeArcDelays(edge, arc, triple);
          }
        }
        matched = true;
      }
    }
  }
  return matched;
}

void
SdfReader::timingCheckWidth(SdfPortSpec *edge,
                            SdfTriple *triple)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_) && instance_) {
    std::string_view port_name = edge->port();
    Cell *cell = network_->cell(instance_);
    Port *port = findPort(cell, port_name);
    if (port) {
      Pin *pin = network_->findPin(instance_, port_name);
      if (pin) {
        const RiseFall *rf = edge->transition()->asRiseFall();
        Edge *edge;
        TimingArc *arc;
        graph_->minPulseWidthArc(graph_->pinLoadVertex(pin), rf, edge, arc);
        if (edge)
          setEdgeArcDelays(edge, arc, triple);
      }
    }
  }
  delete edge;
  deleteTriple(triple);
}

void
SdfReader::timingCheckSetupHold(SdfPortSpec *data_edge,
                                SdfPortSpec *clk_edge,
                                SdfTriple *setup_triple,
                                SdfTriple *hold_triple)
{
  timingCheckSetupHold1(data_edge, clk_edge, setup_triple, hold_triple,
                        TimingRole::setup(), TimingRole::hold());
}

void
SdfReader::timingCheckRecRem(SdfPortSpec *data_edge,
                             SdfPortSpec *clk_edge,
                             SdfTriple *rec_triple,
                             SdfTriple *rem_triple)
{
  timingCheckSetupHold1(data_edge, clk_edge, rec_triple, rem_triple,
                        TimingRole::recovery(), TimingRole::removal());
}

void
SdfReader::timingCheckSetupHold1(SdfPortSpec *data_edge,
                                 SdfPortSpec *clk_edge,
                                 SdfTriple *setup_triple,
                                 SdfTriple *hold_triple,
                                 const TimingRole *setup_role,
                                 const TimingRole *hold_role)
{
  std::string_view data_port_name = data_edge->port();
  std::string_view clk_port_name = clk_edge->port();
  Cell *cell = network_->cell(instance_);
  Port *data_port = findPort(cell, data_port_name);
  Port *clk_port = findPort(cell, clk_port_name);
  if (data_port && clk_port) {
    timingCheck1(setup_role, data_port, data_edge, clk_port, clk_edge, setup_triple);
    timingCheck1(hold_role, data_port, data_edge, clk_port, clk_edge, hold_triple);
  }
  delete data_edge;
  delete clk_edge;
  deleteTriple(setup_triple);
  deleteTriple(hold_triple);
}

void
SdfReader::timingCheckPeriod(SdfPortSpec *edge,
                             SdfTriple *triple)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_) && instance_) {
    std::string_view port_name = edge->port();
    Cell *cell = network_->cell(instance_);
    Port *port = findPort(cell, port_name);
    if (port) {
      // Edge specifier is ignored for period checks.
      Pin *pin = network_->findPin(instance_, port_name);
      if (pin) {
        float **values = triple->values();
        float *value_ptr = values[triple_min_index_];
        if (value_ptr) {
          float value = *value_ptr;
          graph_->setPeriodCheckAnnotation(pin, arc_delay_min_index_, value);
        }
        if (triple_max_index_ != null_index_) {
          value_ptr = values[triple_max_index_];
          if (value_ptr) {
            float value = *value_ptr;
            graph_->setPeriodCheckAnnotation(pin, arc_delay_max_index_, value);
          }
        }
      }
    }
  }
  delete edge;
  deleteTriple(triple);
}

void
SdfReader::timingCheckNochange(SdfPortSpec *data_edge,
                               SdfPortSpec *clk_edge,
                               SdfTriple *before_triple,
                               SdfTriple *after_triple)
{
  warn(173, "NOCHANGE not supported.");
  delete data_edge;
  delete clk_edge;
  deleteTriple(before_triple);
  deleteTriple(after_triple);
}

void
SdfReader::device(SdfTripleSeq *triples)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_) && instance_) {
    InstancePinIterator *pin_iter = network_->pinIterator(instance_);
    while (pin_iter->hasNext()) {
      Pin *to_pin = pin_iter->next();
      setDevicePinDelays(to_pin, triples);
    }
    delete pin_iter;
  }
  deleteTripleSeq(triples);
}

void
SdfReader::device(std::string_view to_port_name,
                  SdfTripleSeq *triples)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_) && instance_) {
    Cell *cell = network_->cell(instance_);
    Port *to_port = findPort(cell, to_port_name);
    if (to_port) {
      Pin *to_pin = network_->findPin(instance_, to_port_name);
      setDevicePinDelays(to_pin, triples);
    }
  }
  deleteTripleSeq(triples);
}

void
SdfReader::setDevicePinDelays(Pin *to_pin,
                              SdfTripleSeq *triples)
{
  Vertex *vertex = graph_->pinDrvrVertex(to_pin);
  if (vertex) {
    VertexInEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->role()->sdfRole() == TimingRole::sdfIopath())
        setEdgeDelays(edge, triples, "DEVICE");
    }
  }
}

void
SdfReader::setEdgeArcDelays(Edge *edge,
                            TimingArc *arc,
                            SdfTriple *triple)
{
  setEdgeArcDelays(edge, arc, triple, triple_min_index_, arc_delay_min_index_);
  setEdgeArcDelays(edge, arc, triple, triple_max_index_, arc_delay_max_index_);
}

void
SdfReader::setEdgeArcDelays(Edge *edge,
                            TimingArc *arc,
                            SdfTriple *triple,
                            int triple_index,
                            int arc_delay_index)
{
  if (triple_index != null_index_) {
    float **values = triple->values();
    float *value_ptr = values[triple_index];
    if (value_ptr) {
      ArcDelay delay;
      if (in_incremental_)
        delay = delaySum(graph_->arcDelay(edge, arc, arc_delay_index), *value_ptr, this);
      else
        delay = *value_ptr;
      graph_->setArcDelay(edge, arc, arc_delay_index, delay);
      graph_->setArcDelayAnnotated(edge, arc, arc_delay_index, true);
      edge->setDelayAnnotationIsIncremental(is_incremental_only_);
    }
  }
}

void
SdfReader::setEdgeArcDelaysCondUse(Edge *edge,
                                   TimingArc *arc,
                                   SdfTriple *triple)
{
  float **values = triple->values();
  float *value_min = values[triple_min_index_];
  float *value_max = values[triple_max_index_];
  const MinMax *min, *max;
  if (cond_use_ == MinMaxAll::min()) {
    min = MinMax::min();
    max = MinMax::min();
  }
  else if (cond_use_ == MinMaxAll::max()) {
    min = MinMax::max();
    max = MinMax::max();
  }
  else {
    min = MinMax::min();
    max = MinMax::max();
  }
  setEdgeArcDelaysCondUse(edge, arc, value_min, triple_min_index_,
                          arc_delay_min_index_, min);
  setEdgeArcDelaysCondUse(edge, arc, value_max, triple_max_index_,
                          arc_delay_max_index_, max);
}

void
SdfReader::setEdgeArcDelaysCondUse(Edge *edge,
                                   TimingArc *arc,
                                   const float *value,
                                   int triple_index,
                                   int arc_delay_index,
                                   const MinMax *min_max)
{
  if (value && triple_index != null_index_) {
    ArcDelay delay(*value);
    if (!is_incremental_only_ && in_incremental_)
      delay = delaySum(graph_->arcDelay(edge, arc, arc_delay_index), *value, this);
    else if (graph_->arcDelayAnnotated(edge, arc, arc_delay_index)) {
      ArcDelay prev_value = graph_->arcDelay(edge, arc, arc_delay_index);
      if (delayGreater(prev_value, delay, min_max, this))
        delay = prev_value;
    }
    graph_->setArcDelay(edge, arc, arc_delay_index, delay);
    graph_->setArcDelayAnnotated(edge, arc, arc_delay_index, true);
    edge->setDelayAnnotationIsIncremental(is_incremental_only_);
  }
}

bool
SdfReader::condMatch(std::string_view sdf_cond,
                     std::string_view lib_cond)
{
  // If the sdf is not conditional it matches any library condition.
  if (sdf_cond.empty())
    return true;
  else if (!sdf_cond.empty() && !lib_cond.empty()) {
    // Match sdf_cond and lib_cond ignoring blanks.
    size_t c1 = 0;
    size_t c2 = 0;
    while (c1 < sdf_cond.size()
           && c2 < lib_cond.size()) {
      char ch1 = sdf_cond[c1++];
      char ch2 = lib_cond[c2++];
      while (c1 < sdf_cond.size()
             && isspace(ch1))
        ch1 = sdf_cond[c1++];
      while (c2 < lib_cond.size()
             && isspace(ch2))
        ch2 = lib_cond[c2++];
      if (ch1 != ch2)
        return false;
    }
    return c1 == sdf_cond.size()
      && c2 == lib_cond.size();
  }
  else
    return false;
}

SdfPortSpec *
SdfReader::makePortSpec(const Transition *tr,
                        std::string_view port)
{
  return new SdfPortSpec(tr, port, "");
}

SdfPortSpec *
SdfReader::makePortSpec(const Transition *tr,
                        std::string_view port,
                        std::string_view cond)
{
  return new SdfPortSpec(tr, port, cond);
}

SdfPortSpec *
SdfReader::makeCondPortSpec(std::string_view cond_port)
{
  // Search from end to find port name because condition may contain spaces.
  std::string cond_port1(cond_port);
  trimRight(cond_port1);
  auto port_idx = cond_port1.find_last_of(' ');
  if (port_idx != std::string::npos) {
    std::string port1 = cond_port1.substr(port_idx + 1);
    size_t cond_end = cond_port1.find_last_not_of(' ', port_idx);
    if (cond_end != std::string::npos) {
      std::string cond1 = cond_port1.substr(0, cond_end + 1);
      return new SdfPortSpec(Transition::riseFall(), port1, cond1);
    }
  }
  return nullptr;
}

SdfTripleSeq *
SdfReader::makeTripleSeq()
{
  return new SdfTripleSeq;
}

void
SdfReader::deleteTripleSeq(SdfTripleSeq *triples)
{
  deleteContents(triples);
  delete triples;
}

SdfTriple *
SdfReader::makeTriple()
{
  return new SdfTriple(nullptr, nullptr, nullptr);
}

SdfTriple *
SdfReader::makeTriple(float value)
{
  value *= timescale_;
  float *fp = new float(value);
  return new SdfTriple(fp, fp, fp);
}

SdfTriple *
SdfReader::makeTriple(float *min,
                      float *typ,
                      float *max)
{
  if (min)
    *min *= timescale_;
  if (typ)
    *typ *= timescale_;
  if (max)
    *max *= timescale_;
  return new SdfTriple(min, typ, max);
}

void
SdfReader::deleteTriple(SdfTriple *triple)
{
  delete triple;
}

void
SdfReader::setInTimingCheck(bool in)
{
  in_timing_check_ = in;
}

void
SdfReader::setInIncremental(bool incr)
{
  in_incremental_ = incr;
}

std::string
SdfReader::unescaped(std::string_view token)
{
  char path_escape = network_->pathEscape();
  char path_divider = network_->pathDivider();
  size_t token_length = token.size();
  std::string result;
  for (size_t i = 0; i < token_length; i++) {
    char ch = token[i];
    if (ch == escape_) {
      char next_ch = token[i + 1];
      if (next_ch == divider_) {
        // Escaped divider.
        // Translate sdf escape to network escape.
        result += path_escape;
        // Translate sdf divider to network divider.
        result += path_divider;
      }
      else if (next_ch == '[' || next_ch == ']' || next_ch == escape_) {
        // Escaped bus bracket or escape.
        // Translate sdf escape to network escape.
        result += path_escape;
        result += next_ch;
      }
      else
        // Escaped non-divider character.
        result += next_ch;
      i++;
    }
    else
      // Just the normal noises.
      result += ch;
  }
  debugPrint(debug_, "sdf_name", 1, "unescape {} -> {}", token,
             result);
  return result;
}

std::string
SdfReader::makePath(std::string_view head,
                    std::string_view tail)
{
  std::string path(head);
  path += network_->pathDivider();
  path += tail;
  return path;
}

std::string
SdfReader::makeBusName(std::string_view base_name,
                       int index)
{
  std::string bus_name = unescaped(base_name);
  bus_name += '[';
  bus_name += std::to_string(index);
  bus_name += ']';
  return bus_name;
}

int
SdfReader::sdfLine() const
{
  return scanner_->lineno();
}

Pin *
SdfReader::findPin(std::string_view name)
{
  if (!path_.empty()) {
    std::string path_name(path_);
    path_name += divider_;
    path_name += name;
    Pin *pin = network_->findPin(path_name);
    return pin;
  }
  else
    return network_->findPin(name);
}

Instance *
SdfReader::findInstance(std::string_view name)
{
  std::string inst_name;
  if (!path_.empty()) {
    inst_name = path_;
    inst_name += divider_;
    inst_name += name;
  }
  else
    inst_name = name;
  Instance *inst = network_->findInstance(inst_name);
  if (inst == nullptr)
    warn(195, "instance {} not found.", inst_name);
  return inst;
}

////////////////////////////////////////////////////////////////

SdfPortSpec::SdfPortSpec(const Transition *tr,
                         std::string_view port,
                         std::string_view cond) :
  tr_(tr),
  port_(port),
  cond_(cond)
{
}

////////////////////////////////////////////////////////////////

SdfTriple::SdfTriple(float *min,
                     float *typ,
                     float *max)
{
  values_[0] = min;
  values_[1] = typ;
  values_[2] = max;
}

SdfTriple::~SdfTriple()
{
  if (values_[0] == values_[1] && values_[0] == values_[2])
    delete values_[0];
  else {
    if (values_[0])
      delete values_[0];
    if (values_[1])
      delete values_[1];
    if (values_[2])
      delete values_[2];
  }
}

bool
SdfTriple::hasValue() const
{
  return values_[0] || values_[1] || values_[2];
}

////////////////////////////////////////////////////////////////

SdfScanner::SdfScanner(std::istream *stream,
                       std::string_view filename,
                       SdfReader *reader,
                       Report *report) :
  yyFlexLexer(stream),
  filename_(filename),
  reader_(reader),
  report_(report)
{
}

void
SdfScanner::error(std::string_view msg)
{
  report_->fileError(196, filename_, lineno(), "{}", msg);
}

}  // namespace sta
