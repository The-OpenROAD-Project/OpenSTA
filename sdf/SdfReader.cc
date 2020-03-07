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

#include <stdarg.h>
#include <ctype.h>
#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "Error.hh"
#include "Report.hh"
#include "MinMax.hh"
#include "TimingArc.hh"
#include "Network.hh"
#include "SdcNetwork.hh"
#include "Graph.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "Sdf.hh"
#include "SdfReader.hh"

extern int
SdfParse_parse();

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
  DISALLOW_COPY_AND_ASSIGN(SdfTriple);

  float *values_[3];
};

class SdfPortSpec
{
public:
  SdfPortSpec(Transition *tr,
	      const char *port,
	      const char *cond = nullptr) :
    tr_(tr), port_(port), cond_(cond) {}
  ~SdfPortSpec()
  {
    stringDelete(port_);
    stringDelete(cond_);
  }
  const char *port() const { return port_; }
  Transition *transition() const { return tr_; }
  const char *cond() const { return cond_; }

private:
  DISALLOW_COPY_AND_ASSIGN(SdfPortSpec);

  Transition *tr_;
  const char *port_;
  const char *cond_;   // timing checks only
};

SdfReader *sdf_reader = nullptr;

bool
readSdfSingle(const char *filename,
	      const char *path,
	      Corner *corner,
	      int sdf_index,
              AnalysisType analysis_type,
	      bool unescaped_dividers,
	      bool incremental_only,
	      MinMaxAll *cond_use,
	      StaState *sta)
{
  int arc_index = corner->findDcalcAnalysisPt(MinMax::max())->index();
  SdfReader reader(filename, path, arc_index, sdf_index,
                   SdfReader::nullIndex(), SdfReader::nullIndex(),
                   analysis_type, unescaped_dividers, incremental_only,
		   cond_use, sta);
  sdf_reader = &reader;
  return reader.read();
}

bool
readSdfMinMax(const char *filename,
	      const char *path,
	      Corner *corner,
	      int sdf_min_index,
	      int sdf_max_index,
	      AnalysisType analysis_type,
	      bool unescaped_dividers,
	      bool incremental_only,
	      MinMaxAll *cond_use,
	      StaState *sta)
{
  int arc_min_index = corner->findDcalcAnalysisPt(MinMax::min())->index();
  int arc_max_index = corner->findDcalcAnalysisPt(MinMax::max())->index();
  SdfReader reader(filename, path,
		   arc_min_index, sdf_min_index,
		   arc_max_index, sdf_max_index,
		   analysis_type, unescaped_dividers, incremental_only,
		   cond_use, sta);
  sdf_reader = &reader;
  return reader.read();
}

