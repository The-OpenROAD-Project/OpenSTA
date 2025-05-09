// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

#include "MinMax.hh"
#include "RiseFallMinMax.hh"
#include "SdcClass.hh"
#include "SdcCmdComment.hh"
#include "GraphClass.hh"

namespace sta {

typedef Map<Pin*, PinSet*> ClkHpinEdgeMap;

class Clock : public SdcCmdComment
{
public:
  ~Clock();
  const char *name() const { return name_; }
  float period() const { return period_; }
  // Virtual clocks have no pins.
  bool isVirtual() const;
  const PinSet &pins() const { return pins_; }
  // The clock source pin's leaf pins.
  //  If the source pin is hierarchical, the leaf pins are:
  //   hierarchical  input - load pins  inside the hierarchical instance
  //   hierarchical output - load pins outside the hierarchical instance
  PinSet &leafPins() { return leaf_pins_; }
  const PinSet &leafPins() const { return leaf_pins_; }
  // Clock pin used by input/output delay for propagated generated
  // clock insertion delay.
  const Pin *defaultPin() const;
  bool addToPins() const { return add_to_pins_; }
  void setAddToPins(bool add_to_pins);
  FloatSeq *waveform() { return waveform_; }
  const FloatSeq *waveform() const { return waveform_; }
  ClockEdge *edge(const RiseFall *rf) const;
  int index() const { return index_; }
  bool isPropagated() const { return is_propagated_; }
  void setIsPropagated(bool propagated);
  bool isIdeal() const { return !is_propagated_; }
  // Ideal clock slew.
  void slew(const RiseFall *rf,
	    const MinMax *min_max,
	    // Return values.
	    float &slew,
	    bool &exists) const;
  // Return zero (default) if no slew exists.
  float slew(const RiseFall *rf,
	     const MinMax *min_max) const;
  void setSlew(const RiseFall *rf,
	       const MinMax *min_max,
	       float slew);
  void setSlew(const RiseFallBoth *rf,
	       const MinMaxAll *min_max,
	       float slew);
  void removeSlew();
  const RiseFallMinMax &slews() const { return slews_; }
  void setSlewLimit(const RiseFallBoth *rf,
		    const PathClkOrData clk_data,
		    const MinMax *min_max,
		    float slew);
  void slewLimit(const RiseFall *rf,
		 const PathClkOrData clk_data,
		 const MinMax *min_max,
		 // Return values.
		 float &slew,
		 bool &exists) const;
  ClockUncertainties *uncertainties() const { return uncertainties_; }
  void uncertainty(const SetupHold *setup_hold,
		   // Return values.
		   float &uncertainty,
		   bool &exists) const;
  void setUncertainty(const SetupHoldAll *setup_hold,
		      float uncertainty);
  void setUncertainty(const SetupHold *setup_hold,
		      float uncertainty);
  void removeUncertainty(const SetupHoldAll *setup_hold);

  void setPeriod(float period);
  void setWaveform(FloatSeq *waveform);

  void addPin(const Pin *pin);
  void deletePin(const Pin *pin);
  void makeLeafPins(const Network *network);

  bool isGenerated() const;
  bool isGeneratedWithPropagatedMaster() const;
  void generate(const Clock *src_clk);
  Pin *srcPin() const { return src_pin_; }
  Clock *masterClk() const { return master_clk_; }
  bool masterClkInfered() const { return master_clk_infered_; }
  void setInferedMasterClk(Clock *master_clk);
  int divideBy() const { return divide_by_; }
  int multiplyBy() const { return multiply_by_; }
  float dutyCycle() const { return duty_cycle_; }
  bool invert() const { return invert_; }
  IntSeq *edges() const { return edges_; }
  FloatSeq *edgeShifts() const { return edge_shifts_; }
  const RiseFall *masterClkEdgeTr(const RiseFall *rf) const;
  bool combinational() const { return combinational_; }
  bool isDivideByOneCombinational() const;
  bool generatedUpToDate() const;
  void srcPinVertices(VertexSet &src_vertices,
		      const Network *network,
		      Graph *graph);
  // True if the generated clock waveform is up to date.
  bool waveformValid() const { return waveform_valid_; }
  void waveformInvalid();

protected:
  // Private to Sdc::makeClock.
  Clock(const char *name,
	int index,
        const Network *network);
  void initClk(PinSet *pins,
	       bool add_to_pins,
	       float period,
	       FloatSeq *waveform,
	       const char *comment,
	       const Network *network);
  void initGeneratedClk(PinSet *pins,
			bool add_to_pins,
			Pin *src_pin,
			Clock *master_clk,
			int divide_by,
			int multiply_by,
			float duty_cycle,
			bool invert,
			bool combinational,
			IntSeq *edges,
			FloatSeq *edge_shifts,
			bool is_propagated,
			const char *comment,
			const Network *network);
  void setPins(PinSet *pins,
	       const Network *network);
  void setMasterClk(Clock *master);
  void makeClkEdges();
  void setClkEdgeTimes();
  void setClkEdgeTime(const RiseFall *rf);
  void generateScaledClk(const Clock *src_clk,
			 float scale);
  void generateEdgesClk(const Clock *src_clk);

