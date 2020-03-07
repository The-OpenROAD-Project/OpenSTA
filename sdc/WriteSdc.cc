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
#include <algorithm>
#include <time.h>
#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "Report.hh"
#include "Error.hh"
#include "Units.hh"
#include "StaState.hh"
#include "PortDirection.hh"
#include "RiseFallValues.hh"
#include "Transition.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "PortDelay.hh"
#include "ExceptionPath.hh"
#include "PortExtCap.hh"
#include "DisabledPorts.hh"
#include "Wireload.hh"
#include "ClockGroups.hh"
#include "ClockInsertion.hh"
#include "ClockLatency.hh"
#include "InputDrive.hh"
#include "DataCheck.hh"
#include "DeratingFactors.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "GraphCmp.hh"
#include "WriteSdc.hh"
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

private:
  DISALLOW_COPY_AND_ASSIGN(WriteSdcObject);
};

class WriteGetPort : public WriteSdcObject
{
public:
  WriteGetPort(const Port *port,
	       const WriteSdc *writer);
  virtual void write() const;

private:
  DISALLOW_COPY_AND_ASSIGN(WriteGetPort);

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
  DISALLOW_COPY_AND_ASSIGN(WriteGetPinAndClkKey);

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
  fprintf(writer_->stream(), " ");
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
  DISALLOW_COPY_AND_ASSIGN(WriteGetPin);

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
  DISALLOW_COPY_AND_ASSIGN(WriteGetNet);

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
  DISALLOW_COPY_AND_ASSIGN(WriteGetInstance);

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
  DISALLOW_COPY_AND_ASSIGN(WriteGetLibCell);

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
  DISALLOW_COPY_AND_ASSIGN(WriteGetClock);

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
	 bool no_timestamp,
	 int digits,
	 Sdc *sdc)
{
  WriteSdc writer(instance, filename, creator, map_hpins, native,
		  digits, no_timestamp, sdc);
  writer.write();
}

WriteSdc::WriteSdc(Instance *instance,
		   const char *filename,
		   const char *creator,
		   bool map_hpins,
		   bool native,
		   int digits,
		   bool no_timestamp,
		   Sdc *sdc) :
  StaState(sdc),
  instance_(instance),
  filename_(filename),
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
WriteSdc::write()
{
  openFile(filename_);
  writeHeader();
  writeTiming();
  writeEnvironment();
  writeDesignRules();
  writeVariables();
  closeFile();
}

void
WriteSdc::openFile(const char *filename)
{
  stream_ = fopen(filename, "w");
  if (stream_ == nullptr)
    throw FileNotWritable(filename);
}

void
WriteSdc::closeFile()
{
  fclose(stream_);
}

void
WriteSdc::flush()
{
  fflush(stream_);
}

void
WriteSdc::writeHeader() const
{
  writeCommentSeparator();
  fprintf(stream_, "# Created by %s\n", creator_);
  if (!no_timestamp_) {
    time_t now;
    time(&now);
    char *time_str = ctime(&now);
    // Remove trailing \n.
    time_str[strlen(time_str) - 1] = '\0';
    fprintf(stream_, "# %s\n", time_str);
  }
  writeCommentSeparator();

  fprintf(stream_, "current_design %s\n", sdc_network_->name(cell_));
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
      fprintf(stream_, "set_propagated_clock ");
      writeGetClock(clk);
      fprintf(stream_, "\n");
    }
  }
}

void
WriteSdc::writeClock(Clock *clk) const
{
  fprintf(stream_, "create_clock -name %s",
	  clk->name());
  if (clk->addToPins())
    fprintf(stream_, " -add");
  fprintf(stream_, " -period ");
  writeTime(clk->period());
  fprintf(stream_, " -waveform ");
  writeFloatSeq(clk->waveform(), scaleTime(1.0));
  writeCmdComment(clk);
  fprintf(stream_, " ");
  writeClockPins(clk);
  fprintf(stream_, "\n");
}

void
WriteSdc::writeGeneratedClock(Clock *clk) const
{
  fprintf(stream_, "create_generated_clock -name %s",
	  clk->name());
  if (clk->addToPins())
    fprintf(stream_, " -add");
  fprintf(stream_, " -source ");
  writeGetPin(clk->srcPin(), true);
  Clock *master = clk->masterClk();
  if (master && !clk->masterClkInfered()) {
    fprintf(stream_, " -master_clock ");
    writeGetClock(master);
  }
  Pin *pll_out = clk->pllOut();
  if (pll_out) {
    fprintf(stream_, " -pll_out ");
    writeGetPin(pll_out, true);
  }
  Pin *pll_fdbk = clk->pllFdbk();
  if (pll_fdbk) {
    fprintf(stream_, " -pll_feedback ");
    writeGetPin(pll_fdbk, false);
  }
  if (clk->combinational())
    fprintf(stream_, " -combinational");
  int divide_by = clk->divideBy();
  if (divide_by != 0)
    fprintf(stream_, " -divide_by %d", divide_by);
  int multiply_by = clk->multiplyBy();
  if (multiply_by != 0)
    fprintf(stream_, " -multiply_by %d", multiply_by);
  float duty_cycle = clk->dutyCycle();
  if (duty_cycle != 0.0) {
    fprintf(stream_, " -duty_cycle ");
    writeFloat(duty_cycle);
  }
  if (clk->invert())
    fprintf(stream_, " -invert");
  IntSeq *edges = clk->edges();
  if (edges && !edges->empty()) {
    fprintf(stream_, " -edges ");
    writeIntSeq(edges);
    FloatSeq *edge_shifts = clk->edgeShifts();
    if (edge_shifts && !edge_shifts->empty()) {
      fprintf(stream_, " -edge_shift ");
      writeFloatSeq(edge_shifts, scaleTime(1.0));
    }
  }
  writeCmdComment(clk);
  fprintf(stream_, " ");
  writeClockPins(clk);
  fprintf(stream_, "\n");
}

void
WriteSdc::writeClockPins(Clock *clk) const
{
  // Sort pins.
  PinSet &pins = clk->pins();
  if (!pins.empty()) {
    if (pins.size() > 1)
      fprintf(stream_, "\\\n    ");
    writeGetPins(&pins, true);
  }
}

