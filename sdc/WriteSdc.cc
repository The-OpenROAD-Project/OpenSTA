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

#include "WriteSdc.hh"

#include <cstdio>
#include <algorithm>
#include <ctime>

#include "Zlib.hh"
#include "Report.hh"
#include "Error.hh"
#include "Units.hh"
#include "Transition.hh"
#include "Liberty.hh"
#include "Wireload.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "NetworkCmp.hh"
#include "Graph.hh"
#include "GraphCmp.hh"
#include "RiseFallValues.hh"
#include "PortDelay.hh"
#include "ExceptionPath.hh"
#include "PortExtCap.hh"
#include "DisabledPorts.hh"
#include "ClockGroups.hh"
#include "ClockInsertion.hh"
#include "ClockLatency.hh"
#include "InputDrive.hh"
#include "DataCheck.hh"
#include "DeratingFactors.hh"
#include "Sdc.hh"
#include "Fuzzy.hh"
#include "StaState.hh"
#include "Corner.hh"
#include "WriteSdcPvt.hh"

namespace sta {

typedef Set<ClockSense*> ClockSenseSet;
typedef Vector<ClockSense*> ClockSenseSeq;

static const char *
transRiseFallFlag(const RiseFall *rf);
static const char *
transRiseFallFlag(const RiseFallBoth *rf);
static const char *
minMaxFlag(const MinMaxAll *min_max);
static const char *
minMaxFlag(const MinMax *min_max);
static const char *
earlyLateFlag(const MinMax *early_late);
static const char *
setupHoldFlag(const MinMax *min_max);
static const char *
timingDerateTypeKeyword(TimingDerateType type);

////////////////////////////////////////////////////////////////

class WriteSdcObject
{
public:
  WriteSdcObject() {}
  virtual ~WriteSdcObject() {}
  virtual void write() const = 0;
};

class WriteGetPort : public WriteSdcObject
{
public:
  WriteGetPort(const Port *port,
	       const WriteSdc *writer);
  virtual void write() const;

private:
  const Port *port_;
  const WriteSdc *writer_;
};

WriteGetPort::WriteGetPort(const Port *port,
                           const WriteSdc *writer) :
  port_(port),
  writer_(writer)
{
}

void
WriteGetPort::write() const
{
  writer_->writeGetPort(port_);
}

class WriteGetPinAndClkKey : public WriteSdcObject
{
public:
  WriteGetPinAndClkKey(const Pin *pin,
		       bool map_hpin_to_drvr,
		       const Clock *clk,
		       const WriteSdc *writer);
  virtual void write() const;

private:
  const Pin *pin_;
  bool map_hpin_to_drvr_;
  const Clock *clk_;
  const WriteSdc *writer_;
};

WriteGetPinAndClkKey::WriteGetPinAndClkKey(const Pin *pin,
					   bool map_hpin_to_drvr,
					   const Clock *clk,
					   const WriteSdc *writer) :
  pin_(pin),
  map_hpin_to_drvr_(map_hpin_to_drvr),
  clk_(clk),
  writer_(writer)
{
}

void
WriteGetPinAndClkKey::write() const
{
  writer_->writeClockKey(clk_);
  gzprintf(writer_->stream(), " ");
  writer_->writeGetPin(pin_, map_hpin_to_drvr_);
}

class WriteGetPin : public WriteSdcObject
{
public:
  WriteGetPin(const Pin *pin,
	      bool map_hpin_to_drvr,
	      const WriteSdc *writer);
  virtual void write() const;

private:
  const Pin *pin_;
  bool map_hpin_to_drvr_;
  const WriteSdc *writer_;
};

WriteGetPin::WriteGetPin(const Pin *pin,
			 bool map_hpin_to_drvr,
                         const WriteSdc *writer) :
  pin_(pin),
  map_hpin_to_drvr_(map_hpin_to_drvr),
  writer_(writer)
{
}

void
WriteGetPin::write() const
{
  writer_->writeGetPin(pin_, map_hpin_to_drvr_);
}

class WriteGetNet : public WriteSdcObject
{
public:
  WriteGetNet(const Net *net,
	      const WriteSdc *writer);
  virtual void write() const;

private:
  const Net *net_;
  const WriteSdc *writer_;
};

WriteGetNet::WriteGetNet(const Net *net,
			 const WriteSdc *writer) :
  net_(net),
  writer_(writer)
{
}

void
WriteGetNet::write() const
{
  writer_->writeGetNet(net_);
}

class WriteGetInstance : public WriteSdcObject
{
public:
  WriteGetInstance(const Instance *inst,
		   const WriteSdc *writer);
  virtual void write() const;

private:
  const Instance *inst_;
  const WriteSdc *writer_;
};

WriteGetInstance::WriteGetInstance(const Instance *inst,
				   const WriteSdc *writer) :
  inst_(inst),
  writer_(writer)
{
}

void
WriteGetInstance::write() const
{
  writer_->writeGetInstance(inst_);
}

class WriteGetLibCell : public WriteSdcObject
{
public:
  WriteGetLibCell(const LibertyCell *cell,
		  const WriteSdc *writer);
  virtual void write() const;

private:
  const LibertyCell *cell_;
  const WriteSdc *writer_;
};

WriteGetLibCell::WriteGetLibCell(const LibertyCell *cell,
				 const WriteSdc *writer) :
  cell_(cell),
  writer_(writer)
{
}

void
WriteGetLibCell::write() const
{
  writer_->writeGetLibCell(cell_);
}

class WriteGetClock : public WriteSdcObject
{
public:
  WriteGetClock(const Clock *clk,
		const WriteSdc *writer);
  virtual void write() const;

private:
  const Clock *clk_;
  const WriteSdc *writer_;
};

WriteGetClock::WriteGetClock(const Clock *clk,
			     const WriteSdc *writer) :
  clk_(clk),
  writer_(writer)
{
}

void
WriteGetClock::write() const
{
  writer_->writeGetClock(clk_);
}

////////////////////////////////////////////////////////////////

void
writeSdc(Instance *instance,
	 const char *filename,
	 const char *creator,
	 bool map_hpins,
	 bool native,
	 int digits,
         bool gzip,
	 bool no_timestamp,
	 Sdc *sdc)
{
  WriteSdc writer(instance, creator, map_hpins, native,
		  digits, no_timestamp, sdc);
  writer.write(filename, gzip);
}

WriteSdc::WriteSdc(Instance *instance,
		   const char *creator,
		   bool map_hpins,
		   bool native,
		   int digits,
		   bool no_timestamp,
		   Sdc *sdc) :
  StaState(sdc),
  instance_(instance),
  creator_(creator),
  map_hpins_(map_hpins),
  native_(native),
  digits_(digits),
  no_timestamp_(no_timestamp),
  top_instance_(instance == sdc_network_->topInstance()),
  instance_name_length_(strlen(sdc_network_->pathName(instance))),
  cell_(sdc_network_->cell(instance))
{
}

WriteSdc::~WriteSdc()
{
}

void
WriteSdc::write(const char *filename,
                bool gzip)
{
  openFile(filename, gzip);
  writeHeader();
  writeTiming();
  writeEnvironment();
  writeDesignRules();
  writeVariables();
  closeFile();
}

void
WriteSdc::openFile(const char *filename,
                   bool gzip)
{
  stream_ = gzopen(filename, gzip ? "wb" : "wT");
  if (stream_ == nullptr)
    throw FileNotWritable(filename);
}

void
WriteSdc::closeFile()
{
  gzclose(stream_);
}

void
WriteSdc::writeHeader() const
{
  writeCommentSeparator();
  gzprintf(stream_, "# Created by %s\n", creator_);
  if (!no_timestamp_) {
    time_t now;
    time(&now);
    char *time_str = ctime(&now);
    // Remove trailing \n.
    time_str[strlen(time_str) - 1] = '\0';
    gzprintf(stream_, "# %s\n", time_str);
  }
  writeCommentSeparator();

  gzprintf(stream_, "current_design %s\n", sdc_network_->name(cell_));
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeTiming() const
{
  writeCommentSection("Timing Constraints");
  writeClocks();
  writePropagatedClkPins();
  writeClockUncertaintyPins();
  writeClockLatencies();
  writeClockInsertions();
  writeInterClockUncertainties();
  writeClockSenses();
  writeClockGroups();
  writeInputDelays();
  writeOutputDelays();
  writeDisables();
  writeExceptions();
  writeDataChecks();
}

void
WriteSdc::writeClocks() const
{
  // Write clocks in the order they were defined because generated
  // clocks depend on master clocks having been previously defined.
  for (auto clk : sdc_->clocks_) {
    if (clk->isGenerated())
      writeGeneratedClock(clk);
    else
      writeClock(clk);
    writeClockSlews(clk);
    writeClockUncertainty(clk);
    if (clk->isPropagated()) {
      gzprintf(stream_, "set_propagated_clock ");
      writeGetClock(clk);
      gzprintf(stream_, "\n");
    }
  }
}

void
WriteSdc::writeClock(Clock *clk) const
{
  gzprintf(stream_, "create_clock -name %s",
	  clk->name());
  if (clk->addToPins())
    gzprintf(stream_, " -add");
  gzprintf(stream_, " -period ");
  float period = clk->period();
  writeTime(period);
  FloatSeq *waveform = clk->waveform();
  if (!(waveform->size() == 2
        && (*waveform)[0] == 0.0
        && fuzzyEqual((*waveform)[1], period / 2.0))) {
    gzprintf(stream_, " -waveform ");
    writeFloatSeq(waveform, scaleTime(1.0));
  }
  writeCmdComment(clk);
  gzprintf(stream_, " ");
  writeClockPins(clk);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeGeneratedClock(Clock *clk) const
{
  gzprintf(stream_, "create_generated_clock -name %s",
	  clk->name());
  if (clk->addToPins())
    gzprintf(stream_, " -add");
  gzprintf(stream_, " -source ");
  writeGetPin(clk->srcPin(), true);
  Clock *master = clk->masterClk();
  if (master && !clk->masterClkInfered()) {
    gzprintf(stream_, " -master_clock ");
    writeGetClock(master);
  }
  if (clk->combinational())
    gzprintf(stream_, " -combinational");
  int divide_by = clk->divideBy();
  if (divide_by != 0)
    gzprintf(stream_, " -divide_by %d", divide_by);
  int multiply_by = clk->multiplyBy();
  if (multiply_by != 0)
    gzprintf(stream_, " -multiply_by %d", multiply_by);
  float duty_cycle = clk->dutyCycle();
  if (duty_cycle != 0.0) {
    gzprintf(stream_, " -duty_cycle ");
    writeFloat(duty_cycle);
  }
  if (clk->invert())
    gzprintf(stream_, " -invert");
  IntSeq *edges = clk->edges();
  if (edges && !edges->empty()) {
    gzprintf(stream_, " -edges ");
    writeIntSeq(edges);
    FloatSeq *edge_shifts = clk->edgeShifts();
    if (edge_shifts && !edge_shifts->empty()) {
      gzprintf(stream_, " -edge_shift ");
      writeFloatSeq(edge_shifts, scaleTime(1.0));
    }
  }
  writeCmdComment(clk);
  gzprintf(stream_, " ");
  writeClockPins(clk);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeClockPins(const Clock *clk) const
{
  const PinSet &pins = clk->pins();
  if (!pins.empty()) {
    if (pins.size() > 1)
      gzprintf(stream_, "\\\n    ");
    writeGetPins(&pins, true);
  }
}

void
WriteSdc::writeClockSlews(const Clock *clk) const
{
  WriteGetClock write_clk(clk, this);
  const RiseFallMinMax slews = clk->slews();
  if (slews.hasValue())
    writeRiseFallMinMaxTimeCmd("set_clock_transition", &slews, write_clk);
}

void
WriteSdc::writeClockUncertainty(const Clock *clk) const
{
  float setup;
  bool setup_exists;
  clk->uncertainty(SetupHold::max(), setup, setup_exists);
  float hold;
  bool hold_exists;
  clk->uncertainty(SetupHold::min(), hold, hold_exists);
  if (setup_exists && hold_exists && setup == hold)
    writeClockUncertainty(clk, "", setup);
  else {
    if (setup_exists)
      writeClockUncertainty(clk, "-setup ", setup);
    if (hold_exists)
      writeClockUncertainty(clk, "-hold ", hold);
  }
}

void
WriteSdc::writeClockUncertainty(const Clock *clk,
				const char *setup_hold,
				float value) const
{
  gzprintf(stream_, "set_clock_uncertainty %s", setup_hold);
  writeTime(value);
  gzprintf(stream_, " %s\n", clk->name());
}

void
WriteSdc::writeClockUncertaintyPins() const
{
  PinClockUncertaintyMap::Iterator iter(sdc_->pin_clk_uncertainty_map_);
  while (iter.hasNext()) {
    const Pin *pin;
    ClockUncertainties *uncertainties;
    iter.next(pin, uncertainties);
    writeClockUncertaintyPin(pin, uncertainties);
  }
}

void
WriteSdc::writeClockUncertaintyPin(const Pin *pin,
				   ClockUncertainties *uncertainties)
  const
{
  float setup;
  bool setup_exists;
  uncertainties->value(SetupHold::max(), setup, setup_exists);
  float hold;
  bool hold_exists;
  uncertainties->value(SetupHold::min(), hold, hold_exists);
  if (setup_exists && hold_exists && setup == hold)
    writeClockUncertaintyPin(pin, "",  setup);
  else {
    if (setup_exists)
      writeClockUncertaintyPin(pin, "-setup ",  setup);
    if (hold_exists)
      writeClockUncertaintyPin(pin, "-hold ",  hold);
  }
}
void
WriteSdc::writeClockUncertaintyPin(const Pin *pin,
				   const char *setup_hold,
				   float value) const
{
  gzprintf(stream_, "set_clock_uncertainty %s", setup_hold);
  writeTime(value);
  gzprintf(stream_, " ");
  writeGetPin(pin, true);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeClockLatencies() const
{
  ClockLatencies::Iterator latency_iter(sdc_->clockLatencies());
  while (latency_iter.hasNext()) {
    ClockLatency *latency = latency_iter.next();
    const Pin *pin = latency->pin();
    const Clock *clk = latency->clock();
    if (pin && clk) {
      WriteGetPinAndClkKey write_pin(pin, true, clk, this);
      writeRiseFallMinMaxTimeCmd("set_clock_latency", latency->delays(),
				 write_pin);
    }
    else if (pin) {
      WriteGetPin write_pin(pin, true, this);
      writeRiseFallMinMaxTimeCmd("set_clock_latency", latency->delays(),
				 write_pin);
    }
    else if (clk) {
      WriteGetClock write_clk(clk, this);
      writeRiseFallMinMaxTimeCmd("set_clock_latency", latency->delays(),
				 write_clk);
    }
  }
}

void
WriteSdc::writeClockInsertions() const
{
  for (ClockInsertion *insert : sdc_->clockInsertions()) {
    const Pin *pin = insert->pin();
    const Clock *clk = insert->clock();
    if (pin && clk) {
      WriteGetPinAndClkKey write_pin_clk(pin, true, clk, this);
      writeClockInsertion(insert, write_pin_clk);
    }
    else if (pin) {
      WriteGetPin write_pin(pin, true, this);
      writeClockInsertion(insert, write_pin);
    }
    else if (clk) {
      WriteGetClock write_clk(clk, this);
      writeClockInsertion(insert, write_clk);
    }
  }
}

void
WriteSdc::writeClockInsertion(ClockInsertion *insert,
			      WriteSdcObject &write_obj) const
{
  RiseFallMinMax *early_values = insert->delays(EarlyLate::early());
  RiseFallMinMax *late_values = insert->delays(EarlyLate::late());
  if (early_values->equal(late_values))
    writeRiseFallMinMaxTimeCmd("set_clock_latency -source",
			       late_values, write_obj);
  else {
    writeRiseFallMinMaxTimeCmd("set_clock_latency -source -early",
			       early_values, write_obj);
    writeRiseFallMinMaxTimeCmd("set_clock_latency -source -late",
			       late_values, write_obj);
  }
}

void
WriteSdc::writePropagatedClkPins() const
{
  PinSet::Iterator pin_iter(sdc_->propagated_clk_pins_);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    gzprintf(stream_, "set_propagated_clock ");
    writeGetPin(pin, true);
    gzprintf(stream_, "\n");
  }
}

void
WriteSdc::writeInterClockUncertainties() const
{
  InterClockUncertaintySet::Iterator
    uncertainty_iter(sdc_->inter_clk_uncertainties_);
  while (uncertainty_iter.hasNext()) {
    InterClockUncertainty *uncertainty = uncertainty_iter.next();
    writeInterClockUncertainty(uncertainty);
  }
}

void
WriteSdc::
writeInterClockUncertainty(InterClockUncertainty *uncertainty) const
{
  const Clock *src_clk = uncertainty->src();
  const Clock *tgt_clk = uncertainty->target();
  const RiseFallMinMax *src_rise =
    uncertainty->uncertainties(RiseFall::rise());
  const RiseFallMinMax *src_fall =
    uncertainty->uncertainties(RiseFall::fall());
  float value;
  if (src_rise->equal(src_fall)
      && src_rise->isOneValue(value)) {
    gzprintf(stream_, "set_clock_uncertainty -from ");
    writeGetClock(src_clk);
    gzprintf(stream_, " -to ");
    writeGetClock(tgt_clk);
    gzprintf(stream_, " ");
    writeTime(value);
    gzprintf(stream_, "\n");
  }
  else {
    for (auto src_rf : RiseFall::range()) {
      for (auto tgt_rf : RiseFall::range()) {
	for (auto setup_hold : SetupHold::range()) {
	  float value;
	  bool exists;
	  sdc_->clockUncertainty(src_clk, src_rf, tgt_clk, tgt_rf,
					 setup_hold, value, exists);
	  if (exists) {
	    gzprintf(stream_, "set_clock_uncertainty -%s_from ",
		    src_rf == RiseFall::rise() ? "rise" : "fall");
	    writeGetClock(uncertainty->src());
	    gzprintf(stream_, " -%s_to ",
		    tgt_rf == RiseFall::rise() ? "rise" : "fall");
	    writeGetClock(uncertainty->target());
	    gzprintf(stream_, " %s ",
		    setupHoldFlag(setup_hold));
	    writeTime(value);
	    gzprintf(stream_, "\n");
	  }
	}
      }
    }
  }
}

void
WriteSdc::writeInputDelays() const
{
  // Sort arrivals by pin and clock name.
  PortDelaySeq delays;
  for (InputDelay *input_delay : sdc_->inputDelays())
    delays.push_back(input_delay);

  PortDelayLess port_delay_less(sdc_network_);
  sort(delays, port_delay_less);

  PortDelaySeq::Iterator delay_iter(delays);
  while (delay_iter.hasNext()) {
    PortDelay *input_delay = delay_iter.next();
    writePortDelay(input_delay, true, "set_input_delay");
  }
}

void
WriteSdc::writeOutputDelays() const
{
  // Sort departures by pin and clock name.
  PortDelaySeq delays;
  for (OutputDelay *output_delay : sdc_->outputDelays())
    delays.push_back(output_delay);

  sort(delays, PortDelayLess(sdc_network_));

  for (PortDelay *output_delay : delays) 
    writePortDelay(output_delay, false, "set_output_delay");
}

void
WriteSdc::writePortDelay(PortDelay *port_delay,
			 bool is_input_delay,
			 const char *sdc_cmd) const
{
  RiseFallMinMax *delays = port_delay->delays();
  float rise_min, rise_max, fall_min, fall_max;
  bool rise_min_exists, rise_max_exists, fall_min_exists, fall_max_exists;
  delays->value(RiseFall::rise(), MinMax::min(),
		rise_min, rise_min_exists);
  delays->value(RiseFall::rise(), MinMax::max(),
		rise_max, rise_max_exists);
  delays->value(RiseFall::fall(), MinMax::min(),
		fall_min, fall_min_exists);
  delays->value(RiseFall::fall(), MinMax::max(),
		fall_max, fall_max_exists);
  // Try to compress the four port delays.
  if (rise_min_exists
      && rise_max_exists
      && fall_min_exists
      && fall_max_exists
      && rise_max == rise_min
      && fall_min == rise_min
      && fall_max == rise_min)
    writePortDelay(port_delay, is_input_delay, rise_min,
		   RiseFallBoth::riseFall(), MinMaxAll::all(), sdc_cmd);
  else if (rise_min_exists
	   && rise_max_exists
	   && rise_max == rise_min
	   && fall_min_exists
	   && fall_max_exists
	   && fall_min == fall_max) {
    writePortDelay(port_delay, is_input_delay, rise_min,
		   RiseFallBoth::rise(), MinMaxAll::all(), sdc_cmd);
    writePortDelay(port_delay, is_input_delay, fall_min,
		   RiseFallBoth::fall(), MinMaxAll::all(), sdc_cmd);
  }
  else if (rise_min_exists
	   && fall_min_exists
	   && rise_min == fall_min
	   && rise_max_exists
	   && fall_max_exists
	   && rise_max == fall_max) {
    writePortDelay(port_delay, is_input_delay, rise_min,
		   RiseFallBoth::riseFall(), MinMaxAll::min(), sdc_cmd);
    writePortDelay(port_delay, is_input_delay, rise_max,
		   RiseFallBoth::riseFall(), MinMaxAll::max(), sdc_cmd);
  }
  else {
    if (rise_min_exists)
      writePortDelay(port_delay, is_input_delay, rise_min,
		     RiseFallBoth::rise(), MinMaxAll::min(), sdc_cmd);
    if (rise_max_exists)
      writePortDelay(port_delay, is_input_delay, rise_max,
		     RiseFallBoth::rise(), MinMaxAll::max(), sdc_cmd);
    if (fall_min_exists)
      writePortDelay(port_delay, is_input_delay, fall_min,
		     RiseFallBoth::fall(), MinMaxAll::min(), sdc_cmd);
    if (fall_max_exists)
      writePortDelay(port_delay, is_input_delay, fall_max,
		     RiseFallBoth::fall(), MinMaxAll::max(), sdc_cmd);
  }
}

void
WriteSdc::writePortDelay(PortDelay *port_delay,
			 bool is_input_delay,
			 float delay,
			 const RiseFallBoth *rf,
			 const MinMaxAll *min_max,
			 const char *sdc_cmd) const
{
  gzprintf(stream_, "%s ", sdc_cmd);
  writeTime(delay);
  const ClockEdge *clk_edge = port_delay->clkEdge();
  if (clk_edge) {
    writeClockKey(clk_edge->clock());
    if (clk_edge->transition() == RiseFall::fall())
      gzprintf(stream_, " -clock_fall");
  }
  gzprintf(stream_, "%s%s -add_delay ",
	  transRiseFallFlag(rf),
	  minMaxFlag(min_max));
  const Pin *ref_pin = port_delay->refPin();
  if (ref_pin) {
    gzprintf(stream_, "-reference_pin ");
    writeGetPin(ref_pin, true);
    gzprintf(stream_, " ");
  }
  writeGetPin(port_delay->pin(), is_input_delay);
  gzprintf(stream_, "\n");
}

class PinClockPairNameLess
{
public:
  PinClockPairNameLess(const Network *network);
  bool operator()(const PinClockPair &pin_clk1,
		  const PinClockPair &pin_clk2) const;

private:
  PinPathNameLess pin_less_;
};

PinClockPairNameLess::PinClockPairNameLess(const Network *network) :
  pin_less_(network)
{
}

bool
PinClockPairNameLess::operator()(const PinClockPair &pin_clk1,
				 const PinClockPair &pin_clk2) const
{
  const Pin *pin1 = pin_clk1.first;
  const Pin *pin2 = pin_clk2.first;
  const Clock *clk1 = pin_clk1.second;
  const Clock *clk2 = pin_clk2.second;
  return pin_less_(pin1, pin2)
    || (pin1 == pin2
	&& ((clk1 == nullptr && clk2)
	    || (clk1 && clk2
		&& clk1->index() < clk2->index())));
}

void
WriteSdc::writeClockSenses() const
{
  Vector<PinClockPair> pin_clks;
  for (auto iter : sdc_->clk_sense_map_)
    pin_clks.push_back(iter.first);

  // Sort by pin/clk pair so regressions results are stable.
  sort(pin_clks, PinClockPairNameLess(sdc_network_));
  
  for (auto pin_clk : pin_clks) {
    ClockSense sense;
    bool exists;
    sdc_->clk_sense_map_.findKey(pin_clk, sense, exists);
    if (exists)
      writeClockSense(pin_clk, sense);
  }
}

void
WriteSdc::writeClockSense(PinClockPair &pin_clk,
			  ClockSense sense) const
{
  const char *flag = nullptr;
  if (sense == ClockSense::positive)
    flag = "-positive";
  else if (sense == ClockSense::negative)
    flag = "-negative";
  else if (sense == ClockSense::stop)
    flag = "-stop_propagation";
  gzprintf(stream_, "set_sense -type clock %s ", flag);
  const Clock *clk = pin_clk.second;
  if (clk) {
    gzprintf(stream_, "-clock ");
    writeGetClock(clk);
    gzprintf(stream_, " ");
  }
  writeGetPin(pin_clk.first, true);
  gzprintf(stream_, "\n");
}

class ClockGroupLess
{
public:
  bool operator()(const ClockGroup *clk_group1,
		  const ClockGroup *clk_group2) const;
};


bool
ClockGroupLess::operator()(const ClockGroup *clk_group1,
			   const ClockGroup *clk_group2) const
{
  size_t size1 = clk_group1->size();
  size_t size2 = clk_group2->size();
  if (size1 < size2)
    return true;
  else if (size1 > size2)
    return false;
  else {
    ClockSeq clks1;
    for (Clock *clk1 : *clk_group1)
      clks1.push_back(clk1);
    sort(clks1, ClockNameLess());

    ClockSeq clks2;
    for (Clock *clk2 : *clk_group2)
      clks2.push_back(clk2);
    sort(clks2, ClockNameLess());

    ClockSeq::Iterator clk_iter3(clks1);
    ClockSeq::Iterator clk_iter4(clks2);
    while (clk_iter3.hasNext()
	   && clk_iter4.hasNext()) {
      Clock *clk1 = clk_iter3.next();
      Clock *clk2 = clk_iter4.next();
      int cmp = strcmp(clk1->name(), clk2->name());
      if (cmp < 0)
	return true;
      else if (cmp > 0)
	return false;
    }
    return false;
  }
}

void
WriteSdc::writeClockGroups() const
{
  for (auto &name_groups : sdc_->clk_groups_name_map_) {
    ClockGroups *clk_groups = name_groups.second;
    writeClockGroups(clk_groups);
  }
}

void
WriteSdc::writeClockGroups(ClockGroups *clk_groups) const
{
  gzprintf(stream_, "set_clock_groups -name %s ", clk_groups->name());
  if (clk_groups->logicallyExclusive())
    gzprintf(stream_, "-logically_exclusive \\\n");
  else if (clk_groups->physicallyExclusive())
    gzprintf(stream_, "-physically_exclusive \\\n");
  else if (clk_groups->asynchronous())
    gzprintf(stream_, "-asynchronous \\\n");
  if (clk_groups->allowPaths())
    gzprintf(stream_, "-allow_paths \\\n");
  Vector<ClockGroup*> groups;
  ClockGroupSet::Iterator group_iter1(clk_groups->groups());
  while (group_iter1.hasNext()) {
    ClockGroup *clk_group = group_iter1.next();
    groups.push_back(clk_group);
  }
  sort(groups, ClockGroupLess());
  bool first = true;
  Vector<ClockGroup*>::Iterator group_iter2(groups);
  while (group_iter2.hasNext()) {
    ClockGroup *clk_group = group_iter2.next();
    if (!first)
      gzprintf(stream_, "\\\n");
    gzprintf(stream_, " -group ");
    writeGetClocks(clk_group);
    first = false;
  }
  writeCmdComment(clk_groups);
  gzprintf(stream_, "\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeDisables() const
{
  writeDisabledCells();
  writeDisabledPorts();
  writeDisabledLibPorts();
  writeDisabledInstances();
  writeDisabledPins();
  writeDisabledEdges();
}

void
WriteSdc::writeDisabledCells() const
{
  DisabledCellPortsSeq disables = sortByName(sdc_->disabledCellPorts());
  for (const DisabledCellPorts *disable : disables) {
    const LibertyCell *cell = disable->cell();
    if (disable->all()) {
      gzprintf(stream_, "set_disable_timing ");
      writeGetLibCell(cell);
      gzprintf(stream_, "\n");
    }
    if (disable->fromTo()) {
      LibertyPortPairSeq from_tos = sortByName(disable->fromTo());
      for (const LibertyPortPair &from_to : from_tos) {
	const LibertyPort *from = from_to.first;
	const LibertyPort *to = from_to.second;
	gzprintf(stream_, "set_disable_timing -from {%s} -to {%s} ",
		from->name(),
		to->name());
	writeGetLibCell(cell);
	gzprintf(stream_, "\n");
      }
    }
    if (disable->from()) {
      LibertyPortSeq from = sortByName(disable->from());
      for (const LibertyPort *from_port : from) {
	gzprintf(stream_, "set_disable_timing -from {%s} ",
		from_port->name());
	writeGetLibCell(cell);
	gzprintf(stream_, "\n");
      }
    }
    if (disable->to()) {
      LibertyPortSeq to = sortByName(disable->to());
      for (const LibertyPort *to_port : to) {
	gzprintf(stream_, "set_disable_timing -to {%s} ",
		to_port->name());
	writeGetLibCell(cell);
	gzprintf(stream_, "\n");
      }
    }
    if (disable->timingArcSets()) {
      // The only syntax to disable timing arc sets disables all of the
      // cell's timing arc sets.
      gzprintf(stream_, "set_disable_timing ");
      writeGetTimingArcsOfOjbects(cell);
      gzprintf(stream_, "\n");
    }
  }
}

void
WriteSdc::writeDisabledPorts() const
{
  const PortSeq ports = sortByName(sdc_->disabledPorts(), sdc_network_);
  for (const Port *port : ports) {
    gzprintf(stream_, "set_disable_timing ");
    writeGetPort(port);
    gzprintf(stream_, "\n");
  }
}

void
WriteSdc::writeDisabledLibPorts() const
{
  LibertyPortSeq ports = sortByName(sdc_->disabledLibPorts());
  for (LibertyPort *port : ports) {
    gzprintf(stream_, "set_disable_timing ");
    writeGetLibPin(port);
    gzprintf(stream_, "\n");
  }
}

void
WriteSdc::writeDisabledInstances() const
{
  DisabledInstancePortsSeq disables = sortByPathName(sdc_->disabledInstancePorts(),
                                                     sdc_network_);
  for (DisabledInstancePorts *disable : disables) {
    Instance *inst = disable->instance();
    if (disable->all()) {
      gzprintf(stream_, "set_disable_timing ");
      writeGetInstance(inst);
      gzprintf(stream_, "\n");
    }
    else if (disable->fromTo()) {
      LibertyPortPairSeq from_tos = sortByName(disable->fromTo());
      for (LibertyPortPair &from_to : from_tos) {
	const LibertyPort *from_port = from_to.first;
	const LibertyPort *to_port = from_to.second;
	gzprintf(stream_, "set_disable_timing -from {%s} -to {%s} ",
		from_port->name(),
		to_port->name());
	writeGetInstance(inst);
	gzprintf(stream_, "\n");
      }
    }
    if (disable->from()) {
      LibertyPortSeq from = sortByName(disable->from());
      for (const LibertyPort *from_port : from) {
	gzprintf(stream_, "set_disable_timing -from {%s} ",
		from_port->name());
	writeGetInstance(inst);
	gzprintf(stream_, "\n");
      }
    }
    if (disable->to()) {
      LibertyPortSeq to = sortByName(disable->to());
      for (const LibertyPort *to_port : to) {
	gzprintf(stream_, "set_disable_timing -to {%s} ",
		to_port->name());
	writeGetInstance(inst);
	gzprintf(stream_, "\n");
      }
    }
  }
}

void
WriteSdc::writeDisabledPins() const
{
  PinSeq pins = sortByPathName(sdc_->disabledPins(), sdc_network_);
  for (const Pin *pin : pins) {
    gzprintf(stream_, "set_disable_timing ");
    writeGetPin(pin, false);
    gzprintf(stream_, "\n");
  }
}

void
WriteSdc::writeDisabledEdges() const
{
  EdgeSeq edges;
  for (Edge *edge : *sdc_->disabledEdges())
    edges.push_back(edge);
  sortEdges(&edges, sdc_network_, graph_);
  for (Edge *edge : edges) {
    EdgeSet matches;
    findMatchingEdges(edge, matches);
    if (matches.size() == 1)
      writeDisabledEdge(edge);
    else if (edgeSenseIsUnique(edge, matches))
      writeDisabledEdgeSense(edge);
  }
}

void
WriteSdc::findMatchingEdges(Edge *edge,
			    EdgeSet &matches) const
{
  Vertex *from_vertex = edge->from(graph_);
  Vertex *to_vertex = edge->to(graph_);
  Pin *to_pin = to_vertex->pin();
  VertexOutEdgeIterator edge_iter(from_vertex, graph_);
  while (edge_iter.hasNext()) {
    Edge *out_edge = edge_iter.next();
    if (out_edge->to(graph_)->pin() == to_pin)
      matches.insert(out_edge);
  }
}

bool
WriteSdc::edgeSenseIsUnique(Edge *edge,
			    EdgeSet &matches) const
{
  for (Edge *match : matches) {
    if (match != edge
	&& match->sense() == edge->sense())
      return false;
  }
  return true;
}

void
WriteSdc::writeDisabledEdge(Edge *edge) const
{
  gzprintf(stream_, "set_disable_timing ");
  writeGetTimingArcs(edge);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeDisabledEdgeSense(Edge *edge) const
{
  gzprintf(stream_, "set_disable_timing ");
  const char *sense = timingSenseString(edge->sense());
  string filter;
  stringPrint(filter, "sense == %s", sense);
  writeGetTimingArcs(edge, filter.c_str());
  gzprintf(stream_, "\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeExceptions() const
{
  ExceptionPathSeq exceptions;
  for (ExceptionPath *exception : *sdc_->exceptions())
    exceptions.push_back(exception);
  sort(exceptions, ExceptionPathLess(network_));
  for (ExceptionPath *exception : exceptions) {
    if (!exception->isFilter()
	&& !exception->isLoop())
      writeException(exception);
  }
}

void
WriteSdc::writeException(ExceptionPath *exception) const
{
  writeExceptionCmd(exception);
  if (exception->from())
    writeExceptionFrom(exception->from());
  if (exception->thrus()) {
    for (ExceptionThru *thru : *exception->thrus())
      writeExceptionThru(thru);
  }
  if (exception->to())
    writeExceptionTo(exception->to());
  writeExceptionValue(exception);
  writeCmdComment(exception);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeExceptionCmd(ExceptionPath *exception) const
{
  if (exception->isFalse()) {
    gzprintf(stream_, "set_false_path");
    writeSetupHoldFlag(exception->minMax());
  }
  else if (exception->isMultiCycle()) {
    gzprintf(stream_, "set_multicycle_path");
    const MinMaxAll *min_max = exception->minMax();
    writeSetupHoldFlag(min_max);
    if (min_max == MinMaxAll::min()) {
      // For hold MCPs default is -start.
      if (exception->useEndClk())
	gzprintf(stream_, " -end");
    }
    else {
      // For setup MCPs default is -end.
      if (!exception->useEndClk())
	gzprintf(stream_, " -start");
    }
  }
  else if (exception->isPathDelay()) {
    if (exception->minMax() == MinMaxAll::max())
      gzprintf(stream_, "set_max_delay");
    else
      gzprintf(stream_, "set_min_delay");
    if (exception->ignoreClkLatency())
      gzprintf(stream_, " -ignore_clock_latency");
  }
  else if (exception->isGroupPath()) {
    if (exception->isDefault())
      gzprintf(stream_, "group_path -default");
    else
      gzprintf(stream_, "group_path -name %s", exception->name());
  }
  else
    report_->critical(214, "unknown exception type");
}

void
WriteSdc::writeExceptionValue(ExceptionPath *exception) const
{
  if (exception->isMultiCycle())
    gzprintf(stream_, " %d",
	    exception->pathMultiplier());
  else if (exception->isPathDelay()) {
    gzprintf(stream_, " ");
    writeTime(exception->delay());
  }
}

void
WriteSdc::writeExceptionFrom(ExceptionFrom *from) const
{
  writeExceptionFromTo(from, "from", true);
}

void
WriteSdc::writeExceptionTo(ExceptionTo *to) const
{
  const RiseFallBoth *end_rf = to->endTransition();
  if (end_rf != RiseFallBoth::riseFall())
    gzprintf(stream_, "%s ", transRiseFallFlag(end_rf));
  if (to->hasObjects())
    writeExceptionFromTo(to, "to", false);
}

void
WriteSdc::writeExceptionFromTo(ExceptionFromTo *from_to,
			       const char *from_to_key,
			       bool map_hpin_to_drvr) const
{
  const RiseFallBoth *rf = from_to->transition();
  const char *rf_prefix = "-";
  if (rf == RiseFallBoth::rise())
    rf_prefix = "-rise_";
  else if (rf == RiseFallBoth::fall())
    rf_prefix = "-fall_";
  gzprintf(stream_, "\\\n    %s%s ", rf_prefix, from_to_key);
  bool multi_objs =
    ((from_to->pins() ? from_to->pins()->size() : 0)
     + (from_to->clks() ? from_to->clks()->size() : 0)
     + (from_to->instances() ? from_to->instances()->size() : 0)) > 1;
  if (multi_objs)
    gzprintf(stream_, "[list ");
  bool first = true;
  if (from_to->pins()) {
    PinSeq pins = sortByPathName(from_to->pins(), sdc_network_);
    for (const Pin *pin : pins) {
      if (multi_objs && !first)
	gzprintf(stream_, "\\\n           ");
      writeGetPin(pin, map_hpin_to_drvr);
      first = false;
    }
  }
  if (from_to->clks())
    writeGetClocks(from_to->clks(), multi_objs, first);
  if (from_to->instances()) {
    InstanceSeq insts = sortByPathName(from_to->instances(), sdc_network_);
    for (const Instance *inst : insts) {
      if (multi_objs && !first)
	gzprintf(stream_, "\\\n           ");
      writeGetInstance(inst);
      first = false;
    }
  }
  if (multi_objs)
    gzprintf(stream_, "]");
}

void
WriteSdc::writeExceptionThru(ExceptionThru *thru) const
{
  const RiseFallBoth *rf = thru->transition();
  const char *rf_prefix = "-";
  if (rf == RiseFallBoth::rise())
    rf_prefix = "-rise_";
  else if (rf == RiseFallBoth::fall())
    rf_prefix = "-fall_";
  gzprintf(stream_, "\\\n    %sthrough ", rf_prefix);
  PinSeq pins;
  mapThruHpins(thru, pins);
  bool multi_objs =
    (pins.size()
     + (thru->nets() ? thru->nets()->size() : 0)
     + (thru->instances() ? thru->instances()->size() : 0)) > 1;
  if (multi_objs)
    gzprintf(stream_, "[list ");
  bool first = true;
  sort(pins, PinPathNameLess(network_));
  for (const Pin *pin : pins) {
    if (multi_objs && !first)
      gzprintf(stream_, "\\\n           ");
    writeGetPin(pin);
    first = false;
  }

  if (thru->nets()) {
    NetSeq nets = sortByPathName(thru->nets(), sdc_network_);
    for (const Net *net : nets) {
      if (multi_objs && !first)
	gzprintf(stream_, "\\\n           ");
      writeGetNet(net);
      first = false;
    }
  }
  if (thru->instances()) {
    InstanceSeq insts = sortByPathName(thru->instances(), sdc_network_);
    for (const Instance *inst : insts) {
      if (multi_objs && !first)
	gzprintf(stream_, "\\\n           ");
      writeGetInstance(inst);
      first = false;
    }
  }
  if (multi_objs)
    gzprintf(stream_, "]");
}

void
WriteSdc::mapThruHpins(ExceptionThru *thru,
		       PinSeq &pins) const
{
  if (thru->pins()) {
    for (const Pin *pin : *thru->pins()) {
      // Map hierarical pins to load pins outside of outputs or inside of inputs.
      if (network_->isHierarchical(pin)) {
	Instance *hinst = network_->instance(pin);
	bool hpin_is_output = network_->direction(pin)->isAnyOutput();
	PinConnectedPinIterator *cpin_iter = network_->connectedPinIterator(pin);
	while (cpin_iter->hasNext()) {
	  const Pin *cpin = cpin_iter->next();
	  if (network_->isLoad(cpin)
	      && ((hpin_is_output
		   && !network_->isInside(network_->instance(cpin), hinst))
		  || (!hpin_is_output
		      && network_->isInside(network_->instance(cpin), hinst))))
	    pins.push_back(cpin);
	}
	delete cpin_iter;
      }
      else
	pins.push_back(pin);
    }
  }
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeDataChecks() const
{
  Vector<DataCheck*> checks;
  for (auto pin_checks : sdc_->data_checks_to_map_) {
    DataCheckSet *checks1 = pin_checks.second;
    for (DataCheck *check : *checks1)
      checks.push_back(check);
  }
  sort(checks, DataCheckLess(sdc_network_));
  for (DataCheck *check : checks)
    writeDataCheck(check);
}

void
WriteSdc::writeDataCheck(DataCheck *check) const
{
  for (auto setup_hold : SetupHold::range()) {
    float margin;
    bool one_value;
    check->marginIsOneValue(setup_hold, margin, one_value);
    if (one_value)
      writeDataCheck(check, RiseFallBoth::riseFall(),
		     RiseFallBoth::riseFall(), setup_hold, margin);
    else {
      for (auto from_rf : RiseFall::range()) {
	for (auto to_rf : RiseFall::range()) {
	  float margin;
	  bool margin_exists;
	  check->margin(from_rf, to_rf, setup_hold, margin, margin_exists);
	  if (margin_exists) {
	    writeDataCheck(check, from_rf->asRiseFallBoth(),
			   to_rf->asRiseFallBoth(), setup_hold, margin);
	  }
	}
      }
    }
  }
}

void
WriteSdc::writeDataCheck(DataCheck *check,
			 RiseFallBoth *from_rf,
			 RiseFallBoth *to_rf,
			 SetupHold *setup_hold,
			 float margin) const
{
  const char *from_key = "-from";
  if (from_rf == RiseFallBoth::rise())
    from_key = "-rise_from";
  else if (from_rf == RiseFallBoth::fall())
    from_key = "-fall_from";
  gzprintf(stream_, "set_data_check %s ", from_key);
  writeGetPin(check->from(), true);
  const char *to_key = "-to";
  if (to_rf == RiseFallBoth::rise())
    to_key = "-rise_to";
  else if (to_rf == RiseFallBoth::fall())
    to_key = "-fall_to";
  gzprintf(stream_, " %s ", to_key);
  writeGetPin(check->to(), false);
  gzprintf(stream_, "%s ",
	  setupHoldFlag(setup_hold));
  writeTime(margin);
  gzprintf(stream_, "\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeEnvironment() const
{
  writeCommentSection("Environment");
  writeOperatingConditions();
  writeWireload();
  writePortLoads();
  writeNetLoads();
  writeDriveResistances();
  writeDrivingCells();
  writeInputTransitions();
  writeNetResistances();
  writeConstants();
  writeCaseAnalysis();
  writeDeratings();
}

void
WriteSdc::writeOperatingConditions() const
{
  OperatingConditions *cond = sdc_->operatingConditions(MinMax::max());
  if (cond)
    gzprintf(stream_, "set_operating_conditions %s\n", cond->name());
}

void
WriteSdc::writeWireload() const
{
  WireloadMode wireload_mode = sdc_->wireloadMode();
  if (wireload_mode != WireloadMode::unknown)
    gzprintf(stream_, "set_wire_load_mode \"%s\"\n",
	    wireloadModeString(wireload_mode));
}

void
WriteSdc::writeNetLoads() const
{
  int corner_index = 0; // missing corner arg
  for (auto net_cap : sdc_->net_wire_cap_maps_[corner_index]) {
    const Net *net = net_cap.first;
    MinMaxFloatValues &caps = net_cap.second;
    float min_cap, max_cap;
    bool min_exists, max_exists;
    caps.value(MinMax::min(), min_cap, min_exists);
    caps.value(MinMax::max(), max_cap, max_exists);
    if (min_exists && max_exists
        && min_cap == max_cap)
      writeNetLoad(net, MinMaxAll::all(), min_cap);
    else {
      if (min_exists)
        writeNetLoad(net, MinMaxAll::min(), min_cap);
      if (max_exists)
        writeNetLoad(net, MinMaxAll::max(), max_cap);
    }
  }
}

void
WriteSdc::writeNetLoad(const Net *net,
		       const MinMaxAll *min_max,
		       float cap) const
{
  gzprintf(stream_, "set_load ");
  gzprintf(stream_, "%s ", minMaxFlag(min_max));
  writeCapacitance(cap);
  gzprintf(stream_, " ");
  writeGetNet(net);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writePortLoads() const
{
  CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    writePortLoads(port);
  }
  delete port_iter;
}

void
WriteSdc::writePortLoads(const Port *port) const
{
  const Corner *corner = corners_->findCorner(0); // missing corner arg
  PortExtCap *ext_cap = sdc_->portExtCap(port, corner);
  if (ext_cap) {
    WriteGetPort write_port(port, this);
    writeRiseFallMinMaxCapCmd("set_load -pin_load",
			      ext_cap->pinCap(),
			      write_port);
    writeRiseFallMinMaxCapCmd("set_load -wire_load",
			      ext_cap->wireCap(),
			      write_port);
    writeMinMaxIntValuesCmd("set_port_fanout_number",
			    ext_cap->fanout(), write_port);
  }
}

void
WriteSdc::writeDriveResistances() const
{
  CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    InputDrive *drive = sdc_->findInputDrive(port);
    if (drive) {
      for (auto rf : RiseFall::range()) {
	if (drive->driveResistanceMinMaxEqual(rf)) {
	  float res;
	  bool exists;
	  drive->driveResistance(rf, MinMax::max(), res, exists);
	  gzprintf(stream_, "set_drive %s ",
		  transRiseFallFlag(rf));
	  writeResistance(res);
	  gzprintf(stream_, " ");
	  writeGetPort(port);
	  gzprintf(stream_, "\n");
	}
	else {
	  for (auto min_max : MinMax::range()) {
	    float res;
	    bool exists;
	    drive->driveResistance(rf, min_max, res, exists);
	    if (exists) {
	      gzprintf(stream_, "set_drive %s %s ",
		      transRiseFallFlag(rf),
		      minMaxFlag(min_max));
	      writeResistance(res);
	      gzprintf(stream_, " ");
	      writeGetPort(port);
	      gzprintf(stream_, "\n");
	    }
	  }
	}
      }
    }
  }
  delete port_iter;
}

void
WriteSdc::writeDrivingCells() const
{
  CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    InputDrive *drive = sdc_->findInputDrive(port);
    if (drive) {
      InputDriveCell *drive_rise_min = drive->driveCell(RiseFall::rise(),
							MinMax::min());
      InputDriveCell *drive_rise_max = drive->driveCell(RiseFall::rise(),
							MinMax::max());
      InputDriveCell *drive_fall_min = drive->driveCell(RiseFall::fall(),
							MinMax::min());
      InputDriveCell *drive_fall_max = drive->driveCell(RiseFall::fall(),
							MinMax::max());
      if (drive_rise_min
	  && drive_rise_max
	  && drive_fall_min
	  && drive_fall_max
	  && drive_rise_min->equal(drive_rise_max)
	  && drive_rise_min->equal(drive_fall_min)
	  && drive_rise_min->equal(drive_fall_max))
	// Only write one set_driving_cell if possible.
	writeDrivingCell(port, drive_rise_min, nullptr, nullptr);
      else {
	if (drive_rise_min
	    && drive_rise_max
	    && drive_rise_min->equal(drive_rise_max))
	  writeDrivingCell(port, drive_rise_min, RiseFall::rise(), nullptr);
	else {
	  if (drive_rise_min)
	    writeDrivingCell(port, drive_rise_min, RiseFall::rise(),
			     MinMax::min());
	  if (drive_rise_max)
	    writeDrivingCell(port, drive_rise_max, RiseFall::rise(),
			     MinMax::max());
	}
	if (drive_fall_min
	    && drive_fall_max
	    && drive_fall_min->equal(drive_fall_max))
	  writeDrivingCell(port, drive_fall_min, RiseFall::fall(), nullptr);
	else {
	  if (drive_fall_min)
	    writeDrivingCell(port, drive_fall_min, RiseFall::fall(),
			     MinMax::min());
	  if (drive_fall_max)
	    writeDrivingCell(port, drive_fall_max, RiseFall::fall(),
			     MinMax::max());
	}
      }
    }
  }
  delete port_iter;
}

void
WriteSdc::writeDrivingCell(Port *port,
			   InputDriveCell *drive_cell,
			   const RiseFall *rf,
			   const MinMax *min_max) const
{
  const LibertyCell *cell = drive_cell->cell();
  const LibertyPort *from_port = drive_cell->fromPort();
  const LibertyPort *to_port = drive_cell->toPort();
  float *from_slews = drive_cell->fromSlews();
  const LibertyLibrary *lib = drive_cell->library();
  gzprintf(stream_, "set_driving_cell");
  if (rf)
    gzprintf(stream_, " %s", transRiseFallFlag(rf));
  if (min_max)
    gzprintf(stream_, " %s", minMaxFlag(min_max));
  // Only write -library if it was specified in the sdc.
  if (lib)
    gzprintf(stream_, " -library %s", lib->name());
  gzprintf(stream_, " -lib_cell %s", cell->name());
  if (from_port)
    gzprintf(stream_, " -from_pin {%s}",
	    from_port->name());
  gzprintf(stream_,
	  " -pin {%s} -input_transition_rise ",
	  to_port->name());
  writeTime(from_slews[RiseFall::riseIndex()]);
  gzprintf(stream_, " -input_transition_fall ");
  writeTime(from_slews[RiseFall::fallIndex()]);
  gzprintf(stream_, " ");
  writeGetPort(port);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeInputTransitions() const
{
  CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    InputDrive *drive = sdc_->findInputDrive(port);
    if (drive) {
      RiseFallMinMax *slews = drive->slews();
      WriteGetPort write_port(port, this);
      writeRiseFallMinMaxTimeCmd("set_input_transition", slews, write_port);
    }
  }
  delete port_iter;
}

void
WriteSdc::writeNetResistances() const
{
  NetSeq nets;
  for (auto net_res : sdc_->netResistances()) {
    const Net *net = net_res.first;
    nets.push_back(net);
  }
  sort(nets, NetPathNameLess(sdc_network_));
  for (const Net *net : nets) {
    float min_res, max_res;
    bool min_exists, max_exists;
    sdc_->resistance(net, MinMax::min(), min_res, min_exists);
    sdc_->resistance(net, MinMax::max(), max_res, max_exists);
    if (min_exists && max_exists
	&& min_res == max_res)
      writeNetResistance(net, MinMaxAll::all(), min_res);
    else {
      if (min_exists)
	writeNetResistance(net, MinMaxAll::min(), min_res);
      if (max_exists)
	writeNetResistance(net, MinMaxAll::max(), max_res);
    }
  }
}

void
WriteSdc::writeNetResistance(const Net *net,
			     const MinMaxAll *min_max,
			     float res) const
{
  gzprintf(stream_, "set_resistance ");
  writeResistance(res);
  gzprintf(stream_, "%s ", minMaxFlag(min_max));
  writeGetNet(net);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeConstants() const
{
  PinSeq pins;
  sortedLogicValuePins(sdc_->logicValues(), pins);
  for (const Pin *pin : pins)
    writeConstant(pin);
}

void
WriteSdc::writeConstant(const Pin *pin) const
{
  const char *cmd = setConstantCmd(pin);
  gzprintf(stream_, "%s ", cmd);
  writeGetPin(pin, false);
  gzprintf(stream_, "\n");
}

const char *
WriteSdc::setConstantCmd(const Pin *pin) const
{
  LogicValue value;
  bool exists;
  sdc_->logicValue(pin, value, exists);
  switch (value) {
  case LogicValue::zero:
    return "set_logic_zero";
  case LogicValue::one:
    return "set_logic_one";
  case LogicValue::unknown:
    return "set_logic_dc";
  case LogicValue::rise:
  case LogicValue::fall:
  default:
    report_->critical(215, "illegal set_logic value");
    return nullptr;
  }
}

void
WriteSdc::writeCaseAnalysis() const
{
  PinSeq pins;
  sortedLogicValuePins(sdc_->caseLogicValues(), pins);
  for (const Pin *pin : pins)
    writeCaseAnalysis(pin);
}


void
WriteSdc::writeCaseAnalysis(const Pin *pin) const
{
  const char *value_str = caseAnalysisValueStr(pin);
  gzprintf(stream_, "set_case_analysis %s ", value_str);
  writeGetPin(pin, false);
  gzprintf(stream_, "\n");
}

const char *
WriteSdc::caseAnalysisValueStr(const Pin *pin) const
{
  LogicValue value;
  bool exists;
  sdc_->caseLogicValue(pin, value, exists);
  switch (value) {
  case LogicValue::zero:
    return "0";
  case LogicValue::one:
    return "1";
  case LogicValue::rise:
    return "rising";
  case LogicValue::fall:
    return "falling";
  case LogicValue::unknown:
  default:
    report_->critical(216, "invalid set_case_analysis value");
    return nullptr;
  }
}

void
WriteSdc::sortedLogicValuePins(LogicValueMap &value_map,
			       PinSeq &pins) const
{
  for (auto pin_value : value_map) {
    const Pin *pin = pin_value.first;
    pins.push_back(pin);
  }
  // Sort pins.
  sort(pins, PinPathNameLess(sdc_network_));
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeDeratings() const
{
  DeratingFactorsGlobal *factors = sdc_->derating_factors_;
  if (factors)
    writeDerating(factors);

  for (auto net_derating : sdc_->net_derating_factors_) {
    const Net *net = net_derating.first;
    DeratingFactorsNet *factors = net_derating.second;
    WriteGetNet write_net(net, this);
    for (auto early_late : EarlyLate::range()) {
      writeDerating(factors, TimingDerateType::net_delay, early_late,
		    &write_net);
    }
  }

  for (auto inst_derating : sdc_->inst_derating_factors_) {
    const Instance *inst = inst_derating.first;
    DeratingFactorsCell *factors = inst_derating.second;
    WriteGetInstance write_inst(inst, this);
    writeDerating(factors, &write_inst);
  }

  for (auto cell_derating : sdc_->cell_derating_factors_) {
    const LibertyCell *cell = cell_derating.first;
    DeratingFactorsCell *factors = cell_derating.second;
    WriteGetLibCell write_cell(cell, this);
    writeDerating(factors, &write_cell);
  }
}

void
WriteSdc::writeDerating(DeratingFactorsGlobal *factors) const
{
  for (auto early_late : EarlyLate::range()) {
    bool delay_is_one_value, check_is_one_value, net_is_one_value;
    float delay_value, check_value, net_value;
    factors->factors(TimingDerateType::cell_delay)->isOneValue(early_late,
							   delay_is_one_value,
							   delay_value);
    factors->factors(TimingDerateType::net_delay)->isOneValue(early_late,
							      net_is_one_value,
							      net_value);
    DeratingFactors *cell_check_factors =
      factors->factors(TimingDerateType::cell_check);
    cell_check_factors->isOneValue(early_late, check_is_one_value, check_value);
    if (delay_is_one_value
	&& net_is_one_value
	&& delay_value == net_value
	&& (!cell_check_factors->hasValue()
	    || (check_is_one_value && check_value == 1.0))) {
      if (delay_value != 1.0) {
	gzprintf(stream_, "set_timing_derate %s ", earlyLateFlag(early_late));
	writeFloat(delay_value);
	gzprintf(stream_, "\n");
      }
    }
    else {
      for (int type_index = 0;
	   type_index < timing_derate_type_count;
	   type_index++) {
	TimingDerateType type = static_cast<TimingDerateType>(type_index);
	DeratingFactors *type_factors = factors->factors(type);
	writeDerating(type_factors, type, early_late, nullptr);
      }
    }
  }
}

void
WriteSdc::writeDerating(DeratingFactorsCell *factors,
			WriteSdcObject *write_obj) const
{
  for (auto early_late : EarlyLate::range()) {
    DeratingFactors *delay_factors=factors->factors(TimingDerateCellType::cell_delay);
    writeDerating(delay_factors, TimingDerateType::cell_delay, early_late, write_obj);
    DeratingFactors *check_factors=factors->factors(TimingDerateCellType::cell_check);
    writeDerating(check_factors, TimingDerateType::cell_check, early_late, write_obj);
  }
}

void
WriteSdc::writeDerating(DeratingFactors *factors,
			TimingDerateType type,
			const MinMax *early_late,
			WriteSdcObject *write_obj) const
{
  const char *type_key = timingDerateTypeKeyword(type);
  bool is_one_value;
  float value;
  factors->isOneValue(early_late, is_one_value, value);
  if (is_one_value) {
    if (value != 1.0) {
      gzprintf(stream_, "set_timing_derate %s %s ",
	      type_key,
	      earlyLateFlag(early_late));
      writeFloat(value);
      if (write_obj) {
	gzprintf(stream_, " ");
	write_obj->write();
      }
      gzprintf(stream_, "\n");
    }
  }
  else {
    for (int clk_data_index = 0;
	 clk_data_index < path_clk_or_data_count;
	 clk_data_index++) {
      PathClkOrData clk_data = static_cast<PathClkOrData>(clk_data_index);
      static const char *clk_data_keys[] = {"-clock", "-data"};
      const char *clk_data_key = clk_data_keys[clk_data_index];
      factors->isOneValue(clk_data, early_late, is_one_value, value);
      if (is_one_value) {
	if (value != 1.0) {
	  gzprintf(stream_, "set_timing_derate %s %s %s ",
		  type_key,
		  earlyLateFlag(early_late),
		  clk_data_key);
	  writeFloat(value);
	  if (write_obj) {
	    gzprintf(stream_, " ");
	    write_obj->write();
	  }
	  gzprintf(stream_, "\n");
	}
      }
      else {
	for (auto rf : RiseFall::range()) {
	  float factor;
	  bool exists;
	  factors->factor(clk_data, rf, early_late, factor, exists);
	  if (exists) {
	    gzprintf(stream_, "set_timing_derate %s %s %s %s ",
		    type_key,
		    clk_data_key,
		    transRiseFallFlag(rf),
		    earlyLateFlag(early_late));
	    writeFloat(factor);
	    if (write_obj) {
	      gzprintf(stream_, " ");
	      write_obj->write();
	    }
	    gzprintf(stream_, "\n");
	  }
	}
      }
    }
  }
}

static const char *
timingDerateTypeKeyword(TimingDerateType type)
{
  static const char *type_keys[] = {"-cell_delay","-cell_check","-net_delay"};
  return type_keys[static_cast<int>(type)];
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeDesignRules() const
{
  writeCommentSection("Design Rules");
  writeMinPulseWidths();
  writeLatchBorowLimits();
  writeSlewLimits();
  writeCapLimits();
  writeFanoutLimits();
  writeMaxArea();
}

void
WriteSdc::writeMinPulseWidths() const
{
  for (auto pin_widths : sdc_->pin_min_pulse_width_map_) {
    const Pin *pin = pin_widths.first;
    RiseFallValues *min_widths = pin_widths.second;
    WriteGetPin write_obj(pin, false, this);
    writeMinPulseWidths(min_widths, write_obj);
  }

  for (auto inst_widths : sdc_->inst_min_pulse_width_map_) {
    const Instance *inst = inst_widths.first;
    RiseFallValues *min_widths = inst_widths.second;
    WriteGetInstance write_obj(inst, this);
    writeMinPulseWidths(min_widths, write_obj);
  }

  for (auto clk_widths : sdc_->clk_min_pulse_width_map_) {
    const Clock *clk = clk_widths.first;
    RiseFallValues *min_widths = clk_widths.second;
    WriteGetClock write_obj(clk, this);
    writeMinPulseWidths(min_widths, write_obj);
  }
}

void
WriteSdc::writeMinPulseWidths(RiseFallValues *min_widths,
			      WriteSdcObject &write_obj) const
{
  bool hi_exists, low_exists;
  float hi, low;
  min_widths->value(RiseFall::rise(), hi, hi_exists);
  min_widths->value(RiseFall::fall(), low, low_exists);
  if (hi_exists && low_exists
      && hi == low)
    writeMinPulseWidth("", hi, write_obj);
  else {
    if (hi_exists)
      writeMinPulseWidth("-high ", hi, write_obj);
    if (low_exists)
      writeMinPulseWidth("-low ", low, write_obj);
  }
}

void
WriteSdc::writeMinPulseWidth(const char *hi_low,
			     float value,
			     WriteSdcObject &write_obj) const
{
  gzprintf(stream_, "set_min_pulse_width %s", hi_low);
  writeTime(value);
  gzprintf(stream_, " ");
  write_obj.write();
  gzprintf(stream_, "\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeLatchBorowLimits() const
{
  for (auto pin_borrow : sdc_->pin_latch_borrow_limit_map_) {
    const Pin *pin = pin_borrow.first;
    float limit = pin_borrow.second;
    gzprintf(stream_, "set_max_time_borrow ");
    writeTime(limit);
    gzprintf(stream_, " ");
    writeGetPin(pin, false);
    gzprintf(stream_, "\n");
  }

  for (auto inst_borrow : sdc_->inst_latch_borrow_limit_map_) {
    const Instance *inst = inst_borrow.first;
    float limit = inst_borrow.second;
    gzprintf(stream_, "set_max_time_borrow ");
    writeTime(limit);
    gzprintf(stream_, " ");
    writeGetInstance(inst);
    gzprintf(stream_, "\n");
  }

  for (auto clk_borrow : sdc_->clk_latch_borrow_limit_map_) {
    const Clock *clk = clk_borrow.first;
    float limit = clk_borrow.second;
    gzprintf(stream_, "set_max_time_borrow ");
    writeTime(limit);
    gzprintf(stream_, " ");
    writeGetClock(clk);
    gzprintf(stream_, "\n");
  }
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeSlewLimits() const
{
  const MinMax *min_max = MinMax::max();
  float slew;
  bool exists;
  sdc_->slewLimit(cell_, min_max, slew, exists);
  if (exists) {
    gzprintf(stream_, "set_max_transition ");
    writeTime(slew);
    gzprintf(stream_, " [current_design]\n");
  }

  CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    sdc_->slewLimit(port, min_max, slew, exists);
    if (exists) {
      gzprintf(stream_, "set_max_transition ");
      writeTime(slew);
      gzprintf(stream_, " ");
      writeGetPort(port);
      gzprintf(stream_, "\n");
    }
  }
  delete port_iter;

  writeClkSlewLimits();
}

void
WriteSdc::writeClkSlewLimits() const
{
  const MinMax *min_max = MinMax::max();
  ClockSeq clks;
  sdc_->sortedClocks(clks);
  for (const Clock *clk : clks) {
    float rise_clk_limit, fall_clk_limit, rise_data_limit, fall_data_limit;
    bool rise_clk_exists, fall_clk_exists, rise_data_exists, fall_data_exists;
    clk->slewLimit(RiseFall::rise(), PathClkOrData::clk, min_max,
		   rise_clk_limit, rise_clk_exists);
    clk->slewLimit(RiseFall::fall(), PathClkOrData::clk, min_max,
		   fall_clk_limit, fall_clk_exists);
    clk->slewLimit(RiseFall::rise(), PathClkOrData::data, min_max,
		   rise_data_limit, rise_data_exists);
    clk->slewLimit(RiseFall::fall(), PathClkOrData::data, min_max,
		   fall_data_limit, fall_data_exists);
    if (rise_clk_exists && fall_clk_exists
	&& rise_data_exists && fall_data_exists
	&& fall_clk_limit == rise_clk_limit
	&& rise_data_limit == rise_clk_limit
	&& fall_data_limit == rise_clk_limit)
      writeClkSlewLimit("", "", clk, rise_clk_limit);
    else {
      if (rise_clk_exists && fall_clk_exists
	  && fall_clk_limit == rise_clk_limit)
	writeClkSlewLimit("-clock_path ", "", clk, rise_clk_limit);
      else {
	if (rise_clk_exists)
	  writeClkSlewLimit("-clock_path ", "-rise ", clk, rise_clk_limit);
	if (fall_clk_exists)
	  writeClkSlewLimit("-clock_path ", "-fall ", clk, fall_clk_limit);
      }
      if (rise_data_exists && fall_data_exists
	  && fall_data_limit == rise_data_limit)
	writeClkSlewLimit("-data_path ", "", clk, rise_data_limit);
      else {
	if (rise_data_exists)
	  writeClkSlewLimit("-data_path ", "-rise ", clk, rise_data_limit);
	if (fall_data_exists) {
	  writeClkSlewLimit("-data_path ", "-fall ", clk, fall_data_limit);
	}
      }
    }
  }
}

void
WriteSdc::writeClkSlewLimit(const char *clk_data,
			    const char *rise_fall,
			    const Clock *clk,
			    float limit) const
{
  gzprintf(stream_, "set_max_transition %s%s", clk_data, rise_fall);
  writeTime(limit);
  gzprintf(stream_, " ");
  writeGetClock(clk);
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeCapLimits() const
{
  writeCapLimits(MinMax::min(), "set_min_capacitance");
  writeCapLimits(MinMax::max(), "set_max_capacitance");
}

void
WriteSdc::writeCapLimits(const MinMax *min_max,
			 const char *cmd) const
{
  float cap;
  bool exists;
  sdc_->capacitanceLimit(cell_, min_max, cap, exists);
  if (exists) {
    gzprintf(stream_, "%s ", cmd);
    writeCapacitance(cap);
    gzprintf(stream_, " [current_design]\n");
  }

  for (auto port_limit : sdc_->port_cap_limit_map_) {
    const Port *port = port_limit.first;
    MinMaxFloatValues values = port_limit.second;
    float cap;
    bool exists;
    values.value(min_max, cap, exists);
    if (exists) {
      gzprintf(stream_, "%s ", cmd);
      writeCapacitance(cap);
      gzprintf(stream_, " ");
      writeGetPort(port);
      gzprintf(stream_, "\n");
    }
  }

  for (auto pin_limit : sdc_->pin_cap_limit_map_) {
    const Pin *pin = pin_limit.first;
    MinMaxFloatValues values = pin_limit.second;
    float cap;
    bool exists;
    values.value(min_max, cap, exists);
    if (exists) {
      gzprintf(stream_, "%s ", cmd);
      writeCapacitance(cap);
      gzprintf(stream_, " ");
      writeGetPin(pin, false);
      gzprintf(stream_, "\n");
    }
  }
}

void
WriteSdc::writeMaxArea() const
{
  float max_area = sdc_->maxArea();
  if (max_area > 0.0) {
    gzprintf(stream_, "set_max_area ");
    writeFloat(max_area);
    gzprintf(stream_, "\n");
  }
}

void
WriteSdc::writeFanoutLimits() const
{
  writeFanoutLimits(MinMax::min(), "set_min_fanout");
  writeFanoutLimits(MinMax::max(), "set_max_fanout");
}

void
WriteSdc::writeFanoutLimits(const MinMax *min_max,
			    const char *cmd) const
{
  float fanout;
  bool exists;
  sdc_->fanoutLimit(cell_, min_max, fanout, exists);
  if (exists) {
    gzprintf(stream_, "%s ", cmd);
    writeFloat(fanout);
    gzprintf(stream_, " [current_design]\n");
  }
  else {
    CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      sdc_->fanoutLimit(port, min_max, fanout, exists);
      if (exists) {
	gzprintf(stream_, "%s ", cmd);
	writeFloat(fanout);
	gzprintf(stream_, " ");
	writeGetPort(port);
	gzprintf(stream_, "\n");
      }
    }
    delete port_iter;
  }
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeVariables() const
{
  if (sdc_->propagateAllClocks()) {
    if (native_)
      gzprintf(stream_, "set sta_propagate_all_clocks 1\n");
    else
      gzprintf(stream_, "set timing_all_clocks_propagated true\n");
  }
  if (sdc_->presetClrArcsEnabled()) {
    if (native_)
      gzprintf(stream_, "set sta_preset_clear_arcs_enabled 1\n");
    else
      gzprintf(stream_, "set timing_enable_preset_clear_arcs true\n");
  }
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeGetTimingArcsOfOjbects(const LibertyCell *cell) const
{
  gzprintf(stream_, "[%s -of_objects ", getTimingArcsCmd());
  writeGetLibCell(cell);
  gzprintf(stream_, "]");
}

void
WriteSdc::writeGetTimingArcs(Edge *edge) const
{
  writeGetTimingArcs(edge, nullptr);
}

void
WriteSdc::writeGetTimingArcs(Edge *edge,
			     const char *filter) const
{
  gzprintf(stream_, "[%s -from ", getTimingArcsCmd());
  Vertex *from_vertex = edge->from(graph_);
  writeGetPin(from_vertex->pin(), true);
  gzprintf(stream_, " -to ");
  Vertex *to_vertex = edge->to(graph_);
  writeGetPin(to_vertex->pin(), false);
  if (filter)
    gzprintf(stream_, " -filter {%s}", filter);
  gzprintf(stream_, "]");
}

const char *
WriteSdc::getTimingArcsCmd() const
{
  return native_ ? "get_timing_edges" : "get_timing_arcs";
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeGetLibCell(const LibertyCell *cell) const
{
  gzprintf(stream_, "[get_lib_cells {%s/%s}]",
	  cell->libertyLibrary()->name(),
	  cell->name());
}

void
WriteSdc::writeGetLibPin(const LibertyPort *port) const
{
  LibertyCell *cell = port->libertyCell();
  LibertyLibrary *lib = cell->libertyLibrary();
  gzprintf(stream_, "[get_lib_pins {%s/%s/%s}]",
	  lib->name(),
	  cell->name(),
	  port->name());
}

void
WriteSdc::writeGetClocks(ClockSet *clks) const
{
  bool first = true;
  bool multiple = clks->size() > 1;
  if (multiple)
    gzprintf(stream_, "[list ");
  writeGetClocks(clks, multiple, first);
  if (multiple)
    gzprintf(stream_, "]");
}

void
WriteSdc::writeGetClocks(ClockSet *clks,
			 bool multiple,
			 bool &first) const
{
  ClockSeq clks1 = sortByName(clks);
  for (const Clock *clk : clks1) {
    if (multiple && !first)
      gzprintf(stream_, "\\\n           ");
    writeGetClock(clk);
    first = false;
  }
}

void
WriteSdc::writeGetClock(const Clock *clk) const
{
  gzprintf(stream_, "[get_clocks {%s}]",
	  clk->name());
}

void
WriteSdc::writeGetPort(const Port *port) const
{
  gzprintf(stream_, "[get_ports {%s}]", sdc_network_->name(port));
}

void
WriteSdc::writeGetPins(const PinSet *pins,
		       bool map_hpin_to_drvr) const
{
  if (map_hpins_) {
    PinSet leaf_pins(network_);;
    for (const Pin *pin : *pins) {
      if (network_->isHierarchical(pin)) {
	if (map_hpin_to_drvr)
	  findLeafDriverPins(const_cast<Pin*>(pin), network_, &leaf_pins);
	else
	  findLeafLoadPins(const_cast<Pin*>(pin), network_, &leaf_pins);
      }
      else
	leaf_pins.insert(pin);
    }
    PinSeq pins1 = sortByPathName(&leaf_pins, sdc_network_);
    writeGetPins1(&pins1);
  }
  else {
    PinSeq pins1 = sortByPathName(pins, sdc_network_);
    writeGetPins1(&pins1);
  }
}

void
WriteSdc::writeGetPins1(PinSeq *pins) const
{
  bool multiple = pins->size() > 1;
  if (multiple)
    gzprintf(stream_, "[list ");
  bool first = true;
  for (const Pin *pin : *pins) {
    if (multiple && !first)
      gzprintf(stream_, "\\\n          ");
    writeGetPin(pin);
    first = false;
  }
  if (multiple)
    gzprintf(stream_, "]");
}

void
WriteSdc::writeGetPin(const Pin *pin) const
{
  if (sdc_network_->instance(pin) == instance_)
    gzprintf(stream_, "[get_ports {%s}]", sdc_network_->portName(pin));
  else
    gzprintf(stream_, "[get_pins {%s}]", pathName(pin));
}

void
WriteSdc::writeGetPin(const Pin *pin,
		      bool map_hpin_to_drvr) const
{
  if (map_hpins_ && network_->isHierarchical(pin)) {
    PinSet pins(network_);
    pins.insert(const_cast<Pin*>(pin));
    writeGetPins(&pins, map_hpin_to_drvr);
  }
  else
    writeGetPin(pin);
}

void
WriteSdc::writeGetNet(const Net *net) const
{
  gzprintf(stream_, "[get_nets {%s}]", pathName(net));
}

void
WriteSdc::writeGetInstance(const Instance *inst) const
{
  gzprintf(stream_, "[get_cells {%s}]", pathName(inst));
}

const char *
WriteSdc::pathName(const Pin *pin) const
{
  const char *pin_path = sdc_network_->pathName(pin);
  if (top_instance_)
    return pin_path;
  else
    return pin_path + instance_name_length_ + 1;
}

const char *
WriteSdc::pathName(const Net *net) const
{
  const char *net_path = sdc_network_->pathName(net);
  if (top_instance_)
    return net_path;
  else
    return net_path + instance_name_length_ + 1;
}

const char *
WriteSdc::pathName(const Instance *inst) const
{
  const char *inst_path = sdc_network_->pathName(inst);
  if (top_instance_)
    return inst_path;
  else
    return inst_path + instance_name_length_ + 1;
}

void
WriteSdc::writeCommentSection(const char *line) const
{
  writeCommentSeparator();
  gzprintf(stream_, "# %s\n", line);
  writeCommentSeparator();
}

void
WriteSdc::writeCommentSeparator() const
{
  gzprintf(stream_, "###############################################################################\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeRiseFallMinMaxTimeCmd(const char *sdc_cmd,
				     const RiseFallMinMax *values,
				     WriteSdcObject &write_object) const
{
  writeRiseFallMinMaxCmd(sdc_cmd, values, units_->timeUnit()->scale(),
			 write_object);
}

void
WriteSdc::writeRiseFallMinMaxCapCmd(const char *sdc_cmd,
				    const RiseFallMinMax *values,
				    WriteSdcObject &write_object) const
{
  writeRiseFallMinMaxCmd(sdc_cmd, values, units_->capacitanceUnit()->scale(),
			 write_object);
}

void
WriteSdc::writeRiseFallMinMaxCmd(const char *sdc_cmd,
				 const RiseFallMinMax *values,
				 float scale,
				 WriteSdcObject &write_object) const
{
  float fall_min, fall_max, rise_min, rise_max;
  bool fall_min_exists, fall_max_exists, rise_min_exists, rise_max_exists;
  values->value(RiseFall::fall(), MinMax::min(),
		fall_min, fall_min_exists);
  values->value(RiseFall::fall(), MinMax::max(),
		fall_max, fall_max_exists);
  values->value(RiseFall::rise(), MinMax::min(),
		rise_min, rise_min_exists);
  values->value(RiseFall::rise(), MinMax::max(),
		rise_max, rise_max_exists);
  if (fall_min_exists && fall_max_exists
      && rise_min_exists && rise_max_exists) {
    if (fall_min == rise_min
	&& rise_max == rise_min
	&& fall_max == rise_min) {
      // rise/fall/min/max match.
      writeRiseFallMinMaxCmd(sdc_cmd, rise_min, scale,
			     RiseFallBoth::riseFall(), MinMaxAll::all(),
			     write_object);
    }
    else if (rise_min == fall_min
	     && rise_max == fall_max) {
      // rise/fall match.
      writeRiseFallMinMaxCmd(sdc_cmd, rise_min, scale,
			     RiseFallBoth::riseFall(), MinMaxAll::min(),
			     write_object);
      writeRiseFallMinMaxCmd(sdc_cmd, rise_max, scale,
			     RiseFallBoth::riseFall(), MinMaxAll::max(),
			     write_object);
    }
    else if (rise_min == rise_max
	     && fall_min == fall_max) {
      // min/max match.
      writeRiseFallMinMaxCmd(sdc_cmd, rise_min, scale,
			     RiseFallBoth::rise(), MinMaxAll::all(),
			     write_object);
      writeRiseFallMinMaxCmd(sdc_cmd, fall_min, scale,
			     RiseFallBoth::fall(), MinMaxAll::all(),
			     write_object);
    }
  }
  else {
    if (rise_min_exists)
      writeRiseFallMinMaxCmd(sdc_cmd, rise_min, scale,
			     RiseFallBoth::rise(), MinMaxAll::min(),
			     write_object);
    if (rise_max_exists)
      writeRiseFallMinMaxCmd(sdc_cmd, rise_max, scale,
			     RiseFallBoth::rise(), MinMaxAll::max(),
			     write_object);
    if (fall_min_exists)
      writeRiseFallMinMaxCmd(sdc_cmd, fall_min, scale,
			     RiseFallBoth::fall(), MinMaxAll::min(),
			     write_object);
    if (fall_max_exists)
      writeRiseFallMinMaxCmd(sdc_cmd, fall_max, scale,
			     RiseFallBoth::fall(), MinMaxAll::max(),
			     write_object);
  }
}

void
WriteSdc::writeRiseFallMinMaxCmd(const char *sdc_cmd,
				 float value,
				 float scale,
				 const RiseFallBoth *rf,
				 const MinMaxAll *min_max,
				 WriteSdcObject &write_object) const
{
  gzprintf(stream_, "%s%s%s ",
	  sdc_cmd,
	  transRiseFallFlag(rf),
	  minMaxFlag(min_max));
  writeFloat(value / scale);
  gzprintf(stream_, " ");
  write_object.write();
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeClockKey(const Clock *clk) const
{
  gzprintf(stream_, " -clock ");
  writeGetClock(clk);
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeMinMaxFloatValuesCmd(const char *sdc_cmd,
				    MinMaxFloatValues *values,
				    float scale,
				    WriteSdcObject &write_object) const
{
  float min, max;
  bool min_exists, max_exists;
  values->value(MinMax::min(), min, min_exists);
  values->value(MinMax::max(), max, max_exists);
  if (min_exists && max_exists
      && min == max) {
    // min/max match.
    writeMinMaxFloatCmd(sdc_cmd, min, scale, MinMaxAll::all(), write_object);
  }
  else {
    if (min_exists)
      writeMinMaxFloatCmd(sdc_cmd, min, scale, MinMaxAll::min(), write_object);
    if (max_exists)
      writeMinMaxFloatCmd(sdc_cmd, max, scale, MinMaxAll::max(), write_object);
  }
}

void
WriteSdc::writeMinMaxFloatCmd(const char *sdc_cmd,
			      float value,
			      float scale,
			      const MinMaxAll *min_max,
			      WriteSdcObject &write_object) const
{
  gzprintf(stream_, "%s%s ",
	  sdc_cmd,
	  minMaxFlag(min_max));
  writeFloat(value / scale);
  gzprintf(stream_, " ");
  write_object.write();
  gzprintf(stream_, "\n");
}

void
WriteSdc::writeMinMaxIntValuesCmd(const char *sdc_cmd,
				  MinMaxIntValues *values,
				  WriteSdcObject &write_object) const
{
  int min, max;
  bool min_exists, max_exists;
  values->value(MinMax::min(), min, min_exists);
  values->value(MinMax::max(), max, max_exists);
  if (min_exists && max_exists
      && min == max) {
    // min/max match.
    writeMinMaxIntCmd(sdc_cmd, min, MinMaxAll::all(), write_object);
  }
  else {
    if (min_exists)
      writeMinMaxIntCmd(sdc_cmd, min, MinMaxAll::min(), write_object);
    if (max_exists)
      writeMinMaxIntCmd(sdc_cmd, max, MinMaxAll::max(), write_object);
  }
}

void
WriteSdc::writeMinMaxIntCmd(const char *sdc_cmd,
			    int value,
			    const MinMaxAll *min_max,
			    WriteSdcObject &write_object) const
{
  gzprintf(stream_, "%s%s ",
	  sdc_cmd,
	  minMaxFlag(min_max));
  gzprintf(stream_, "%d ", value);
  write_object.write();
  gzprintf(stream_, "\n");
}

////////////////////////////////////////////////////////////////

float
WriteSdc::scaleTime(float time) const
{
  return time / units_->timeUnit()->scale();
}

float
WriteSdc::scaleCapacitance(float cap) const
{
  return cap / units_->capacitanceUnit()->scale();
}

float
WriteSdc::scaleResistance(float res) const
{
  return res / units_->resistanceUnit()->scale();
}

void
WriteSdc::writeFloat(float value) const
{
  gzprintf(stream_, "%.*f", digits_, value);
}

void
WriteSdc::writeTime(float time) const
{
  gzprintf(stream_, "%.*f", digits_, scaleTime(time));
}

void
WriteSdc::writeCapacitance(float cap) const
{
  gzprintf(stream_, "%.*f", digits_, scaleCapacitance(cap));
}

void
WriteSdc::writeResistance(float res) const
{
  gzprintf(stream_, "%.*f", digits_, scaleResistance(res));
}

void
WriteSdc::writeFloatSeq(FloatSeq *floats,
			float scale) const
{
  gzprintf(stream_, "{");
  bool first = true;
  for (float flt : *floats) {
    if (!first)
      gzprintf(stream_, " ");
    writeFloat(flt * scale);
    first = false;
  }
  gzprintf(stream_, "}");
}

void
WriteSdc::writeIntSeq(IntSeq *ints) const
{
  gzprintf(stream_, "{");
  bool first = true;
  for (int i : *ints) {
    if (!first)
      gzprintf(stream_, " ");
    gzprintf(stream_, "%d", i);
    first = false;
  }
  gzprintf(stream_, "}");
}


////////////////////////////////////////////////////////////////

static const char *
transRiseFallFlag(const RiseFall *rf)
{
  return (rf == RiseFall::rise()) ? "-rise" : "-fall";
}

static const char *
transRiseFallFlag(const RiseFallBoth *rf)
{
  if (rf == RiseFallBoth::rise())
    return " -rise";
  else if (rf == RiseFallBoth::fall())
    return " -fall";
  else
    return "";
}

static const char *
minMaxFlag(const MinMaxAll *min_max)
{
  if (min_max == MinMaxAll::min())
    return " -min";
  else if (min_max == MinMaxAll::max())
    return " -max";
  else
    return "";
}

static const char *
minMaxFlag(const MinMax *min_max)
{
  return (min_max == MinMax::min()) ? " -min" : " -max";
}

static const char *
earlyLateFlag(const MinMax *early_late)
{
  return (early_late == MinMax::min()) ? "-early" : "-late";
}

void
WriteSdc::writeSetupHoldFlag(const MinMaxAll *min_max) const
{
  if (min_max == MinMaxAll::min())
    gzprintf(stream_,  " -hold");
  else if (min_max == MinMaxAll::max())
    gzprintf(stream_,  " -setup");
}

static const char *
setupHoldFlag(const MinMax *min_max)
{
  return (min_max == MinMax::min()) ?  " -hold" : " -setup";
}

void
WriteSdc::writeCmdComment(SdcCmdComment *cmd) const
{
  const char *comment = cmd->comment();
  if (comment) {
    gzprintf(stream_,  " -comment {%s}", comment);
  }
}

} // namespace
