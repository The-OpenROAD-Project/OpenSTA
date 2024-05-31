// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#include "ArcDelayCalc.hh"

#include "Liberty.hh"
#include "TimingArc.hh"
#include "Network.hh"

namespace sta {

ArcDelayCalc::ArcDelayCalc(StaState *sta):
  StaState(sta)
{
}

void
ArcDelayCalc::gateDelay(const TimingArc *arc,
                        const Slew &in_slew,
                        float load_cap,
                        const Parasitic *parasitic,
                        float,
                        const Pvt *,
                        const DcalcAnalysisPt *dcalc_ap,
                        // Return values.
                        ArcDelay &gate_delay,
                        Slew &drvr_slew)
{
  LoadPinIndexMap load_pin_index_map(network_);
  ArcDcalcResult dcalc_result = gateDelay(nullptr, arc, in_slew, load_cap, parasitic,
                                          load_pin_index_map, dcalc_ap);
  gate_delay = dcalc_result.gateDelay();
  drvr_slew = dcalc_result.drvrSlew();
}

////////////////////////////////////////////////////////////////

ArcDcalcArg::ArcDcalcArg() :
  in_pin_(nullptr),
  drvr_pin_(nullptr),
  edge_(nullptr),
  arc_(nullptr),
  in_slew_(0.0),
  parasitic_(nullptr),
  input_delay_(0.0)
{
}

ArcDcalcArg::ArcDcalcArg(const Pin *in_pin,
                         const Pin *drvr_pin,
                         Edge *edge,
                         const TimingArc *arc,
                         const Slew in_slew,
                         const Parasitic *parasitic) :
  in_pin_(in_pin),
  drvr_pin_(drvr_pin),
  edge_(edge),
  arc_(arc),
  in_slew_(in_slew),
  parasitic_(parasitic),
  input_delay_(0.0)
{
}

ArcDcalcArg::ArcDcalcArg(const Pin *in_pin,
                         const Pin *drvr_pin,
                         Edge *edge,
                         const TimingArc *arc,
                         float input_delay) :
  in_pin_(in_pin),
  drvr_pin_(drvr_pin),
  edge_(edge),
  arc_(arc),
  in_slew_(0.0),
  parasitic_(nullptr),
  input_delay_(input_delay)
{
}

ArcDcalcArg::ArcDcalcArg(const ArcDcalcArg &arg) :
  in_pin_(arg.in_pin_),
  drvr_pin_(arg.drvr_pin_),
  edge_(arg.edge_),
  arc_(arg.arc_),
  in_slew_(arg.in_slew_),
  parasitic_(arg.parasitic_),
  input_delay_(arg.input_delay_)
{
}

const RiseFall *
ArcDcalcArg::inEdge() const
{
  return arc_->fromEdge()->asRiseFall();
}

LibertyCell *
ArcDcalcArg::drvrCell() const
{

  return arc_->to()->libertyCell();
}

const LibertyLibrary *
ArcDcalcArg::drvrLibrary() const
{
  return arc_->to()->libertyLibrary();
}

const RiseFall *
ArcDcalcArg::drvrEdge() const
{
  return arc_->toEdge()->asRiseFall();
}

const Net *
ArcDcalcArg::drvrNet(const Network *network) const
{
  return network->net(drvr_pin_);
}

float
ArcDcalcArg::inSlewFlt() const
{
  return delayAsFloat(in_slew_);
}

void
ArcDcalcArg::setInSlew(Slew in_slew)
{
  in_slew_ = in_slew;
}

void
ArcDcalcArg::setParasitic(const Parasitic *parasitic)
{
  parasitic_ = parasitic;
}

////////////////////////////////////////////////////////////////

ArcDcalcResult::ArcDcalcResult() :
  gate_delay_(0.0),
  drvr_slew_(0.0)
{
}

ArcDcalcResult::ArcDcalcResult(size_t load_count) :
  gate_delay_(0.0),
  drvr_slew_(0.0)
{
  wire_delays_.resize(load_count);
  load_slews_.resize(load_count);
}

void
ArcDcalcResult::setGateDelay(ArcDelay gate_delay)
{
  gate_delay_ = gate_delay;
}

void
ArcDcalcResult::setDrvrSlew(Slew drvr_slew)
{
  drvr_slew_ = drvr_slew;
}

ArcDelay
ArcDcalcResult::wireDelay(size_t load_idx) const
{
  return wire_delays_[load_idx];
}

void
ArcDcalcResult::setWireDelay(size_t load_idx,
                             ArcDelay wire_delay)
{
  wire_delays_[load_idx] = wire_delay;
}

void
ArcDcalcResult::setLoadCount(size_t load_count)
{
  wire_delays_.resize(load_count);
  load_slews_.resize(load_count);
}

Slew
ArcDcalcResult::loadSlew(size_t load_idx) const
{
  return load_slews_[load_idx];
}

void
ArcDcalcResult::setLoadSlew(size_t load_idx,
                            Slew load_slew)
{
  load_slews_[load_idx] = load_slew;
}

} // namespace
