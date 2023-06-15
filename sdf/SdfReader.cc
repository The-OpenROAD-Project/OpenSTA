// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include "sdf/SdfReader.hh"

#include <cstdarg>
#include <cctype>

#include "Error.hh"
#include "Debug.hh"
#include "Report.hh"
#include "MinMax.hh"
#include "TimingArc.hh"
#include "Network.hh"
#include "SdcNetwork.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "Sdc.hh"
#include "sdf/SdfReaderPvt.hh"

extern int
SdfParse_parse();
extern int SdfParse_debug;

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
  SdfPortSpec(Transition *tr,
	      const char *port,
	      const char *cond);
  ~SdfPortSpec();
  const char *port() const { return port_; }
  Transition *transition() const { return tr_; }
  const char *cond() const { return cond_; }

private:
  Transition *tr_;
  const char *port_;
  const char *cond_;   // timing checks only
};

SdfReader *sdf_reader = nullptr;

bool
readSdf(const char *filename,
        const char *path,
        Corner *corner,
        bool unescaped_dividers,
        bool incremental_only,
        MinMaxAll *cond_use,
        StaState *sta)
{
  int arc_min_index = corner->findDcalcAnalysisPt(MinMax::min())->index();
  int arc_max_index = corner->findDcalcAnalysisPt(MinMax::max())->index();
  SdfReader reader(filename, path,
		   arc_min_index, arc_max_index, 
		   sta->sdc()->analysisType(),
                   unescaped_dividers, incremental_only,
		   cond_use, sta);
  sdf_reader = &reader;
  bool success = reader.read();
  sdf_reader = nullptr;
  return success;
}

SdfReader::SdfReader(const char *filename,
		     const char *path,
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
  triple_min_index_(0),
  triple_max_index_(2),
  arc_delay_min_index_(arc_min_index),
  arc_delay_max_index_(arc_max_index),
  analysis_type_(analysis_type),
  unescaped_dividers_(unescaped_dividers),
  is_incremental_only_(is_incremental_only),
  cond_use_(cond_use),
  line_(1),
  divider_('/'),
  escape_('\\'),
  instance_(nullptr),
  cell_name_(nullptr),
  in_timing_check_(false),
  in_incremental_(false),
  timescale_(1.0E-9F)		// default units of ns
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
  // Use zlib to uncompress gzip'd files automagically.
  stream_ = gzopen(filename_, "rb");
  //::SdfParse_debug = 1;
  if (stream_) {
    // yyparse returns 0 on success.
    bool success = (::SdfParse_parse() == 0);
    gzclose(stream_);
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
			const char *units)
{
  if (multiplier == 1.0
      || multiplier == 10.0
      || multiplier == 100.0) {
    if (stringEq(units, "us"))
      timescale_ = multiplier * 1E-6F;
    else if (stringEq(units, "ns"))
      timescale_ = multiplier * 1E-9F;
    else if (stringEq(units, "ps"))
      timescale_ = multiplier * 1E-12F;
    else
      sdfError(180, "TIMESCALE units not us, ns, or ps.");
  }
  else
    sdfError(181, "TIMESCALE multiplier not 1, 10, or 100.");
  stringDelete(units);
}

void
SdfReader::interconnect(const char *from_pin_name,
			const char *to_pin_name,
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
	    sdfError(182, "pin %s is a hierarchical pin.", from_pin_name);
	  if (to_is_hier)
	    sdfError(183, "pin %s is a hierarchical pin.", to_pin_name);
	}
	else
	  sdfWarn(184, "INTERCONNECT from %s to %s not found.",
                  from_pin_name, to_pin_name);
      }
    }
    else {
      if (from_pin == nullptr)
	sdfWarn(185, "pin %s not found.", from_pin_name);
      if (to_pin == nullptr)
	sdfWarn(186, "pin %s not found.", to_pin_name);
    }
  }
  stringDelete(from_pin_name);
  stringDelete(to_pin_name);
  deleteTripleSeq(triples);
}

