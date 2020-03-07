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
#include "Transition.hh"
#include "MinMax.hh"
#include "MinMaxValues.hh"
#include "RiseFallValues.hh"
#include "RiseFallMinMax.hh"
#include "ConcreteLibrary.hh"
#include "LibertyClass.hh"

namespace sta {

class LibertyCellIterator;
class LibertyCellPortIterator;
class LibertyCellPortBitIterator;
class LibertyPortMemberIterator;
class ModeValueDef;
class TestCell;
class PatternMatch;
class LatchEnable;
class Report;
class Debug;
class LibertyBuilder;
class LibertyReader;
class OcvDerate;
class TimingArcAttrs;
class InternalPowerAttrs;
class LibertyPgPort;

typedef Set<Library*> LibrarySet;
typedef Map<const char*, TableTemplate*, CharPtrLess> TableTemplateMap;
typedef Map<const char*, BusDcl *, CharPtrLess> BusDclMap;
typedef Map<const char*, ScaleFactors*, CharPtrLess> ScaleFactorsMap;
typedef Map<const char*, Wireload*, CharPtrLess> WireloadMap;
typedef Map<const char*, WireloadSelection*, CharPtrLess> WireloadSelectionMap;
typedef Map<const char*, OperatingConditions*,
	    CharPtrLess> OperatingConditionsMap;
typedef Map<LibertyPort*, Sequential*> PortToSequentialMap;
typedef Vector<TimingArcSet*> TimingArcSetSeq;
typedef Set<TimingArcSet*, TimingArcSetLess> TimingArcSetMap;
typedef Map<LibertyPortPair, TimingArcSetSeq*,
	    LibertyPortPairLess> LibertyPortPairTimingArcMap;
typedef Vector<InternalPower*> InternalPowerSeq;
typedef Vector<LeakagePower*> LeakagePowerSeq;
typedef Map<const LibertyPort*, TimingArcSetSeq*> LibertyPortTimingArcMap;
typedef Map<const OperatingConditions*, LibertyCell*> ScaledCellMap;
typedef Map<const OperatingConditions*, LibertyPort*> ScaledPortMap;
typedef Map<const char *, ModeDef*, CharPtrLess> ModeDefMap;
typedef Map<const char *, ModeValueDef*, CharPtrLess> ModeValueMap;
typedef Map<TimingArcSet*, LatchEnable*> LatchEnableMap;
typedef Map<const char *, OcvDerate*, CharPtrLess> OcvDerateMap;
typedef Vector<TimingArcAttrs*> TimingArcAttrsSeq;
typedef Vector<InternalPowerAttrs*> InternalPowerAttrsSeq;
typedef Map<const char *, float, CharPtrLess> SupplyVoltageMap;
typedef Map<const char *, LibertyPgPort*, CharPtrLess> LibertyPgPortMap;

enum class ClockGateType { none, latch_posedge, latch_negedge, other };

enum class DelayModelType { cmos_linear, cmos_pwl, cmos2, table, polynomial, dcm };

enum class ScaleFactorPvt { process, volt, temp, count, unknown };

enum class TableTemplateType { delay, power, output_current, ocv, count };

void
initLiberty();
void
deleteLiberty();

ScaleFactorPvt
findScaleFactorPvt(const char *name);
const char *
scaleFactorPvtName(ScaleFactorPvt pvt);

ScaleFactorType
findScaleFactorType(const char *name);
const char *
scaleFactorTypeName(ScaleFactorType type);
bool
scaleFactorTypeRiseFallSuffix(ScaleFactorType type);
bool
scaleFactorTypeRiseFallPrefix(ScaleFactorType type);
bool
scaleFactorTypeLowHighSuffix(ScaleFactorType type);

// Timing sense as a string.
const char *
timingSenseString(TimingSense sense);

// Opposite timing sense.
TimingSense
timingSenseOpposite(TimingSense sense);

class LibertyLibrary : public ConcreteLibrary
{
public:
  LibertyLibrary(const char *name,
		 const char *filename);
  virtual ~LibertyLibrary();
  LibertyCell *findLibertyCell(const char *name) const;
  void findLibertyCellsMatching(PatternMatch *pattern,
				LibertyCellSeq *cells);
  // Liberty cells that are buffers.
  LibertyCellSeq *buffers();