SdfReader::SdfReader(const char *filename,
		     const char *path,
                     int arc_min_index,
		     int triple_min_index,
		     int arc_max_index,
		     int triple_max_index,
		     AnalysisType analysis_type,
		     bool unescaped_dividers,
		     bool is_incremental_only,
                     MinMaxAll *cond_use,
		     StaState *sta) :
  StaState(sta),
  filename_(filename),
  path_(path),
  triple_min_index_(triple_min_index),
  triple_max_index_(triple_max_index),
  arc_delay_min_index_(arc_min_index),
  arc_delay_max_index_(arc_max_index),
  analysis_type_(analysis_type),
  unescaped_dividers_(unescaped_dividers),
  is_incremental_only_(is_incremental_only),
  cond_use_(cond_use),
  line_(1),
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
      sdfError("TIMESCALE units not us, ns, or ps.\n");
  }
  else
      sdfError("TIMESCALE multiplier not 1, 10, or 100.\n");
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
	    sdfError("pin %s is a hierarchical pin.\n", from_pin_name);
	  if (to_is_hier)
	    sdfError("pin %s is a hierarchical pin.\n", to_pin_name);
	}
	else
	  sdfError("INTERCONNECT from %s to %s not found.\n",
		   from_pin_name, to_pin_name);
      }
    }
    else {
      if (from_pin == nullptr)
	sdfError("pin %s not found.\n", from_pin_name);
      if (to_pin == nullptr)
	sdfError("pin %s not found.\n", to_pin_name);
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
      sdfError("pin %s not found.\n", to_pin_name);
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
  // Fanin < fanout, so search for driver from load.
  VertexInEdgeIterator edge_iter(to_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    const TimingRole *edge_role = edge->role();
    if (edge->from(graph_)->pin() == from_pin
	&& edge_role->sdfRole()->isWire())
      return edge;
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
    TimingArcSetArcIterator arc_iter(arc_set);
    while (arc_iter.hasNext()) {
      TimingArc *arc = arc_iter.next();
      size_t triple_index;
      if (triple_count == 1)
	triple_index = 0;
      else
	triple_index = arc->toTrans()->sdfTripleIndex();
      SdfTriple *triple = (*triples)[triple_index];
      setEdgeArcDelays(edge, arc, triple);
    }
  }
  else if (triple_count == 0)
    sdfError("%s with no triples.\n", sdf_cmd);
  else
    sdfError("%s with more than 2 triples.\n", sdf_cmd);
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
	  sdfError("instance %s cell %s does not match enclosing cell %s.\n",
		   instance_name,
		   inst_cell_name,
		   cell_name_);
      }
    }
    stringDelete(instance_name);
  }
  else
    instance_ = nullptr;
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
    Port *from_port = network_->findPort(cell, from_port_name);
    Port *to_port = network_->findPort(cell, to_port_name);
    if (from_port == nullptr)
      portNotFound(from_port_name);
    if (to_port == nullptr)
      portNotFound(to_port_name);
    if (from_port && to_port) {
      Pin *from_pin = network_->findPin(instance_, from_port_name);
      Pin *to_pin = network_->findPin(instance_, to_port_name);
      // Do not report an error if the pin is not found because the
      // instance may not have the pin.
      if (from_pin && to_pin) {
	Vertex *to_vertex = graph_->pinDrvrVertex(to_pin);
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
	    TimingArcSetArcIterator arc_iter(arc_set);
	    while (arc_iter.hasNext()) {
	      TimingArc *arc = arc_iter.next();
	      if ((from_edge->transition() == Transition::riseFall())
		  || (arc->fromTrans() == from_edge->transition())) {
		size_t triple_index = arc->toTrans()->sdfTripleIndex();
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
	  sdfError("cell %s IOPATH %s -> %s not found.\n",
		   network_->cellName(instance_),
		   from_port_name,
		   to_port_name);
      }
    }
  }
  deletePortSpec(from_edge);
  stringDelete(to_port_name);
  deleteTripleSeq(triples);
  if (cond)
    stringDelete(cond);
}

void
SdfReader::timingCheck(TimingRole *role, SdfPortSpec *data_edge,
		       SdfPortSpec *clk_edge, SdfTriple *triple)
{
  timingCheck1(role, data_edge, clk_edge, triple, true);
  deletePortSpec(data_edge);
  deletePortSpec(clk_edge);
  deleteTriple(triple);
}

