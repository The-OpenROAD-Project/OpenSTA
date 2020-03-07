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

#pragma once

#include "DisallowCopyAssign.hh"

namespace sta {

class WriteSdcObject;

class WriteSdc : public StaState
{
public:
  WriteSdc(Instance *instance,
	   const char *filename,
	   const char *creator,
	   bool map_hpins,
	   bool native,
	   int digits,
	   bool no_timestamp,
	   Sdc *sdc);
  virtual ~WriteSdc();
  void write();

  void openFile(const char *filename);
  void closeFile();
  void flush();
  virtual void writeHeader() const;
  void writeTiming() const;
  void writeDisables() const;
  void writeDisabledCells() const;
  void writeDisabledPorts() const;
  void writeDisabledLibPorts() const;
  void writeDisabledInstances() const;
  void writeDisabledPins() const;
  void writeDisabledEdges() const;
  void writeDisabledEdge(Edge *edge) const;
  void findMatchingEdges(Edge *edge,
			 EdgeSet &matches) const;
  bool edgeSenseIsUnique(Edge *edge,
			 EdgeSet &matches) const;
  void writeDisabledEdgeSense(Edge *edge) const;
  void writeClocks() const;
  void writeClock(Clock *clk) const;
  void writeGeneratedClock(Clock *clk) const;
  void writeClockPins(Clock *clk) const;
  void writeFloatSeq(FloatSeq *floats,
		     float scale) const;
  void writeIntSeq(IntSeq *ints) const;
  void writeClockSlews(Clock *clk) const;
  void writeClockUncertainty(Clock *clk) const;
  void writeClockUncertainty(Clock *clk,
			     const char *setup_hold,
			     float value) const;
  void writeClockUncertaintyPins() const;
  void writeClockUncertaintyPin(const Pin *pin,
				ClockUncertainties *uncertainties) const;
  void writeClockUncertaintyPin(const Pin *pin,
				const char *setup_hold,
				float value) const;
  void writeClockLatencies() const;
  void writeClockInsertions() const;
  void writeClockInsertion(ClockInsertion *insert,
			   WriteSdcObject &write_obj) const;
  void writeInterClockUncertainties() const;
  void writeInterClockUncertainty(InterClockUncertainty *uncertainty) const;
  void writePropagatedClkPins() const;
  void writeInputDelays() const;
  void writeOutputDelays() const;
  void writePortDelay(PortDelay *port_delay,
		      bool is_input_delay,
		      const char *sdc_cmd) const;
  void writePortDelay(PortDelay *port_delay,
		      bool is_input_delay,
		      float delay,
		      const RiseFallBoth *rf,
		      const MinMaxAll *min_max,
		      const char *sdc_cmd) const;
  void writeClockSenses() const;
  void writeClockSense(PinClockPair &pin_clk,
		       ClockSense sense) const;
  void writeClockGroups() const;
  void writeClockGroups(ClockGroups *clk_groups) const;
  void writeExceptions() const;
  void writeException(ExceptionPath *exception) const;
  void writeExceptionCmd(ExceptionPath *exception) const;
  void writeExceptionValue(ExceptionPath *exception) const;
  void writeExceptionFrom(ExceptionFrom *from) const;
  void writeExceptionTo(ExceptionTo *to) const;
  void writeExceptionFromTo(ExceptionFromTo *from_to,
			    const char *from_to_key,
			    bool map_hpin_to_drvr) const;
  void writeExceptionThru(ExceptionThru *thru) const;
  void mapThruHpins(ExceptionThru *thru,
		    PinSeq &pins) const;
  void writeDataChecks() const;
  void writeDataCheck(DataCheck *check) const;
  void writeDataCheck(DataCheck *check,
		      RiseFallBoth *from_rf,
		      RiseFallBoth *to_rf,
		      SetupHold *setup_hold,
		      float margin) const;
  void writeEnvironment() const;
  void writeOperatingConditions() const;
  void writeWireload() const;
  void writePinLoads() const;
  void writePortLoads(Port *port) const;
  void writePortFanout(Port *port) const;
  void writeDriveResistances() const;
  void writeDrivingCells() const;
  void writeInputTransitions() const;
  void writeDrivingCell(Port *port,
			InputDriveCell *drive_cell,
			const RiseFall *rf,
			const MinMax *min_max) const;
  void writeConstants() const;
  virtual void writeConstant(Pin *pin) const;
  const char *setConstantCmd(Pin *pin) const;
  void writeCaseAnalysis() const;
  virtual void writeCaseAnalysis(Pin *pin) const;
  const char *caseAnalysisValueStr(Pin *pin) const;
  void sortedLogicValuePins(LogicValueMap *value_map,
			    PinSeq &pins) const;
  void writeNetResistances() const;
  void writeNetResistance(Net *net,
			  const MinMaxAll *min_max,
			  float res) const;
  void writeDesignRules() const;
  void writeMinPulseWidths() const;
  void writeMinPulseWidths(RiseFallValues *min_widths,
			   WriteSdcObject &write_obj) const;
  void writeMinPulseWidth(const char *hi_low,
			  float value,
			  WriteSdcObject &write_obj) const;
  void writeSlewLimits() const;
  void writeCapLimits() const;
  void writeCapLimits(const MinMax *min_max,
		      const char *cmd) const;
  void writeMaxArea() const;
  void writeFanoutLimits() const;
  void writeFanoutLimits(const MinMax *min_max,
			 const char *cmd) const;
  void writeLatchBorowLimits() const;
  void writeDeratings() const;
  void writeDerating(DeratingFactorsGlobal *factors) const;
  void writeDerating(DeratingFactorsCell *factors,
		     WriteSdcObject *write_obj) const;
  void writeDerating(DeratingFactors *factors,
		     TimingDerateType type,
		     const MinMax *early_late,
		     WriteSdcObject *write_obj) const;

