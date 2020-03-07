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

#include <algorithm>
#include "Machine.hh"
#include "Error.hh"
#include "StringUtil.hh"
#include "Transition.hh"
#include "TimingRole.hh"
#include "MinMax.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "Clock.hh"

namespace sta {

static bool
isPowerOfTwo(int i);

Clock::Clock(const char *name,
	     int index) :
  name_(stringCopy(name)),
  add_to_pins_(false),
  pll_out_(nullptr),
  pll_fdbk_(nullptr),
  period_(0.0),
  waveform_(nullptr),
  waveform_valid_(false),
  index_(index),
  clk_edges_(nullptr),
  is_propagated_(false),
  uncertainties_(nullptr),
  is_generated_(false),
  src_pin_(nullptr),
  master_clk_(nullptr),
  master_clk_infered_(false),
  divide_by_(0),
  multiply_by_(0),
  duty_cycle_(0),
  invert_(false),
  combinational_(false),
  edges_(nullptr),
  edge_shifts_(nullptr)
{
  makeClkEdges();
}

void
Clock::initClk(PinSet *pins,
	       bool add_to_pins,
	       float period,
	       FloatSeq *waveform,
	       const char *comment,
	       const Network *network)
{
  is_generated_ = false;
  setPins(pins, network);
  add_to_pins_ = add_to_pins;
  delete waveform_;
  waveform_ = waveform;
  waveform_valid_ = true;
  period_ = period;
  setClkEdgeTimes();
  setComment(comment);
}

bool
Clock::isVirtual() const
{
  return pins_.empty();
}

void
Clock::setPins(PinSet *pins,
	       const Network *network)
{
  if (pins)
    pins_ = *pins;
  delete pins;
  makeLeafPins(network);
}

void
Clock::makeLeafPins(const Network *network)
{
  leaf_pins_.clear();
  PinSet::Iterator pin_iter(pins_);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    findLeafDriverPins(pin, network, &leaf_pins_);
  }
}

void
Clock::setMasterClk(Clock *master)
{
  master_clk_ = master;
  waveform_valid_ = false;
}

void
Clock::makeClkEdges()
{
  clk_edges_ = new ClockEdge*[RiseFall::index_count];
  for (auto tr : RiseFall::range()) {
    clk_edges_[tr->index()] = new ClockEdge(this, tr);
  }
}

Clock::~Clock()
{
  stringDelete(name_);
  if (clk_edges_) {
    delete clk_edges_[RiseFall::riseIndex()];
    delete clk_edges_[RiseFall::fallIndex()];
    delete [] clk_edges_;
  }
  delete waveform_;
  delete edges_;
  delete edge_shifts_;
  delete uncertainties_;
}

void
Clock::addPin(Pin *pin)
{
  pins_.insert(pin);
  leaf_pins_.insert(pin);
}

void
Clock::deletePin(Pin *pin)
{
  pins_.erase(pin);
}

void
Clock::setAddToPins(bool add_to_pins)
{
  add_to_pins_ = add_to_pins;
}

void
Clock::setClkEdgeTimes()
{
  setClkEdgeTime(RiseFall::rise());
  setClkEdgeTime(RiseFall::fall());
}

void
Clock::setClkEdgeTime(const RiseFall *rf)
{
  float time = (rf == RiseFall::rise()) ? (*waveform_)[0]:(*waveform_)[1];
  clk_edges_[rf->index()]->setTime(time);
}

Pin *
Clock::defaultPin() const
{
  PinSet::ConstIterator pin_iter(leaf_pins_);
  if (pin_iter.hasNext())
    return pin_iter.next();
  else
    return nullptr;
}

ClockEdge *
Clock::edge(const RiseFall *rf) const
{
  return clk_edges_[rf->index()];
}

void
Clock::setIsPropagated(bool propagated)
{
  is_propagated_ = propagated;
}

void
Clock::slew(const RiseFall *rf,
	    const MinMax *min_max,
	    // Return values.
	    float &slew,
	    bool &exists) const
{
  slews_.value(rf, min_max, slew, exists);
}

float
Clock::slew(const RiseFall *rf,
	    const MinMax *min_max) const
{
  float slew;
  bool exists;
  slews_.value(rf, min_max, slew, exists);
  if (!exists)
    slew = 0.0;
  return slew;
}

void
Clock::setSlew(const RiseFallBoth *rf,
	       const MinMaxAll *min_max,
	       float slew)
{
  slews_.setValue(rf, min_max, slew);
}