void
SdfReader::port(const char *to_pin_name,
		SdfTripleSeq *triples)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)) {
    Pin *to_pin = (instance_)
      ? network_->findPinRelative(instance_, to_pin_name)
      : network_->findPin(to_pin_name);
    if (to_pin == nullptr)
      sdfWarn(187, "pin %s not found.", to_pin_name);
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
  stringDelete(to_pin_name);
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
      if (edge->from(graph_)->pin() == from_pin
          && edge_role->sdfRole()->isWire())
        return edge;
    }
  }
  return nullptr;
}

void
SdfReader::setEdgeDelays(Edge *edge,
			 SdfTripleSeq *triples,
			 const char *sdf_cmd)
{
  // Rise/fall triples.
  size_t triple_count = triples->size();
  if (triple_count == 1
      || triple_count == 2) {
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
    sdfError(188, "%s with no triples.", sdf_cmd);
  else
    sdfError(189, "%s with more than 2 triples.", sdf_cmd);
}

void
SdfReader::setCell(const char *cell_name)
{
  cell_name_ = cell_name;
}

void
SdfReader::setInstance(const char *instance_name)
{
  if (instance_name) {
    if (stringEq(instance_name, "*")) {
      notSupported("INSTANCE wildcards");
      instance_ = nullptr;
    }
    else {
      instance_ = findInstance(instance_name);
      if (instance_) {
	Cell *inst_cell = network_->cell(instance_);
	const char *inst_cell_name = network_->name(inst_cell);
	if (cell_name_ && !stringEqual(inst_cell_name, cell_name_))
	  sdfWarn(190, "instance %s cell %s does not match enclosing cell %s.",
                  instance_name,
                  inst_cell_name,
                  cell_name_);
      }
    }
  }
  else
    instance_ = nullptr;
  stringDelete(instance_name);
}

void
SdfReader::setInstanceWildcard()
{
  notSupported("INSTANCE wildcards");
  instance_ = nullptr;
}

void
SdfReader::cellFinish()
{
  stringDelete(cell_name_);
  cell_name_ = nullptr;
  instance_ = nullptr;
}

void
SdfReader::iopath(SdfPortSpec *from_edge,
		  const char *to_port_name,
		  SdfTripleSeq *triples,
		  const char *cond,
		  bool condelse)
{
  if (instance_) {
    const char *from_port_name = from_edge->port();
    Cell *cell = network_->cell(instance_);
    Port *from_port = findPort(cell, from_port_name);
    Port *to_port = findPort(cell, to_port_name);
    if (from_port && to_port) {
      Pin *from_pin = network_->findPin(instance_, from_port_name);
      Pin *to_pin = network_->findPin(instance_, to_port_name);
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
            const char *lib_cond = arc_set->sdfCond();
            const TimingRole *edge_role = arc_set->role();
            bool cond_use_flag = cond_use_ && cond && lib_cond == nullptr
              && !(!is_incremental_only_ && in_incremental_);
            if (edge->from(graph_)->pin() == from_pin
                && edge_role->sdfRole() == TimingRole::sdfIopath()
                && (cond_use_flag
                    || (!condelse && condMatch(cond, lib_cond))
                    // condelse matches the default (unconditional) arc.
                    || (condelse && lib_cond == nullptr))) {
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
            sdfWarn(191, "cell %s IOPATH %s -> %s not found.",
                    network_->cellName(instance_),
                    from_port_name,
                    to_port_name);
        }
      }
    }
  }
  stringDelete(to_port_name);
  deletePortSpec(from_edge);
  deleteTripleSeq(triples);
  stringDelete(cond);
}

Port *
SdfReader::findPort(const Cell *cell,
                    const char *port_name)
{
  Port *port = network_->findPort(cell, port_name);
  if (port == nullptr)
    sdfWarn(194, "instance %s port %s not found.",
            network_->pathName(instance_),
            port_name);
  return port;
}