void
WriteSdc::writeClockSlews(Clock *clk) const
{
  WriteGetClock write_clk(clk, this);
  RiseFallMinMax *slews = clk->slews();
  if (slews->hasValue())
    writeRiseFallMinMaxTimeCmd("set_clock_transition", slews, write_clk);
}

void
WriteSdc::writeClockUncertainty(Clock *clk) const
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
WriteSdc::writeClockUncertainty(Clock *clk,
				const char *setup_hold,
				float value) const
{
  fprintf(stream_, "set_clock_uncertainty %s", setup_hold);
  writeTime(value);
  fprintf(stream_, " %s\n", clk->name());
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
  fprintf(stream_, "set_clock_uncertainty %s", setup_hold);
  writeTime(value);
  fprintf(stream_, " ");
  writeGetPin(pin, true);
  fprintf(stream_, "\n");
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
  ClockInsertions::Iterator insert_iter(sdc_->clockInsertions());
  while (insert_iter.hasNext()) {
    ClockInsertion *insert = insert_iter.next();
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
    fprintf(stream_, "set_propagated_clock ");
    writeGetPin(pin, true);
    fprintf(stream_, "\n");
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
    fprintf(stream_, "set_clock_uncertainty -from ");
    writeGetClock(src_clk);
    fprintf(stream_, " -to ");
    writeGetClock(tgt_clk);
    fprintf(stream_, " ");
    writeTime(value);
    fprintf(stream_, "\n");
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
	    fprintf(stream_, "set_clock_uncertainty -%s_from ",
		    src_rf == RiseFall::rise() ? "rise" : "fall");
	    writeGetClock(uncertainty->src());
	    fprintf(stream_, " -%s_to ",
		    tgt_rf == RiseFall::rise() ? "rise" : "fall");
	    writeGetClock(uncertainty->target());
	    fprintf(stream_, " %s ",
		    setupHoldFlag(setup_hold));
	    writeTime(value);
	    fprintf(stream_, "\n");
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

  PortDelayLess port_delay_less(sdc_network_);
  sort(delays, port_delay_less);

  PortDelaySeq::Iterator delay_iter(delays);
  while (delay_iter.hasNext()) {
    PortDelay *output_delay = delay_iter.next();
    writePortDelay(output_delay, false, "set_output_delay");
  }
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
  fprintf(stream_, "%s ", sdc_cmd);
  writeTime(delay);
  ClockEdge *clk_edge = port_delay->clkEdge();
  if (clk_edge) {
    writeClockKey(clk_edge->clock());
    if (clk_edge->transition() == RiseFall::fall())
      fprintf(stream_, " -clock_fall");
  }
  fprintf(stream_, "%s%s -add_delay ",
	  transRiseFallFlag(rf),
	  minMaxFlag(min_max));
  Pin *ref_pin = port_delay->refPin();
  if (ref_pin) {
    fprintf(stream_, "-reference_pin ");
    writeGetPin(ref_pin, true);
    fprintf(stream_, " ");
  }
  writeGetPin(port_delay->pin(), is_input_delay);
  fprintf(stream_, "\n");
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
  fprintf(stream_, "set_sense -type clock %s ", flag);
  const Clock *clk = pin_clk.second;
  if (clk) {
    fprintf(stream_, "-clock ");
    writeGetClock(clk);
    fprintf(stream_, " ");
  }
  writeGetPin(pin_clk.first, true);
  fprintf(stream_, "\n");
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
  ClockSet *clk_set1 = clk_group1->clks();
  ClockSet *clk_set2 = clk_group2->clks();
  size_t size1 = clk_set1->size();
  size_t size2 = clk_set2->size();
  if (size1 < size2)
    return true;
  else if (size1 > size2)
    return false;
  else {
    ClockSeq clks1, clks2;
    ClockSet::Iterator clk_iter1(clk_set1);
    while (clk_iter1.hasNext())
      clks1.push_back(clk_iter1.next());
    sort(clks1, ClockNameLess());

    ClockSet::Iterator clk_iter2(clk_set2);
    while (clk_iter2.hasNext())
      clks2.push_back(clk_iter2.next());
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
  ClockGroupIterator groups_iter(sdc_);
  while (groups_iter.hasNext()) {
    ClockGroups *clk_groups = groups_iter.next();
    writeClockGroups(clk_groups);
  }
}

void
WriteSdc::writeClockGroups(ClockGroups *clk_groups) const
{
  fprintf(stream_, "set_clock_groups -name %s ", clk_groups->name());
  if (clk_groups->logicallyExclusive())
    fprintf(stream_, "-logically_exclusive \\\n");
  else if (clk_groups->physicallyExclusive())
    fprintf(stream_, "-physically_exclusive \\\n");
  else if (clk_groups->asynchronous())
    fprintf(stream_, "-asynchronous \\\n");
  if (clk_groups->allowPaths())
    fprintf(stream_, "-allow_paths \\\n");
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
      fprintf(stream_, "\\\n");
    fprintf(stream_, " -group ");
    writeGetClocks(clk_group->clks());
    first = false;
  }
  writeCmdComment(clk_groups);
  fprintf(stream_, "\n");
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
  DisabledCellPortsSeq disables;
  sortDisabledCellPortsMap(sdc_->disabledCellPorts(), disables);
  DisabledCellPortsSeq::Iterator disabled_iter(disables);
  while (disabled_iter.hasNext()) {
    DisabledCellPorts *disable = disabled_iter.next();
    LibertyCell *cell = disable->cell();
    if (disable->all()) {
      fprintf(stream_, "set_disable_timing ");
      writeGetLibCell(cell);
      fprintf(stream_, "\n");
    }
    if (disable->fromTo()) {
      LibertyPortPairSeq pairs;
      sortLibertyPortPairSet(disable->fromTo(), pairs);
      LibertyPortPairSeq::Iterator pair_iter(pairs);
      while (pair_iter.hasNext()) {
	LibertyPortPair *from_to = pair_iter.next();
	const LibertyPort *from = from_to->first;
	const LibertyPort *to = from_to->second;
	fprintf(stream_, "set_disable_timing -from {%s} -to {%s} ",
		from->name(),
		to->name());
	writeGetLibCell(cell);
	fprintf(stream_, "\n");
      }
    }
    if (disable->from()) {
      LibertyPortSeq from;
      sortLibertyPortSet(disable->from(), from);
      LibertyPortSeq::Iterator from_iter(from);
      while (from_iter.hasNext()) {
	LibertyPort *from_port = from_iter.next();
	fprintf(stream_, "set_disable_timing -from {%s} ",
		from_port->name());
	writeGetLibCell(cell);
	fprintf(stream_, "\n");
      }
    }
    if (disable->to()) {
      LibertyPortSeq to;
      sortLibertyPortSet(disable->to(), to);
      LibertyPortSeq::Iterator to_iter(to);
      while (to_iter.hasNext()) {
	LibertyPort *to_port = to_iter.next();
	fprintf(stream_, "set_disable_timing -to {%s} ",
		to_port->name());
	writeGetLibCell(cell);
	fprintf(stream_, "\n");
      }
    }
    if (disable->timingArcSets()) {
      // The only syntax to disable timing arc sets disables all of the
      // cell's timing arc sets.
      fprintf(stream_, "set_disable_timing ");
      writeGetTimingArcsOfOjbects(cell);
      fprintf(stream_, "\n");
    }
  }
}

void
WriteSdc::writeDisabledPorts() const
{
  PortSeq ports;
  sortPortSet(sdc_->disabledPorts(), sdc_network_, ports);
  PortSeq::Iterator port_iter(ports);
  while (port_iter.hasNext()) {
    Port *port = port_iter.next();
    fprintf(stream_, "set_disable_timing ");
    writeGetPort(port);
    fprintf(stream_, "\n");
  }
}

void
WriteSdc::writeDisabledLibPorts() const
{
  LibertyPortSeq ports;
  sortLibertyPortSet(sdc_->disabledLibPorts(), ports);
  LibertyPortSeq::Iterator port_iter(ports);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    fprintf(stream_, "set_disable_timing ");
    writeGetLibPin(port);
    fprintf(stream_, "\n");
  }
}