void
Clock::setSlew(const RiseFall *rf,
	       const MinMax *min_max,
	       float slew)
{
  slews_.setValue(rf, min_max, slew);
}

void
Clock::removeSlew()
{
  slews_.clear();
}

void
Clock::setSlewLimit(const RiseFallBoth *rf,
		    const PathClkOrData clk_data,
		    const MinMax *min_max,
		    float slew)
{
  slew_limits_[int(clk_data)].setValue(rf, min_max, slew);
}

void
Clock::slewLimit(const RiseFall *rf,
		 const PathClkOrData clk_data,
		 const MinMax *min_max,
		 // Return values.
		 float &slew,
		 bool &exists) const
{
  slew_limits_[int(clk_data)].value(rf, min_max, slew, exists);
}

void
Clock::uncertainty(const SetupHold *setup_hold,
		   // Return values.
		   float &uncertainty,
		   bool &exists) const
{
  if (uncertainties_)
    uncertainties_->value(setup_hold, uncertainty, exists);
  else {
    uncertainty = 0.0F;
    exists = false;
  }
}

void
Clock::setUncertainty(const SetupHoldAll *setup_hold,
		      float uncertainty)
{
  if (uncertainties_ == nullptr)
    uncertainties_ = new ClockUncertainties;
  uncertainties_->setValue(setup_hold, uncertainty);
}

void
Clock::setUncertainty(const SetupHold *setup_hold,
		      float uncertainty)
{
  if (uncertainties_ == nullptr)
    uncertainties_ = new ClockUncertainties;
  uncertainties_->setValue(setup_hold, uncertainty);
}

void
Clock::removeUncertainty(const SetupHoldAll *setup_hold)
{
  if (uncertainties_) {
    uncertainties_->removeValue(setup_hold);
    if (uncertainties_->empty()) {
      delete uncertainties_;
      uncertainties_ = nullptr;
    }
  }
}

void
Clock::waveformInvalid()
{
  waveform_valid_ = false;
}

////////////////////////////////////////////////////////////////

void
Clock::initGeneratedClk(PinSet *pins,
			bool add_to_pins,
			Pin *src_pin,
			Clock *master_clk,
			Pin *pll_out,
			Pin *pll_fdbk,
			int divide_by,
			int multiply_by,
			float duty_cycle,
			bool invert,
			bool combinational,
			IntSeq *edges,
			FloatSeq *edge_shifts,
			bool is_propagated,
			const char *comment,
			const Network *network)
{
  is_generated_ = true;
  setPins(pins, network);
  add_to_pins_ = add_to_pins;
  src_pin_ = src_pin;
  master_clk_ = master_clk;
  master_clk_infered_ = false;
  waveform_valid_ = false;
  pll_out_= pll_out;
  pll_fdbk_ = pll_fdbk;
  divide_by_ = divide_by;
  multiply_by_ = multiply_by;
  duty_cycle_ = duty_cycle;
  invert_ = invert;
  combinational_ = combinational;
  is_propagated_ = is_propagated;
  setComment(comment);

  delete edges_;
  if (edges
      && edges->empty()) {
    delete edges;
    edges = nullptr;
  }
  edges_ = edges;

  delete edge_shifts_;
  if (edge_shifts
      && edge_shifts->empty()) {
    delete edge_shifts;
    edge_shifts = nullptr;
  }
  edge_shifts_ = edge_shifts;
}

void
Clock::setInferedMasterClk(Clock *master_clk)
{
  master_clk_ = master_clk;
  master_clk_infered_ = true;
  waveform_valid_ = false;
}

bool
Clock::isGenerated() const
{
  return is_generated_;
}

bool
Clock::isGeneratedWithPropagatedMaster() const
{
  return is_generated_
    && master_clk_
    // Insertion is zero if the master clock is ideal.
    && master_clk_->isPropagated();
}