  DelayModelType delayModelType() const { return delay_model_type_; }
  void setDelayModelType(DelayModelType type);
  void addBusDcl(BusDcl *bus_dcl);
  BusDcl *findBusDcl(const char *name) const;
  void addTableTemplate(TableTemplate *tbl_template,
			TableTemplateType type);
  TableTemplate *findTableTemplate(const char *name,
				   TableTemplateType type);
  float nominalProcess() { return nominal_process_; }
  void setNominalProcess(float process);
  float nominalVoltage() const { return nominal_voltage_; }
  void setNominalVoltage(float voltage);
  float nominalTemperature() const { return nominal_temperature_; }
  void setNominalTemperature(float temperature);
  void setScaleFactors(ScaleFactors *scales);
  // Add named scale factor group.
  void addScaleFactors(ScaleFactors *scales);
  ScaleFactors *findScaleFactors(const char *name);
  ScaleFactors *scaleFactors() const { return scale_factors_; }
  float scaleFactor(ScaleFactorType type,
		    const Pvt *pvt) const;
  float scaleFactor(ScaleFactorType type,
		    const LibertyCell *cell,
		    const Pvt *pvt) const;
  float scaleFactor(ScaleFactorType type,
		    int tr_index,
		    const LibertyCell *cell,
		    const Pvt *pvt) const;
  void setWireSlewDegradationTable(TableModel *model,
				   RiseFall *rf);
  TableModel *wireSlewDegradationTable(const RiseFall *rf) const;
  float degradeWireSlew(const LibertyCell *cell,
			const RiseFall *rf,
			const Pvt *pvt,
			float in_slew,
			float wire_delay) const;
  // Check for supported axis variables.
  // Return true if axes are supported.
  static bool checkSlewDegradationAxes(Table *table);

  float defaultInputPinCap() const { return default_input_pin_cap_; }
  void setDefaultInputPinCap(float cap);
  float defaultOutputPinCap() const { return default_output_pin_cap_; }
  void setDefaultOutputPinCap(float cap);
  float defaultBidirectPinCap() const { return default_bidirect_pin_cap_; }
  void setDefaultBidirectPinCap(float cap);

  void defaultIntrinsic(const RiseFall *rf,
			// Return values.
			float &intrisic,
			bool &exists) const;
  void setDefaultIntrinsic(const RiseFall *rf,
			   float value);
  // Uses defaultOutputPinRes or defaultBidirectPinRes based on dir.
  void defaultPinResistance(const RiseFall *rf,
			    const PortDirection *dir,
			    // Return values.
			    float &res,
			    bool &exists) const;
  void defaultBidirectPinRes(const RiseFall *rf,
			     // Return values.
			     float &res,
			     bool &exists) const;
  void setDefaultBidirectPinRes(const RiseFall *rf,
				float value);
  void defaultOutputPinRes(const RiseFall *rf,
			   // Return values.
			   float &res,
			   bool &exists) const;
  void setDefaultOutputPinRes(const RiseFall *rf,
			      float value);

  void defaultMaxSlew(float &slew,
		      bool &exists) const;
  void setDefaultMaxSlew(float slew);
  void defaultMaxCapacitance(float &cap,
			     bool &exists) const;
  void setDefaultMaxCapacitance(float cap);
  void defaultMaxFanout(float &fanout,
			bool &exists) const;
  void setDefaultMaxFanout(float fanout);
  float defaultFanoutLoad() const { return default_fanout_load_; }
  void setDefaultFanoutLoad(float load);

  // Logic thresholds.
  float inputThreshold(const RiseFall *rf) const;
  void setInputThreshold(const RiseFall *rf,
			 float th);
  float outputThreshold(const RiseFall *rf) const;
  void setOutputThreshold(const RiseFall *rf,
			  float th);
  // Slew thresholds (measured).
  float slewLowerThreshold(const RiseFall *rf) const;
  void setSlewLowerThreshold(const RiseFall *rf,
			     float th);
  float slewUpperThreshold(const RiseFall *rf) const;
  void setSlewUpperThreshold(const RiseFall *rf,
			     float th);
  // The library and delay calculator use the liberty slew upper/lower
  // (measured) thresholds for the table axes and value.  These slews
  // are scaled by slew_derate_from_library to get slews reported to
  // the user.
  float slewDerateFromLibrary() const;
  void setSlewDerateFromLibrary(float derate);

  Units *units() { return units_; }
  const Units *units() const { return units_; }

  Wireload *findWireload(const char *name) const;
  void setDefaultWireload(Wireload *wireload);
  Wireload *defaultWireload() const;
  WireloadSelection *findWireloadSelection(const char *name) const;
  WireloadSelection *defaultWireloadSelection() const;

  void addWireload(Wireload *wireload);
  WireloadMode defaultWireloadMode() const;
  void setDefaultWireloadMode(WireloadMode mode);
  void addWireloadSelection(WireloadSelection *selection);
  void setDefaultWireloadSelection(WireloadSelection *selection);