void
SdfReader::timingCheck(TimingRole *role, SdfPortSpec *data_edge,
		       SdfPortSpec *clk_edge, SdfTriple *triple)
{
  const char *data_port_name = data_edge->port();
  const char *clk_port_name = clk_edge->port();
  Cell *cell = network_->cell(instance_);
  Port *data_port = findPort(cell, data_port_name);
  Port *clk_port = findPort(cell, clk_port_name);
  if (data_port && clk_port)
    timingCheck1(role, data_port, data_edge, clk_port, clk_edge, triple);
  deletePortSpec(data_edge);
  deletePortSpec(clk_edge);
  deleteTriple(triple);
}

void
SdfReader::timingCheck1(TimingRole *role,
                        Port *data_port,
			SdfPortSpec *data_edge,
                        Port *clk_port,
			SdfPortSpec *clk_edge,
			SdfTriple *triple)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)
      && instance_) {
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
      bool matched = annotateCheckEdges(data_pin, data_edge,
                                        clk_pin, clk_edge, role,
                                        triple, false);
      // Liberty setup/hold checks on preset/clear pins can be translated
      // into recovery/removal checks, so be flexible about matching.
      if (!matched)
        matched = annotateCheckEdges(data_pin, data_edge,
                                     clk_pin, clk_edge, role,
                                     triple, true);
      if (!matched
          // Only warn when non-null values are present.
          && triple->hasValue())
        sdfWarn(192, "cell %s %s -> %s %s check not found.",
                network_->cellName(instance_),
                network_->name(data_port),
                network_->name(clk_port),
                role->asString());
    }
  }
}

// Return true if matched.
bool
SdfReader::annotateCheckEdges(Pin *data_pin,
			      SdfPortSpec *data_edge,
			      Pin *clk_pin,
			      SdfPortSpec *clk_edge,
			      TimingRole *sdf_role,
			      SdfTriple *triple,
			      bool match_generic)
{
  bool matched = false;
  const char *cond_start = data_edge->cond();
  const char *cond_end = clk_edge->cond();
  // Timing check graph edges from clk to data.
  Vertex *to_vertex = graph_->pinLoadVertex(data_pin);
  // Fanin < fanout, so search for driver from load.
  VertexInEdgeIterator edge_iter(to_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->from(graph_)->pin() == clk_pin) {
      TimingArcSet *arc_set = edge->timingArcSet();
      const TimingRole *edge_role = arc_set->role();
      const char *lib_cond_start = arc_set->sdfCondStart();
      const char *lib_cond_end = arc_set->sdfCondEnd();
      bool cond_matches = condMatch(cond_start, lib_cond_start)
	&& condMatch(cond_end, lib_cond_end);
      if (((!match_generic && edge_role->sdfRole() == sdf_role)
	   || (match_generic
	       && edge_role->genericRole() == sdf_role->genericRole()))
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
  if (!(is_incremental_only_ && !in_incremental_)
      && instance_) {
    const char *port_name = edge->port();
    Cell *cell = network_->cell(instance_);
    Port *port = findPort(cell, port_name);
    if (port) {
      Pin *pin = network_->findPin(instance_, port_name);
      if (pin) {
	const RiseFall *rf = edge->transition()->asRiseFall();
	float **values = triple->values();
	float *value_ptr = values[triple_min_index_];
	if (value_ptr) {
	  float value = *value_ptr;
	  graph_->setWidthCheckAnnotation(pin, rf, arc_delay_min_index_,
					  value);
	}
	if (triple_max_index_ != null_index_) {
	  value_ptr = values[triple_max_index_];
	  if (value_ptr) {
	    float value = *value_ptr;
	    graph_->setWidthCheckAnnotation(pin, rf, arc_delay_max_index_,
					    value);
	  }
	}
      }
    }
  }
  deletePortSpec(edge);
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
                                 TimingRole *setup_role,
                                 TimingRole *hold_role)
{
  const char *data_port_name = data_edge->port();
  const char *clk_port_name = clk_edge->port();
  Cell *cell = network_->cell(instance_);
  Port *data_port = findPort(cell, data_port_name);
  Port *clk_port = findPort(cell, clk_port_name);
  if (data_port && clk_port) {
    timingCheck1(setup_role, data_port, data_edge, clk_port, clk_edge, setup_triple);
    timingCheck1(hold_role, data_port, data_edge, clk_port, clk_edge, hold_triple);
  }
  deletePortSpec(data_edge);
  deletePortSpec(clk_edge);
  deleteTriple(setup_triple);
  deleteTriple(hold_triple);
}

void
SdfReader::timingCheckPeriod(SdfPortSpec *edge,
			     SdfTriple *triple)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)
      && instance_) {
    const char *port_name = edge->port();
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
  deletePortSpec(edge);
  deleteTriple(triple);
}