void
SdfReader::timingCheck1(TimingRole *role,
			SdfPortSpec *data_edge,
			SdfPortSpec *clk_edge,
			SdfTriple *triple,
			bool warn)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)
      && instance_) {
    const char *data_port_name = data_edge->port();
    const char *clk_port_name = clk_edge->port();
    Cell *cell = network_->cell(instance_);
    Port *data_port = network_->findPort(cell, data_port_name);
    Port *clk_port = network_->findPort(cell, clk_port_name);
    if (data_port == nullptr && warn)
      portNotFound(data_port_name);
    if (clk_port == nullptr && warn)
      portNotFound(clk_port_name);
    if (data_port && clk_port) {
      Pin *data_pin = network_->findPin(instance_, data_port_name);
      Pin *clk_pin = network_->findPin(instance_, clk_port_name);
      if (data_pin && clk_pin) {
	// Hack to match PT; always use triple max value for check.
	if (triple_min_index_ != null_index_
	    && triple_max_index_ != null_index_) {
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
	  sdfError("cell %s %s -> %s %s check not found.\n",
		   network_->cellName(instance_),
		   data_port_name,
		   clk_port_name,
		   role->asString());
      }
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
	TimingArcSetArcIterator arc_iter(arc_set);
	while (arc_iter.hasNext()) {
	  TimingArc *arc = arc_iter.next();
	  if (((data_edge->transition() == Transition::riseFall())
	       || (arc->toTrans() == data_edge->transition()))
	      && ((clk_edge->transition() == Transition::riseFall())
		  || (arc->fromTrans() == clk_edge->transition()))) {
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
    Port *port = network_->findPort(cell, port_name);
    if (port == nullptr)
      portNotFound(port_name);
    else {
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
SdfReader::timingCheckPeriod(SdfPortSpec *edge,
			     SdfTriple *triple)
{
  // Ignore non-incremental annotations in incremental only mode.
  if (!(is_incremental_only_ && !in_incremental_)
      && instance_) {
    const char *port_name = edge->port();
    Cell *cell = network_->cell(instance_);
    Port *port = network_->findPort(cell, port_name);
    if (port == nullptr)
      portNotFound(port_name);
    else {
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
SdfReader::timingCheckSetupHold(SdfPortSpec *data_edge,
				SdfPortSpec *clk_edge,
				SdfTriple *setup_triple,
				SdfTriple *hold_triple)
{
  timingCheck1(TimingRole::setup(), data_edge, clk_edge, setup_triple, true);
  timingCheck1(TimingRole::hold(), data_edge, clk_edge, hold_triple, false);
  deletePortSpec(data_edge);
  deletePortSpec(clk_edge);
  deleteTriple(setup_triple);
  deleteTriple(hold_triple);
}

void
SdfReader::timingCheckRecRem(SdfPortSpec *data_edge,
			     SdfPortSpec *clk_edge,
			     SdfTriple *rec_triple,
			     SdfTriple *rem_triple)
{
  timingCheck1(TimingRole::recovery(), data_edge, clk_edge, rec_triple, true);
  timingCheck1(TimingRole::removal(), data_edge, clk_edge, rem_triple, false);
  deletePortSpec(data_edge);
  deletePortSpec(clk_edge);
  deleteTriple(rec_triple);
  deleteTriple(rem_triple);
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
    Port *to_port = network_->findPort(cell, to_port_name);
    if (to_port == nullptr)
      portNotFound(to_port_name);
    else {
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
  VertexInEdgeIterator edge_iter(vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->role()->sdfRole() == TimingRole::sdfIopath())
      setEdgeDelays(edge, triples, "DEVICE");
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
  float *value_min = nullptr;
  if (triple_min_index_ != null_index_)
    value_min = values[triple_min_index_];
  float *value_max = nullptr;
  if (triple_max_index_ != null_index_)
    value_max = values[triple_max_index_];
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
      if (fuzzyGreater(prev_value, delay, min_max))
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
  return new SdfPortSpec(tr, port, cond);
}

SdfPortSpec *
SdfReader::makeCondPortSpec(char *cond_port)
{
  char *cond = cond_port;
  // Search from end to find port name because condition may contain spaces.
  char *p = &cond_port[strlen(cond_port) - 1];
  // Trim trailing port spaces.
  while (*p != '\0' && isspace(*p))
    p--;
  p[1] = '\0';
  while (*p != '\0' && !isspace(*p))
    p--;
  char *port = &p[1];
  // Trim trailing cond spaces.
  while (*p != '\0' && isspace(*p))
    p--;
  p[1] = '\0';
  SdfPortSpec *port_spec = makePortSpec(Transition::riseFall(),
					stringCopy(port),
					stringCopy(cond));
  stringDelete(cond_port);
  return port_spec;
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
  char *unescaped = makeTmpString(strlen(token) + 1);
  char *u = unescaped;
  for (const char *s = token; *s ; s++) {
    char ch = *s;

    if (ch == escape_) {
      char next_ch = s[1];

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
	// Escaped non-divider/bracket character.
	*u++ = next_ch;
      s++;
    }
    else if (ch == divider_)
      // Translate sdf divider to network divider.
      *u++ = path_divider;
    else
      // Just the normal noises.
      *u++ = ch;
  }
  *u = '\0';
  return unescaped;
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
  sdfError("%s not supported.\n", feature);
}

void
SdfReader::portNotFound(const char *port_name)
{
  sdfError("instance %s port %s not found.\n",
	   network_->pathName(instance_),
	   port_name);
}

void
SdfReader::sdfError(const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileError(filename_, line_, fmt, args);
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
    sdfError("instance %s not found.\n", inst_name.c_str());
  return inst;
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
  sta::sdf_reader->sdfError("%s.\n", msg);
  sdfFlushBuffer();
  return 0;
}