  OperatingConditions *findOperatingConditions(const char *name);
  OperatingConditions *defaultOperatingConditions() const;
  void addOperatingConditions(OperatingConditions *op_cond);
  void setDefaultOperatingConditions(OperatingConditions *op_cond);

  // AOCV
  // Zero means the ocv depth is not specified.
  float ocvArcDepth() const;
  void setOcvArcDepth(float depth);
  OcvDerate *defaultOcvDerate() const;
  void setDefaultOcvDerate(OcvDerate *derate);
  OcvDerate *findOcvDerate(const char *derate_name);
  void addOcvDerate(OcvDerate *derate);
  void addSupplyVoltage(const char *suppy_name,
			float voltage);
  bool supplyExists(const char *suppy_name) const;
  void supplyVoltage(const char *supply_name,
		     // Return value.
		     float &voltage,
		     bool &exists) const;

  // Make scaled cell.  Call LibertyCell::addScaledCell after it is complete.
  LibertyCell *makeScaledCell(const char *name,
			      const char *filename);

  static void
  makeCornerMap(LibertyLibrary *lib,
		int ap_index,
		Network *network,
		Report *report);
  static void
  makeCornerMap(LibertyCell *link_cell,
		LibertyCell *map_cell,
		int ap_index,
		Report *report);
  static void
  makeCornerMap(LibertyCell *cell1,
		LibertyCell *cell2,
		bool link,
		int ap_index,
		Report *report);

protected:
  float degradeWireSlew(const LibertyCell *cell,
			const Pvt *pvt,
			const TableModel *model,
			float in_slew,
			float wire_delay) const;

  Units *units_;
  DelayModelType delay_model_type_;
  BusDclMap bus_dcls_;
  TableTemplateMap template_maps_[int(TableTemplateType::count)];
  float nominal_process_;
  float nominal_voltage_;
  float nominal_temperature_;
  ScaleFactors *scale_factors_;
  ScaleFactorsMap scale_factors_map_;
  TableModel *wire_slew_degradation_tbls_[RiseFall::index_count];
  float default_input_pin_cap_;
  float default_output_pin_cap_;
  float default_bidirect_pin_cap_;
  RiseFallValues default_intrinsic_;
  RiseFallValues default_inout_pin_res_;
  RiseFallValues default_output_pin_res_;
  float default_fanout_load_;
  float default_max_cap_;
  bool default_max_cap_exists_;
  float default_max_fanout_;
  bool default_max_fanout_exists_;
  float default_max_slew_;
  bool default_max_slew_exists_;
  float input_threshold_[RiseFall::index_count];
  float output_threshold_[RiseFall::index_count];
  float slew_lower_threshold_[RiseFall::index_count];
  float slew_upper_threshold_[RiseFall::index_count];
  float slew_derate_from_library_;
  WireloadMap wireloads_;
  Wireload *default_wire_load_;
  WireloadMode default_wire_load_mode_;
  WireloadSelection *default_wire_load_selection_;
  WireloadSelectionMap wire_load_selections_;
  OperatingConditionsMap operating_conditions_;
  OperatingConditions *default_operating_conditions_;
  float ocv_arc_depth_;
  OcvDerate *default_ocv_derate_;
  OcvDerateMap ocv_derate_map_;
  SupplyVoltageMap supply_voltage_map_;
  LibertyCellSeq *buffers_;

  // Set if any library has rise/fall capacitances.
  static bool found_rise_fall_caps_;
  static constexpr float input_threshold_default_ = .5;
  static constexpr float output_threshold_default_ = .5;
  static constexpr float slew_lower_threshold_default_ = .2;
  static constexpr float slew_upper_threshold_default_ = .8;

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyLibrary);

  friend class LibertyCell;
  friend class LibertyCellIterator;
  friend class TableTemplateIterator;
  friend class OperatingConditionsIterator;
};

class LibertyCellIterator : public Iterator<LibertyCell*>
{
public:
  explicit LibertyCellIterator(const LibertyLibrary *library);
  bool hasNext();
  LibertyCell *next();

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyCellIterator);

  ConcreteCellMap::ConstIterator iter_;
};

class TableTemplateIterator : public TableTemplateMap::ConstIterator
{
public:
  TableTemplateIterator(const LibertyLibrary *library,
			TableTemplateType type) :
    TableTemplateMap::ConstIterator(library->template_maps_[int(type)]) {}
};

class OperatingConditionsIterator : public OperatingConditionsMap::ConstIterator
{
public:
  OperatingConditionsIterator(const LibertyLibrary *library) :
    OperatingConditionsMap::ConstIterator(library->operating_conditions_) {}
};

////////////////////////////////////////////////////////////////

