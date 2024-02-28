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

#pragma once

#include <string>
#include <vector>
#include <map>

#include "MinMax.hh"
#include "LibertyClass.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"
#include "Delay.hh"
#include "ParasiticsClass.hh"
#include "StaState.hh"

namespace sta {

using std::string;
using std::vector;
using std::map;

class Corner;
class Parasitic;
class DcalcAnalysisPt;
class MultiDrvrNet;

// Driver load pin -> index in driver loads.
typedef map<const Pin *, size_t, PinIdLess> LoadPinIndexMap;

// Arguments for gate delay calculation delay/slew at one driver pin
// through one timing arc at one delay calc analysis point.
class ArcDcalcArg
{
public:
  ArcDcalcArg();
  ArcDcalcArg(const Pin *drvr_pin,
               Edge *edge,
               const TimingArc *arc,
               const Slew in_slew,
               const Parasitic *parasitic);
  const Pin *drvrPin() const { return drvr_pin_; }
  Edge *edge() const { return edge_; }
  const TimingArc *arc() const { return arc_; }
  Slew inSlew() const { return in_slew_; }
  const Parasitic *parasitic() { return parasitic_; }
  void setParasitic(const Parasitic *parasitic);

protected:
  const Pin *drvr_pin_;
  Edge *edge_;
  const TimingArc *arc_;
  Slew in_slew_;
  const Parasitic *parasitic_;
};

// Arc delay calc result.
class ArcDcalcResult
{
public:
  ArcDcalcResult();
  ArcDcalcResult(size_t load_count);
  void setLoadCount(size_t load_count);
  ArcDelay &gateDelay() { return gate_delay_; }
  void setGateDelay(ArcDelay gate_delay);
  Slew &drvrSlew() { return drvr_slew_; }
  void setDrvrSlew(Slew drvr_slew);
  ArcDelay wireDelay(size_t load_idx) const;
  void setWireDelay(size_t load_idx,
                    ArcDelay wire_delay);
  Slew loadSlew(size_t load_idx) const;
  void setLoadSlew(size_t load_idx,
                   Slew load_slew);

protected:
  ArcDelay gate_delay_;
  Slew drvr_slew_;
  // Load wire delay and slews indexed by load pin index.
  vector<ArcDelay> wire_delays_;
  vector<Slew> load_slews_;
};

typedef vector<ArcDcalcArg> ArcDcalcArgSeq;
typedef vector<ArcDcalcResult> ArcDcalcResultSeq;

// Delay calculator class hierarchy.
//  ArcDelayCalc
//   UnitDelayCalc
//   DelayCalcBase
//    ParallelDelayCalc
//     LumpedCapDelayCalc
//      DmpCeffDelayCalc
//       DmpCeffElmoreDelayCalc
//       DmpCeffTwoPoleDelayCalc
//      ArnoldiDelayCalc

// Abstract class for the graph delay calculator traversal to interface
// to a delay calculator primitive.
class ArcDelayCalc : public StaState
{
public:
  explicit ArcDelayCalc(StaState *sta);
  virtual ~ArcDelayCalc() {}
  virtual ArcDelayCalc *copy() = 0;

  // Find the parasitic for drvr_pin that is acceptable to the delay
  // calculator by probing parasitics_.
  virtual Parasitic *findParasitic(const Pin *drvr_pin,
				   const RiseFall *rf,
				   const DcalcAnalysisPt *dcalc_ap) = 0;
  // Reduce parasitic_network to a representation acceptable to the delay calculator.
  virtual Parasitic *reduceParasitic(const Parasitic *parasitic_network,
                                     const Pin *drvr_pin,
                                     const RiseFall *rf,
                                     const DcalcAnalysisPt *dcalc_ap) = 0;
  // Reduce parasitic_network to a representation acceptable to the delay calculator
  // for one or more corners and min/max rise/fall.
  // Null corner means reduce all corners.
  virtual void reduceParasitic(const Parasitic *parasitic_network,
                               const Net *net,
                               const Corner *corner,
                               const MinMaxAll *min_max) = 0;
  // Find the wire delays and slews for an input port without a driving cell.
  // This call primarily initializes the load delay/slew iterator.
  virtual ArcDcalcResult inputPortDelay(const Pin *port_pin,
                                        float in_slew,
                                        const RiseFall *rf,
                                        const Parasitic *parasitic,
                                        const LoadPinIndexMap &load_pin_index_map,
                                        const DcalcAnalysisPt *dcalc_ap) = 0;

  // Find the delay and slew for arc driving drvr_pin.
  virtual ArcDcalcResult gateDelay(const Pin *drvr_pin,
                                   const TimingArc *arc,
                                   const Slew &in_slew,
                                   // Pass in load_cap or parasitic.
                                   float load_cap,
                                   const Parasitic *parasitic,
                                   const LoadPinIndexMap &load_pin_index_map,
                                   const DcalcAnalysisPt *dcalc_ap) = 0;
  virtual void gateDelay(const TimingArc *arc,
			 const Slew &in_slew,
			 float load_cap,
			 const Parasitic *parasitic,
			 float related_out_cap,
			 const Pvt *pvt,
			 const DcalcAnalysisPt *dcalc_ap,
			 // Return values.
			 ArcDelay &gate_delay,
			 Slew &drvr_slew);  // __attribute__ ((deprecated));

  // Find gate delays and slews for parallel gates.
  virtual ArcDcalcResultSeq gateDelays(ArcDcalcArgSeq &args,
                                       float load_cap,
                                       const LoadPinIndexMap &load_pin_index_map,
                                       const DcalcAnalysisPt *dcalc_ap) = 0;

  // Find the delay for a timing check arc given the arc's
  // from/clock, to/data slews and related output pin parasitic.
  virtual ArcDelay checkDelay(const Pin *check_pin,
                              const TimingArc *arc,
                              const Slew &from_slew,
                              const Slew &to_slew,
                              float related_out_cap,
                              const DcalcAnalysisPt *dcalc_ap) = 0;
  // Report delay and slew calculation.
  virtual string reportGateDelay(const Pin *drvr_pin,
                                 const TimingArc *arc,
                                 const Slew &in_slew,
                                 float load_cap,
                                 const Parasitic *parasitic,
                                 const LoadPinIndexMap &load_pin_index_map,
                                 const DcalcAnalysisPt *dcalc_ap,
                                 int digits) = 0;
  // Report timing check delay calculation.
  virtual string reportCheckDelay(const Pin *check_pin,
                                  const TimingArc *arc,
                                  const Slew &from_slew,
                                  const char *from_slew_annotation,
                                  const Slew &to_slew,
                                  float related_out_cap,
                                  const DcalcAnalysisPt *dcalc_ap,
                                  int digits) = 0;
  virtual void finishDrvrPin() = 0;
};

} // namespace
