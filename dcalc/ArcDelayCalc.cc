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

#include "Units.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "Network.hh"
#include "Graph.hh"

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

ArcDcalcArg
makeArcDcalcArg(const char *inst_name,
                const char *in_port_name,
                const char *in_rf_name,
                const char *drvr_port_name,
                const char *drvr_rf_name,
                const char *input_delay_str,
                const StaState *sta)
{
  Report *report = sta->report();
  const Network *network = sta->sdcNetwork();
  const Instance *inst = network->findInstance(inst_name);
  if (inst) {
    const Pin *in_pin = network->findPin(inst, in_port_name);
    if (in_pin) {
      const RiseFall *in_rf = RiseFall::find(in_rf_name);
      if (in_rf) {
        const Pin *drvr_pin = network->findPin(inst, drvr_port_name);
        if (drvr_pin) {
          const RiseFall *drvr_rf = RiseFall::find(drvr_rf_name);
          if (drvr_rf) {
            float input_delay = strtof(input_delay_str, nullptr);
            input_delay = sta->units()->timeUnit()->userToSta(input_delay);

            const Graph *graph = sta->graph();
            Edge *edge;
            const TimingArc *arc;
            graph->gateEdgeArc(in_pin, in_rf, drvr_pin, drvr_rf, edge, arc);
            if (edge)
              return ArcDcalcArg(in_pin, drvr_pin, edge, arc, input_delay);
            else {
              const Network *network = sta->network();
              const Instance *inst = network->instance(in_pin);
              report->warn(2100, "no timing arc for %s input/driver pins.",
                           network->pathName(inst));
            }
          }
          else
            report->warn(2101, "%s not a valid rise/fall.", drvr_rf_name);
        }
        else
          report->warn(2102, "Pin %s/%s not found.", inst_name, drvr_port_name);
      }
      else
        report->warn(2103, "%s not a valid rise/fall.", in_rf_name);
    }
    else
      report->warn(2104, "Pin %s/%s not found.", inst_name, in_port_name);
  }
  else
    report->warn(2105, "Instance %s not found.", inst_name);
  return ArcDcalcArg();
}

ArcDcalcArg::ArcDcalcArg() :
  in_pin_(nullptr),
  drvr_pin_(nullptr),
  edge_(nullptr),
  arc_(nullptr),
  in_slew_(0.0),
  load_cap_(0.0),
  parasitic_(nullptr),
  input_delay_(0.0)
{
}

ArcDcalcArg::ArcDcalcArg(const Pin *in_pin,
                         const Pin *drvr_pin,
                         Edge *edge,
                         const TimingArc *arc,
                         const Slew in_slew,
                         float load_cap,
                         const Parasitic *parasitic) :
  in_pin_(in_pin),
  drvr_pin_(drvr_pin),
  edge_(edge),
  arc_(arc),
  in_slew_(in_slew),
  load_cap_(load_cap),
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
  load_cap_(0.0),
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
  load_cap_(arg.load_cap_),
  parasitic_(arg.parasitic_),
  input_delay_(arg.input_delay_)
{
}

const RiseFall *
ArcDcalcArg::inEdge() const
{
  return arc_->fromEdge()->asRiseFall();
}

Vertex *
ArcDcalcArg::drvrVertex(const Graph *graph) const
{
  return edge_->to(graph);
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

void
ArcDcalcArg::setLoadCap(float load_cap)
{
  load_cap_ = load_cap;
}

void
ArcDcalcArg::setInputDelay(float input_delay)
{
  input_delay_ = input_delay;
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