class LibertyCell : public ConcreteCell
{
public:
  LibertyCell(LibertyLibrary *library,
	      const char *name,
	      const char *filename);
  virtual ~LibertyCell();
  LibertyLibrary *libertyLibrary() const { return liberty_library_; }
  LibertyLibrary *libertyLibrary() { return liberty_library_; }
  LibertyPort *findLibertyPort(const char *name) const;
  void findLibertyPortsMatching(PatternMatch *pattern,
				LibertyPortSeq *ports) const;
  bool hasInternalPorts() const { return has_internal_ports_; }
  LibertyPgPort *findPgPort(const char *name);
  ScaleFactors *scaleFactors() const { return scale_factors_; }
  void setScaleFactors(ScaleFactors *scale_factors);
  ModeDef *makeModeDef(const char *name);
  ModeDef *findModeDef(const char *name);

  float area() const { return area_; }
  void setArea(float area);
  bool dontUse() const { return dont_use_; }
  void setDontUse(bool dont_use);
  bool isMacro() const { return is_macro_; }
  void setIsMacro(bool is_macro);
  bool isPad() const { return is_pad_; }
  void setIsPad(bool is_pad);
  bool interfaceTiming() const { return interface_timing_; }
  void setInterfaceTiming(bool value);
  bool isClockGateLatchPosedge() const;
  bool isClockGateLatchNegedge() const;
  bool isClockGateOther() const;
  bool isClockGate() const;
  void setClockGateType(ClockGateType type);
  // from or to may be nullptr to wildcard.
  TimingArcSetSeq *timingArcSets(const LibertyPort *from,
				 const LibertyPort *to) const;
  size_t timingArcSetCount() const;
  // Find a timing arc set equivalent to key.
  TimingArcSet *findTimingArcSet(TimingArcSet *key) const;
  TimingArcSet *findTimingArcSet(unsigned arc_set_index) const;
  bool hasTimingArcs(LibertyPort *port) const;

  InternalPowerSeq *internalPowers() { return &internal_powers_; }
  LeakagePowerSeq *leakagePowers() { return &leakage_powers_; }
  void leakagePower(// Return values.
		    float &leakage,
		    bool &exists) const;
  bool leakagePowerEx() const { return leakage_power_exists_; }

  bool hasSequentials() const;
  // Find the sequential with the output connected to an (internal) port.
  Sequential *outputPortSequential(LibertyPort *port);

  // Find bus declaration local to this cell.
  BusDcl *findBusDcl(const char *name) const;
  // True when TimingArcSetBuilder::makeRegLatchArcs infers register
  // timing arcs.
  bool hasInferedRegTimingArcs() const { return has_infered_reg_timing_arcs_; }
  TestCell *testCell() const { return test_cell_; }
  bool isLatchData(LibertyPort *port);
  void latchEnable(TimingArcSet *arc_set,
		   // Return values.
		   LibertyPort *&enable_port,
		   FuncExpr *&enable_func,
		   RiseFall *&enable_rf) const;
  RiseFall *latchCheckEnableTrans(TimingArcSet *check_set);
  bool isDisabledConstraint() const { return is_disabled_constraint_; }
  LibertyCell *cornerCell(int ap_index);

  // AOCV
  float ocvArcDepth() const;
  OcvDerate *ocvDerate() const;
  OcvDerate *findOcvDerate(const char *derate_name);

  // Build helpers.
  void makeSequential(int size,
		      bool is_register,
		      FuncExpr *clk,
		      FuncExpr *data,
		      FuncExpr *clear,
		      FuncExpr *preset,
		      LogicValue clr_preset_out,
		      LogicValue clr_preset_out_inv,
		      LibertyPort *output,
		      LibertyPort *output_inv);
  void addBusDcl(BusDcl *bus_dcl);
  // Add scaled cell after it is complete.
  void addScaledCell(OperatingConditions *op_cond,
		     LibertyCell *scaled_cell);
  unsigned addTimingArcSet(TimingArcSet *set);
  void addTimingArcAttrs(TimingArcAttrs *attrs);
  void addInternalPower(InternalPower *power);
  void addInternalPowerAttrs(InternalPowerAttrs *attrs);
  void addLeakagePower(LeakagePower *power);
  void setLeakagePower(float leakage);
  void setOcvArcDepth(float depth);
  void setOcvDerate(OcvDerate *derate);
  void addOcvDerate(OcvDerate *derate);
  void addPgPort(LibertyPgPort *pg_port);
  void setTestCell(TestCell *test);
  void setHasInferedRegTimingArcs(bool infered);
  void setIsDisabledConstraint(bool is_disabled);
  void setCornerCell(LibertyCell *corner_cell,
		     int ap_index);
  // Call after cell is finished being constructed.
  void finish(bool infer_latches,
	      Report *report,
	      Debug *debug);
  bool isBuffer() const;
  // Only valid when isBuffer() returns true.
  void bufferPorts(// Return values.
		   LibertyPort *&input,
		   LibertyPort *&output) const;

protected:
  void addPort(ConcretePort *port);
  void setHasInternalPorts(bool has_internal);
  void setLibertyLibrary(LibertyLibrary *library);
  void deleteTimingArcAttrs();
  void makeLatchEnables(Report *report,
			Debug *debug);
  FuncExpr *findLatchEnableFunc(LibertyPort *data,
				LibertyPort *enable) const;
  LatchEnable *makeLatchEnable(LibertyPort *d,
			       LibertyPort *en,
			       LibertyPort *q,
			       TimingArcSet *d_to_q,
			       TimingArcSet *en_to_q,
			       TimingArcSet *setup_check,
			       Debug *debug);
  void findDefaultCondArcs();
  void translatePresetClrCheckRoles();
  void inferLatchRoles(Debug *debug);
  void deleteInternalPowerAttrs();
  void makeTimingArcMap(Report *report);
  void makeTimingArcPortMaps();
  bool hasBufferFunc(const LibertyPort *input,
		     const LibertyPort *output) const;