void
Clock::generate(const Clock *src_clk)
{
  if (waveform_ == nullptr)
    waveform_ = new FloatSeq;
  else
    waveform_->clear();

  if (divide_by_ == 1.0) {
    period_ = src_clk->period();
    const FloatSeq *src_wave = src_clk->waveform();
    waveform_->push_back((*src_wave)[0]);
    waveform_->push_back((*src_wave)[1]);
  }
  else if (divide_by_ > 1) {
    if (isPowerOfTwo(divide_by_)) {
      period_ = src_clk->period() * divide_by_;
      const FloatSeq *src_wave = src_clk->waveform();
      float rise = (*src_wave)[0];
      waveform_->push_back(rise);
      waveform_->push_back(rise + period_ / 2);
    }
    else
      generateScaledClk(src_clk, static_cast<float>(divide_by_));
  }
  else if (multiply_by_ >= 1)
    generateScaledClk(src_clk, 1.0F / multiply_by_);
  else if (edges_)
    generateEdgesClk(src_clk);

  if (invert_) {
    float first_time = (*waveform_)[0];
    float offset = (first_time >= period_) ? period_ : 0.0F;
    size_t edge_count = waveform_->size();
    for (size_t i = 0; i < edge_count - 1; i++)
      (*waveform_)[i] = (*waveform_)[i + 1] - offset;
    (*waveform_)[edge_count - 1] = first_time - offset + period_;
  }
  setClkEdgeTimes();
  waveform_valid_ = true;
}

void
Clock::generateScaledClk(const Clock *src_clk,
			 float scale)
{
  period_ = src_clk->period() * scale;
  if (duty_cycle_ != 0.0) {
    float rise = (*src_clk->waveform())[0] * scale;
    waveform_->push_back(rise);
    waveform_->push_back(rise + period_ * duty_cycle_ / 100.0F);
  }
  else {
    FloatSeq::ConstIterator wave_iter(src_clk->waveform());
    while (wave_iter.hasNext()) {
      float time = wave_iter.next();
      waveform_->push_back(time * scale);
    }
  }
}

void
Clock::generateEdgesClk(const Clock *src_clk)
{
  // The create_generated_clock tcl cmd and Sta::makeClock
  // enforce this restriction.
  if (edges_->size() == 3) {
    const FloatSeq *src_wave = src_clk->waveform();
    size_t src_size = src_wave->size();
    float src_period = src_clk->period();

    int edge0_1 = (*edges_)[0] - 1;
    float rise = (*src_wave)[edge0_1 % src_size]
      + (edge0_1 / src_size) * src_period;
    if (edge_shifts_)
      rise += (*edge_shifts_)[0];
    waveform_->push_back(rise);

    int edge1_1 = (*edges_)[1] - 1;
    float fall = (*src_wave)[edge1_1 % src_size]
      + (edge1_1 / src_size) * src_period;
    if (edge_shifts_)
      fall += (*edge_shifts_)[1];
    waveform_->push_back(fall);

    int edge2_1 = (*edges_)[2] - 1;
    period_ = (*src_wave)[edge2_1 % src_size]
      + (edge2_1 / src_size) * src_period - rise;
    if (edge_shifts_)
      period_ += (*edge_shifts_)[2];
  }
  else
    internalError("generated clock edges size is not three.");
}

static bool
isPowerOfTwo(int i)
{
  return (i & (i - 1)) == 0;
}

const RiseFall *
Clock::masterClkEdgeTr(const RiseFall *rf) const
{
  int edge_index = (rf == RiseFall::rise()) ? 0 : 1;
  return ((*edges_)[edge_index] - 1) % 2 
    ? RiseFall::fall()
    : RiseFall::rise();
}

void
Clock::srcPinVertices(VertexSet &src_vertices,
		      const Network *network,
		      Graph *graph)
{
  if (network->isHierarchical(src_pin_)) {
    // Use the clocks on a non-hierarchical pin on the same net.
    PinSet leaf_pins;
    findLeafDriverPins(src_pin_, network, &leaf_pins);
    PinSet::Iterator pin_iter(leaf_pins);
    while (pin_iter.hasNext()) {
      Pin *pin = pin_iter.next();
      Vertex *vertex, *bidirect_drvr_vertex;
      graph->pinVertices(pin, vertex, bidirect_drvr_vertex);
      if (vertex)
	src_vertices.insert(vertex);
      if (bidirect_drvr_vertex)
	src_vertices.insert(bidirect_drvr_vertex);
    }
  }
  else {
    Vertex *vertex = graph->pinDrvrVertex(src_pin_);
    src_vertices.insert(vertex);
  }
}

bool
Clock::isDivideByOneCombinational() const
{
  return combinational_
    && divide_by_ == 1
    && multiply_by_ == 0
    && edge_shifts_ == 0;
}

////////////////////////////////////////////////////////////////

