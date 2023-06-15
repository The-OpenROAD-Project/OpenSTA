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

#include "sdf/SdfWriter.hh"

#include <cstdio>
#include <ctime>

#include "Zlib.hh"
#include "StaConfig.hh"  // STA_VERSION
#include "Fuzzy.hh"
#include "StringUtil.hh"
#include "Units.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "Sdc.hh"
#include "MinMaxValues.hh"
#include "Network.hh"
#include "Graph.hh"
#include "DcalcAnalysisPt.hh"
#include "GraphDelayCalc.hh"
#include "StaState.hh"
#include "Corner.hh"
#include "PathAnalysisPt.hh"

namespace sta {

class SdfWriter : public StaState
{
public:
  SdfWriter(StaState *sta);
  ~SdfWriter();
  void write(const char *filename,
	     Corner *corner,
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
  void writeSdfTriple(RiseFallMinMax &delays,
                      RiseFall *rf);
  void writeSdfTriple(float min,
                      float max);
  void writeSdfDelay(double delay);
  string sdfPortName(const Pin *pin);
  string sdfPathName(const Pin *pin);
  string sdfPathName(const Instance *inst);
  string sdfName(const Instance *inst);

private:
  char sdf_divider_;
  bool include_typ_;
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
         bool include_typ,
	 int digits,
	 bool gzip,
	 bool no_timestamp,
	 bool no_version,
	 StaState *sta)
{
  SdfWriter writer(sta);
  writer.write(filename, corner, sdf_divider, include_typ, digits, gzip,
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
                 bool include_typ,
		 int digits,
		 bool gzip,
		 bool no_timestamp,
		 bool no_version)
{
  sdf_divider_ = sdf_divider;
  include_typ_ = include_typ;
  if (delay_format_ == nullptr)
    delay_format_ = stringPrint("%%.%df", digits);

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
  if (drvr_vertex) {
    VertexOutEdgeIterator edge_iter(drvr_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      if (edge->isWire()) {
        Pin *load_pin = edge->to(graph_)->pin();
        string drvr_pin_name = sdfPathName(drvr_pin);
        string load_pin_name = sdfPathName(load_pin);
        gzprintf(stream_, "    (INTERCONNECT %s %s ",
                 drvr_pin_name.c_str(),
                 load_pin_name.c_str());
        writeArcDelays(edge);
        gzprintf(stream_, ")\n");
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
  gzprintf(stream_, " (CELL\n");
  gzprintf(stream_, "  (CELLTYPE \"%s\")\n", network_->cellName(inst));
  string inst_name = sdfPathName(inst);
  gzprintf(stream_, "  (INSTANCE %s)\n", inst_name.c_str());
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
	  string from_pin_name = sdfPortName(from_pin);
	  string to_pin_name = sdfPortName(to_pin);
          gzprintf(stream_, "    (IOPATH %s %s ",
		   from_pin_name.c_str(),
		   to_pin_name.c_str());
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
  for (TimingArc *arc : arc_set->arcs()) {
    RiseFall *rf = arc->toEdge()->asRiseFall();
    ArcDelay min_delay = graph_->arcDelay(edge, arc, arc_delay_min_index_);
    delays.setValue(rf, MinMax::min(), delayAsFloat(min_delay));

    ArcDelay max_delay = graph_->arcDelay(edge, arc, arc_delay_max_index_);
    delays.setValue(rf, MinMax::max(), delayAsFloat(max_delay));
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
      gzprintf(stream_, " ");
      writeSdfTriple(delays, RiseFall::fall());
    }
  }
  else if (delays.hasValue(RiseFall::rise(), MinMax::min()))
    // Rise only.
    writeSdfTriple(delays, RiseFall::rise());
  else if (delays.hasValue(RiseFall::fall(), MinMax::min())) {
    // Fall only.
    gzprintf(stream_, "() ");
    writeSdfTriple(delays, RiseFall::fall());
  }
}

void
SdfWriter::writeSdfTriple(RiseFallMinMax &delays,
                          RiseFall *rf)
{
  float min = delays.value(rf, MinMax::min());
  float max = delays.value(rf, MinMax::max());
  writeSdfTriple(min, max);
}

void
SdfWriter::writeSdfTriple(float min,
                          float max)
{
  gzprintf(stream_, "(");
  writeSdfDelay(min);
  if (include_typ_) {
    gzprintf(stream_, ":");
    writeSdfDelay((min + max) / 2.0);
    gzprintf(stream_, ":");
  }
  else
    gzprintf(stream_, "::");
  writeSdfDelay(max);
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
  for (TimingArc *arc : arc_set->arcs()) {
    RiseFall *clk_rf = arc->fromEdge()->asRiseFall();
    RiseFall *data_rf = arc->toEdge()->asRiseFall();;
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
      && delayEqual(graph_->arcDelay(edge, 
				     arcs[clk_rf_index][RiseFall::riseIndex()],
				     arc_delay_min_index_),
		    graph_->arcDelay(edge,
				     arcs[clk_rf_index][RiseFall::fallIndex()],
				     arc_delay_min_index_))
      && delayEqual(graph_->arcDelay(edge,
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

  string to_pin_name = sdfPortName(to_pin);
  if (use_data_edge) {
    gzprintf(stream_, "(%s %s)",
	     sdfEdge(arc->toEdge()),
	     to_pin_name.c_str());
  }
  else
    gzprintf(stream_, "%s", to_pin_name.c_str());

  if (sdf_cond_start)
    gzprintf(stream_, ")");

  gzprintf(stream_, " ");

  if (sdf_cond_end)
    gzprintf(stream_, "(COND %s ", sdf_cond_end);

  string from_pin_name = sdfPortName(from_pin);
  if (use_clk_edge)
    gzprintf(stream_, "(%s %s)",
	     sdfEdge(arc->fromEdge()),
	     from_pin_name.c_str());
  else
    gzprintf(stream_, "%s", from_pin_name.c_str());

  if (sdf_cond_end)
    gzprintf(stream_, ")");

  gzprintf(stream_, " ");

  ArcDelay min_delay = graph_->arcDelay(edge, arc, arc_delay_min_index_);
  ArcDelay max_delay = graph_->arcDelay(edge, arc, arc_delay_max_index_);
  writeSdfTriple(delayAsFloat(min_delay), delayAsFloat(max_delay));

  gzprintf(stream_, ")\n");
}

void
SdfWriter::writeWidthCheck(const Pin *pin,
			   const RiseFall *hi_low,
			   float min_width,
			   float max_width)
{
  string pin_name = sdfPortName(pin);
  gzprintf(stream_, "    (WIDTH (%s %s) ",
	   sdfEdge(hi_low->asTransition()),
	   pin_name.c_str());
  writeSdfTriple(min_width, max_width);
  gzprintf(stream_, ")\n");
}

void
SdfWriter::writePeriodCheck(const Pin *pin,
			    float min_period)
{
  string pin_name = sdfPortName(pin);
  gzprintf(stream_, "    (PERIOD %s ", pin_name.c_str());
  writeSdfTriple(min_period, min_period);
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

string
SdfWriter::sdfPathName(const Pin *pin)
{
  Instance *inst = network_->instance(pin);
  if (network_->isTopInstance(inst))
    return sdfPortName(pin);
  else {
    string inst_path = sdfPathName(inst);
    string port_name = sdfPortName(pin);
    string sdf_name = inst_path;
    sdf_name += sdf_divider_;
    sdf_name += port_name;
    return sdf_name;
  }
}

// Based on Network::pathName.
string
SdfWriter::sdfPathName(const Instance *instance)
{
  InstanceSeq inst_path;
  network_->path(instance, inst_path);
  InstanceSeq::Iterator path_iter1(inst_path);
  string path_name;
  while (!inst_path.empty()) {
    const Instance *inst = inst_path.back();
    string inst_name = sdfName(inst);
    path_name += inst_name;
    inst_path.pop_back();
    if (!inst_path.empty())
      path_name += sdf_divider_;
  }
  return path_name;
}

// Escape for non-alpha numeric characters.
string
SdfWriter::sdfName(const Instance *inst)
{
  const char *name = network_->name(inst);
  string sdf_name;
  const char *p = name;
  while (*p) {
    char ch = *p;
    // Ignore sta escapes.
    if (ch != network_escape_) {
      if (!(isalnum(ch) || ch == '_'))
	// Insert escape.
	sdf_name += sdf_escape_;
      sdf_name += ch;
    }
    p++;
  }
  return sdf_name;
}

string
SdfWriter::sdfPortName(const Pin *pin)
{
  const char *name = network_->portName(pin);
  size_t name_length = strlen(name);
  string sdf_name;

  constexpr char bus_brkt_left = '[';
  constexpr char bus_brkt_right = ']';
  size_t bus_index = name_length;
  if (name_length >= 4
      && name[name_length - 1] == bus_brkt_right) {
    const char *left = strrchr(name, bus_brkt_left);
    if (left)
      bus_index = left - name;
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

} // namespace