  LibertyLibrary *liberty_library_;
  float area_;
  bool dont_use_;
  bool is_macro_;
  bool is_pad_;
  bool has_internal_ports_;
  bool interface_timing_;
  ClockGateType clock_gate_type_;
  TimingArcSetSeq timing_arc_sets_;
  TimingArcSetMap timing_arc_set_map_;
  LibertyPortPairTimingArcMap port_timing_arc_set_map_;
  LibertyPortTimingArcMap timing_arc_set_from_map_;
  LibertyPortTimingArcMap timing_arc_set_to_map_;
  TimingArcAttrsSeq timing_arc_attrs_;
  bool has_infered_reg_timing_arcs_;
  InternalPowerSeq internal_powers_;
  InternalPowerAttrsSeq internal_power_attrs_;
  LeakagePowerSeq leakage_powers_;
  SequentialSeq sequentials_;
  PortToSequentialMap port_to_seq_map_;
  BusDclMap bus_dcls_;
  ModeDefMap mode_defs_;
  ScaleFactors *scale_factors_;
  ScaledCellMap scaled_cells_;
  TestCell *test_cell_;
  // Latch D->Q to LatchEnable.
  LatchEnableMap latch_d_to_q_map_;
  // Latch EN->D setup to LatchEnable.
  LatchEnableMap latch_check_map_;
  // Ports that have latch D->Q timing arc sets from them.
  LibertyPortSet latch_data_ports_;
  float ocv_arc_depth_;
  OcvDerate *ocv_derate_;
  OcvDerateMap ocv_derate_map_;
  bool is_disabled_constraint_;
  Vector<LibertyCell*> corner_cells_;
  float leakage_power_;
  bool leakage_power_exists_;
  LibertyPgPortMap pg_port_map_;

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyCell);

  friend class LibertyLibrary;
  friend class LibertyCellPortIterator;
  friend class LibertyPort;
  friend class LibertyBuilder;
  friend class LibertyCellTimingArcSetIterator;
  friend class LibertyCellSequentialIterator;
};

class LibertyCellPortIterator : public Iterator<LibertyPort*>
{
public:
  explicit LibertyCellPortIterator(const LibertyCell *cell);
  bool hasNext();
  LibertyPort *next();

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyCellPortIterator);

  ConcretePortSeq::ConstIterator iter_;
};

class LibertyCellPortBitIterator : public Iterator<LibertyPort*>
{
public:
  explicit LibertyCellPortBitIterator(const LibertyCell *cell);
  virtual ~LibertyCellPortBitIterator();
  bool hasNext();
  LibertyPort *next();

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyCellPortBitIterator);

  ConcreteCellPortBitIterator *iter_;
};

class LibertyCellTimingArcSetIterator : public TimingArcSetSeq::ConstIterator
{
public:
  LibertyCellTimingArcSetIterator(const LibertyCell *cell);
  // from or to may be nullptr to wildcard.
  LibertyCellTimingArcSetIterator(const LibertyCell *cell,
				  const LibertyPort *from,
				  const LibertyPort *to);
};

class LibertyCellSequentialIterator : public SequentialSeq::ConstIterator
{
public:
  LibertyCellSequentialIterator(const LibertyCell *cell) :
    SequentialSeq::ConstIterator(cell->sequentials_) {}
};

class LibertyCellLeakagePowerIterator : public LeakagePowerSeq::Iterator
{
public:
  LibertyCellLeakagePowerIterator(LibertyCell *cell) :
    LeakagePowerSeq::Iterator(cell->leakagePowers()) {}
};