void
WriteSdc::writeDisabledInstances() const
{
  DisabledInstancePortsSeq disables;
  sortDisabledInstancePortsMap(sdc_->disabledInstancePorts(),
			       sdc_network_, disables);
  DisabledInstancePortsSeq::Iterator disabled_iter(disables);
  while (disabled_iter.hasNext()) {
    DisabledInstancePorts *disable = disabled_iter.next();
    Instance *inst = disable->instance();
    if (disable->all()) {
      fprintf(stream_, "set_disable_timing ");
      writeGetInstance(inst);
      fprintf(stream_, "\n");
    }
    else if (disable->fromTo()) {
      LibertyPortPairSeq pairs;
      sortLibertyPortPairSet(disable->fromTo(), pairs);
      LibertyPortPairSeq::Iterator pair_iter(pairs);
      while (pair_iter.hasNext()) {
	LibertyPortPair *from_to = pair_iter.next();
	const LibertyPort *from_port = from_to->first;
	const LibertyPort *to_port = from_to->second;
	fprintf(stream_, "set_disable_timing -from {%s} -to {%s} ",
		from_port->name(),
		to_port->name());
	writeGetInstance(inst);
	fprintf(stream_, "\n");
      }
    }
    if (disable->from()) {
      LibertyPortSeq from;
      sortLibertyPortSet(disable->from(), from);
      LibertyPortSeq::Iterator from_iter(from);
      while (from_iter.hasNext()) {
	LibertyPort *from_port = from_iter.next();
	fprintf(stream_, "set_disable_timing -from {%s} ",
		from_port->name());
	writeGetInstance(inst);
	fprintf(stream_, "\n");
      }
    }
    if (disable->to()) {
      LibertyPortSeq to;
      sortLibertyPortSet(disable->to(), to);
      LibertyPortSeq::Iterator to_iter(to);
      while (to_iter.hasNext()) {
	LibertyPort *to_port = to_iter.next();
	fprintf(stream_, "set_disable_timing -to {%s} ",
		to_port->name());
	writeGetInstance(inst);
	fprintf(stream_, "\n");
      }
    }
  }
}

void
WriteSdc::writeDisabledPins() const
{
  PinSeq pins;
  sortPinSet(sdc_->disabledPins(), sdc_network_, pins);
  PinSeq::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    fprintf(stream_, "set_disable_timing ");
    writeGetPin(pin, false);
    fprintf(stream_, "\n");
  }
}

void
WriteSdc::writeDisabledEdges() const
{
  EdgeSeq edges;
  EdgeSet::Iterator edge_iter(sdc_->disabledEdges());
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    edges.push_back(edge);
  }
  sortEdges(&edges, sdc_network_, graph_);
  EdgeSeq::Iterator edge_iter2(edges);
  while (edge_iter2.hasNext()) {
    Edge *edge = edge_iter2.next();
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
  EdgeSet::Iterator match_iter(matches);
  while (match_iter.hasNext()) {
    Edge *match = match_iter.next();
    if (match != edge
	&& match->sense() == edge->sense())
      return false;
  }
  return true;
}

void
WriteSdc::writeDisabledEdge(Edge *edge) const
{
  fprintf(stream_, "set_disable_timing ");
  writeGetTimingArcs(edge);
  fprintf(stream_, "\n");
}