void
SdfReader::timingCheckNochange(SdfPortSpec *data_edge,
			       SdfPortSpec *clk_edge,
			       SdfTriple *before_triple,
			       SdfTriple *after_triple)
{
  notSupported("NOCHANGE");
  deletePortSpec(data_edge);
  deletePortSpec(clk_edge);
  deleteTriple(before_triple);
  deleteTriple(after_triple);
}

void
SdfReader::device(SdfTripleSeq *triples)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)
      && instance_) {
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
SdfReader::device(const char *to_port_name,
		  SdfTripleSeq *triples)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)
      && instance_) {
    Cell *cell = network_->cell(instance_);
    Port *to_port = findPort(cell, to_port_name);
    if (to_port) {
      Pin *to_pin = network_->findPin(instance_, to_port_name);
      setDevicePinDelays(to_pin, triples);
    }
  }
  stringDelete(to_port_name);
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
	delay = *value_ptr + graph_->arcDelay(edge, arc, arc_delay_index);
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
  MinMax *min, *max;
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
				   float *value,
				   int triple_index,
				   int arc_delay_index,
				   const MinMax *min_max)
{
  if (value
      && triple_index != null_index_) {
    ArcDelay delay(*value);
    if (!is_incremental_only_ && in_incremental_)
      delay = graph_->arcDelay(edge, arc, arc_delay_index) + *value;
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
SdfReader::condMatch(const char *sdf_cond,
		     const char *lib_cond)
{
  // If the sdf is not conditional it matches any library condition.
  if (sdf_cond == nullptr)
    return true;
  else if (sdf_cond && lib_cond) {
    // Match sdf_cond and lib_cond ignoring blanks.
    const char *c1 = sdf_cond;
    const char *c2 = lib_cond;
    char ch1, ch2;
    do {
      ch1 = *c1++;
      ch2 = *c2++;
      while (ch1 && isspace(ch1))
	ch1 = *c1++;
      while (ch2 && isspace(ch2))
	ch2 = *c2++;
      if (ch1 != ch2)
	return false;
    } while (ch1 && ch2);
    return (ch1 == '\0' && ch2 == '\0');
  }
  else
    return false;
}

SdfPortSpec *
SdfReader::makePortSpec(Transition *tr,
			const char *port,
			const char *cond)
{
  SdfPortSpec *port_spec = new SdfPortSpec(tr, port, cond);
  stringDelete(port);
  stringDelete(cond);
  return port_spec;
}

SdfPortSpec *
SdfReader::makeCondPortSpec(const char *cond_port)
{
  // Search from end to find port name because condition may contain spaces.
  string cond_port1(cond_port);
  trimRight(cond_port1);
  auto port_idx = cond_port1.find_last_of(" ");
  if (port_idx != cond_port1.npos) {
    string port1 = cond_port1.substr(port_idx + 1);
    auto cond_end = cond_port1.find_last_not_of(" ", port_idx);
    if (cond_end != cond_port1.npos) {
      string cond1 = cond_port1.substr(0, cond_end + 1);
      SdfPortSpec *port_spec = new SdfPortSpec(Transition::riseFall(),
                                               port1.c_str(),
                                               cond1.c_str());
      stringDelete(cond_port);
      return port_spec;
    }
  }
  stringDelete(cond_port);
  return nullptr;
}

void
SdfReader::deletePortSpec(SdfPortSpec *edge)
{
  delete edge;
}

SdfTripleSeq *
SdfReader::makeTripleSeq()
{
  return new SdfTripleSeq;
}

void
SdfReader::deleteTripleSeq(SdfTripleSeq *triples)
{
  SdfTripleSeq::Iterator iter(triples);
  while (iter.hasNext()) {
    SdfTriple *triple = iter.next();
    delete triple;
  }
  delete triples;
}

SdfTriple *
SdfReader::makeTriple()
{
  return new SdfTriple(0, 0, 0);
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
  if (min) *min *= timescale_;
  if (typ) *typ *= timescale_;
  if (max) *max *= timescale_;
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

const char *
SdfReader::unescaped(const char *token)
{
  char path_escape = network_->pathEscape();
  char path_divider = network_->pathDivider();
  char *unescaped = new char[strlen(token) + 1];
  char *u = unescaped;
  size_t token_length = strlen(token);

  for (size_t i = 0; i < token_length; i++) {
    char ch = token[i];
    if (ch == escape_) {
      char next_ch = token[i + 1];
      if (next_ch == divider_) {
        // Escaped divider.
	// Translate sdf escape to network escape.
	*u++ = path_escape;
	// Translate sdf divider to network divider.
	*u++ = path_divider;
      }
      else if (next_ch == '['
	       || next_ch == ']'
	       || next_ch == escape_) {
	// Escaped bus bracket or escape.
	// Translate sdf escape to network escape.
	*u++ = path_escape;
	*u++ = next_ch;
      }
      else
        // Escaped non-divider character.
        *u++ = next_ch;
      i++;
    }
    else
      // Just the normal noises.
      *u++ = ch;
  }
  *u = '\0';
  debugPrint(debug_, "sdf_name", 1, "token %s -> %s", token, unescaped);
  stringDelete(token);
  return unescaped;
}

// Translate sdf divider to network divider.
char *
SdfReader::makePath(const char *head,
                    const char *tail)
{
  char *path = stringPrint("%s%c%s",
                           head,
                           network_->pathDivider(),
                           tail);
  sta::stringDelete(head);
  sta::stringDelete(tail);
  return path;
}

void
SdfReader::incrLine()
{
  line_++;
}

void
SdfReader::getChars(char *buf,
		    size_t &result,
		    size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_nullptr
  else
    result = strlen(buf);
}

void
SdfReader::getChars(char *buf,
		    int &result,
		    size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_nullptr
  else
    result = strlen(buf);
}

void
SdfReader::notSupported(const char *feature)
{
  sdfError(193, "%s not supported.", feature);
}

void
SdfReader::sdfWarn(int id,
                   const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(id, filename_, line_, fmt, args);
  va_end(args);
}

void
SdfReader::sdfError(int id,
                    const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileError(id, filename_, line_, fmt, args);
  va_end(args);
}

Pin *
SdfReader::findPin(const char *name)
{
  if (path_) {
    string path_name;
    stringPrint(path_name, "%s%c%s", path_, divider_, name);
    Pin *pin = network_->findPin(path_name.c_str());
    return pin;
  }
  else
    return network_->findPin(name);
}

Instance *
SdfReader::findInstance(const char *name)
{
  string inst_name = name;
  if (path_)
    stringPrint(inst_name, "%s%c%s", path_, divider_, name);
  Instance *inst = network_->findInstance(inst_name.c_str());
  if (inst == nullptr)
    sdfWarn(195, "instance %s not found.", inst_name.c_str());
  return inst;
}

////////////////////////////////////////////////////////////////

SdfPortSpec::SdfPortSpec(Transition *tr,
                         const char *port,
                         const char *cond) :
  tr_(tr),
  port_(stringCopy(port)),
  cond_(stringCopy(cond))
{
}

SdfPortSpec::~SdfPortSpec()
{
  stringDelete(port_);
  stringDelete(cond_);
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
    if (values_[0]) delete values_[0];
    if (values_[1]) delete values_[1];
    if (values_[2]) delete values_[2];
  }
}

bool
SdfTriple::hasValue() const
{
  return values_[0] || values_[1] || values_[2];
}

} // namespace

// Global namespace

void sdfFlushBuffer();

int
SdfParse_error(const char *msg)
{
  sdfFlushBuffer();
  sta::sdf_reader->sdfError(196, "%s.\n", msg);
  return 0;
}
