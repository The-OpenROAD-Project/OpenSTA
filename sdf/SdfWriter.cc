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

#include <stdio.h>
#include <time.h>
#include "Machine.hh"
#include "Zlib.hh"
#include "StaConfig.hh"  // STA_VERSION
#include "Fuzzy.hh"
#include "StringUtil.hh"
#include "MinMaxValues.hh"
#include "Units.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Sdc.hh"
#include "Network.hh"
#include "Graph.hh"
#include "StaState.hh"
#include "Corner.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc1.hh"
#include "PathAnalysisPt.hh"
#include "SdfWriter.hh"

namespace sta {

class SdfWriter : public StaState
{
public:
  SdfWriter(StaState *sta);
  ~SdfWriter();
  void write(const char *filename,
	     Corner *corner,
	     char sdf_divider,
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
		  const char *sdf_check);
  void writeCheck(Edge *edge,
		  TimingArc *arc,
		  const char *sdf_check,
		  bool use_data_edge,
		  bool use_clk_edge);
  void writeEdgeCheck(Edge *edge,
		      const char *sdf_check,
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
  const char *sdfEdge(const Transition *tr);
  void writeArcDelays(Edge *edge);
  void writeSdfTuple(RiseFallMinMax &delays,
		     RiseFall *rf);
  void writeSdfTuple(float min_delay,
		     float max_delay);
  void writeSdfDelay(double delay);
  char *sdfPortName(const Pin *pin);
  char *sdfPathName(const Pin *pin);
  char *sdfPathName(const Instance *inst);
  char *sdfName(const Instance *inst);

private:
  DISALLOW_COPY_AND_ASSIGN(SdfWriter);

  char sdf_divider_;
  float timescale_;

  char sdf_escape_;
  char network_escape_;
  char *delay_format_;

  gzFile stream_;
  const Corner *corner_;
  int arc_delay_min_index_;
  int arc_delay_max_index_;
};

void
writeSdf(const char *filename,
	 Corner *corner,
	 char sdf_divider,
	 int digits,
	 bool gzip,
	 bool no_timestamp,
	 bool no_version,
	 StaState *sta)
{
  SdfWriter writer(sta);
  writer.write(filename, corner, sdf_divider, digits, gzip,
	       no_timestamp, no_version);
}

SdfWriter::SdfWriter(StaState *sta) :
  StaState(sta),
  sdf_escape_('\\'),
  network_escape_(network_->pathEscape()),
  delay_format_(nullptr)
{
}

SdfWriter::~SdfWriter()
{
  stringDelete(delay_format_);
}

void
SdfWriter::write(const char *filename,
		 Corner *corner,
		 char sdf_divider,
		 int digits,
		 bool gzip,
		 bool no_timestamp,
		 bool no_version)
{
  sdf_divider_ = sdf_divider;
  if (delay_format_ == nullptr)
    delay_format_ = new char[10];
  sprintf(delay_format_, "%%.%df", digits);

  LibertyLibrary *default_lib = network_->defaultLibertyLibrary();
  timescale_ = default_lib->units()->timeUnit()->scale();

  corner_ = corner;
  MinMax *min_max;
  const DcalcAnalysisPt *dcalc_ap;
  min_max = MinMax::min();
  dcalc_ap = corner_->findDcalcAnalysisPt(min_max);
  arc_delay_min_index_ = dcalc_ap->index();
  min_max = MinMax::max();
  dcalc_ap = corner_->findDcalcAnalysisPt(min_max);
  arc_delay_max_index_ = dcalc_ap->index();

  stream_ = gzopen(filename, gzip ? "wb" : "wT");

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
  gzprintf(stream_, "(DELAYFILE\n");
  gzprintf(stream_, " (SDFVERSION \"3.0\")\n");
  gzprintf(stream_, " (DESIGN \"%s\")\n", 
	   network_->cellName(network_->topInstance()));
  
  if (!no_timestamp) {
    time_t now;
    time(&now);
    char *time_str = ctime(&now);
    // Remove trailing \n.
    time_str[strlen(time_str) - 1] = '\0';
    gzprintf(stream_, " (DATE \"%s\")\n", time_str);
  }

  gzprintf(stream_, " (VENDOR \"Parallax\")\n");
  gzprintf(stream_, " (PROGRAM \"STA\")\n");
  if (!no_version)
    gzprintf(stream_, " (VERSION \"%s\")\n", STA_VERSION);
  gzprintf(stream_, " (DIVIDER %c)\n", sdf_divider_);

  OperatingConditions *cond_min = 
    sdc_->operatingConditions(MinMax::min());
  if (cond_min == nullptr)
    cond_min = default_lib->defaultOperatingConditions();
  OperatingConditions *cond_max = 
    sdc_->operatingConditions(MinMax::max());
  if (cond_max == nullptr)
    cond_max = default_lib->defaultOperatingConditions();
  if (cond_min && cond_max) {
    gzprintf(stream_, " (VOLTAGE %.3f::%.3f)\n",
	     cond_min->voltage(),
	     cond_max->voltage());
    gzprintf(stream_, " (PROCESS \"%.3f::%.3f\")\n",
	     cond_min->process(),
	     cond_max->process());
    gzprintf(stream_, " (TEMPERATURE %.3f::%.3f)\n",
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
    gzprintf(stream_, " (TIMESCALE %s)\n", sdf_timescale);
}

void
SdfWriter::writeTrailer()
{
  gzprintf(stream_, ")\n");
}

void
SdfWriter::writeInterconnects()
{
  gzprintf(stream_, " (CELL\n");
  gzprintf(stream_, "  (CELLTYPE \"%s\")\n",
	   network_->cellName(network_->topInstance()));
  gzprintf(stream_, "  (INSTANCE)\n");
  gzprintf(stream_, "  (DELAY\n");
  gzprintf(stream_, "   (ABSOLUTE\n");

  writeInstInterconnects(network_->topInstance());

  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    writeInstInterconnects(inst);
  }
  delete inst_iter;

  gzprintf(stream_, "   )\n");
  gzprintf(stream_, "  )\n");
  gzprintf(stream_, " )\n");
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
  VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    if (edge->isWire()) {
      Pin *load_pin = edge->to(graph_)->pin();
      gzprintf(stream_, "    (INTERCONNECT %s %s ",
	       sdfPathName(drvr_pin),
	       sdfPathName(load_pin));
      writeArcDelays(edge);
      gzprintf(stream_, ")\n");
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
  gzprintf(stream_, " (CELL\n");
  gzprintf(stream_, "  (CELLTYPE \"%s\")\n", network_->cellName(inst));
  gzprintf(stream_, "  (INSTANCE %s)\n", sdfPathName(inst));
}

void
SdfWriter::writeInstTrailer()
{
  gzprintf(stream_, " )\n");
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
	TimingRole *role = edge->role();
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
	  const char *sdf_cond = edge->timingArcSet()->sdfCond();
	  if (sdf_cond) {
	    gzprintf(stream_, "    (COND %s\n", sdf_cond);
	    gzprintf(stream_, " ");
	  }
	  gzprintf(stream_, "    (IOPATH %s %s ",
		   sdfPortName(from_pin),
		   sdfPortName(to_pin));
	  writeArcDelays(edge);
	  if (sdf_cond)
	    gzprintf(stream_, ")");
	  gzprintf(stream_, ")\n");
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
  gzprintf(stream_, "  (DELAY\n");
  gzprintf(stream_, "   (ABSOLUTE\n");
}

void
SdfWriter::writeIopathTrailer()
{
  gzprintf(stream_, "   )\n");
  gzprintf(stream_, "  )\n");
}

void
SdfWriter::writeArcDelays(Edge *edge)
{
  RiseFallMinMax delays;
  TimingArcSet *arc_set = edge->timingArcSet();
  TimingArcSetArcIterator arc_iter(arc_set);
  while (arc_iter.hasNext()) {
    TimingArc *arc = arc_iter.next();
    RiseFall *rf = arc->toTrans()->asRiseFall();
    ArcDelay min_delay = graph_->arcDelay(edge, arc, arc_delay_min_index_);
    delays.setValue(rf, MinMax::min(), delayAsFloat(min_delay));

    ArcDelay max_delay = graph_->arcDelay(edge, arc, arc_delay_max_index_);
    delays.setValue(rf, MinMax::max(), delayAsFloat(max_delay));
  }

  if (delays.hasValue(RiseFall::rise(), MinMax::min())
      && delays.hasValue(RiseFall::fall(), MinMax::min())) {
    // Rise and fall.
    writeSdfTuple(delays, RiseFall::rise());
    // Merge rise/fall values if they are the same.
    if (!(fuzzyEqual(delays.value(RiseFall::rise(), MinMax::min()),
		     delays.value(RiseFall::fall(), MinMax::min()))
	  && fuzzyEqual(delays.value(RiseFall::rise(), MinMax::max()),
			delays.value(RiseFall::fall(),MinMax::max())))) {
      gzprintf(stream_, " ");
      writeSdfTuple(delays, RiseFall::fall());
    }
  }
  else if (delays.hasValue(RiseFall::rise(), MinMax::min()))
    // Rise only.
    writeSdfTuple(delays, RiseFall::rise());
  else if (delays.hasValue(RiseFall::fall(), MinMax::min())) {
    // Fall only.
    gzprintf(stream_, "() ");
    writeSdfTuple(delays, RiseFall::fall());
  }
}

void
SdfWriter::writeSdfTuple(RiseFallMinMax &delays,
			 RiseFall *rf)
{
  gzprintf(stream_, "(");
  writeSdfDelay(delays.value(rf, MinMax::min()));
  gzprintf(stream_, "::");
  writeSdfDelay(delays.value(rf, MinMax::max()));
  gzprintf(stream_, ")");
}

void
SdfWriter::writeSdfTuple(float min_delay,
			 float max_delay)
{
  gzprintf(stream_, "(");
  writeSdfDelay(min_delay);
  gzprintf(stream_, "::");
  writeSdfDelay(max_delay);
  gzprintf(stream_, ")");
}

void
SdfWriter::writeSdfDelay(double delay)
{
  gzprintf(stream_, delay_format_, delay / timescale_);
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
    VertexOutEdgeIterator edge_iter(vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      TimingRole *role = edge->role();
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
      bool exists;
      graph_delay_calc_->minPulseWidth(pin, hi_low, arc_delay_min_index_,
				       MinMax::min(),
				       min_width, exists);
      graph_delay_calc_->minPulseWidth(pin, hi_low, arc_delay_max_index_,
				       MinMax::max(),
		    max_width, exists);
      if (exists) {
	ensureTimingCheckheaders(check_header, inst, inst_header);
	writeWidthCheck(pin, hi_low, min_width, max_width);
      }
    }
    float min_period;
    bool exists;
    graph_delay_calc_->minPeriod(pin, min_period, exists);
    if (exists) {
      ensureTimingCheckheaders(check_header, inst, inst_header);
      writePeriodCheck(pin, min_period);
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
  gzprintf(stream_, "  (TIMINGCHECK\n");
}

void
SdfWriter::writeTimingCheckTrailer()
{
  gzprintf(stream_, "  )\n");
}

void
SdfWriter::writeCheck(Edge *edge,
		      const char *sdf_check)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  // Examine the arcs to see if the check requires clk or data edge specifiers.
  TimingArc *arcs[RiseFall::index_count][RiseFall::index_count] = 
    {{nullptr, nullptr}, {nullptr, nullptr}};
  TimingArcSetArcIterator arc_iter(arc_set);
  while (arc_iter.hasNext()) {
    TimingArc *arc = arc_iter.next();
    RiseFall *clk_rf = arc->fromTrans()->asRiseFall();
    RiseFall *data_rf = arc->toTrans()->asRiseFall();;
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
    TimingArcSetArcIterator arc_iter(arc_set);
    while (arc_iter.hasNext()) {
      TimingArc *arc = arc_iter.next();
      writeCheck(edge, arc, sdf_check, true, true);
    }
  }
}

void
SdfWriter::writeEdgeCheck(Edge *edge,
			  const char *sdf_check,
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
      && arcs[clk_rf_index][RiseFall::fallIndex()]
      && fuzzyEqual(graph_->arcDelay(edge, 
				       arcs[clk_rf_index][RiseFall::riseIndex()],
				       arc_delay_min_index_),
			 graph_->arcDelay(edge,
					  arcs[clk_rf_index][RiseFall::fallIndex()],
					  arc_delay_min_index_))
      && fuzzyEqual(graph_->arcDelay(edge,
					  arcs[clk_rf_index][RiseFall::riseIndex()],
					  arc_delay_max_index_),
			 graph_->arcDelay(edge,
					  arcs[clk_rf_index][RiseFall::fallIndex()],
					  arc_delay_max_index_)))
    // Rise/fall margins are the same, so no data edge specifier is required.
    writeCheck(edge, arcs[clk_rf_index][RiseFall::riseIndex()],
	       sdf_check, false, true);
  else {
    if (arcs[clk_rf_index][RiseFall::riseIndex()])
      writeCheck(edge, arcs[clk_rf_index][RiseFall::riseIndex()], 
		 sdf_check, true, true);
    if (arcs[clk_rf_index][RiseFall::fallIndex()])
      writeCheck(edge, arcs[clk_rf_index][RiseFall::fallIndex()],
		 sdf_check, true, true);
  }
}

void
SdfWriter::writeCheck(Edge *edge,
		      TimingArc *arc,
		      const char *sdf_check,
		      bool use_data_edge,
		      bool use_clk_edge)
{
  TimingArcSet *arc_set = edge->timingArcSet();
  Pin *from_pin = edge->from(graph_)->pin();
  Pin *to_pin = edge->to(graph_)->pin();
  const char *sdf_cond_start = arc_set->sdfCondStart();
  const char *sdf_cond_end = arc_set->sdfCondEnd();

  gzprintf(stream_, "    (%s ", sdf_check);

  if (sdf_cond_start)
    gzprintf(stream_, "(COND %s ", sdf_cond_start);

  if (use_data_edge)
    gzprintf(stream_, "(%s %s)",
	     sdfEdge(arc->toTrans()),
	     sdfPortName(to_pin));
  else
    gzprintf(stream_, "%s", sdfPortName(to_pin));

  if (sdf_cond_start)
    gzprintf(stream_, ")");

  gzprintf(stream_, " ");

  if (sdf_cond_end)
    gzprintf(stream_, "(COND %s ", sdf_cond_end);

  if (use_clk_edge)
    gzprintf(stream_, "(%s %s)",
	     sdfEdge(arc->fromTrans()),
	     sdfPortName(from_pin));
  else
    gzprintf(stream_, "%s", sdfPortName(from_pin));

  if (sdf_cond_end)
    gzprintf(stream_, ")");

  gzprintf(stream_, " ");

  ArcDelay min_delay = graph_->arcDelay(edge, arc, arc_delay_min_index_);
  ArcDelay max_delay = graph_->arcDelay(edge, arc, arc_delay_max_index_);
  writeSdfTuple(delayAsFloat(min_delay), delayAsFloat(max_delay));

  gzprintf(stream_, ")\n");
}

void
SdfWriter::writeWidthCheck(const Pin *pin,
			   const RiseFall *hi_low,
			   float min_width,
			   float max_width)
{
  gzprintf(stream_, "    (WIDTH (%s %s) ",
	   sdfEdge(hi_low->asTransition()),
	   sdfPortName(pin));
  writeSdfTuple(min_width, max_width);
  gzprintf(stream_, ")\n");
}

void
SdfWriter::writePeriodCheck(const Pin *pin,
			    float min_period)
{
  gzprintf(stream_, "    (PERIOD %s ",
	   sdfPortName(pin));
  writeSdfTuple(min_period, min_period);
  gzprintf(stream_, ")\n");
}

const char *
SdfWriter::sdfEdge(const Transition *tr)
{
  if (tr == Transition::rise())
    return "posedge";
  else if (tr == Transition::fall())
    return "negedge";
  return nullptr;
}

////////////////////////////////////////////////////////////////

char *
SdfWriter::sdfPortName(const Pin *pin)
{
  return const_cast<char *>(network_->portName(pin));
}

char *
SdfWriter::sdfPathName(const Pin *pin)
{
  Instance *inst = network_->instance(pin);
  if (network_->isTopInstance(inst))
    return sdfPortName(pin);
  else {
    char *inst_path = sdfPathName(inst);
    const char *port_name = sdfPortName(pin);
    char *sdf_name = makeTmpString(strlen(inst_path)+1+strlen(port_name)+1);
    sprintf(sdf_name, "%s%c%s", inst_path, sdf_divider_, port_name);
    return sdf_name;
  }
}

// Based on Network::pathName.
char *
SdfWriter::sdfPathName(const Instance *instance)
{
  ConstInstanceSeq inst_path;
  network_->path(instance, inst_path);
  size_t name_length = 0;
  ConstInstanceSeq::Iterator path_iter1(inst_path);
  while (path_iter1.hasNext()) {
    const Instance *inst = path_iter1.next();
    name_length += strlen(sdfName(inst)) + 1;
  }
  char *path_name = makeTmpString(name_length);
  char *path_ptr = path_name;
  // Top instance has null string name, so terminate the string here.
  *path_name = '\0';
  while (inst_path.size()) {
    const Instance *inst = inst_path.back();
    const char *inst_name = sdfName(inst);
    strcpy(path_ptr, inst_name);
    path_ptr += strlen(inst_name);
    inst_path.pop_back();
    if (inst_path.size())
      *path_ptr++ = sdf_divider_;
    *path_ptr = '\0';
  }
  return path_name;
}

// Escape for non-alpha numeric characters.
char *
SdfWriter::sdfName(const Instance *inst)
{
  const char *name = network_->name(inst);
  char *sdf_name = makeTmpString(strlen(name) * 2 + 1);
  const char *p = name;
  char *s = sdf_name;
  while (*p) {
    char ch = *p;
    // Ignore sta escapes.
    if (ch != network_escape_) {
      if (!(isalnum(ch) || ch == '_'))
	// Insert escape.
	*s++ = sdf_escape_;
      *s++ = ch;
    }
    p++;
  }
  *s = '\0';
  return sdf_name;
}

} // namespace