class LibertyCellInternalPowerIterator : public InternalPowerSeq::Iterator
{
public:
  LibertyCellInternalPowerIterator(LibertyCell *cell) :
    InternalPowerSeq::Iterator(cell->internalPowers()) {}
};

////////////////////////////////////////////////////////////////

class LibertyPort : public ConcretePort
{
public:
  LibertyCell *libertyCell() const { return liberty_cell_; }
  LibertyPort *findLibertyMember(int index) const;
  LibertyPort *findLibertyBusBit(int index) const;
  float capacitance(const RiseFall *rf,
		    const MinMax *min_max) const;
  void capacitance(const RiseFall *rf,
		   const MinMax *min_max,
		   // Return values.
		   float &cap,
		   bool &exists) const;
  // Capacitance at op_cond derated by library/cell scale factors
  // using pvt.
  float capacitance(const RiseFall *rf,
		    const MinMax *min_max,
		    const OperatingConditions *op_cond,
		    const Pvt *pvt) const;
  bool capacitanceIsOneValue() const;
  void setCapacitance(float cap);
  void setCapacitance(const RiseFall *rf,
		      const MinMax *min_max,
		      float cap);
  float driveResistance(const RiseFall *rf,
			const MinMax *min_max) const;
  // Max of rise/fall.
  float driveResistance() const;
  FuncExpr *function() const { return function_; }
  void setFunction(FuncExpr *func);
  FuncExpr *&functionRef() { return function_; }
  // Tristate enable function.
  FuncExpr *tristateEnable() const { return tristate_enable_; }
  void setTristateEnable(FuncExpr *enable);
  FuncExpr *&tristateEnableRef() { return tristate_enable_; }
  void slewLimit(const MinMax *min_max,
		 // Return values.
		 float &limit,
		 bool &exists) const;
  void setSlewLimit(float slew,
		    const MinMax *min_max);
  void capacitanceLimit(const MinMax *min_max,
			// Return values.
			float &limit,
			bool &exists) const;
  void setCapacitanceLimit(float cap,
			   const MinMax *min_max);
  void fanoutLimit(const MinMax *min_max,
		   // Return values.
		   float &limit,
		   bool &exists) const;
  void setFanoutLimit(float fanout,
		      const MinMax *min_max);
  void minPeriod(const OperatingConditions *op_cond,
		 const Pvt *pvt,
		 float &min_period,
		 bool &exists) const;
  // Unscaled value.
  void minPeriod(float &min_period,
		 bool &exists) const;
  void setMinPeriod(float min_period);
  // high = rise, low = fall
  void minPulseWidth(const RiseFall *hi_low,
		     const OperatingConditions *op_cond,
		     const Pvt *pvt,
		     float &min_width,
		     bool &exists) const;
  // Unscaled value.
  void minPulseWidth(const RiseFall *hi_low,
		     float &min_width,
		     bool &exists) const;
  void setMinPulseWidth(RiseFall *hi_low,
			float min_width);
  bool isClock() const;
  void setIsClock(bool is_clk);
  bool isClockGateClockPin() const { return is_clk_gate_clk_pin_; }
  void setIsClockGateClockPin(bool is_clk_gate_clk);
  bool isClockGateEnablePin() const { return is_clk_gate_enable_pin_; }
  void setIsClockGateEnablePin(bool is_clk_gate_enable);
  bool isClockGateOutPin() const { return is_clk_gate_out_pin_; }
  void setIsClockGateOutPin(bool is_clk_gate_out);
  bool isPllFeedbackPin() const { return is_pll_feedback_pin_; }
  void setIsPllFeedbackPin(bool is_pll_feedback_pin);
  // Has register/latch rise/fall edges from pin.
  bool isRegClk() const { return is_reg_clk_; }
  void setIsRegClk(bool is_clk);
  // Is the clock for timing checks.
  bool isCheckClk() const { return is_check_clk_; }
  void setIsCheckClk(bool is_clk);
  RiseFall *pulseClkTrigger() const { return pulse_clk_trigger_; }
  // Rise for high, fall for low.
  RiseFall *pulseClkSense() const { return pulse_clk_sense_; }
  void setPulseClk(RiseFall *rfigger,
		   RiseFall *sense);
  bool isDisabledConstraint() const { return is_disabled_constraint_; }
  void setIsDisabledConstraint(bool is_disabled);
  LibertyPort *cornerPort(int ap_index);
  void setCornerPort(LibertyPort *corner_port,
		     int ap_index);
  const char *relatedGroundPin() const { return related_ground_pin_; }
  void setRelatedGroundPin(const char *related_ground_pin);
  const char *relatedPowerPin() const { return related_power_pin_; }
  void setRelatedPowerPin(const char *related_power_pin);