void
WriteSdc::writeDisabledEdgeSense(Edge *edge) const
{
  fprintf(stream_, "set_disable_timing ");
  const char *sense = timingSenseString(edge->sense());
  string filter;
  stringPrint(filter, "sense == %s", sense);
  writeGetTimingArcs(edge, filter.c_str());
  fprintf(stream_, "\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeExceptions() const
{
  ExceptionPathSeq exceptions;
  sortExceptions(sdc_->exceptions(), exceptions, sdc_network_);
  ExceptionPathSeq::Iterator except_iter(exceptions);
  while (except_iter.hasNext()) {
    ExceptionPath *exception = except_iter.next();
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
    ExceptionThruSeq::Iterator thru_iter(exception->thrus());
    while (thru_iter.hasNext()) {
      ExceptionThru *thru = thru_iter.next();
      writeExceptionThru(thru);
    }
  }
  if (exception->to())
    writeExceptionTo(exception->to());
  writeExceptionValue(exception);
  writeCmdComment(exception);
  fprintf(stream_, "\n");
}

void
WriteSdc::writeExceptionCmd(ExceptionPath *exception) const
{
  if (exception->isFalse()) {
    fprintf(stream_, "set_false_path");
    writeSetupHoldFlag(exception->minMax());
  }
  else if (exception->isMultiCycle()) {
    fprintf(stream_, "set_multicycle_path");
    const MinMaxAll *min_max = exception->minMax();
    writeSetupHoldFlag(min_max);
    if (min_max == MinMaxAll::min()) {
      // For hold MCPs default is -start.
      if (exception->useEndClk())
	fprintf(stream_, " -end");
    }
    else {
      // For setup MCPs default is -end.
      if (!exception->useEndClk())
	fprintf(stream_, " -start");
    }
  }
  else if (exception->isPathDelay()) {
    if (exception->minMax() == MinMaxAll::max())
      fprintf(stream_, "set_max_delay");
    else
      fprintf(stream_, "set_min_delay");
    if (exception->ignoreClkLatency())
      fprintf(stream_, " -ignore_clock_latency");
  }
  else if (exception->isGroupPath()) {
    if (exception->isDefault())
      fprintf(stream_, "group_path -default");
    else
      fprintf(stream_, "group_path -name %s", exception->name());
  }
  else
    internalError("unknown exception type");
}

void
WriteSdc::writeExceptionValue(ExceptionPath *exception) const
{
  if (exception->isMultiCycle())
    fprintf(stream_, " %d",
	    exception->pathMultiplier());
  else if (exception->isPathDelay()) {
    fprintf(stream_, " ");
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
    fprintf(stream_, "%s ", transRiseFallFlag(end_rf));
  if (to->hasObjects())
    writeExceptionFromTo(to, "to", false);
}

void
WriteSdc::writeExceptionFromTo(ExceptionFromTo *from_to,
			       const char *from_to_key,
			       bool map_hpin_to_drvr) const
{
  const RiseFallBoth *rf = from_to->transition();
  const char *tr_prefix = "-";
  if (rf == RiseFallBoth::rise())
    tr_prefix = "-rise_";
  else if (rf == RiseFallBoth::fall())
    tr_prefix = "-fall_";
  fprintf(stream_, "\\\n    %s%s ", tr_prefix, from_to_key);
  bool multi_objs =
    ((from_to->pins() ? from_to->pins()->size() : 0)
     + (from_to->clks() ? from_to->clks()->size() : 0)
     + (from_to->instances() ? from_to->instances()->size() : 0)) > 1;
  if (multi_objs)
    fprintf(stream_, "[list ");
  bool first = true;
  if (from_to->pins()) {
    PinSeq pins;
    sortPinSet(from_to->pins(), sdc_network_, pins);
    PinSeq::Iterator pin_iter(pins);
    while (pin_iter.hasNext()) {
      Pin *pin = pin_iter.next();
      if (multi_objs && !first)
	fprintf(stream_, "\\\n           ");
      writeGetPin(pin, map_hpin_to_drvr);
      first = false;
    }
  }
  if (from_to->clks())
    writeGetClocks(from_to->clks(), multi_objs, first);
  if (from_to->instances()) {
    InstanceSeq insts;
    sortInstanceSet(from_to->instances(), sdc_network_, insts);
    InstanceSeq::Iterator inst_iter(insts);
    while (inst_iter.hasNext()) {
      Instance *inst = inst_iter.next();
      if (multi_objs && !first)
	fprintf(stream_, "\\\n           ");
      writeGetInstance(inst);
      first = false;
    }
  }
  if (multi_objs)
    fprintf(stream_, "]");
}

void
WriteSdc::writeExceptionThru(ExceptionThru *thru) const
{
  const RiseFallBoth *rf = thru->transition();
  const char *tr_prefix = "-";
  if (rf == RiseFallBoth::rise())
    tr_prefix = "-rise_";
  else if (rf == RiseFallBoth::fall())
    tr_prefix = "-fall_";
  fprintf(stream_, "\\\n    %sthrough ", tr_prefix);
  PinSeq pins;
  mapThruHpins(thru, pins);
  bool multi_objs =
    (pins.size()
     + (thru->nets() ? thru->nets()->size() : 0)
     + (thru->instances() ? thru->instances()->size() : 0)) > 1;
  if (multi_objs)
    fprintf(stream_, "[list ");
  bool first = true;
  sort(pins, PinPathNameLess(network_));
  PinSeq::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    if (multi_objs && !first)
      fprintf(stream_, "\\\n           ");
    writeGetPin(pin);
    first = false;
  }

  if (thru->nets()) {
    NetSeq nets;
    sortNetSet(thru->nets(), sdc_network_, nets);
    NetSeq::Iterator net_iter(nets);
    while (net_iter.hasNext()) {
      Net *net = net_iter.next();
      if (multi_objs && !first)
	fprintf(stream_, "\\\n           ");
      writeGetNet(net);
      first = false;
    }
  }
  if (thru->instances()) {
    InstanceSeq insts;
    sortInstanceSet(thru->instances(), sdc_network_, insts);
    InstanceSeq::Iterator inst_iter(insts);
    while (inst_iter.hasNext()) {
      Instance *inst = inst_iter.next();
      if (multi_objs && !first)
	fprintf(stream_, "\\\n           ");
      writeGetInstance(inst);
      first = false;
    }
  }
  if (multi_objs)
    fprintf(stream_, "]");
}

void
WriteSdc::mapThruHpins(ExceptionThru *thru,
		       PinSeq &pins) const
{
  if (thru->pins()) {
    for (Pin *pin : *thru->pins()) {
      // Map hierarical pins to load pins outside of outputs or inside of inputs.
      if (network_->isHierarchical(pin)) {
	Instance *hinst = network_->instance(pin);
	bool hpin_is_output = network_->direction(pin)->isAnyOutput();
	PinConnectedPinIterator *cpin_iter = network_->connectedPinIterator(pin);
	while (cpin_iter->hasNext()) {
	  Pin *cpin = cpin_iter->next();
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
  DataChecksMap::Iterator checks_iter(sdc_->data_checks_to_map_);
  while (checks_iter.hasNext()) {
    DataCheckSet *checks1 = checks_iter.next();
    DataCheckSet::Iterator check_iter(checks1);
    while (check_iter.hasNext()) {
      DataCheck *check = check_iter.next();
      checks.push_back(check);
    }
  }
  sort(checks, DataCheckLess(sdc_network_));
  Vector<DataCheck*>::Iterator check_iter(checks);
  while (check_iter.hasNext()) {
    DataCheck *check = check_iter.next();
    writeDataCheck(check);
  }
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
  fprintf(stream_, "set_data_check %s ", from_key);
  writeGetPin(check->from(), true);
  const char *to_key = "-to";
  if (to_rf == RiseFallBoth::rise())
    to_key = "-rise_to";
  else if (to_rf == RiseFallBoth::fall())
    to_key = "-fall_to";
  fprintf(stream_, " %s ", to_key);
  writeGetPin(check->to(), false);
  fprintf(stream_, "%s ",
	  setupHoldFlag(setup_hold));
  writeTime(margin);
  fprintf(stream_, "\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeEnvironment() const
{
  writeCommentSection("Environment");
  writeOperatingConditions();
  writeWireload();
  writePinLoads();
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
    fprintf(stream_, "set_operating_conditions %s\n", cond->name());
}

void
WriteSdc::writeWireload() const
{
  WireloadMode wireload_mode = sdc_->wireloadMode();
  if (wireload_mode != WireloadMode::unknown)
    fprintf(stream_, "set_wire_load_mode \"%s\"\n",
	    wireloadModeString(wireload_mode));
}

void
WriteSdc::writePinLoads() const
{
  CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    writePortLoads(port);
  }
  delete port_iter;
}

void
WriteSdc::writePortLoads(Port *port) const
{
  PortExtCap *ext_cap = sdc_->portExtCap(port);
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
      for (auto tr : RiseFall::range()) {
	if (drive->driveResistanceMinMaxEqual(tr)) {
	  float res;
	  bool exists;
	  drive->driveResistance(tr, MinMax::max(), res, exists);
	  fprintf(stream_, "set_drive %s ",
		  transRiseFallFlag(tr));
	  writeResistance(res);
	  fprintf(stream_, " ");
	  writeGetPort(port);
	  fprintf(stream_, "\n");
	}
	else {
	  for (auto min_max : MinMax::range()) {
	    float res;
	    bool exists;
	    drive->driveResistance(tr, min_max, res, exists);
	    if (exists) {
	      fprintf(stream_, "set_drive %s %s ",
		      transRiseFallFlag(tr),
		      minMaxFlag(min_max));
	      writeResistance(res);
	      fprintf(stream_, " ");
	      writeGetPort(port);
	      fprintf(stream_, "\n");
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
  LibertyCell *cell = drive_cell->cell();
  LibertyPort *from_port = drive_cell->fromPort();
  LibertyPort *to_port = drive_cell->toPort();
  float *from_slews = drive_cell->fromSlews();
  LibertyLibrary *lib = drive_cell->library();
  fprintf(stream_, "set_driving_cell");
  if (rf)
    fprintf(stream_, " %s", transRiseFallFlag(rf));
  if (min_max)
    fprintf(stream_, " %s", minMaxFlag(min_max));
  // Only write -library if it was specified in the sdc.
  if (lib)
    fprintf(stream_, " -library %s", lib->name());
  fprintf(stream_, " -lib_cell %s", cell->name());
  if (from_port)
    fprintf(stream_, " -from_pin {%s}",
	    from_port->name());
  fprintf(stream_,
	  " -pin {%s} -input_transition_rise ",
	  to_port->name());
  writeTime(from_slews[RiseFall::riseIndex()]);
  fprintf(stream_, " -input_transition_fall ");
  writeTime(from_slews[RiseFall::fallIndex()]);
  fprintf(stream_, " ");
  writeGetPort(port);
  fprintf(stream_, "\n");
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
  NetResistanceMap *net_res_map = sdc_->netResistances();
  NetSeq nets;
  NetResistanceMap::Iterator res_iter(net_res_map);
  while (res_iter.hasNext()) {
    Net *net;
    MinMaxFloatValues values;
    res_iter.next(net, values);
    nets.push_back(net);
  }
  sort(nets, NetPathNameLess(sdc_network_));
  NetSeq::Iterator net_iter(nets);
  while (net_iter.hasNext()) {
    Net *net = net_iter.next();
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
WriteSdc::writeNetResistance(Net *net,
			     const MinMaxAll *min_max,
			     float res) const
{
  fprintf(stream_, "set_resistance ");
  writeResistance(res);
  fprintf(stream_, "%s ", minMaxFlag(min_max));
  writeGetNet(net);
  fprintf(stream_, "\n");
}

void
WriteSdc::writeConstants() const
{
  PinSeq pins;
  sortedLogicValuePins(sdc_->logicValues(), pins);
  PinSeq::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    writeConstant(pin);
  }
}

void
WriteSdc::writeConstant(Pin *pin) const
{
  const char *cmd = setConstantCmd(pin);
  fprintf(stream_, "%s ", cmd);
  writeGetPin(pin, false);
  fprintf(stream_, "\n");
}

const char *
WriteSdc::setConstantCmd(Pin *pin) const
{
  LogicValue value;
  bool exists;
  sdc_->logicValue(pin, value, exists);
  switch (value) {
  case LogicValue::zero:
    return "set_LogicValue::zero";
  case LogicValue::one:
    return "set_logic_one";
  case LogicValue::unknown:
    return "set_logic_dc";
  case LogicValue::rise:
  case LogicValue::fall:
  default:
    internalError("illegal set_logic value");
  }
}

void
WriteSdc::writeCaseAnalysis() const
{
  PinSeq pins;
  sortedLogicValuePins(sdc_->caseLogicValues(), pins);
  PinSeq::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    writeCaseAnalysis(pin);
  }
}


void
WriteSdc::writeCaseAnalysis(Pin *pin) const
{
  const char *value_str = caseAnalysisValueStr(pin);
  fprintf(stream_, "set_case_analysis %s ", value_str);
  writeGetPin(pin, false);
  fprintf(stream_, "\n");
}

const char *
WriteSdc::caseAnalysisValueStr(Pin *pin) const
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
    internalError("invalid set_case_analysis value");
  }
}

void
WriteSdc::sortedLogicValuePins(LogicValueMap *value_map,
			       PinSeq &pins) const
{
  LogicValueMap::ConstIterator value_iter(value_map);
  while (value_iter.hasNext()) {
    LogicValue value;
    const Pin *pin;
    value_iter.next(pin, value);
    pins.push_back(const_cast<Pin*>(pin));
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

  NetDeratingFactorsMap::Iterator net_iter(sdc_->net_derating_factors_);
  while (net_iter.hasNext()) {
    const Net *net;
    DeratingFactorsNet *factors;
    net_iter.next(net, factors);
    WriteGetNet write_net(net, this);
    for (auto early_late : EarlyLate::range()) {
      writeDerating(factors, TimingDerateType::net_delay, early_late,
		    &write_net);
    }
  }

  InstDeratingFactorsMap::Iterator inst_iter(sdc_->inst_derating_factors_);
  while (inst_iter.hasNext()) {
    const Instance *inst;
    DeratingFactorsCell *factors;
    inst_iter.next(inst, factors);
    WriteGetInstance write_inst(inst, this);
    writeDerating(factors, &write_inst);
  }

  CellDeratingFactorsMap::Iterator cell_iter(sdc_->cell_derating_factors_);
  while (cell_iter.hasNext()) {
    const LibertyCell *cell;
    DeratingFactorsCell *factors;
    cell_iter.next(cell, factors);
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
	fprintf(stream_, "set_timing_derate %s ", earlyLateFlag(early_late));
	writeFloat(delay_value);
	fprintf(stream_, "\n");
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
    DeratingFactors *delay_factors=factors->factors(TimingDerateType::cell_delay);
    writeDerating(delay_factors, TimingDerateType::cell_delay, early_late, write_obj);
    DeratingFactors *check_factors=factors->factors(TimingDerateType::cell_check);
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
      fprintf(stream_, "set_timing_derate %s %s ",
	      type_key,
	      earlyLateFlag(early_late));
      writeFloat(value);
      if (write_obj) {
	fprintf(stream_, " ");
	write_obj->write();
      }
      fprintf(stream_, "\n");
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
	  fprintf(stream_, "set_timing_derate %s %s %s ",
		  type_key,
		  earlyLateFlag(early_late),
		  clk_data_key);
	  writeFloat(value);
	  if (write_obj) {
	    fprintf(stream_, " ");
	    write_obj->write();
	  }
	  fprintf(stream_, "\n");
	}
      }
      else {
	for (auto tr : RiseFall::range()) {
	  float factor;
	  bool exists;
	  factors->factor(clk_data, tr, early_late, factor, exists);
	  if (exists) {
	    fprintf(stream_, "set_timing_derate %s %s %s %s ",
		    type_key,
		    clk_data_key,
		    transRiseFallFlag(tr),
		    earlyLateFlag(early_late));
	    writeFloat(factor);
	    if (write_obj) {
	      fprintf(stream_, " ");
	      write_obj->write();
	    }
	    fprintf(stream_, "\n");
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
  PinMinPulseWidthMap::Iterator
    pin_iter(sdc_->pin_min_pulse_width_map_);
  while (pin_iter.hasNext()) {
    const Pin *pin;
    RiseFallValues *min_widths;
    pin_iter.next(pin, min_widths);
    WriteGetPin write_obj(pin, false, this);
    writeMinPulseWidths(min_widths, write_obj);
  }
  InstMinPulseWidthMap::Iterator
    inst_iter(sdc_->inst_min_pulse_width_map_);
  while (inst_iter.hasNext()) {
    const Instance *inst;
    RiseFallValues *min_widths;
    inst_iter.next(inst, min_widths);
    WriteGetInstance write_obj(inst, this);
    writeMinPulseWidths(min_widths, write_obj);
  }
  ClockMinPulseWidthMap::Iterator
    clk_iter(sdc_->clk_min_pulse_width_map_);
  while (clk_iter.hasNext()) {
    const Clock *clk;
    RiseFallValues *min_widths;
    clk_iter.next(clk, min_widths);
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
  fprintf(stream_, "set_min_pulse_width %s", hi_low);
  writeTime(value);
  fprintf(stream_, " ");
  write_obj.write();
  fprintf(stream_, "\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeLatchBorowLimits() const
{
  PinLatchBorrowLimitMap::Iterator
    pin_iter(sdc_->pin_latch_borrow_limit_map_);
  while (pin_iter.hasNext()) {
    const Pin *pin;
    float limit;
    pin_iter.next(pin, limit);
    fprintf(stream_, "set_max_time_borrow ");
    writeTime(limit);
    fprintf(stream_, " ");
    writeGetPin(pin, false);
    fprintf(stream_, "\n");
  }
  InstLatchBorrowLimitMap::Iterator
    inst_iter(sdc_->inst_latch_borrow_limit_map_);
  while (inst_iter.hasNext()) {
    const Instance *inst;
    float limit;
    inst_iter.next(inst, limit);
    fprintf(stream_, "set_max_time_borrow ");
    writeTime(limit);
    fprintf(stream_, " ");
    writeGetInstance(inst);
    fprintf(stream_, "\n");
  }
  ClockLatchBorrowLimitMap::Iterator
    clk_iter(sdc_->clk_latch_borrow_limit_map_);
  while (clk_iter.hasNext()) {
    const Clock *clk;
    float limit;
    clk_iter.next(clk, limit);
    fprintf(stream_, "set_max_time_borrow ");
    writeTime(limit);
    fprintf(stream_, " ");
    writeGetClock(clk);
    fprintf(stream_, "\n");
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
    fprintf(stream_, "set_max_transition ");
    writeTime(slew);
    fprintf(stream_, " [current_design]\n");
  }

  CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
  while (port_iter->hasNext()) {
    Port *port = port_iter->next();
    sdc_->slewLimit(port, min_max, slew, exists);
    if (exists) {
      fprintf(stream_, "set_max_transition ");
      writeTime(slew);
      fprintf(stream_, " ");
      writeGetPort(port);
      fprintf(stream_, "\n");
    }
  }
  delete port_iter;

  ConstPinSeq pins;
  sdc_->slewLimitPins(pins);
  sort(pins, PinPathNameLess(network_));
  ConstPinSeq::Iterator pin_iter(pins);
  while (pin_iter.hasNext()) {
    const Pin *pin = pin_iter.next();
    float slew;
    bool exists;
    sdc_->slewLimit(pin, min_max, slew, exists);
    if (exists) {
      fprintf(stream_, "set_max_transition ");
      writeTime(slew);
      fprintf(stream_, " ");
      writeGetPin(pin, false);
      fprintf(stream_, "\n");
    }
  }

  writeClkSlewLimits();
}

void
WriteSdc::writeClkSlewLimits() const
{
  const MinMax *min_max = MinMax::max();
  ClockSeq clks;
  sdc_->sortedClocks(clks);
  ClockSeq::Iterator clk_iter(clks);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
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
  fprintf(stream_, "set_max_transition %s%s", clk_data, rise_fall);
  writeTime(limit);
  fprintf(stream_, " ");
  writeGetClock(clk);
  fprintf(stream_, "\n");
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
    fprintf(stream_, "%s ", cmd);
    writeCapacitance(cap);
    fprintf(stream_, " [current_design]\n");
  }

  PortCapLimitMap::Iterator port_iter(sdc_->port_cap_limit_map_);
  while (port_iter.hasNext()) {
    Port *port;
    MinMaxFloatValues values;
    port_iter.next(port, values);
    float cap;
    bool exists;
    values.value(min_max, cap, exists);
    if (exists) {
      fprintf(stream_, "%s ", cmd);
      writeCapacitance(cap);
      fprintf(stream_, " ");
      writeGetPort(port);
      fprintf(stream_, "\n");
    }
  }

  PinCapLimitMap::Iterator pin_iter(sdc_->pin_cap_limit_map_);
  while (pin_iter.hasNext()) {
    Pin *pin;
    MinMaxFloatValues values;
    pin_iter.next(pin, values);
    float cap;
    bool exists;
    values.value(min_max, cap, exists);
    if (exists) {
      fprintf(stream_, "%s ", cmd);
      writeCapacitance(cap);
      fprintf(stream_, " ");
      writeGetPin(pin, false);
      fprintf(stream_, "\n");
    }
  }
}

void
WriteSdc::writeMaxArea() const
{
  float max_area = sdc_->maxArea();
  if (max_area > 0.0) {
    fprintf(stream_, "set_max_area ");
    writeFloat(max_area);
    fprintf(stream_, "\n");
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
    fprintf(stream_, "%s ", cmd);
    writeFloat(fanout);
    fprintf(stream_, " [current_design]\n");
  }
  else {
    CellPortBitIterator *port_iter = sdc_network_->portBitIterator(cell_);
    while (port_iter->hasNext()) {
      Port *port = port_iter->next();
      sdc_->fanoutLimit(port, min_max, fanout, exists);
      if (exists) {
	fprintf(stream_, "%s ", cmd);
	writeFloat(fanout);
	fprintf(stream_, " ");
	writeGetPort(port);
	fprintf(stream_, "\n");
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
      fprintf(stream_, "set sta_propagate_all_clocks 1\n");
    else
      fprintf(stream_, "set timing_all_clocks_propagated true\n");
  }
  if (sdc_->presetClrArcsEnabled()) {
    if (native_)
      fprintf(stream_, "set sta_preset_clear_arcs_enabled 1\n");
    else
      fprintf(stream_, "set timing_enable_preset_clear_arcs true\n");
  }
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeGetTimingArcsOfOjbects(LibertyCell *cell) const
{
  fprintf(stream_, "[%s -of_objects ", getTimingArcsCmd());
  writeGetLibCell(cell);
  fprintf(stream_, "]");
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
  fprintf(stream_, "[%s -from ", getTimingArcsCmd());
  Vertex *from_vertex = edge->from(graph_);
  writeGetPin(from_vertex->pin(), true);
  fprintf(stream_, " -to ");
  Vertex *to_vertex = edge->to(graph_);
  writeGetPin(to_vertex->pin(), false);
  if (filter)
    fprintf(stream_, " -filter {%s}", filter);
  fprintf(stream_, "]");
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
  fprintf(stream_, "[get_lib_cells {%s/%s}]",
	  cell->libertyLibrary()->name(),
	  cell->name());
}

void
WriteSdc::writeGetLibPin(const LibertyPort *port) const
{
  LibertyCell *cell = port->libertyCell();
  LibertyLibrary *lib = cell->libertyLibrary();
  fprintf(stream_, "[get_lib_pins {%s/%s/%s}]",
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
    fprintf(stream_, "[list ");
  writeGetClocks(clks, multiple, first);
  if (multiple)
    fprintf(stream_, "]");
}

void
WriteSdc::writeGetClocks(ClockSet *clks,
			 bool multiple,
			 bool &first) const
{
  ClockSeq clks1;
  sortClockSet(clks, clks1);
  ClockSeq::Iterator clk_iter(clks1);
  while (clk_iter.hasNext()) {
    Clock *clk = clk_iter.next();
    if (multiple && !first)
      fprintf(stream_, "\\\n           ");
    writeGetClock(clk);
    first = false;
  }
}

void
WriteSdc::writeGetClock(const Clock *clk) const
{
  fprintf(stream_, "[get_clocks {%s}]",
	  clk->name());
}

void
WriteSdc::writeGetPort(const Port *port) const
{
  fprintf(stream_, "[get_ports {%s}]", sdc_network_->name(port));
}

void
WriteSdc::writeGetPins(PinSet *pins,
		       bool map_hpin_to_drvr) const
{
  PinSeq pins1;
  if (map_hpins_) {
    PinSet leaf_pins;
    for (Pin *pin : *pins) {
      if (network_->isHierarchical(pin)) {
	if (map_hpin_to_drvr)
	  findLeafDriverPins(const_cast<Pin*>(pin), network_, &leaf_pins);
	else
	  findLeafLoadPins(const_cast<Pin*>(pin), network_, &leaf_pins);
      }
      else
	leaf_pins.insert(pin);
    }
    sortPinSet(&leaf_pins, sdc_network_, pins1);
  }
  else
    sortPinSet(pins, sdc_network_, pins1);
  writeGetPins1(&pins1);
}

void
WriteSdc::writeGetPins1(PinSeq *pins) const
{
  bool multiple = pins->size() > 1;
  if (multiple)
    fprintf(stream_, "[list ");
  PinSeq::Iterator pin_iter(pins);
  bool first = true;
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    if (multiple && !first)
      fprintf(stream_, "\\\n          ");
    writeGetPin(pin);
    first = false;
  }
  if (multiple)
    fprintf(stream_, "]");
}

void
WriteSdc::writeGetPin(const Pin *pin) const
{
  if (sdc_network_->instance(pin) == instance_)
    fprintf(stream_, "[get_ports {%s}]", sdc_network_->portName(pin));
  else
    fprintf(stream_, "[get_pins {%s}]", pathName(pin));
}

void
WriteSdc::writeGetPin(const Pin *pin,
		      bool map_hpin_to_drvr) const
{
  if (map_hpins_ && network_->isHierarchical(pin)) {
    PinSet pins;
    pins.insert(const_cast<Pin*>(pin));
    writeGetPins(&pins, map_hpin_to_drvr);
  }
  else
    writeGetPin(pin);
}

void
WriteSdc::writeGetNet(const Net *net) const
{
  fprintf(stream_, "[get_nets {%s}]", pathName(net));
}

void
WriteSdc::writeGetInstance(const Instance *inst) const
{
  fprintf(stream_, "[get_cells {%s}]", pathName(inst));
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
  fprintf(stream_, "# %s\n", line);
  writeCommentSeparator();
}

void
WriteSdc::writeCommentSeparator() const
{
  fprintf(stream_, "###############################################################################\n");
}

////////////////////////////////////////////////////////////////

void
WriteSdc::writeRiseFallMinMaxTimeCmd(const char *sdc_cmd,
				     RiseFallMinMax *values,
				     WriteSdcObject &write_object) const
{
  writeRiseFallMinMaxCmd(sdc_cmd, values, units_->timeUnit()->scale(),
			 write_object);
}

void
WriteSdc::writeRiseFallMinMaxCapCmd(const char *sdc_cmd,
				    RiseFallMinMax *values,
				    WriteSdcObject &write_object) const
{
  writeRiseFallMinMaxCmd(sdc_cmd, values, units_->capacitanceUnit()->scale(),
			 write_object);
}

void
WriteSdc::writeRiseFallMinMaxCmd(const char *sdc_cmd,
				 RiseFallMinMax *values,
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
  fprintf(stream_, "%s%s%s ",
	  sdc_cmd,
	  transRiseFallFlag(rf),
	  minMaxFlag(min_max));
  writeFloat(value / scale);
  fprintf(stream_, " ");
  write_object.write();
  fprintf(stream_, "\n");
}

void
WriteSdc::writeClockKey(const Clock *clk) const
{
  fprintf(stream_, " -clock ");
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
  fprintf(stream_, "%s%s ",
	  sdc_cmd,
	  minMaxFlag(min_max));
  writeFloat(value / scale);
  fprintf(stream_, " ");
  write_object.write();
  fprintf(stream_, "\n");
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
  fprintf(stream_, "%s%s ",
	  sdc_cmd,
	  minMaxFlag(min_max));
  fprintf(stream_, "%d ", value);
  write_object.write();
  fprintf(stream_, "\n");
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
  fprintf(stream_, "%.*f", digits_, value);
}

void
WriteSdc::writeTime(float time) const
{
  fprintf(stream_, "%.*f", digits_, scaleTime(time));
}

void
WriteSdc::writeCapacitance(float cap) const
{
  fprintf(stream_, "%.*f", digits_, scaleCapacitance(cap));
}

void
WriteSdc::writeResistance(float res) const
{
  fprintf(stream_, "%.*f", digits_, scaleResistance(res));
}

void
WriteSdc::writeFloatSeq(FloatSeq *floats,
			float scale) const
{
  fprintf(stream_, "{");
  FloatSeq::ConstIterator iter(floats);
  bool first = true;
  while (iter.hasNext()) {
    float flt = iter.next();
    if (!first)
      fprintf(stream_, " ");
    writeFloat(flt * scale);
    first = false;
  }
  fprintf(stream_, "}");
}

void
WriteSdc::writeIntSeq(IntSeq *ints) const
{
  fprintf(stream_, "{");
  IntSeq::ConstIterator iter(ints);
  bool first = true;
  while (iter.hasNext()) {
    int i = iter.next();
    if (!first)
      fprintf(stream_, " ");
    fprintf(stream_, "%d", i);
    first = false;
  }
  fprintf(stream_, "}");
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
  else if (rf == RiseFallBoth::riseFall())
    return "";
  else {
    internalError("unknown transition");
  }
  return nullptr;
}

static const char *
minMaxFlag(const MinMaxAll *min_max)
{
  if (min_max == MinMaxAll::all())
    return "";
  else if (min_max == MinMaxAll::min())
    return " -min";
  else if (min_max == MinMaxAll::max())
    return " -max";
  else {
    internalError("unknown MinMaxAll");
    return nullptr;
  }
}

static const char *
minMaxFlag(const MinMax *min_max)
{
  if (min_max == MinMax::min())
    return " -min";
  else if (min_max == MinMax::max())
    return " -max";
  else {
    internalError("unknown MinMax");
    return nullptr;
  }
}

static const char *
earlyLateFlag(const MinMax *early_late)
{
  if (early_late == MinMax::min())
    return "-early";
  else if (early_late == MinMax::max())
    return "-late";
  else {
    internalError("unknown EarlyLate");
    return nullptr;
  }
}

void
WriteSdc::writeSetupHoldFlag(const MinMaxAll *min_max) const
{
  if (min_max == MinMaxAll::min())
    fprintf(stream_,  " -hold");
  else if (min_max == MinMaxAll::max())
    fprintf(stream_,  " -setup");
}

static const char *
setupHoldFlag(const MinMax *min_max)
{
  if (min_max == MinMax::min())
    return " -hold";
  else if (min_max == MinMax::max())
    return " -setup";
  else {
    internalError("unknown MinMax");
    return nullptr;
  }
}

void
WriteSdc::writeCmdComment(SdcCmdComment *cmd) const
{
  const char *comment = cmd->comment();
  if (comment) {
    fprintf(stream_,  " -comment {%s}", comment);
  }
}

} // namespace