  const char *pathName(const Pin *pin) const;
  const char *pathName(const Net *net) const;
  const char *pathName(const Instance *inst) const;
  void writeCommentSection(const char *line) const;
  void writeCommentSeparator() const;

  void writeGetTimingArcsOfOjbects(LibertyCell *cell) const;
  void writeGetTimingArcs(Edge *edge) const;
  void writeGetTimingArcs(Edge *edge,
			  const char *filter) const;
  const char *getTimingArcsCmd() const;
  void writeGetLibCell(const LibertyCell *cell) const;
  void writeGetLibPin(const LibertyPort *port) const;
  void writeGetClock(const Clock *clk) const;
  void writeGetClocks(ClockSet *clks) const;
  void writeGetClocks(ClockSet *clks,
		      bool multiple,
		      bool &first) const;
  virtual void writeGetPort(const Port *port) const;
  virtual void writeGetNet(const Net *net) const;
  virtual void writeGetInstance(const Instance *inst) const;
  virtual void writeGetPin(const Pin *pin) const;
  void writeGetPin(const Pin *pin,
		   bool map_hpin_to_drvr) const;
  void writeGetPins(PinSet *pins,
		    bool map_hpin_to_drvr) const;
  void writeGetPins1(PinSeq *pins) const;
  void writeClockKey(const Clock *clk) const;
  float scaleTime(float time) const;
  float scaleCapacitance(float cap) const;
  float scaleResistance(float res) const;
  void writeFloat(float value) const;
  void writeTime(float time) const;
  void writeCapacitance(float cap) const;
  void writeResistance(float res) const;

  void writeClkSlewLimits() const;
  void writeClkSlewLimit(const char *clk_data,
			 const char *rise_fall,
			 const Clock *clk,
			 float limit) const;
  void writeRiseFallMinMaxTimeCmd(const char *sdc_cmd,
				  RiseFallMinMax *values,
				  WriteSdcObject &write_object) const;
  void writeRiseFallMinMaxCapCmd(const char *sdc_cmd,
				 RiseFallMinMax *values,
				 WriteSdcObject &write_object) const;
  void writeRiseFallMinMaxCmd(const char *sdc_cmd,
			      RiseFallMinMax *values,
			      float scale,
			      WriteSdcObject &write_object) const;
  void writeRiseFallMinMaxCmd(const char *sdc_cmd,
			      float value,
			      float scale,
			      const RiseFallBoth *rf,
			      const MinMaxAll *min_max,
			      WriteSdcObject &write_object) const;
  void writeMinMaxFloatValuesCmd(const char *sdc_cmd,
				 MinMaxFloatValues *values,
				 float scale,
				 WriteSdcObject &write_object) const;
  void writeMinMaxFloatCmd(const char *sdc_cmd,
			   float value,
			   float scale,
			   const MinMaxAll *min_max,
			   WriteSdcObject &write_object) const;
  void writeMinMaxIntValuesCmd(const char *sdc_cmd,
			       MinMaxIntValues *values,
			       WriteSdcObject &write_object) const;
  void writeMinMaxIntCmd(const char *sdc_cmd,
			 int value,
			 const MinMaxAll *min_max,
			 WriteSdcObject &write_object) const;
  void writeSetupHoldFlag(const MinMaxAll *min_max) const;
  void writeVariables() const;
  void writeCmdComment(SdcCmdComment *cmd) const;

  FILE *stream() const { return stream_; }

protected:
  Instance *instance_;
  const char *filename_;
  const char *creator_;
  bool map_hpins_;
  bool native_;
  int digits_;
  bool no_timestamp_;
  bool top_instance_;
  size_t instance_name_length_;
  Cell *cell_;
  FILE *stream_;

private:
  DISALLOW_COPY_AND_ASSIGN(WriteSdc);
};

} // namespace