  static bool equiv(const LibertyPort *port1,
		    const LibertyPort *port2);
  static bool less(const LibertyPort *port1,
		   const LibertyPort *port2);

protected:
  // Constructor is internal to LibertyBuilder.
  LibertyPort(LibertyCell *cell,
	      const char *name,
	      bool is_bus,
	      int from_index,
	      int to_index,
	      bool is_bundle,
	      ConcretePortSeq *members);
  virtual ~LibertyPort();
  void setDirection(PortDirection *dir);
  void setMinPort(LibertyPort *min);
  void addScaledPort(OperatingConditions *op_cond,
		     LibertyPort *scaled_port);

  LibertyCell *liberty_cell_;
  FuncExpr *function_;
  FuncExpr *tristate_enable_;
  ScaledPortMap *scaled_ports_;
  RiseFallMinMax capacitance_;
  MinMaxFloatValues slew_limit_; // inputs and outputs
  MinMaxFloatValues cap_limit_;    // outputs
  MinMaxFloatValues fanout_limit_; // outputs
  float min_period_;
  float min_pulse_width_[RiseFall::index_count];
  RiseFall *pulse_clk_trigger_;
  RiseFall *pulse_clk_sense_;
  const char *related_ground_pin_;
  const char *related_power_pin_;
  Vector<LibertyPort*> corner_ports_;

  unsigned int min_pulse_width_exists_:RiseFall::index_count;
  bool min_period_exists_:1;
  bool is_clk_:1;
  bool is_reg_clk_:1;
  bool is_check_clk_:1;
  bool is_clk_gate_clk_pin_:1;
  bool is_clk_gate_enable_pin_:1;
  bool is_clk_gate_out_pin_:1;
  bool is_pll_feedback_pin_:1;
  bool is_disabled_constraint_:1;

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyPort);

  friend class LibertyLibrary;
  friend class LibertyCell;
  friend class LibertyBuilder;
  friend class LibertyReader;
};

void
sortLibertyPortSet(LibertyPortSet *set,
		   LibertyPortSeq &ports);

class LibertyPortMemberIterator : public Iterator<LibertyPort*>
{
public:
  explicit LibertyPortMemberIterator(const LibertyPort *port);
  virtual ~LibertyPortMemberIterator();
  virtual bool hasNext();
  virtual LibertyPort *next();

private:
  DISALLOW_COPY_AND_ASSIGN(LibertyPortMemberIterator);

  ConcretePortMemberIterator *iter_;
};

// Process, voltage temperature.
class Pvt
{
public:
  Pvt(float process,
      float voltage,
      float temperature);
  virtual ~Pvt() {}
  float process() const { return process_; }
  void setProcess(float process);
  float voltage() const { return voltage_; }
  void setVoltage(float voltage);
  float temperature() const { return temperature_; }
  void setTemperature(float temp);

protected:
  float process_;
  float voltage_;
  float temperature_;

private:
  DISALLOW_COPY_AND_ASSIGN(Pvt);
};

class OperatingConditions : public Pvt
{
public:
  explicit OperatingConditions(const char *name);
  OperatingConditions(const char *name,
		      float process,
		      float voltage,
		      float temperature,
		      WireloadTree wire_load_tree);
  virtual ~OperatingConditions();
  const char *name() const { return name_; }
  WireloadTree wireloadTree() const { return wire_load_tree_; }
  void setWireloadTree(WireloadTree tree);

protected:
  const char *name_;
  WireloadTree wire_load_tree_;

private:
  DISALLOW_COPY_AND_ASSIGN(OperatingConditions);
};

class ScaleFactors
{
public:
  explicit ScaleFactors(const char *name);
  ~ScaleFactors();
  const char *name() const { return name_; }
  float scale(ScaleFactorType type,
	      ScaleFactorPvt pvt,
	      RiseFall *rf);
  float scale(ScaleFactorType type,
	      ScaleFactorPvt pvt,
	      int tr_index);
  float scale(ScaleFactorType type,
	      ScaleFactorPvt pvt);
  void setScale(ScaleFactorType type,
		ScaleFactorPvt pvt,
		RiseFall *rf,
		float scale);
  void setScale(ScaleFactorType type,
		ScaleFactorPvt pvt,
		float scale);
  void print();

protected:
  const char *name_;
  float scales_[scale_factor_type_count][int(ScaleFactorPvt::count)][RiseFall::index_count];

private:
  DISALLOW_COPY_AND_ASSIGN(ScaleFactors);
};