ClockEdge::ClockEdge(Clock *clock,
		     RiseFall *rf) :
  clock_(clock),
  rf_(rf),
  name_(stringPrint("%s %s", clock_->name(), rf_->asString())),
  time_(0.0),
  index_(clock_->index() * RiseFall::index_count + rf_->index())
{
}

ClockEdge::~ClockEdge()
{
  stringDelete(name_);
}

void
ClockEdge::setTime(float time)
{
  time_ = time;
}

ClockEdge *
ClockEdge::opposite() const
{
  return clock_->edge(rf_->opposite());
}

float
ClockEdge::pulseWidth() const
{
  ClockEdge *opp_clk_edge = opposite();
  float width = opp_clk_edge->time() - time_;
  if (width < 0.0)
    width += clock_->period();
  return width;
}

////////////////////////////////////////////////////////////////

int
clkCmp(const Clock *clk1,
       const Clock *clk2)
{
  if (clk1 == nullptr && clk2)
    return -1;
  else if (clk1 == nullptr && clk2 == nullptr)
    return 0;
  else if (clk1 && clk2 == nullptr)
    return 1;
  else {
    int index1 = clk1->index();
    int index2 = clk2->index();
    if (index1 < index2)
      return -1;
    else if (index1 == index2)
      return 0;
    else
      return 1;
  }
}

int
clkEdgeCmp(ClockEdge *clk_edge1,
	   ClockEdge *clk_edge2)
{
  if (clk_edge1 == nullptr && clk_edge2)
    return -1;
  else if (clk_edge1 == nullptr && clk_edge2 == nullptr)
    return 0;
  else if (clk_edge1 && clk_edge2 == nullptr)
    return 1;
  else {
    int index1 = clk_edge1->index();
    int index2 = clk_edge2->index();
    if (index1 == index2)
      return 0;
    else if (index1 < index2)
      return -1;
    else
      return 1;
  }
}

bool
clkEdgeLess(ClockEdge *clk_edge1,
	    ClockEdge *clk_edge2)
{
  return clkEdgeCmp(clk_edge1, clk_edge2) < 0;
}

////////////////////////////////////////////////////////////////

InterClockUncertainty::InterClockUncertainty(const Clock *src,
					     const Clock *target) :
  src_(src),
  target_(target)
{
}

bool
InterClockUncertainty::empty() const
{
  return uncertainties_[RiseFall::riseIndex()].empty()
    && uncertainties_[RiseFall::fallIndex()].empty();
}

void
InterClockUncertainty::uncertainty(const RiseFall *src_rf,
				   const RiseFall *tgt_rf,
				   const SetupHold *setup_hold,
				   float &uncertainty,
				   bool &exists) const
{
  uncertainties_[src_rf->index()].value(tgt_rf, setup_hold,
					uncertainty, exists);
}

void
InterClockUncertainty::setUncertainty(const RiseFallBoth *src_rf,
				      const RiseFallBoth *tgt_rf,
				      const SetupHoldAll *setup_hold,
				      float uncertainty)
{
  for (auto src_rf_index : src_rf->rangeIndex())
    uncertainties_[src_rf_index].setValue(tgt_rf, setup_hold, uncertainty);
}

void
InterClockUncertainty::removeUncertainty(const RiseFallBoth *src_rf,
					 const RiseFallBoth *tgt_rf,
					 const SetupHoldAll *setup_hold)
{
  for (auto src_rf_index : src_rf->rangeIndex())
    uncertainties_[src_rf_index].removeValue(tgt_rf, setup_hold);
}

const RiseFallMinMax *
InterClockUncertainty::uncertainties(RiseFall *src_rf) const
{
  return &uncertainties_[src_rf->index()];
}

bool
InterClockUncertaintyLess::operator()(const InterClockUncertainty *inter1,
				      const InterClockUncertainty *inter2)const
{
  return inter1->src()->index() < inter2->src()->index()
    || (inter1->src() == inter2->src()
	&& inter1->target()->index() < inter2->target()->index());
}

////////////////////////////////////////////////////////////////

bool
ClockNameLess::operator()(const Clock *clk1,
			  const Clock *clk2)
{
  return stringLess(clk1->name(), clk2->name());
}

void
sortClockSet(ClockSet *set,
	     ClockSeq &clks)
{
  ClockSet::Iterator clk_iter(set);
  while (clk_iter.hasNext())
    clks.push_back(clk_iter.next());
  sort(clks, ClockNameLess());
}

ClockPinIterator::ClockPinIterator(Clock *clk) :
  PinSet::Iterator(clk->pins())
{
}

} // namespace
