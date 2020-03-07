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

#include "Machine.hh"
#include "Wireload.hh"
#include "Liberty.hh"
#include "Parasitics.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "EstimateParasitics.hh"

namespace sta {

// For multiple driver nets, output pin capacitances are treated as
// loads when driven by a different pin.
void
EstimateParasitics::estimatePiElmore(const Pin *drvr_pin,
				     const RiseFall *rf,
				     const Wireload *wireload,
				     float fanout,
				     float net_pin_cap,
				     const OperatingConditions *op_cond,
				     const Corner *corner,
				     const MinMax *min_max,
				     const StaState *sta,
				     float &c2,
				     float &rpi,
				     float &c1,
				     float &elmore_res,
				     float &elmore_cap,
				     bool &elmore_use_load_cap)
{
  float wireload_cap, wireload_res;
  wireload->findWireload(fanout, op_cond, wireload_cap, wireload_res);

  WireloadTree tree = WireloadTree::unknown;
  if (op_cond)
    tree = op_cond->wireloadTree();
  switch (tree) {
  case WireloadTree::worst_case:
    estimatePiElmoreWorst(drvr_pin, wireload_cap, wireload_res,
			  fanout, net_pin_cap, rf, op_cond, corner,
			  min_max, sta,
			  c2, rpi, c1, elmore_res,
			  elmore_cap, elmore_use_load_cap);
    break;
  case WireloadTree::balanced:
  case WireloadTree::unknown:
    estimatePiElmoreBalanced(drvr_pin, wireload_cap, wireload_res,
			     fanout, net_pin_cap, rf, op_cond,
			     corner, min_max,sta,
			     c2, rpi, c1, elmore_res,
			     elmore_cap, elmore_use_load_cap);
    break;
  case WireloadTree::best_case:
    estimatePiElmoreBest(drvr_pin, wireload_cap, net_pin_cap, rf,
			 op_cond, corner, min_max,
			 c2, rpi, c1, elmore_res, elmore_cap,
			 elmore_use_load_cap);
    break;
  }
}

// No wire resistance, so load is lumped capacitance.
void
EstimateParasitics::estimatePiElmoreBest(const Pin *,
					 float wireload_cap,
					 float net_pin_cap,
					 const RiseFall *,
					 const OperatingConditions *,
					 const Corner *,
					 const MinMax *,
					 float &c2,
					 float &rpi,
					 float &c1,
					 float &elmore_res,
					 float &elmore_cap,
					 bool &elmore_use_load_cap) const
{
  c2 = wireload_cap + net_pin_cap;
  rpi = 0.0;
  c1 = 0.0;
  elmore_res = 0.0;
  elmore_cap = 0.0;
  elmore_use_load_cap = false;
}

// All load capacitance (except driver pin cap) is on the far side of
// the resistor.
void
EstimateParasitics::estimatePiElmoreWorst(const Pin *drvr_pin,
					  float wireload_cap,
					  float wireload_res,
					  float,
					  float net_pin_cap,
					  const RiseFall *rf,
					  const OperatingConditions *op_cond,
					  const Corner *corner,
					  const MinMax *min_max,
					  const StaState *sta,
					  float &c2,
					  float &rpi,
					  float &c1,
					  float &elmore_res,
					  float &elmore_cap,
					  bool &elmore_use_load_cap)
{
  Sdc *sdc = sta->sdc();
  float drvr_pin_cap = 0.0;
  drvr_pin_cap = sdc->pinCapacitance(drvr_pin, rf, op_cond, corner, min_max);
  c2 = drvr_pin_cap;
  rpi = wireload_res;
  c1 = net_pin_cap - drvr_pin_cap + wireload_cap;
  elmore_res = wireload_res;
  elmore_cap = c1;
  elmore_use_load_cap = false;
}

// Each load capacitance and wireload cap/fanout has resistance/fanout
// connecting it to the driver.
// Use O'Brien/Savarino reduction to rspf (pi elmore) model.
void
EstimateParasitics::estimatePiElmoreBalanced(const Pin *drvr_pin,
					     float wireload_cap,
					     float wireload_res,
					     float fanout,
					     float net_pin_cap,
					     const RiseFall *rf,
					     const OperatingConditions *op_cond,
					     const Corner *corner,
					     const MinMax *min_max,
					     const StaState *sta,
					     float &c2,
					     float &rpi,
					     float &c1,
					     float &elmore_res,
					     float &elmore_cap,
					     bool &elmore_use_load_cap)
{
  if (wireload_res == 0.0) {
    // No resistance, so load is capacitance only.
    c2 = wireload_cap + net_pin_cap;
    rpi = 0.0;
    c1 = 0.0;
    elmore_res = 0.0;
    elmore_cap = 0.0;
    elmore_use_load_cap = false;
  }
  else {
    Sdc *sdc = sta->sdc();
    Network *network = sta->network();
    double res_fanout = wireload_res / fanout;
    double cap_fanout = wireload_cap / fanout;
    // Find admittance moments.
    double y1 = 0.0;
    double y2 = 0.0;
    double y3 = 0.0;
    y1 = sdc->pinCapacitance(drvr_pin, rf, op_cond, corner, min_max);
    PinConnectedPinIterator *load_iter =
      network->connectedPinIterator(drvr_pin);
    while (load_iter->hasNext()) {
      const Pin *load_pin = load_iter->next();
      // Bidirects don't count themselves as loads.
      if (load_pin != drvr_pin && network->isLoad(load_pin)) {
	Port *port = network->port(load_pin);
	double load_cap = 0.0;
	if (network->isLeaf(load_pin))
	  load_cap = sdc->pinCapacitance(load_pin, rf, op_cond,
						 corner, min_max);
	else if (network->isTopLevelPort(load_pin))
	  load_cap = sdc->portExtCap(port, rf, min_max);
	else
	  internalError("load pin not leaf or top level");
	double cap = load_cap + cap_fanout;
	double y2_ = res_fanout * cap * cap;
	y1 += cap;
	y2 += -y2_;
	y3 += y2_ * res_fanout * cap;
      }
    }
    delete load_iter;

    if (y3 == 0) {
      // No loads.
      c1 = 0.0;
      c2 = 0.0;
      rpi = 0.0;
    }
    else {
      c1 = static_cast<float>(y2 * y2 / y3);
      c2 = static_cast<float>(y1 - y2 * y2 / y3);
      rpi = static_cast<float>(-y3 * y3 / (y2 * y2 * y2));
    }
    elmore_res = static_cast<float>(res_fanout);
    elmore_cap = static_cast<float>(cap_fanout);
    elmore_use_load_cap = true;
  }
}

#if 0
static void
selectWireload(Network *network)
{
  // Look for a default wireload selection group.
  WireloadSelection *selection;
  float area = instanceArea(network->topInstance(), network);
  Wireload *wireload = selection->findWireload(area);
}

static float
instanceArea(Instance *inst,
	     Network *network)
{
  float area = 0.0;
  LeafInstanceIterator *inst_iter = network->leafInstanceIterator();
  while (network->hasNext(inst_iter)) {
    Instance *leaf = network->next(inst_iter);
    area += network->cell(leaf)->area();
  }
  return area;
}
#endif

} // namespace