class BusDcl
{
public:
  BusDcl(const char *name,
	 int from,
	 int to);
  ~BusDcl();
  const char *name() const { return name_; }
  int from() const { return from_; }
  int to() const { return to_; }

protected:
  const char *name_;
  int from_;
  int to_;

private:
  DISALLOW_COPY_AND_ASSIGN(BusDcl);
};

// Cell mode_definition group.
class ModeDef
{
public:
  ~ModeDef();
  const char *name() const { return name_; }
  ModeValueDef *defineValue(const char *value,
			    FuncExpr *cond,
			    const char *sdf_cond);
  ModeValueDef *findValueDef(const char *value);
  ModeValueMap *values() { return &values_; }

protected:
  // Private to LibertyCell::makeModeDef.
  explicit ModeDef(const char *name);

  const char *name_;
  ModeValueMap values_;

private:
  DISALLOW_COPY_AND_ASSIGN(ModeDef);

  friend class LibertyCell;
};

// Mode definition mode_value group.
class ModeValueDef
{
public:
  ~ModeValueDef();
  const char *value() const { return value_; }
  FuncExpr *cond() const { return cond_; }
  FuncExpr *&condRef() { return cond_; }
  const char *sdfCond() const { return sdf_cond_; }
  void setSdfCond(const char *sdf_cond);

protected:
  // Private to ModeDef::defineValue.
  ModeValueDef(const char *value,
	       FuncExpr *cond,
	       const char *sdf_cond);

  const char *value_;
  FuncExpr *cond_;
  const char *sdf_cond_;

private:
  DISALLOW_COPY_AND_ASSIGN(ModeValueDef);

  friend class ModeDef;
};

class TableTemplate
{
public:
  explicit TableTemplate(const char *name);
  TableTemplate(const char *name,
		TableAxis *axis1,
		TableAxis *axis2,
		TableAxis *axis3);
  ~TableTemplate();
  const char *name() const { return name_; }
  void setName(const char *name);
  TableAxis *axis1() const { return axis1_; }
  void setAxis1(TableAxis *axis);
  TableAxis *axis2() const { return axis2_; }
  void setAxis2(TableAxis *axis);
  TableAxis *axis3() const { return axis3_; }
  void setAxis3(TableAxis *axis);

protected:
  const char *name_;
  TableAxis *axis1_;
  TableAxis *axis2_;
  TableAxis *axis3_;

private:
  DISALLOW_COPY_AND_ASSIGN(TableTemplate);
};

class TestCell
{
public:
  TestCell();
  TestCell(LibertyPort *data_in,
	   LibertyPort *scan_in,
	   LibertyPort *scan_enable,
	   LibertyPort *scan_out,
	   LibertyPort *scan_out_inv);
  LibertyPort *dataIn() const { return data_in_; }
  void setDataIn(LibertyPort *port);
  LibertyPort *scanIn() const { return scan_in_; }
  void setScanIn(LibertyPort *port);
  LibertyPort *scanEnable() const { return scan_enable_; }
  void setScanEnable(LibertyPort *port);
  LibertyPort *scanOut() const { return scan_out_; }
  void setScanOut(LibertyPort *port);
  LibertyPort *scanOutInv() const { return scan_out_inv_; }
  void setScanOutInv(LibertyPort *port);

protected:
  LibertyPort *data_in_;
  LibertyPort *scan_in_;
  LibertyPort *scan_enable_;
  LibertyPort *scan_out_;
  LibertyPort *scan_out_inv_;

private:
  DISALLOW_COPY_AND_ASSIGN(TestCell);
};

class OcvDerate
{
public:
  OcvDerate(const char *name);
  ~OcvDerate();
  const char *name() const { return name_; }
  Table *derateTable(const RiseFall *rf,
		     const EarlyLate *early_late,
		     PathType path_type);
  void setDerateTable(const RiseFall *rf,
		      const EarlyLate *early_late,
		      PathType path_type,
		      Table *derate);

private:
  const char *name_;
  // [rf_type][derate_type][path_type]
  Table *derate_[RiseFall::index_count][EarlyLate::index_count][path_type_count];
};

// Power/ground port.
class LibertyPgPort
{
public:
  enum PgType { unknown,
		primary_power, primary_ground,
		backup_power, backup_ground,
		internal_power, internal_ground,
		nwell, pwell,
		deepnwell, deeppwell};
  LibertyPgPort(const char *name,
		LibertyCell *cell);
  ~LibertyPgPort();
  const char *name() { return name_; }
  LibertyCell *cell() const { return cell_; }
  PgType pgType() const { return pg_type_; }
  void setPgType(PgType type);
  const char *voltageName() const { return voltage_name_; }
  void setVoltageName(const char *voltage_name);

private:
  const char *name_;
  PgType pg_type_;
  const char *voltage_name_;
  LibertyCell *cell_;
};

} // namespace