  const char *name_;
  PinSet pins_;
  bool add_to_pins_;
  // Hierarchical pins in pins_ become driver pins through the pin.
  PinSet leaf_pins_;
  float period_;
  FloatSeq *waveform_;
  bool waveform_valid_;
  const int index_;
  ClockEdge **clk_edges_;
  bool is_propagated_;
  RiseFallMinMax slews_;
  RiseFallMinMax slew_limits_[path_clk_or_data_count];
  ClockUncertainties *uncertainties_;
  bool is_generated_;
  // Generated clock variables.
  Pin *src_pin_;
  Clock *master_clk_;
  // True if the master clock is infered rather than specified by command.
  bool master_clk_infered_;
  int divide_by_;
  int multiply_by_;
  float duty_cycle_;
  bool invert_;
  bool combinational_;
  IntSeq *edges_;
  FloatSeq *edge_shifts_;

private:
  friend class Sdc;
};

// A single rise/fall edge of a clock
class ClockEdge
{
public:
  Clock *clock() const { return clock_; }
  ~ClockEdge();
  const RiseFall *transition() const { return rf_; }
  float time() const { return time_; }
  const char *name() const { return name_; }
  int index() const { return index_; }
  ClockEdge *opposite() const;
  // Pulse width if this is the leading edge of the pulse.
  float pulseWidth() const;

  friend class Clock;  // builder
private:
  ClockEdge(Clock *clock,
            const RiseFall *rf);
  void setTime(float time);

  Clock *clock_;
  const RiseFall *rf_;
  const char *name_;
  float time_;
  int index_;
};

int
clkCmp(const Clock *clk1,
       const Clock *clk2);
int
clkEdgeCmp(const ClockEdge *clk_edge1,
	   const ClockEdge *clk_edge2);
bool
clkEdgeLess(const ClockEdge *clk_edge1,
	    const ClockEdge *clk_edge2);

class ClockNameLess
{
public:
  bool operator()(const Clock *clk1,
                  const Clock *clk2);
};

////////////////////////////////////////////////////////////////

class InterClockUncertainty
{
public:
  InterClockUncertainty(const Clock *src,
			const Clock *target);
  const Clock *src() const { return src_; }
  const Clock *target() const { return target_; }
  void uncertainty(const RiseFall *src_rf,
		   const RiseFall *tgt_rf,
		   const SetupHold *setup_hold,
		   // Return values.
		   float &uncertainty,
		   bool &exists) const;
  void setUncertainty(const RiseFallBoth *src_rf,
		      const RiseFallBoth *tgt_rf,
		      const SetupHoldAll *setup_hold,
		      float uncertainty);
  void removeUncertainty(const RiseFallBoth *src_rf,
			 const RiseFallBoth *tgt_rf,
			 const SetupHoldAll *setup_hold);
  const RiseFallMinMax *uncertainties(const RiseFall *src_rf) const;
  bool empty() const;

private:
  const Clock *src_;
  const Clock *target_;
  RiseFallMinMax uncertainties_[RiseFall::index_count];
};

class InterClockUncertaintyLess
{
public:
  bool operator()(const InterClockUncertainty *inter1,
		  const InterClockUncertainty *inter2) const;
};

class ClkNameLess
{
public:
  bool operator()(const Clock *clk1,
		  const Clock *clk2) const
  {
    return stringLess(clk1->name(), clk2->name());
  }
};

ClockSeq
sortByName(ClockSet *set);
int
compare(const ClockSet *set1,
        const ClockSet *set2);

} // namespace
