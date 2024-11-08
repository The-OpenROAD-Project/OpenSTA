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

#include "LibertyReader.hh"

#include <cctype>
#include <cstdlib>

#include "EnumNameMap.hh"
#include "Report.hh"
#include "Debug.hh"
#include "TokenParser.hh"
#include "Units.hh"
#include "Transition.hh"
#include "FuncExpr.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "LeakagePower.hh"
#include "InternalPower.hh"
#include "LinearModel.hh"
#include "Wireload.hh"
#include "EquivCells.hh"
#include "LibertyExpr.hh"
#include "Liberty.hh"
#include "LibertyBuilder.hh"
#include "LibertyReaderPvt.hh"
#include "PortDirection.hh"
#include "ParseBus.hh"
#include "Network.hh"

extern int LibertyParse_debug;

namespace sta {

using std::make_shared;
using std::string;

static void
scaleFloats(FloatSeq *floats,
	    float scale);

LibertyLibrary *
readLibertyFile(const char *filename,
		bool infer_latches,
		Network *network)
{
  LibertyReader reader(filename, infer_latches, network);
  return reader.readLibertyFile(filename);
}

LibertyReader::LibertyReader(const char *filename,
                             bool infer_latches,
                             Network *network) :
  LibertyGroupVisitor()
{
  init(filename, infer_latches, network);
  defineVisitors();
}

void
LibertyReader::init(const char *filename,
                    bool infer_latches,
                    Network *network)
{
  filename_ = filename;
  infer_latches_ = infer_latches;
  report_ = network->report();
  debug_ = network->debug();
  network_ = network;
  var_map_ = nullptr;
  library_ = nullptr;
  wireload_ = nullptr;
  wireload_selection_ = nullptr;
  default_wireload_ = nullptr;
  default_wireload_selection_ = nullptr;
  scale_factors_ = nullptr;
  save_scale_factors_ = nullptr;
  tbl_template_ = nullptr;
  cell_ = nullptr;
  save_cell_ = nullptr;
  scaled_cell_owner_ = nullptr;
  test_cell_ = nullptr;
  ocv_derate_name_ = nullptr;
  op_cond_ = nullptr;
  ports_ = nullptr;
  port_ = nullptr;
  test_port_ = nullptr;
  port_group_ = nullptr;
  saved_ports_ = nullptr;
  saved_port_group_ = nullptr;
  in_bus_ = false;
  in_bundle_ = false;
  sequential_ = nullptr;
  statetable_ = nullptr;
  timing_ = nullptr;
  internal_power_ = nullptr;
  leakage_power_ = nullptr;
  table_ = nullptr;
  rf_ = nullptr;
  index_ = 0;
  table_model_scale_ = 1.0;
  mode_def_ = nullptr;
  mode_value_ = nullptr;
  ocv_derate_ = nullptr;
  pg_port_ = nullptr;
  default_operating_condition_ = nullptr;
  receiver_model_ = nullptr;

  builder_.init(debug_, report_);

  for (auto rf_index : RiseFall::rangeIndex()) {
    have_input_threshold_[rf_index] = false;
    have_output_threshold_[rf_index] = false;
    have_slew_lower_threshold_[rf_index] = false;
    have_slew_upper_threshold_[rf_index] = false;
  }
}

LibertyReader::~LibertyReader()
{
  if (var_map_) {
    LibertyVariableMap::Iterator iter(var_map_);
    while (iter.hasNext()) {
      const char *var;
      float value;
      iter.next(var, value);
      stringDelete(var);
    }
    delete var_map_;
  }

  // Scaling factor attribute names are allocated, so delete them.
  LibraryAttrMap::Iterator attr_iter(attr_visitor_map_);
  while (attr_iter.hasNext()) {
    const char *attr_name;
    LibraryAttrVisitor visitor;
    attr_iter.next(attr_name, visitor);
    stringDelete(attr_name);
  }
}

LibertyLibrary *
LibertyReader::readLibertyFile(const char *filename)
{
  //::LibertyParse_debug = 1;
  parseLibertyFile(filename, this, report_);
  return library_;
}

void
LibertyReader::defineGroupVisitor(const char *type,
				  LibraryGroupVisitor begin_visitor,
				  LibraryGroupVisitor end_visitor)
{
  group_begin_map_[type] = begin_visitor;
  group_end_map_[type] = end_visitor;
}

void
LibertyReader::defineAttrVisitor(const char *attr_name,
				 LibraryAttrVisitor visitor)
{
  attr_visitor_map_[stringCopy(attr_name)] = visitor;
}

void
LibertyReader::defineVisitors()
{
  // Library
  defineGroupVisitor("library", &LibertyReader::beginLibrary,
		     &LibertyReader::endLibrary);
  defineAttrVisitor("time_unit", &LibertyReader::visitTimeUnit);
  defineAttrVisitor("pulling_resistance_unit",
		    &LibertyReader::visitPullingResistanceUnit);
  defineAttrVisitor("resistance_unit", &LibertyReader::visitResistanceUnit);
  defineAttrVisitor("capacitive_load_unit",
		    &LibertyReader::visitCapacitiveLoadUnit);
  defineAttrVisitor("voltage_unit", &LibertyReader::visitVoltageUnit);
  defineAttrVisitor("current_unit", &LibertyReader::visitCurrentUnit);
  defineAttrVisitor("leakage_power_unit", &LibertyReader::visitPowerUnit);
  defineAttrVisitor("distance_unit", &LibertyReader::visitDistanceUnit);
  defineAttrVisitor("delay_model", &LibertyReader::visitDelayModel);
  defineAttrVisitor("bus_naming_style", &LibertyReader::visitBusStyle);
  defineAttrVisitor("voltage_map", &LibertyReader::visitVoltageMap);
  defineAttrVisitor("nom_temperature", &LibertyReader::visitNomTemp);
  defineAttrVisitor("nom_voltage", &LibertyReader::visitNomVolt);
  defineAttrVisitor("nom_process", &LibertyReader::visitNomProc);
  defineAttrVisitor("default_inout_pin_cap",
		    &LibertyReader::visitDefaultInoutPinCap);
  defineAttrVisitor("default_input_pin_cap",
		    &LibertyReader::visitDefaultInputPinCap);
  defineAttrVisitor("default_output_pin_cap",
		    &LibertyReader::visitDefaultOutputPinCap);
  defineAttrVisitor("default_max_transition",
		    &LibertyReader::visitDefaultMaxTransition);
  defineAttrVisitor("default_max_fanout",
		    &LibertyReader::visitDefaultMaxFanout);
  defineAttrVisitor("default_intrinsic_rise",
		    &LibertyReader::visitDefaultIntrinsicRise);
  defineAttrVisitor("default_intrinsic_fall",
		    &LibertyReader::visitDefaultIntrinsicFall);
  defineAttrVisitor("default_inout_pin_rise_res",
		    &LibertyReader::visitDefaultInoutPinRiseRes);
  defineAttrVisitor("default_inout_pin_fall_res",
		    &LibertyReader::visitDefaultInoutPinFallRes);
  defineAttrVisitor("default_output_pin_rise_res",
		    &LibertyReader::visitDefaultOutputPinRiseRes);
  defineAttrVisitor("default_output_pin_fall_res",
		    &LibertyReader::visitDefaultOutputPinFallRes);
  defineAttrVisitor("default_fanout_load",
		    &LibertyReader::visitDefaultFanoutLoad);
  defineAttrVisitor("default_wire_load",
		    &LibertyReader::visitDefaultWireLoad);
  defineAttrVisitor("default_wire_load_mode",
		    &LibertyReader::visitDefaultWireLoadMode);
  defineAttrVisitor("default_wire_load_selection",
		    &LibertyReader::visitDefaultWireLoadSelection);
  defineAttrVisitor("default_operating_conditions",
		    &LibertyReader::visitDefaultOperatingConditions);
  defineAttrVisitor("input_threshold_pct_fall",
		    &LibertyReader::visitInputThresholdPctFall);
  defineAttrVisitor("input_threshold_pct_rise",
		    &LibertyReader::visitInputThresholdPctRise);
  defineAttrVisitor("output_threshold_pct_fall",
		    &LibertyReader::visitOutputThresholdPctFall);
  defineAttrVisitor("output_threshold_pct_rise",
		    &LibertyReader::visitOutputThresholdPctRise);
  defineAttrVisitor("slew_lower_threshold_pct_fall",
		    &LibertyReader::visitSlewLowerThresholdPctFall);
  defineAttrVisitor("slew_lower_threshold_pct_rise",
		    &LibertyReader::visitSlewLowerThresholdPctRise);
  defineAttrVisitor("slew_upper_threshold_pct_fall",
		    &LibertyReader::visitSlewUpperThresholdPctFall);
  defineAttrVisitor("slew_upper_threshold_pct_rise",
		    &LibertyReader::visitSlewUpperThresholdPctRise);
  defineAttrVisitor("slew_derate_from_library",
		    &LibertyReader::visitSlewDerateFromLibrary);

  defineGroupVisitor("lu_table_template",
		     &LibertyReader::beginTableTemplateDelay,
		     &LibertyReader::endTableTemplate);
  defineGroupVisitor("output_current_template",
		     &LibertyReader::beginTableTemplateOutputCurrent,
		     &LibertyReader::endTableTemplate);
  defineAttrVisitor("variable_1", &LibertyReader::visitVariable1);
  defineAttrVisitor("variable_2", &LibertyReader::visitVariable2);
  defineAttrVisitor("variable_3", &LibertyReader::visitVariable3);
  defineAttrVisitor("index_1", &LibertyReader::visitIndex1);
  defineAttrVisitor("index_2", &LibertyReader::visitIndex2);
  defineAttrVisitor("index_3", &LibertyReader::visitIndex3);

  defineGroupVisitor("rise_transition_degradation",
		     &LibertyReader::beginRiseTransitionDegredation,
		     &LibertyReader::endRiseFallTransitionDegredation);
  defineGroupVisitor("fall_transition_degradation",
		     &LibertyReader::beginFallTransitionDegredation,
		     &LibertyReader::endRiseFallTransitionDegredation);

  defineGroupVisitor("type", &LibertyReader::beginType,
		     &LibertyReader::endType);
  defineAttrVisitor("bit_from", &LibertyReader::visitBitFrom);
  defineAttrVisitor("bit_to", &LibertyReader::visitBitTo);

  defineGroupVisitor("scaling_factors", &LibertyReader::beginScalingFactors,
		     &LibertyReader::endScalingFactors);
  defineScalingFactorVisitors();

  defineGroupVisitor("operating_conditions", &LibertyReader::beginOpCond,
		     &LibertyReader::endOpCond);
  defineAttrVisitor("process", &LibertyReader::visitProc);
  defineAttrVisitor("voltage", &LibertyReader::visitVolt);
  defineAttrVisitor("temperature", &LibertyReader::visitTemp);
  defineAttrVisitor("tree_type", &LibertyReader::visitTreeType);

  defineGroupVisitor("wire_load", &LibertyReader::beginWireload,
		     &LibertyReader::endWireload);
  defineAttrVisitor("resistance", &LibertyReader::visitResistance);
  defineAttrVisitor("slope", &LibertyReader::visitSlope);
  defineAttrVisitor("fanout_length", &LibertyReader::visitFanoutLength);

  defineGroupVisitor("wire_load_selection",
		     &LibertyReader::beginWireloadSelection,
		     &LibertyReader::endWireloadSelection);
  defineAttrVisitor("wire_load_from_area",
		    &LibertyReader::visitWireloadFromArea);

  // Cells
  defineGroupVisitor("cell", &LibertyReader::beginCell,
		     &LibertyReader::endCell);
  defineGroupVisitor("scaled_cell", &LibertyReader::beginScaledCell,
		     &LibertyReader::endScaledCell);
  defineAttrVisitor("clock_gating_integrated_cell",
		    &LibertyReader::visitClockGatingIntegratedCell);
  defineAttrVisitor("area", &LibertyReader::visitArea);
  defineAttrVisitor("dont_use", &LibertyReader::visitDontUse);
  defineAttrVisitor("is_macro_cell", &LibertyReader::visitIsMacro);
  defineAttrVisitor("is_memory", &LibertyReader::visitIsMemory);
  defineAttrVisitor("pad_cell", &LibertyReader::visitIsPadCell);
  defineAttrVisitor("is_pad", &LibertyReader::visitIsPad);
  defineAttrVisitor("is_clock_cell", &LibertyReader::visitIsClockCell);
  defineAttrVisitor("is_level_shifter", &LibertyReader::visitIsLevelShifter);
  defineAttrVisitor("level_shifter_type", &LibertyReader::visitLevelShifterType);
  defineAttrVisitor("is_isolation_cell", &LibertyReader::visitIsIsolationCell);
  defineAttrVisitor("always_on", &LibertyReader::visitAlwaysOn);
  defineAttrVisitor("switch_cell_type", &LibertyReader::visitSwitchCellType);
  defineAttrVisitor("interface_timing", &LibertyReader::visitInterfaceTiming);
  defineAttrVisitor("scaling_factors", &LibertyReader::visitScalingFactors);
  defineAttrVisitor("cell_footprint", &LibertyReader::visitCellFootprint);
  defineAttrVisitor("user_function_class",
                    &LibertyReader::visitCellUserFunctionClass);

  // Pins
  defineGroupVisitor("pin", &LibertyReader::beginPin,&LibertyReader::endPin);
  defineGroupVisitor("bus", &LibertyReader::beginBus,&LibertyReader::endBus);
  defineGroupVisitor("bundle", &LibertyReader::beginBundle,
		     &LibertyReader::endBundle);
  defineAttrVisitor("direction", &LibertyReader::visitDirection);
  defineAttrVisitor("clock", &LibertyReader::visitClock);
  defineAttrVisitor("bus_type", &LibertyReader::visitBusType);
  defineAttrVisitor("members", &LibertyReader::visitMembers);
  defineAttrVisitor("function", &LibertyReader::visitFunction);
  defineAttrVisitor("three_state", &LibertyReader::visitThreeState);
  defineAttrVisitor("capacitance", &LibertyReader::visitCapacitance);
  defineAttrVisitor("rise_capacitance", &LibertyReader::visitRiseCap);
  defineAttrVisitor("fall_capacitance", &LibertyReader::visitFallCap);
  defineAttrVisitor("rise_capacitance_range",
		    &LibertyReader::visitRiseCapRange);
  defineAttrVisitor("fall_capacitance_range",
		    &LibertyReader::visitFallCapRange);
  defineAttrVisitor("fanout_load", &LibertyReader::visitFanoutLoad);
  defineAttrVisitor("max_fanout", &LibertyReader::visitMaxFanout);
  defineAttrVisitor("min_fanout", &LibertyReader::visitMinFanout);
  defineAttrVisitor("max_transition", &LibertyReader::visitMaxTransition);
  defineAttrVisitor("min_transition", &LibertyReader::visitMinTransition);
  defineAttrVisitor("max_capacitance", &LibertyReader::visitMaxCapacitance);
  defineAttrVisitor("min_capacitance", &LibertyReader::visitMinCapacitance);
  defineAttrVisitor("min_period", &LibertyReader::visitMinPeriod);
  defineAttrVisitor("min_pulse_width_low",
		    &LibertyReader::visitMinPulseWidthLow);
  defineAttrVisitor("min_pulse_width_high",
		    &LibertyReader::visitMinPulseWidthHigh);
  defineAttrVisitor("pulse_clock",
		    &LibertyReader::visitPulseClock);
  defineAttrVisitor("clock_gate_clock_pin",
		    &LibertyReader::visitClockGateClockPin);
  defineAttrVisitor("clock_gate_enable_pin",
		    &LibertyReader::visitClockGateEnablePin);
  defineAttrVisitor("clock_gate_out_pin",
		    &LibertyReader::visitClockGateOutPin);
  defineAttrVisitor("is_pll_feedback_pin",
		    &LibertyReader::visitIsPllFeedbackPin);
  defineAttrVisitor("signal_type", &LibertyReader::visitSignalType);

  defineAttrVisitor("isolation_cell_data_pin",
                    &LibertyReader::visitIsolationCellDataPin);
  defineAttrVisitor("isolation_cell_enable_pin",
                    &LibertyReader::visitIsolationCellEnablePin);
  defineAttrVisitor("level_shifter_data_pin",
                    &LibertyReader::visitLevelShifterDataPin);
  defineAttrVisitor("switch_pin", &LibertyReader::visitSwitchPin);

  // Register/latch
  defineGroupVisitor("ff", &LibertyReader::beginFF, &LibertyReader::endFF);
  defineGroupVisitor("ff_bank", &LibertyReader::beginFFBank,
		     &LibertyReader::endFFBank);
  defineGroupVisitor("latch", &LibertyReader::beginLatch,
		     &LibertyReader::endLatch);
  defineGroupVisitor("latch_bank", &LibertyReader::beginLatchBank,
		     &LibertyReader::endLatchBank);
  defineAttrVisitor("clocked_on", &LibertyReader::visitClockedOn);
  defineAttrVisitor("enable", &LibertyReader::visitClockedOn);
  defineAttrVisitor("data_in", &LibertyReader::visitDataIn);
  defineAttrVisitor("next_state", &LibertyReader::visitDataIn);
  defineAttrVisitor("clear", &LibertyReader::visitClear);
  defineAttrVisitor("preset", &LibertyReader::visitPreset);
  defineAttrVisitor("clear_preset_var1", &LibertyReader::visitClrPresetVar1);
  defineAttrVisitor("clear_preset_var2", &LibertyReader::visitClrPresetVar2);

  // Statetable
  defineGroupVisitor("statetable", &LibertyReader::beginStatetable,
                     &LibertyReader::endStatetable);
  defineAttrVisitor("table", &LibertyReader::visitTable);

  defineGroupVisitor("timing", &LibertyReader::beginTiming,
		     &LibertyReader::endTiming);
  defineAttrVisitor("related_pin", &LibertyReader::visitRelatedPin);
  defineAttrVisitor("related_bus_pins", &LibertyReader::visitRelatedBusPins);
  defineAttrVisitor("related_output_pin",
		    &LibertyReader::visitRelatedOutputPin);
  defineAttrVisitor("timing_type", &LibertyReader::visitTimingType);
  defineAttrVisitor("timing_sense", &LibertyReader::visitTimingSense);
  defineAttrVisitor("sdf_cond_start", &LibertyReader::visitSdfCondStart);
  defineAttrVisitor("sdf_cond_end", &LibertyReader::visitSdfCondEnd);
  defineAttrVisitor("mode", &LibertyReader::visitMode);
  defineAttrVisitor("intrinsic_rise", &LibertyReader::visitIntrinsicRise);
  defineAttrVisitor("intrinsic_fall", &LibertyReader::visitIntrinsicFall);
  defineAttrVisitor("rise_resistance", &LibertyReader::visitRiseResistance);
  defineAttrVisitor("fall_resistance", &LibertyReader::visitFallResistance);
  defineGroupVisitor("cell_rise", &LibertyReader::beginCellRise,
		     &LibertyReader::endCellRiseFall);
  defineGroupVisitor("cell_fall", &LibertyReader::beginCellFall,
		     &LibertyReader::endCellRiseFall);
  defineGroupVisitor("rise_transition", &LibertyReader::beginRiseTransition,
		     &LibertyReader::endRiseFallTransition);
  defineGroupVisitor("fall_transition", &LibertyReader::beginFallTransition,
		     &LibertyReader::endRiseFallTransition);
  defineGroupVisitor("rise_constraint", &LibertyReader::beginRiseConstraint,
		     &LibertyReader::endRiseFallConstraint);
  defineGroupVisitor("fall_constraint", &LibertyReader::beginFallConstraint,
		     &LibertyReader::endRiseFallConstraint);
  defineAttrVisitor("value", &LibertyReader::visitValue);
  defineAttrVisitor("values", &LibertyReader::visitValues);

  defineGroupVisitor("lut", &LibertyReader::beginLut,&LibertyReader::endLut);

  defineGroupVisitor("test_cell", &LibertyReader::beginTestCell,
		     &LibertyReader::endTestCell);

  defineGroupVisitor("mode_definition", &LibertyReader::beginModeDef,
		     &LibertyReader::endModeDef);
  defineGroupVisitor("mode_value", &LibertyReader::beginModeValue,
		     &LibertyReader::endModeValue);
  defineAttrVisitor("when", &LibertyReader::visitWhen);
  defineAttrVisitor("sdf_cond", &LibertyReader::visitSdfCond);

  // Power attributes.
  defineGroupVisitor("power_lut_template",
		     &LibertyReader::beginTableTemplatePower,
		     &LibertyReader::endTableTemplate);
  defineGroupVisitor("leakage_power", &LibertyReader::beginLeakagePower,
		     &LibertyReader::endLeakagePower);
  defineGroupVisitor("internal_power", &LibertyReader::beginInternalPower,
		     &LibertyReader::endInternalPower);
  // power group for both rise/fall
  defineGroupVisitor("power", &LibertyReader::beginRisePower,
		     &LibertyReader::endPower);
  defineGroupVisitor("fall_power", &LibertyReader::beginFallPower,
		     &LibertyReader::endRiseFallPower);
  defineGroupVisitor("rise_power", &LibertyReader::beginRisePower,
		     &LibertyReader::endRiseFallPower);
  defineAttrVisitor("related_ground_pin",&LibertyReader::visitRelatedGroundPin);
  defineAttrVisitor("related_power_pin", &LibertyReader::visitRelatedPowerPin);
  defineAttrVisitor("related_pg_pin", &LibertyReader::visitRelatedPgPin);

  // AOCV attributes.
  defineAttrVisitor("ocv_arc_depth", &LibertyReader::visitOcvArcDepth);
  defineAttrVisitor("default_ocv_derate_group",
		    &LibertyReader::visitDefaultOcvDerateGroup);
  defineAttrVisitor("ocv_derate_group", &LibertyReader::visitOcvDerateGroup);
  defineGroupVisitor("ocv_table_template",
		     &LibertyReader::beginTableTemplateOcv,
		     &LibertyReader::endTableTemplate);
  defineGroupVisitor("ocv_derate",
		     &LibertyReader::beginOcvDerate,
		     &LibertyReader::endOcvDerate);
  defineGroupVisitor("ocv_derate_factors",
		     &LibertyReader::beginOcvDerateFactors,
		     &LibertyReader::endOcvDerateFactors);
  defineAttrVisitor("rf_type", &LibertyReader::visitRfType);
  defineAttrVisitor("derate_type", &LibertyReader::visitDerateType);
  defineAttrVisitor("path_type", &LibertyReader::visitPathType);

  // POCV attributes.
  defineGroupVisitor("ocv_sigma_cell_rise", &LibertyReader::beginOcvSigmaCellRise,
		     &LibertyReader::endOcvSigmaCell);
  defineGroupVisitor("ocv_sigma_cell_fall", &LibertyReader::beginOcvSigmaCellFall,
		     &LibertyReader::endOcvSigmaCell);
  defineGroupVisitor("ocv_sigma_rise_transition",
		     &LibertyReader::beginOcvSigmaRiseTransition,
		     &LibertyReader::endOcvSigmaTransition);
  defineGroupVisitor("ocv_sigma_fall_transition",
		     &LibertyReader::beginOcvSigmaFallTransition,
		     &LibertyReader::endOcvSigmaTransition);
  defineGroupVisitor("ocv_sigma_rise_constraint",
		     &LibertyReader::beginOcvSigmaRiseConstraint,
		     &LibertyReader::endOcvSigmaConstraint);
  defineGroupVisitor("ocv_sigma_fall_constraint",
		     &LibertyReader::beginOcvSigmaFallConstraint,
		     &LibertyReader::endOcvSigmaConstraint);
  defineAttrVisitor("sigma_type", &LibertyReader::visitSigmaType);
  defineAttrVisitor("cell_leakage_power", &LibertyReader::visitCellLeakagePower);

  defineGroupVisitor("pg_pin", &LibertyReader::beginPgPin,
		     &LibertyReader::endPgPin);
  defineAttrVisitor("pg_type", &LibertyReader::visitPgType);
  defineAttrVisitor("voltage_name", &LibertyReader::visitVoltageName);

  // ccs receiver capacitance
  defineGroupVisitor("receiver_capacitance",
                    &LibertyReader::beginReceiverCapacitance,
                    &LibertyReader::endReceiverCapacitance);

  defineGroupVisitor("receiver_capacitance_rise",
                    &LibertyReader::beginReceiverCapacitance1Rise,
                    &LibertyReader::endReceiverCapacitanceRiseFall);
  defineGroupVisitor("receiver_capacitance_fall",
                    &LibertyReader::beginReceiverCapacitance1Fall,
                    &LibertyReader::endReceiverCapacitanceRiseFall);
  defineAttrVisitor("segment", &LibertyReader::visitSegement);

  defineGroupVisitor("receiver_capacitance1_rise",
                    &LibertyReader::beginReceiverCapacitance1Rise,
                    &LibertyReader::endReceiverCapacitanceRiseFall);
  defineGroupVisitor("receiver_capacitance1_fall",
                    &LibertyReader::beginReceiverCapacitance1Fall,
                    &LibertyReader::endReceiverCapacitanceRiseFall);
  defineGroupVisitor("receiver_capacitance2_rise",
                    &LibertyReader::beginReceiverCapacitance2Rise,
                    &LibertyReader::endReceiverCapacitanceRiseFall);
  defineGroupVisitor("receiver_capacitance2_fall",
                    &LibertyReader::beginReceiverCapacitance2Fall,
                    &LibertyReader::endReceiverCapacitanceRiseFall);
  // ccs
  defineGroupVisitor("output_current_rise",
                    &LibertyReader::beginOutputCurrentRise,
                    &LibertyReader::endOutputCurrentRiseFall);
  defineGroupVisitor("output_current_fall",
                    &LibertyReader::beginOutputCurrentFall,
                    &LibertyReader::endOutputCurrentRiseFall);
  defineGroupVisitor("vector", &LibertyReader::beginVector, &LibertyReader::endVector);
  defineAttrVisitor("reference_time", &LibertyReader::visitReferenceTime);
  defineGroupVisitor("normalized_driver_waveform",
                     &LibertyReader::beginNormalizedDriverWaveform,
                     &LibertyReader::endNormalizedDriverWaveform);
  defineAttrVisitor("driver_waveform_name", &LibertyReader::visitDriverWaveformName);
  defineAttrVisitor("driver_waveform_rise", &LibertyReader::visitDriverWaveformRise);
  defineAttrVisitor("driver_waveform_fall", &LibertyReader::visitDriverWaveformFall);
}

void
LibertyReader::defineScalingFactorVisitors()
{
  for (int type_index = 0; type_index < scale_factor_type_count; type_index++) {
    ScaleFactorType type = static_cast<ScaleFactorType>(type_index);
    const char *type_name = scaleFactorTypeName(type);
    for (int pvt_index = 0; pvt_index < scale_factor_pvt_count; pvt_index++) {
      ScaleFactorPvt pvt = static_cast<ScaleFactorPvt>(pvt_index);
      const char *pvt_name = scaleFactorPvtName(pvt);
      if (scaleFactorTypeRiseFallSuffix(type)) {
	for (auto tr : RiseFall::range()) {
	  const char *tr_name = (tr == RiseFall::rise()) ? "rise":"fall";
	  string attr_name;
          stringPrint(attr_name, "k_%s_%s_%s",
                      pvt_name,
                      type_name,
                      tr_name);
	  defineAttrVisitor(attr_name.c_str() ,&LibertyReader::visitScaleFactorSuffix);
	}
      }
      else if (scaleFactorTypeRiseFallPrefix(type)) {
	for (auto tr : RiseFall::range()) {
	  const char *tr_name = (tr == RiseFall::rise()) ? "rise":"fall";
	  string attr_name;
          stringPrint(attr_name, "k_%s_%s_%s",
                      pvt_name,
                      tr_name,
                      type_name);
	  defineAttrVisitor(attr_name.c_str(),&LibertyReader::visitScaleFactorPrefix);
	}
      }
      else if (scaleFactorTypeLowHighSuffix(type)) {
	for (auto tr : RiseFall::range()) {
	  const char *tr_name = (tr == RiseFall::rise()) ? "high":"low";
	  string attr_name;
          stringPrint(attr_name, "k_%s_%s_%s",
                      pvt_name,
                      tr_name,
                      type_name);
	  defineAttrVisitor(attr_name.c_str(),&LibertyReader::visitScaleFactorHiLow);
	}
      }
      else {
	  string attr_name;
          stringPrint(attr_name, "k_%s_%s",
                      pvt_name,
                      type_name);
          defineAttrVisitor(attr_name.c_str(),&LibertyReader::visitScaleFactor);
      }
    }
  }
}

void
LibertyReader::visitAttr(LibertyAttr *attr)
{
  LibraryAttrVisitor visitor = attr_visitor_map_.findKey(attr->name());
  if (visitor)
    (this->*visitor)(attr);
}

void
LibertyReader::begin(LibertyGroup *group)
{
  LibraryGroupVisitor visitor = group_begin_map_.findKey(group->type());
  if (visitor)
    (this->*visitor)(group);
}

void
LibertyReader::end(LibertyGroup *group)
{
  LibraryGroupVisitor visitor = group_end_map_.findKey(group->type());
  if (visitor)
    (this->*visitor)(group);
}

void
LibertyReader::beginLibrary(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name) {
    LibertyLibrary *library = network_->findLiberty(name);
    if (library)
      libWarn(1140, group, "library %s already exists.", name);
    // Make a new library even if a library with the same name exists.
    // Both libraries may be accessed by min/max analysis points.
    library_ = network_->makeLibertyLibrary(name, filename_);
    // 1ns default
    time_scale_ = 1E-9F;
    // 1ohm default
    res_scale_ = 1.0F;
    // pF default
    cap_scale_ = 1E-12F;
    // 1v default
    volt_scale_ = 1;
    // Default is 1mA.
    current_scale_ = 1E-3F;
    // Default is 1;
    power_scale_ = 1;
    // Default is fJ.
    setEnergyScale();
    // Default is 1 micron.
    distance_scale_ = 1e-6;

    library_->units()->timeUnit()->setScale(time_scale_);
    library_->units()->capacitanceUnit()->setScale(cap_scale_);
    library_->units()->resistanceUnit()->setScale(res_scale_);
    library_->units()->voltageUnit()->setScale(volt_scale_);
    library_->units()->currentUnit()->setScale(current_scale_);
    library_->units()->distanceUnit()->setScale(distance_scale_);

    scale_factors_ = new ScaleFactors("");
    library_->setScaleFactors(scale_factors_);
  }
  else
    libError(1141, group, "library missing name.");
}

// Energy scale is derived.
void
LibertyReader::setEnergyScale()
{
    energy_scale_ = volt_scale_ * volt_scale_ * cap_scale_;
}

void
LibertyReader::endLibrary(LibertyGroup *group)
{
  endLibraryAttrs(group);
}

void
LibertyReader::endLibraryAttrs(LibertyGroup *group)
{
  // These attributes reference named groups in the library so
  // wait until the end of the library to resolve them.
  if (default_wireload_) {
    Wireload *wireload = library_->findWireload(default_wireload_);
    if (wireload)
      library_->setDefaultWireload(wireload);
    else
      libWarn(1142, group, "default_wire_load %s not found.", default_wireload_);
    stringDelete(default_wireload_);
    default_wireload_ = nullptr;
  }

  if (default_wireload_selection_) {
    WireloadSelection *selection =
      library_->findWireloadSelection(default_wireload_selection_);
    if (selection)
      library_->setDefaultWireloadSelection(selection);
    else
      libWarn(1143, group, "default_wire_selection %s not found.",
	      default_wireload_selection_);
    stringDelete(default_wireload_selection_);
    default_wireload_selection_ = nullptr;
  }

  if (default_operating_condition_) {
    OperatingConditions *op_cond =
      library_->findOperatingConditions(default_operating_condition_);
    if (op_cond)
      library_->setDefaultOperatingConditions(op_cond);
    else
      libWarn(1144, group, "default_operating_condition %s not found.",
	      default_operating_condition_);
    stringDelete(default_operating_condition_);
    default_operating_condition_ = nullptr;
  }

  bool missing_threshold = false;
  for (auto rf : RiseFall::range()) {
    int rf_index = rf->index();
    if (!have_input_threshold_[rf_index]) {
      libWarn(1145, group, "input_threshold_pct_%s not found.", rf->name());
      missing_threshold = true;
    }
    if (!have_output_threshold_[rf_index]) {
      libWarn(1146, group, "output_threshold_pct_%s not found.", rf->name());
      missing_threshold = true;
    }
    if (!have_slew_lower_threshold_[rf_index]) {
      libWarn(1147, group, "slew_lower_threshold_pct_%s not found.", rf->name());
      missing_threshold = true;
    }
    if (!have_slew_upper_threshold_[rf_index]) {
      libWarn(1148, group, "slew_upper_threshold_pct_%s not found.", rf->name());
      missing_threshold = true;
    }
  }
  if (missing_threshold)
    libError(1149, group, "Library %s is missing one or more thresholds.",
	     library_->name());
}

void
LibertyReader::visitTimeUnit(LibertyAttr *attr)
{
  if (library_)
    parseUnits(attr, "s", time_scale_, library_->units()->timeUnit());
}

void
LibertyReader::visitPullingResistanceUnit(LibertyAttr *attr)
{
  if (library_)
    parseUnits(attr, "ohm", res_scale_,
	       library_->units()->resistanceUnit());
}

void
LibertyReader::visitResistanceUnit(LibertyAttr *attr)
{
  if (library_)
    parseUnits(attr, "ohm", res_scale_, library_->units()->resistanceUnit());
}

void
LibertyReader::visitCurrentUnit(LibertyAttr *attr)
{
  if (library_)
    parseUnits(attr, "A", current_scale_, library_->units()->currentUnit());
}

void
LibertyReader::visitVoltageUnit(LibertyAttr *attr)
{
  if (library_)
    parseUnits(attr, "V", volt_scale_, library_->units()->voltageUnit());
  setEnergyScale();
}

void
LibertyReader::visitPowerUnit(LibertyAttr *attr)
{
  if (library_)
    parseUnits(attr, "W", power_scale_, library_->units()->powerUnit());
}

void
LibertyReader::visitDistanceUnit(LibertyAttr *attr)
{
  if (library_)
    parseUnits(attr, "m", distance_scale_, library_->units()->distanceUnit());
}

void
LibertyReader::parseUnits(LibertyAttr *attr,
			  const char *unit_suffix,
			  float &scale_var,
			  Unit *unit)
{
  string units = getAttrString(attr);
  if (!units.empty()) {
    // Unit format is <multipler_digits><scale_suffix_char><unit_suffix>.
    // Find the multiplier digits.
    string units = getAttrString(attr);
    size_t mult_end = units.find_first_not_of("0123456789");
    float mult = 1.0F;
    string scale_suffix;
    if (mult_end != units.npos) {
      string unit_mult = units.substr(0, mult_end);
      scale_suffix = units.substr(mult_end);
      if (unit_mult == "1")
        mult = 1.0F;
      else if (unit_mult == "10")
        mult = 10.0F;
      else if (unit_mult == "100")
        mult = 100.0F;
      else
        libWarn(1150, attr, "unknown unit multiplier %s.", unit_mult.c_str());
    }
    else
      scale_suffix = units;

    float scale_mult = 1.0F;
    if (scale_suffix.size() == strlen(unit_suffix) + 1) {
      string suffix = scale_suffix.substr(1);
      if (stringEqual(suffix.c_str(), unit_suffix)) {
        char scale_char = tolower(scale_suffix[0]);
        if (scale_char == 'k')
          scale_mult = 1E+3F;
        else if (scale_char == 'm')
          scale_mult = 1E-3F;
        else if (scale_char == 'u')
          scale_mult = 1E-6F;
        else if (scale_char == 'n')
          scale_mult = 1E-9F;
        else if (scale_char == 'p')
          scale_mult = 1E-12F;
        else if (scale_char == 'f')
          scale_mult = 1E-15F;
        else
          libWarn(1151, attr, "unknown unit scale %c.", scale_char);
      }
      else
        libWarn(1152, attr, "unknown unit suffix %s.", suffix.c_str());
    }
    else if (!stringEqual(scale_suffix.c_str(), unit_suffix))
      libWarn(1153, attr, "unknown unit suffix %s.", scale_suffix.c_str());
    scale_var = scale_mult * mult;
    unit->setScale(scale_var);
  }
}

void
LibertyReader::visitCapacitiveLoadUnit(LibertyAttr *attr)
{
  if (library_) {
    if (attr->isComplex()) {
      LibertyAttrValueIterator value_iter(attr->values());
      if (value_iter.hasNext()) {
	LibertyAttrValue *value = value_iter.next();
	if (value->isFloat()) {
	  float scale = value->floatValue();
	  if (value_iter.hasNext()) {
	    value = value_iter.next();
	    if (value->isString()) {
	      const char *suffix = value->stringValue();
	      if (stringEqual(suffix, "ff"))
		cap_scale_ = scale * 1E-15F;
	      else if (stringEqual(suffix, "pf"))
		cap_scale_ = scale * 1E-12F;
	      else
		libWarn(1154, attr, "capacitive_load_units are not ff or pf.");
	    }
	    else
	      libWarn(1155, attr, "capacitive_load_units are not a string.");
	  }
	  else
	    libWarn(1156, attr, "capacitive_load_units missing suffix.");
	}
	else
	  libWarn(1157, attr, "capacitive_load_units scale is not a float.");
      }
      else
	libWarn(1158, attr, "capacitive_load_units missing scale and suffix.");
    }
    else
      libWarn(1159, attr, "capacitive_load_unit missing values suffix.");
    library_->units()->capacitanceUnit()->setScale(cap_scale_);
    setEnergyScale();
  }
}

void
LibertyReader::visitDelayModel(LibertyAttr *attr)
{
  if (library_) {
    const char *type_name = getAttrString(attr);
    if (type_name) {
      if (stringEq(type_name, "table_lookup"))
	library_->setDelayModelType(DelayModelType::table);
      else if (stringEq(type_name, "generic_cmos"))
	library_->setDelayModelType(DelayModelType::cmos_linear);
      else if (stringEq(type_name, "piecewise_cmos")) {
	library_->setDelayModelType(DelayModelType::cmos_pwl);
	libWarn(1160, attr, "delay_model %s not supported.", type_name);
      }
      else if (stringEq(type_name, "cmos2")) {
	library_->setDelayModelType(DelayModelType::cmos2);
	libWarn(1161, attr, "delay_model %s not supported.", type_name);
      }
      else if (stringEq(type_name, "polynomial")) {
	library_->setDelayModelType(DelayModelType::polynomial);
	libWarn(1162, attr, "delay_model %s not supported.", type_name);
      }
      // Evil IBM garbage.
      else if (stringEq(type_name, "dcm")) {
	library_->setDelayModelType(DelayModelType::dcm);
	libWarn(1163, attr, "delay_model %s not supported..", type_name);
      }
      else
	libWarn(1164, attr, "unknown delay_model %s.", type_name);
    }
  }
}

void
LibertyReader::visitBusStyle(LibertyAttr *attr)
{
  if (library_) {
    const char *bus_style = getAttrString(attr);
    // Assume bus style is of the form "%s[%d]".
    if (bus_style
	&& strlen(bus_style) == 6
	&& bus_style[0] == '%'
	&& bus_style[1] == 's'
	&& bus_style[3] == '%'
	&& bus_style[4] == 'd')
      library_->setBusBrkts(bus_style[2], bus_style[5]);
    else
      libWarn(1165, attr, "unknown bus_naming_style format.");
  }
}

void
LibertyReader::visitVoltageMap(LibertyAttr *attr)
{
  if (library_) {
    if (attr->isComplex()) {
      LibertyAttrValueIterator value_iter(attr->values());
      if (value_iter.hasNext()) {
	LibertyAttrValue *value = value_iter.next();
	if (value->isString()) {
	  const char *supply_name = value->stringValue();
	  if (value_iter.hasNext()) {
	    value = value_iter.next();
	    if (value->isFloat()) {
	      float voltage = value->floatValue();
	      library_->addSupplyVoltage(supply_name, voltage);
	    }
	    else
	      libWarn(1166, attr, "voltage_map voltage is not a float.");
	  }
	  else
	    libWarn(1167, attr, "voltage_map missing voltage.");
	}
	else
	  libWarn(1168, attr, "voltage_map supply name is not a string.");
      }
      else
	libWarn(1169, attr, "voltage_map missing supply name and voltage.");
    }
    else
      libWarn(1170, attr, "voltage_map missing values suffix.");
  }
}

void
LibertyReader::visitNomTemp(LibertyAttr *attr)
{
  if (library_) {
    float value;
    bool valid;
    getAttrFloat(attr, value, valid);
    if (valid)
      library_->setNominalTemperature(value);
  }
}

void
LibertyReader::visitNomProc(LibertyAttr *attr)
{
  if (library_) {
    float value;
    bool valid;
    getAttrFloat(attr, value, valid);
    if (valid)
      library_->setNominalProcess(value);
  }
}

void
LibertyReader::visitNomVolt(LibertyAttr *attr)
{
  if (library_) {
    float value;
    bool valid;
    getAttrFloat(attr, value, valid);
    if (valid)
      library_->setNominalVoltage(value);
  }
}

void
LibertyReader::visitDefaultInoutPinCap(LibertyAttr *attr)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      library_->setDefaultBidirectPinCap(value * cap_scale_);
  }
}

void
LibertyReader::visitDefaultInputPinCap(LibertyAttr *attr)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      library_->setDefaultInputPinCap(value * cap_scale_);
  }
}

void
LibertyReader::visitDefaultOutputPinCap(LibertyAttr *attr)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      library_->setDefaultOutputPinCap(value * cap_scale_);
  }
}

void
LibertyReader::visitDefaultMaxTransition(LibertyAttr *attr)
{
  if (library_){
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists) {
      if (value == 0.0)
	libWarn(1171, attr, "default_max_transition is 0.0.");
      library_->setDefaultMaxSlew(value * time_scale_);
    }
  }
}

void
LibertyReader::visitDefaultMaxFanout(LibertyAttr *attr)
{
  if (library_){
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists) {
      if (value == 0.0)
	libWarn(1172, attr, "default_max_fanout is 0.0.");
      library_->setDefaultMaxFanout(value);
    }
  }
}

void
LibertyReader::visitDefaultIntrinsicRise(LibertyAttr *attr)
{
  visitDefaultIntrinsic(attr, RiseFall::rise());
}

void
LibertyReader::visitDefaultIntrinsicFall(LibertyAttr *attr)
{
  visitDefaultIntrinsic(attr, RiseFall::fall());
}

void
LibertyReader::visitDefaultIntrinsic(LibertyAttr *attr,
				     RiseFall *rf)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      library_->setDefaultIntrinsic(rf, value * time_scale_);
  }
}

void
LibertyReader::visitDefaultInoutPinRiseRes(LibertyAttr *attr)
{
  visitDefaultInoutPinRes(attr, RiseFall::rise());
}

void
LibertyReader::visitDefaultInoutPinFallRes(LibertyAttr *attr)
{
  visitDefaultInoutPinRes(attr, RiseFall::fall());
}

void
LibertyReader::visitDefaultInoutPinRes(LibertyAttr *attr,
				       RiseFall *rf)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      library_->setDefaultBidirectPinRes(rf, value * res_scale_);
  }
}

void
LibertyReader::visitDefaultOutputPinRiseRes(LibertyAttr *attr)
{
  visitDefaultOutputPinRes(attr, RiseFall::rise());
}

void
LibertyReader::visitDefaultOutputPinFallRes(LibertyAttr *attr)
{
  visitDefaultOutputPinRes(attr, RiseFall::fall());
}

void
LibertyReader::visitDefaultOutputPinRes(LibertyAttr *attr,
					RiseFall *rf)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      library_->setDefaultOutputPinRes(rf, value * res_scale_);
  }
}

void
LibertyReader::visitDefaultFanoutLoad(LibertyAttr *attr)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists) {
      if (value == 0.0)
	libWarn(1173, attr, "default_fanout_load is 0.0.");
      library_->setDefaultFanoutLoad(value);
    }
  }
}

void
LibertyReader::visitDefaultWireLoad(LibertyAttr *attr)
{
  if (library_) {
    const char *value = getAttrString(attr);
    if (value) {
      stringDelete(default_wireload_);
      default_wireload_ = stringCopy(value);
    }
  }
}

void
LibertyReader::visitDefaultWireLoadMode(LibertyAttr *attr)
{
  if (library_) {
    const char *wire_load_mode = getAttrString(attr);
    if (wire_load_mode) {
      WireloadMode mode = stringWireloadMode(wire_load_mode);
      if (mode != WireloadMode::unknown)
	library_->setDefaultWireloadMode(mode);
      else
	libWarn(1174, attr, "default_wire_load_mode %s not found.",
		wire_load_mode);
    }
  }
}

void
LibertyReader::visitDefaultWireLoadSelection(LibertyAttr *attr)
{
  if (library_) {
    const char *value = getAttrString(attr);
    if (value) {
      stringDelete(default_wireload_selection_);
      default_wireload_selection_ = stringCopy(value);
    }
  }
}

void
LibertyReader::visitDefaultOperatingConditions(LibertyAttr *attr)
{
  if (library_) {
    const char *value = getAttrString(attr);
    if (value) {
      stringDelete(default_operating_condition_);
      default_operating_condition_ = stringCopy(value);
    }
  }
}

void
LibertyReader::visitInputThresholdPctFall(LibertyAttr *attr)
{
  visitInputThresholdPct(attr, RiseFall::fall());
}

void
LibertyReader::visitInputThresholdPctRise(LibertyAttr *attr)
{
  visitInputThresholdPct(attr, RiseFall::rise());
}

void
LibertyReader::visitInputThresholdPct(LibertyAttr *attr,
				      RiseFall *rf)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
    library_->setInputThreshold(rf, value / 100.0F);
  }
  have_input_threshold_[rf->index()] = true;
}

void
LibertyReader::visitOutputThresholdPctFall(LibertyAttr *attr)
{
  visitOutputThresholdPct(attr, RiseFall::fall());
}

void
LibertyReader::visitOutputThresholdPctRise(LibertyAttr *attr)
{
  visitOutputThresholdPct(attr, RiseFall::rise());
}

void
LibertyReader::visitOutputThresholdPct(LibertyAttr *attr,
				       RiseFall *rf)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
    library_->setOutputThreshold(rf, value / 100.0F);
  }
  have_output_threshold_[rf->index()] = true;
}

void
LibertyReader::visitSlewLowerThresholdPctFall(LibertyAttr *attr)
{
  visitSlewLowerThresholdPct(attr, RiseFall::fall());
}

void
LibertyReader::visitSlewLowerThresholdPctRise(LibertyAttr *attr)
{
  visitSlewLowerThresholdPct(attr, RiseFall::rise());
}

void
LibertyReader::visitSlewLowerThresholdPct(LibertyAttr *attr,
					  RiseFall *rf)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      library_->setSlewLowerThreshold(rf, value / 100.0F);
  }
  have_slew_lower_threshold_[rf->index()] = true;
}

void
LibertyReader::visitSlewUpperThresholdPctFall(LibertyAttr *attr)
{
  visitSlewUpperThresholdPct(attr, RiseFall::fall());
}

void
LibertyReader::visitSlewUpperThresholdPctRise(LibertyAttr *attr)
{
  visitSlewUpperThresholdPct(attr, RiseFall::rise());
}

void
LibertyReader::visitSlewUpperThresholdPct(LibertyAttr *attr,
					  RiseFall *rf)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
    library_->setSlewUpperThreshold(rf, value / 100.0F);
  }
  have_slew_upper_threshold_[rf->index()] = true;
}

void
LibertyReader::visitSlewDerateFromLibrary(LibertyAttr *attr)
{
  if (library_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
    library_->setSlewDerateFromLibrary(value);
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginTableTemplateDelay(LibertyGroup *group)
{
  beginTableTemplate(group, TableTemplateType::delay);
}

void
LibertyReader::beginTableTemplateOutputCurrent(LibertyGroup *group)
{
  beginTableTemplate(group, TableTemplateType::output_current);
}

void
LibertyReader::beginTableTemplate(LibertyGroup *group,
				  TableTemplateType type)
{
  if (library_) {
    const char *name = group->firstName();
    if (name) {
      tbl_template_ = new TableTemplate(name);
      library_->addTableTemplate(tbl_template_, type);
    }
    else
      libWarn(1175, group, "table template missing name.");
    axis_var_[0] = axis_var_[1] = axis_var_[2] = TableAxisVariable::unknown;
    clearAxisValues();
  }
}

void
LibertyReader::clearAxisValues()
{
  axis_values_[0] = axis_values_[1] = axis_values_[2] = nullptr;
}

void
LibertyReader::endTableTemplate(LibertyGroup *group)
{
  if (tbl_template_) {
    TableAxisPtr axis1 = makeAxis(0, group);
    if (axis1)
      tbl_template_->setAxis1(axis1);
    TableAxisPtr axis2 = makeAxis(1, group);
    if (axis2)
      tbl_template_->setAxis2(axis2);
    TableAxisPtr axis3 = makeAxis(2, group);
    if (axis3)
      tbl_template_->setAxis3(axis3);
    tbl_template_ = nullptr;
    axis_var_[0] = axis_var_[1] = axis_var_[2] = TableAxisVariable::unknown;
  }
}

TableAxisPtr
LibertyReader::makeAxis(int index,
			LibertyGroup *group)
{
  TableAxisVariable axis_var = axis_var_[index];
  FloatSeq *axis_values = axis_values_[index];
  if (axis_var != TableAxisVariable::unknown) {
    if (axis_values) {
      const Units *units = library_->units();
      float scale = tableVariableUnit(axis_var, units)->scale();
      scaleFloats(axis_values, scale);
    }
    return make_shared<TableAxis>(axis_var, axis_values);
  }
  else if (axis_values) {
    libWarn(1176, group, "missing variable_%d attribute.", index + 1);
    delete axis_values;
    axis_values_[index] = nullptr;
  }
  // No warning for missing index_xx attributes because they are not required.
  return nullptr;
}

static void
scaleFloats(FloatSeq *floats, float scale)
{
  size_t count = floats->size();
  for (size_t i = 0; i < count; i++)
    (*floats)[i] *= scale;
}

void
LibertyReader::visitVariable1(LibertyAttr *attr)
{
  visitVariable(0, attr);
}

void
LibertyReader::visitVariable2(LibertyAttr *attr)
{
  visitVariable(1, attr);
}

void
LibertyReader::visitVariable3(LibertyAttr *attr)
{
  visitVariable(2, attr);
}

void
LibertyReader::visitVariable(int index,
			     LibertyAttr *attr)
{
  if (tbl_template_) {
    const char *type = getAttrString(attr);
    TableAxisVariable var = stringTableAxisVariable(type);
    if (var == TableAxisVariable::unknown)
      libWarn(1297, attr, "axis type %s not supported.", type);
    else
      axis_var_[index] = var;
  }
}

void
LibertyReader::visitIndex1(LibertyAttr *attr)
{
  visitIndex(0, attr);
}

void
LibertyReader::visitIndex2(LibertyAttr *attr)
{
  visitIndex(1, attr);
}

void
LibertyReader::visitIndex3(LibertyAttr *attr)
{
  visitIndex(2, attr);
}

void
LibertyReader::visitIndex(int index,
			  LibertyAttr *attr)
{
  if (tbl_template_
      // Ignore index_xx in ecsm_waveform groups.
      && !stringEq(libertyGroup()->type(), "ecsm_waveform")) {
    FloatSeq *axis_values = readFloatSeq(attr, 1.0F);
    if (axis_values) {
      if (axis_values->empty())
        libWarn(1177, attr, "missing table index values.");
      else {
        float prev = (*axis_values)[0];
        for (size_t i = 1; i < axis_values->size(); i++) {
          float value = (*axis_values)[i];
          if (value <= prev)
            libWarn(1178, attr, "non-increasing table index values.");
          prev = value;
        }
      }
      axis_values_[index] = axis_values;
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginType(LibertyGroup *)
{
  type_bit_from_exists_ = false;
  type_bit_to_exists_ = false;
}

void
LibertyReader::endType(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name) {
    if (type_bit_from_exists_ && type_bit_to_exists_) {
      BusDcl *bus_dcl = new BusDcl(name, type_bit_from_, type_bit_to_);
      if (cell_)
	cell_->addBusDcl(bus_dcl);
      else if (library_)
	library_->addBusDcl(bus_dcl);
    }
    else {
      if (!type_bit_from_exists_)
	libWarn(1179, group, "bus type %s missing bit_from.", name);
      if (!type_bit_to_exists_)
	libWarn(1180, group, "bus type %s missing bit_to.", name);
    }
  }
  else
    libWarn(1181, group, "type missing name.");
}

void
LibertyReader::visitBitFrom(LibertyAttr *attr)
{
  getAttrInt(attr, type_bit_from_, type_bit_from_exists_);
}

void
LibertyReader::visitBitTo(LibertyAttr *attr)
{
  getAttrInt(attr, type_bit_to_, type_bit_to_exists_);
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginScalingFactors(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name) {
    save_scale_factors_ = scale_factors_;
    scale_factors_ = new ScaleFactors(name);
    library_->addScaleFactors(scale_factors_);
  }
  else
    libWarn(1182, group, "scaling_factors do not have a name.");
}

void
LibertyReader::endScalingFactors(LibertyGroup *)
{
  scale_factors_ = save_scale_factors_;
}

void
LibertyReader::visitScaleFactorSuffix(LibertyAttr *attr)
{
  if (scale_factors_) {
    ScaleFactorPvt pvt = ScaleFactorPvt::unknown;
    ScaleFactorType type = ScaleFactorType::unknown;
    RiseFall *rf = nullptr;
    // Parse the attribute name.
    TokenParser parser(attr->name(), "_");
    if (parser.hasNext())
      parser.next();
    if (parser.hasNext()) {
      const char *pvt_name = parser.next();
      pvt = findScaleFactorPvt(pvt_name);
    }
    if (parser.hasNext()) {
      const char *type_name = parser.next();
      type = findScaleFactorType(type_name);
    }
    if (parser.hasNext()) {
      const char *tr_name = parser.next();
      if (stringEq(tr_name, "rise"))
	rf = RiseFall::rise();
      else if (stringEq(tr_name, "fall"))
	rf = RiseFall::fall();
    }
    if (pvt != ScaleFactorPvt::unknown
	&& type != ScaleFactorType::unknown
	&& rf) {
      float value;
      bool exists;
      getAttrFloat(attr, value, exists);
      if (exists)
	scale_factors_->setScale(type, pvt, rf, value);
    }
  }
}

void
LibertyReader::visitScaleFactorPrefix(LibertyAttr *attr)
{
  if (scale_factors_) {
    ScaleFactorPvt pvt = ScaleFactorPvt::unknown;
    ScaleFactorType type = ScaleFactorType::unknown;
    RiseFall *rf = nullptr;
    // Parse the attribute name.
    TokenParser parser(attr->name(), "_");
    if (parser.hasNext())
      parser.next();
    if (parser.hasNext()) {
      const char *pvt_name = parser.next();
      pvt = findScaleFactorPvt(pvt_name);
    }
    if (parser.hasNext()) {
      const char *tr_name = parser.next();
      if (stringEq(tr_name, "rise"))
	rf = RiseFall::rise();
      else if (stringEq(tr_name, "fall"))
	rf = RiseFall::fall();
    }
    if (parser.hasNext()) {
      const char *type_name = parser.next();
      type = findScaleFactorType(type_name);
    }
    if (pvt != ScaleFactorPvt::unknown
	&& type != ScaleFactorType::unknown
	&& rf) {
      float value;
      bool exists;
      getAttrFloat(attr, value, exists);
      if (exists)
	scale_factors_->setScale(type, pvt, rf, value);
    }
  }
}

void
LibertyReader::visitScaleFactorHiLow(LibertyAttr *attr)
{
  if (scale_factors_) {
    ScaleFactorPvt pvt = ScaleFactorPvt::unknown;
    ScaleFactorType type = ScaleFactorType::unknown;
    RiseFall *rf = nullptr;
    const char *pvt_name = nullptr;
    const char *type_name = nullptr;
    const char *tr_name = nullptr;
    // Parse the attribute name.
    TokenParser parser(attr->name(), "_");
    if (parser.hasNext())
      parser.next();
    if (parser.hasNext()) {
      pvt_name = parser.next();
      pvt = findScaleFactorPvt(pvt_name);
    }
    if (parser.hasNext()) {
      type_name = parser.next();
      type = findScaleFactorType(type_name);
    }
    if (parser.hasNext()) {
      tr_name = parser.next();
      if (stringEq(tr_name, "high"))
	rf = RiseFall::rise();
      else if (stringEq(tr_name, "low"))
	rf = RiseFall::fall();
    }
    if (pvt != ScaleFactorPvt::unknown
	&& type != ScaleFactorType::unknown
	&& rf) {
      float value;
      bool exists;
      getAttrFloat(attr, value, exists);
      if (exists)
	scale_factors_->setScale(type, pvt, rf, value);
    }
  }
}

void
LibertyReader::visitScaleFactor(LibertyAttr *attr)
{
  if (scale_factors_) {
    ScaleFactorPvt pvt = ScaleFactorPvt::unknown;
    ScaleFactorType type = ScaleFactorType::unknown;
    const char *pvt_name = nullptr;
    const char *type_name = nullptr;
    // Parse the attribute name.
    TokenParser parser(attr->name(), " ");
    if (parser.hasNext())
      parser.next();
    if (parser.hasNext()) {
      pvt_name = parser.next();
      pvt = findScaleFactorPvt(pvt_name);
    }
    if (parser.hasNext()) {
      type_name = parser.next();
      type = findScaleFactorType(type_name);
    }
    if (pvt != ScaleFactorPvt::unknown
	&& type != ScaleFactorType::unknown) {
      float value;
      bool exists;
      getAttrFloat(attr, value, exists);
      if (exists)
	scale_factors_->setScale(type, pvt, value);
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginOpCond(LibertyGroup *group)
{
  if (library_) {
    const char *name = group->firstName();
    if (name) {
      op_cond_ = new OperatingConditions(name);
      library_->addOperatingConditions(op_cond_);
    }
    else
      libWarn(1183, group, "operating_conditions missing name.");
  }
}

void
LibertyReader::visitProc(LibertyAttr *attr)
{
  if (op_cond_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      op_cond_->setProcess(value);
  }
}

void
LibertyReader::visitVolt(LibertyAttr *attr)
{
  if (op_cond_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      op_cond_->setVoltage(value * volt_scale_);
  }
}

void
LibertyReader::visitTemp(LibertyAttr *attr)
{
  if (op_cond_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      op_cond_->setTemperature(value);
  }
}

void
LibertyReader::visitTreeType(LibertyAttr *attr)
{
  if (op_cond_) {
    const char *tree_type = getAttrString(attr);
    if (tree_type) {
      WireloadTree wire_load_tree = stringWireloadTree(tree_type);
      op_cond_->setWireloadTree(wire_load_tree);
    }
  }
}

void
LibertyReader::endOpCond(LibertyGroup *)
{
  op_cond_ = nullptr;
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginWireload(LibertyGroup *group)
{
  if (library_) {
    const char *name = group->firstName();
    if (name) {
      wireload_ = new Wireload(name, library_);
      library_->addWireload(wireload_);
    }
  }
  else
    libWarn(1184, group, "wire_load missing name.");
}

void
LibertyReader::endWireload(LibertyGroup *)
{
  wireload_ = nullptr;
}

void
LibertyReader::visitResistance(LibertyAttr *attr)
{
  if (wireload_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      wireload_->setResistance(value * res_scale_);
  }
}

void
LibertyReader::visitSlope(LibertyAttr *attr)
{
  if (wireload_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      wireload_->setSlope(value);
  }
}

void
LibertyReader::visitFanoutLength(LibertyAttr *attr)
{
  if (wireload_) {
    float fanout, length;
    bool exists;
    getAttrFloat2(attr, fanout, length, exists);
    if (exists)
      wireload_->addFanoutLength(fanout, length);
    else
      libWarn(1185, attr, "fanout_length is missing length and fanout.");
  }
}

void
LibertyReader::beginWireloadSelection(LibertyGroup *group)
{
  if (library_) {
    const char *name = group->firstName();
    if (name) {
      wireload_selection_ = new WireloadSelection(name);
      library_->addWireloadSelection(wireload_selection_);
    }
  }
  else
    libWarn(1186, group, "wire_load_selection missing name.");
}

void
LibertyReader::endWireloadSelection(LibertyGroup *)
{
  wireload_selection_ = nullptr;
}

void
LibertyReader::visitWireloadFromArea(LibertyAttr *attr)
{
  if (wireload_selection_) {
    if (attr->isComplex()) {
      LibertyAttrValueIterator value_iter(attr->values());
      if (value_iter.hasNext()) {
	LibertyAttrValue *value = value_iter.next();
	if (value->isFloat()) {
	  float min_area = value->floatValue();
	  value = value_iter.next();
	  if (value->isFloat()) {
	    float max_area = value->floatValue();
	    value = value_iter.next();
	    if (value->isString()) {
	      const char *wireload_name = value->stringValue();
	      const Wireload *wireload =
		library_->findWireload(wireload_name);
	      if (wireload)
		wireload_selection_->addWireloadFromArea(min_area, max_area,
							 wireload);
	      else
		libWarn(1187, attr, "wireload %s not found.", wireload_name);
	    }
	    else
	      libWarn(1188, attr,
		      "wire_load_from_area wireload name not a string.");
	  }
	  else
	    libWarn(1189, attr, "wire_load_from_area min not a float.");
	}
	else
	  libWarn(1190, attr, "wire_load_from_area max not a float.");
      }
      else
	libWarn(1191, attr, "wire_load_from_area missing parameters.");
    }
    else
      libWarn(1192, attr, "wire_load_from_area missing parameters.");
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginCell(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name) {
    debugPrint(debug_, "liberty", 1, "cell %s", name);
    if (library_) {
      cell_ = builder_.makeCell(library_, name, filename_);
      in_bus_ = false;
      in_bundle_ = false;
    }
  }
  else
    libWarn(1193, group, "cell missing name.");
}

void
LibertyReader::endCell(LibertyGroup *group)
{
  if (cell_) {
    // Sequentials and leakage powers reference expressions outside of port definitions
    // so they do not require LibertyFunc's.
    makeCellSequentials();
    makeStatetable();
    // Parse functions defined inside of port groups that reference other ports
    // and replace the references with the parsed expressions.
    parseCellFuncs();
    makeLeakagePowers();
    finishPortGroups();

    if (ocv_derate_name_) {
      OcvDerate *derate = cell_->findOcvDerate(ocv_derate_name_);
      if (derate == nullptr)
	derate = library_->findOcvDerate(ocv_derate_name_);
      if (derate)
	cell_->setOcvDerate(derate);
      else
	libWarn(1194, group, "cell %s ocv_derate_group %s not found.",
		cell_->name(), ocv_derate_name_);
      stringDelete(ocv_derate_name_);
      ocv_derate_name_ = nullptr;
    }
    cell_->finish(infer_latches_, report_, debug_);
    cell_ = nullptr;
  }
}

void
LibertyReader::finishPortGroups()
{
  for (PortGroup *port_group : cell_port_groups_) {
    int line = port_group->line();
    for (LibertyPort *port : *port_group->ports()) {
      checkPort(port, line);
      makeMinPulseWidthArcs(port, line);
    }
    makeTimingArcs(port_group);
    makeInternalPowers(port_group);
    delete port_group;
  }
  cell_port_groups_.clear();
}

void
LibertyReader::checkPort(LibertyPort *port,
			 int line)
{
  FuncExpr *func_expr = port->function();
  if (func_expr) {
    if (func_expr->checkSize(port)) {
      libWarn(1195, line, "port %s function size does not match port size.",
	      port->name());
    }
  }
  if (port->tristateEnable()
      && port->direction() == PortDirection::output())
    port->setDirection(PortDirection::tristate());
}

// Make timing arcs for the port min_pulse_width_low/high attributes.
// This is redundant but makes sdf annotation consistent.
void
LibertyReader::makeMinPulseWidthArcs(LibertyPort *port,
                                     int line)
{
  TimingArcAttrsPtr attrs = nullptr;
  for (auto hi_low : RiseFall::range()) {
    float min_width;
    bool exists;
    port->minPulseWidth(hi_low, min_width, exists);
    if (exists) {
      if (attrs == nullptr) {
        attrs = make_shared<TimingArcAttrs>();
        attrs->setTimingType(TimingType::min_pulse_width);
      }
      // rise/fall_constraint model is on the trailing edge of the pulse.
      const RiseFall *model_rf = hi_low->opposite();
      TimingModel *check_model =
        makeScalarCheckModel(min_width, ScaleFactorType::min_pulse_width, model_rf);
      attrs->setModel(model_rf, check_model);
    }
  }
  if (attrs)
    builder_.makeTimingArcs(cell_, port, port, nullptr, attrs, line);
}

TimingModel *
LibertyReader::makeScalarCheckModel(float value,
                                    ScaleFactorType scale_factor_type,
                                    const RiseFall *rf)
{
  TablePtr table = make_shared<Table0>(value);
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *table_model = new TableModel(table, tbl_template,
                                           scale_factor_type, rf);
  CheckTableModel *check_model = new CheckTableModel(cell_, table_model, nullptr);
  return check_model;
}

void
LibertyReader::makeTimingArcs(PortGroup *port_group)
{
  for (TimingGroup *timing : port_group->timingGroups()) {
    timing->makeTimingModels(cell_, this);

    for (LibertyPort *port : *port_group->ports())
      makeTimingArcs(port, timing);
  }
}

void
LibertyReader::makeInternalPowers(PortGroup *port_group)
{
  for (InternalPowerGroup *power_group : port_group->internalPowerGroups()) {
    for (LibertyPort *port : *port_group->ports())
      makeInternalPowers(port, power_group);
    cell_->addInternalPowerAttrs(power_group);
  }
}

void
LibertyReader::makeCellSequentials()
{
  for (SequentialGroup *seq : cell_sequentials_) {
    makeCellSequential(seq);
    delete seq;
  }
  cell_sequentials_.clear();
}

void
LibertyReader::makeCellSequential(SequentialGroup *seq)
{
  int line = seq->line();
  int size = seq->size();
  bool is_register = seq->isRegister();
  bool is_bank = seq->isBank();
  const char *type = is_register
    ? (is_bank ? "ff_bank" : "ff")
    : (is_bank ? "latch_bank" : "latch");
  const char *clk = seq->clock();
  FuncExpr *clk_expr = nullptr;
  if (clk) {
    const char *clk_attr = is_register ? "clocked_on" : "enable";
    clk_expr = parseFunc(clk, clk_attr, line);
    if (clk_expr && clk_expr->checkSize(size)) {
      libWarn(1196, line, "%s %s bus width mismatch.", type, clk_attr);
      clk_expr->deleteSubexprs();
      clk_expr = nullptr;
    }
  }
  const char *data = seq->data();
  FuncExpr *data_expr = nullptr;
  if (data) {
    const char *data_attr = is_register ? "next_state" : "data_in";
    data_expr = parseFunc(data, data_attr, line);
    if (data_expr && data_expr->checkSize(size)) {
      libWarn(1197, line, "%s %s bus width mismatch.", type, data_attr);
      data_expr->deleteSubexprs();
      data_expr = nullptr;
    }
  }
  const char *clr = seq->clear();
  FuncExpr *clr_expr = nullptr;
  if (clr) {
    clr_expr = parseFunc(clr, "clear", line);
    if (clr_expr && clr_expr->checkSize(size)) {
      libWarn(1198, line, "%s %s bus width mismatch.", type, "clear");
      clr_expr->deleteSubexprs();
      clr_expr = nullptr;
    }
  }
  const char *preset = seq->preset();
  FuncExpr *preset_expr = nullptr;
  if (preset) {
    preset_expr = parseFunc(preset, "preset", line);
    if (preset_expr && preset_expr->checkSize(size)) {
      libWarn(1199, line, "%s %s bus width mismatch.", type, "preset");
      preset_expr->deleteSubexprs();
      preset_expr = nullptr;
    }
  }
  cell_->makeSequential(size, is_register, clk_expr, data_expr, clr_expr,
			preset_expr, seq->clrPresetVar1(),
			seq->clrPresetVar2(),
			seq->outPort(), seq->outInvPort());
  if (!is_register)
    checkLatchEnableSense(clk_expr, line);

  // The expressions used in the sequentials are copied by bitSubExpr.
  if (clk_expr)
    clk_expr->deleteSubexprs();
  if (data_expr)
    data_expr->deleteSubexprs();
  if (clr_expr)
    clr_expr->deleteSubexprs();
  if (preset_expr)
    preset_expr->deleteSubexprs();
}

void
LibertyReader::checkLatchEnableSense(FuncExpr *enable_func,
				     int line)
{
  FuncExprPortIterator enable_iter(enable_func);
  while (enable_iter.hasNext()) {
    LibertyPort *enable_port = enable_iter.next();
    TimingSense enable_sense = enable_func->portTimingSense(enable_port);
    switch (enable_sense) {
    case TimingSense::positive_unate:
    case TimingSense::negative_unate:
      break;
    case TimingSense::non_unate:
      libWarn(1200, line, "latch enable function is non-unate for port %s.",
	      enable_port->name());
      break;
    case TimingSense::none:
    case TimingSense::unknown:
      libWarn(1201, line, "latch enable function is unknown for port %s.",
	      enable_port->name());
      break;
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::makeStatetable()
{
  if (statetable_) {
    LibertyPortSeq input_ports;
    for (const string &input : statetable_->inputPorts()) {
      LibertyPort *port = cell_->findLibertyPort(input.c_str());
      if (port)
        input_ports.push_back(port);
      else
	libWarn(1298, statetable_->line(), "statetable input port %s not found.",
                input.c_str());
    }
    LibertyPortSeq internal_ports;
    for (const string &internal : statetable_->internalPorts()) {
      LibertyPort *port = cell_->findLibertyPort(internal.c_str());
      if (port == nullptr)
	port = builder_.makePort(cell_, internal.c_str());
      internal_ports.push_back(port);
    }
    cell_->makeStatetable(input_ports, internal_ports, statetable_->table());
    delete statetable_;
    statetable_ = nullptr;
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::makeLeakagePowers()
{
  for (LeakagePowerGroup *power_group : leakage_powers_) {
    builder_.makeLeakagePower(cell_, power_group);
    delete power_group;
  }
  leakage_powers_.clear();
}

// Record a reference to a function that will be parsed at the end of
// the cell definition when all of the ports are defined.
void
LibertyReader::makeLibertyFunc(const char *expr,
			       FuncExpr *&func_ref,
			       bool invert,
			       const char *attr_name,
			       LibertyStmt *stmt)
{
  LibertyFunc *func = new LibertyFunc(expr, func_ref, invert, attr_name,
				      stmt->line());
  cell_funcs_.push_back(func);
}

void
LibertyReader::parseCellFuncs()
{
  for (LibertyFunc *func : cell_funcs_) {
    FuncExpr *expr = parseFunc(func->expr(), func->attrName(), func->line());
    if (func->invert() && expr) {
      if (expr->op() == FuncExpr::op_not) {
	FuncExpr *inv = expr;
	expr = expr->left();
	delete inv;
      }
      else
	expr = FuncExpr::makeNot(expr);
    }
    if (expr) {
      FuncExpr *prev_func = func->funcRef(); 
      if (prev_func)
        prev_func->deleteSubexprs();
      func->funcRef() = expr;
    }
    delete func;
  }
  cell_funcs_.clear();
}

void
LibertyReader::beginScaledCell(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name) {
    scaled_cell_owner_ = library_->findLibertyCell(name);
    if (scaled_cell_owner_) {
      const char *op_cond_name = group->secondName();
      if (op_cond_name) {
	op_cond_ = library_->findOperatingConditions(op_cond_name);
	if (op_cond_) {
	  debugPrint(debug_, "liberty", 1, "scaled cell %s %s",
                     name, op_cond_name);
	  cell_ = library_->makeScaledCell(name, filename_);
	}
	else
	  libWarn(1202, group, "operating conditions %s not found.", op_cond_name);
      }
      else
	libWarn(1203, group, "scaled_cell missing operating condition.");
    }
    else
      libWarn(1204, group, "scaled_cell cell %s has not been defined.", name);
  }
  else
    libWarn(1205, group, "scaled_cell missing name.");
}

void
LibertyReader::endScaledCell(LibertyGroup *group)
{
  if (cell_) {
    makeCellSequentials();
    parseCellFuncs();
    finishPortGroups();
    cell_->finish(infer_latches_, report_, debug_);
    checkScaledCell(group);
    // Add scaled cell AFTER ports and timing arcs are defined.
    scaled_cell_owner_->addScaledCell(op_cond_, cell_);
    cell_ = nullptr;
    scaled_cell_owner_ = nullptr;
    op_cond_ = nullptr;
  }
}

// Minimal check that is not very specific about where the discrepancies are.
void
LibertyReader::checkScaledCell(LibertyGroup *group)
{
  if (equivCellPorts(cell_, scaled_cell_owner_)) {
    if (!equivCellPortsAndFuncs(cell_, scaled_cell_owner_))
      libWarn(1206, group, "scaled_cell %s, %s port functions do not match cell port functions.",
	      cell_->name(),
	      op_cond_->name());
  }
  else
    libWarn(1207, group, "scaled_cell ports do not match cell ports.");
  if (!equivCellTimingArcSets(cell_, scaled_cell_owner_))
    libWarn(1208, group, "scaled_cell %s, %s timing does not match cell timing.",
	    cell_->name(),
	    op_cond_->name());
}

void
LibertyReader::makeTimingArcs(LibertyPort *to_port,
			      TimingGroup *timing)
{
  LibertyPort *related_out_port = nullptr;
  const char *related_out_port_name = timing->relatedOutputPortName();
  if (related_out_port_name)
    related_out_port = findPort(related_out_port_name);
  int line = timing->line();
  PortDirection *to_port_dir = to_port->direction();
  // Checks should be more comprehensive (timing checks on inputs, etc).
  TimingType type = timing->attrs()->timingType();
  if (type == TimingType::combinational &&
      to_port_dir->isInput())
    libWarn(1209, line, "combinational timing to an input port.");
  if (timing->relatedPortNames()) {
    for (const char *from_port_name : *timing->relatedPortNames()) {
      PortNameBitIterator from_port_iter(cell_, from_port_name, this, line);
      if (from_port_iter.hasNext()) {
        debugPrint(debug_, "liberty", 2, "  timing %s -> %s",
                   from_port_name, to_port->name());
        makeTimingArcs(from_port_name, from_port_iter, to_port,
                       related_out_port, timing);
      }
    }
  }
  else
    makeTimingArcs(to_port, related_out_port, timing);
}

void
TimingGroup::makeTimingModels(LibertyCell *cell,
			      LibertyReader *visitor)
{
  switch (cell->libertyLibrary()->delayModelType()) {
  case DelayModelType::cmos_linear:
    makeLinearModels(cell);
    break;
  case DelayModelType::table:
    makeTableModels(cell, visitor);
    break;
  case DelayModelType::cmos_pwl:
  case DelayModelType::cmos2:
  case DelayModelType::polynomial:
  case DelayModelType::dcm:
    break;
  }
}

void
TimingGroup::makeLinearModels(LibertyCell *cell)
{
  LibertyLibrary *library = cell->libertyLibrary();
  for (auto rf : RiseFall::range()) {
    int rf_index = rf->index();
    float intr = intrinsic_[rf_index];
    bool intr_exists = intrinsic_exists_[rf_index];
    if (!intr_exists)
      library->defaultIntrinsic(rf, intr, intr_exists);
    TimingModel *model = nullptr;
    if (timingTypeIsCheck(attrs_->timingType())) {
      if (intr_exists)
	model = new CheckLinearModel(cell, intr);
    }
    else {
      float res = resistance_[rf_index];
      bool res_exists = resistance_exists_[rf_index];
      if (!res_exists)
	library->defaultPinResistance(rf, PortDirection::output(),
				      res, res_exists);
      if (!res_exists)
	res = 0.0F;
      if (intr_exists)
	model = new GateLinearModel(cell, intr, res);
    }
    attrs_->setModel(rf, model);
  }
}

void
TimingGroup::makeTableModels(LibertyCell *cell,
                             LibertyReader *reader)
{
  for (auto rf : RiseFall::range()) {
    int rf_index = rf->index();
    TableModel *delay = cell_[rf_index];
    TableModel *transition = transition_[rf_index];
    TableModel *constraint = constraint_[rf_index];
    if (delay || transition) {
      attrs_->setModel(rf, new GateTableModel(cell, delay, delay_sigma_[rf_index],
                                              transition,
                                              slew_sigma_[rf_index],
                                              receiver_model_,
                                              output_waveforms_[rf_index]));
      TimingType timing_type = attrs_->timingType();
      if (timing_type == TimingType::clear
	  || timing_type == TimingType::combinational
	  || timing_type == TimingType::combinational_fall
	  || timing_type == TimingType::combinational_rise
	  || timing_type == TimingType::falling_edge
	  || timing_type == TimingType::preset
	  || timing_type == TimingType::rising_edge
	  || timing_type == TimingType::three_state_disable
	  || timing_type == TimingType::three_state_disable_rise
	  || timing_type == TimingType::three_state_disable_fall
	  || timing_type == TimingType::three_state_enable
	  || timing_type == TimingType::three_state_enable_fall
	  || timing_type == TimingType::three_state_enable_rise) {
	if (transition == nullptr)
	  reader->libWarn(1210, line_, "missing %s_transition.", rf->name());
	if (delay == nullptr)
	  reader->libWarn(1211, line_, "missing cell_%s.", rf->name());
      }
    }
    else if (constraint)
      attrs_->setModel(rf, new CheckTableModel(cell, constraint,
                                               constraint_sigma_[rf_index]));
  }
}

void
LibertyReader::makeTimingArcs(const char *from_port_name,
			      PortNameBitIterator &from_port_iter,
			      LibertyPort *to_port,
			      LibertyPort *related_out_port,
			      TimingGroup *timing)
{
  if (from_port_iter.size() == 1 && !to_port->hasMembers()) {
    // one -> one
    if (from_port_iter.hasNext()) {
      LibertyPort *from_port = from_port_iter.next();
      if (from_port->direction()->isOutput())
        libWarn(1212, timing->line(), "timing group from output port.");
      builder_.makeTimingArcs(cell_, from_port, to_port, related_out_port,
                              timing->attrs(), timing->line());
    }
  }
  else if (from_port_iter.size() > 1 && !to_port->hasMembers()) {
    // bus -> one
    while (from_port_iter.hasNext()) {
      LibertyPort *from_port = from_port_iter.next();
      if (from_port->direction()->isOutput())
        libWarn(1213, timing->line(), "timing group from output port.");
      builder_.makeTimingArcs(cell_, from_port, to_port, related_out_port,
                              timing->attrs(), timing->line());
    }
  }
  else if (from_port_iter.size() == 1 && to_port->hasMembers()) {
    // one -> bus
    if (from_port_iter.hasNext()) {
      LibertyPort *from_port = from_port_iter.next();
      if (from_port->direction()->isOutput())
        libWarn(1214, timing->line(), "timing group from output port.");
      LibertyPortMemberIterator bit_iter(to_port);
      while (bit_iter.hasNext()) {
	LibertyPort *to_port_bit = bit_iter.next();
	builder_.makeTimingArcs(cell_, from_port, to_port_bit, related_out_port,
                                timing->attrs(), timing->line());
      }
    }
  }
  else {
    // bus -> bus
    if (timing->isOneToOne()) {
      int from_size = from_port_iter.size();
      int to_size = to_port->size();
      LibertyPortMemberIterator to_port_iter(to_port);
      // warn about different sizes
      if (from_size != to_size)
	libWarn(1216, timing->line(),
		"timing port %s and related port %s are different sizes.",
		from_port_name,
		to_port->name());
      // align to/from iterators for one-to-one mapping
      while (from_size > to_size) {
	from_size--;
	from_port_iter.next();
      }
      while (to_size > from_size) {
	to_size--;
	to_port_iter.next();
      }
      // make timing arcs
      while (from_port_iter.hasNext() && to_port_iter.hasNext()) {
	LibertyPort *from_port_bit = from_port_iter.next();
	LibertyPort *to_port_bit = to_port_iter.next();
	if (from_port_bit->direction()->isOutput())
	  libWarn(1215, timing->line(), "timing group from output port.");
	builder_.makeTimingArcs(cell_, from_port_bit, to_port_bit,
				related_out_port, timing->attrs(),
				timing->line());
      }
    }
    else {
      while (from_port_iter.hasNext()) {
	LibertyPort *from_port_bit = from_port_iter.next();
        if (from_port_bit->direction()->isOutput())
          libWarn(1217, timing->line(), "timing group from output port.");
	LibertyPortMemberIterator to_iter(to_port);
	while (to_iter.hasNext()) {
	  LibertyPort *to_port_bit = to_iter.next();
	  builder_.makeTimingArcs(cell_, from_port_bit, to_port_bit,
                                  related_out_port, timing->attrs(),
                                  timing->line());
	}
      }
    }
  }
}

void
LibertyReader::makeTimingArcs(LibertyPort *to_port,
                              LibertyPort *related_out_port,
			      TimingGroup *timing)
{
  if (to_port->hasMembers()) {
    LibertyPortMemberIterator bit_iter(to_port);
    while (bit_iter.hasNext()) {
      LibertyPort *to_port_bit = bit_iter.next();
      builder_.makeTimingArcs(cell_, nullptr, to_port_bit,
                              related_out_port, timing->attrs(),
                              timing->line());
    }
  }
  else
    builder_.makeTimingArcs(cell_, nullptr, to_port,
                            related_out_port, timing->attrs(),
                            timing->line());
}

////////////////////////////////////////////////////////////////

// Group that encloses receiver_capacitance1/2 etc groups.
void
LibertyReader::beginReceiverCapacitance(LibertyGroup *)
{
  receiver_model_ = make_shared<ReceiverModel>();
}

void
LibertyReader::endReceiverCapacitance(LibertyGroup *)
{
  if (ports_) {
    for (LibertyPort *port : *ports_)
      port->setReceiverModel(receiver_model_);
  }
  receiver_model_ = nullptr;
}

// For receiver_capacitance groups with mulitiple segments this
// overrides the index passed in beginReceiverCapacitance1Rise/Fall.
void
LibertyReader::visitSegement(LibertyAttr *attr)
{
  if (receiver_model_) {
    int segment;
    bool exists;
    getAttrInt(attr, segment, exists);
    if (exists)
      index_ = segment;
  }
}

void
LibertyReader::beginReceiverCapacitance1Rise(LibertyGroup *group)
{
  beginReceiverCapacitance(group, 0, RiseFall::rise());
}

void
LibertyReader::beginReceiverCapacitance1Fall(LibertyGroup *group)
{
  beginReceiverCapacitance(group, 0, RiseFall::fall());
}

void
LibertyReader::beginReceiverCapacitance2Rise(LibertyGroup *group)
{
  beginReceiverCapacitance(group, 1, RiseFall::rise());
}

void
LibertyReader::beginReceiverCapacitance2Fall(LibertyGroup *group)
{
  beginReceiverCapacitance(group, 1, RiseFall::fall());
}

void
LibertyReader::beginReceiverCapacitance(LibertyGroup *group,
                                        int index,
                                        RiseFall *rf)
{
  if (timing_ || ports_) {
    beginTableModel(group, TableTemplateType::delay, rf, 1.0,
                    ScaleFactorType::pin_cap);
    index_ = index;
  }
  else
    libWarn(1218, group, "receiver_capacitance group not in timing or pin group.");
}

void
LibertyReader::endReceiverCapacitanceRiseFall(LibertyGroup *group)
{
  if (table_) {
    if (ReceiverModel::checkAxes(table_)) {
      TableModel *table_model = new TableModel(table_, tbl_template_,
                                               scale_factor_type_, rf_);
      if (receiver_model_ == nullptr) {
        receiver_model_ = make_shared<ReceiverModel>();
        if (timing_)
          timing_->setReceiverModel(receiver_model_);
      }
      receiver_model_->setCapacitanceModel(table_model, index_, rf_);
    }
    else
      libWarn(1219, group, "unsupported model axis.");
    endTableModel();
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginOutputCurrentRise(LibertyGroup *group)
{
  beginOutputCurrent(RiseFall::rise(), group);
}

void
LibertyReader::beginOutputCurrentFall(LibertyGroup *group)
{
  beginOutputCurrent(RiseFall::fall(), group);
}

void
LibertyReader::beginOutputCurrent(RiseFall *rf,
                                  LibertyGroup *group)
{
  if (timing_) {
    rf_ = rf;
    output_currents_.clear();
  }
  else
    libWarn(1220, group, "output_current_%s group not in timing group.",
            rf->name());
}

void
LibertyReader::endOutputCurrentRiseFall(LibertyGroup *group)
{
  if (timing_) {
    Set<float> slew_set, cap_set;
    FloatSeq *slew_values = new FloatSeq;
    FloatSeq *cap_values = new FloatSeq;
    for (OutputWaveform *waveform : output_currents_) {
      float slew = waveform->slew();
      if (!slew_set.hasKey(slew)) {
        slew_set.insert(slew);
        slew_values->push_back(slew);
      }
      float cap = waveform->cap();
      if (!cap_set.hasKey(cap)) {
        cap_set.insert(cap);
        cap_values->push_back(cap);
      }
    }
    sort(slew_values, std::less<float>());
    sort(cap_values, std::less<float>());
    TableAxisPtr slew_axis = make_shared<TableAxis>(TableAxisVariable::input_net_transition,
                                                    slew_values);
    TableAxisPtr cap_axis = make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance,
                                                   cap_values);
    FloatSeq *ref_times = new FloatSeq(slew_values->size());
    Table1Seq current_waveforms(slew_axis->size() * cap_axis->size());
    for (OutputWaveform *waveform : output_currents_) {
      size_t slew_index, cap_index;
      bool slew_exists, cap_exists;
      slew_axis->findAxisIndex(waveform->slew(), slew_index, slew_exists);
      cap_axis->findAxisIndex(waveform->cap(), cap_index, cap_exists);
      if (slew_exists && cap_exists) {
        size_t index = slew_index * cap_axis->size() + cap_index;
        current_waveforms[index] = waveform->stealCurrents();
        (*ref_times)[slew_index] = waveform->referenceTime();
      }
      else
        libWarn(1221, group, "output current waveform %.2e %.2e not found.",
                waveform->slew(),
                waveform->cap());
    }
    Table1 *ref_time_tbl = new Table1(ref_times, slew_axis);
    OutputWaveforms *output_current = new OutputWaveforms(slew_axis, cap_axis, rf_,
                                                          current_waveforms,
                                                          ref_time_tbl);
    timing_->setOutputWaveforms(rf_, output_current);
    output_currents_.deleteContentsClear();
  }
}

void
LibertyReader::beginVector(LibertyGroup *group)
{
  if (timing_) {
    beginTable(group, TableTemplateType::output_current, current_scale_);
    scale_factor_type_ = ScaleFactorType::unknown;
    reference_time_exists_ = false;
    if (tbl_template_ && !OutputWaveforms::checkAxes(tbl_template_))
      libWarn(1222, group, "unsupported model axis.");
  }
}

void
LibertyReader::visitReferenceTime(LibertyAttr *attr)
{
  getAttrFloat(attr, reference_time_, reference_time_exists_);
  if (reference_time_exists_)
    reference_time_ *= time_scale_;
}

void
LibertyReader::endVector(LibertyGroup *group)
{
  if (timing_ && tbl_template_) {
    FloatSeq *slew_values = axis_values_[0];
    FloatSeq *cap_values = axis_values_[1];
    // Canonicalize axis order.
    if (tbl_template_->axis1()->variable() == TableAxisVariable::input_net_transition) {
      slew_values = axis_values_[0];
      cap_values = axis_values_[1];
    }
    else {
      slew_values = axis_values_[1];
      cap_values = axis_values_[0];
    }

    if (slew_values->size() == 1 && cap_values->size() == 1) {
      // Convert 1x1xN Table3 to Table1.
      float slew = (*slew_values)[0];
      float cap = (*cap_values)[0];
      Table3 *table3 = dynamic_cast<Table3*>(table_.get());
      FloatTable *values3 = table3->values3();
      // Steal the values.
      FloatSeq *values = (*values3)[0];
      (*values3)[0] = nullptr;
      Table1 *table1 = new Table1(values, axis_[2]);
      OutputWaveform *waveform = new OutputWaveform(slew, cap, table1, reference_time_);
      output_currents_.push_back(waveform);
    }
    else
      libWarn(1223,group->line(), "vector index_1 and index_2 must have exactly one value.");
    if (!reference_time_exists_)
      libWarn(1224, group->line(), "vector reference_time not found.");
    reference_time_exists_ = false;
  }
}

///////////////////////////////////////////////////////////////

void
LibertyReader::beginNormalizedDriverWaveform(LibertyGroup *group)
{
  beginTable(group, TableTemplateType::delay, time_scale_);
  driver_waveform_name_ = nullptr;
}

void
LibertyReader::visitDriverWaveformName(LibertyAttr *attr)
{
  driver_waveform_name_ = stringCopy(getAttrString(attr));
}

void
LibertyReader::endNormalizedDriverWaveform(LibertyGroup *group)
{
  if (table_) {
    if (table_->axis1()->variable() == TableAxisVariable::input_net_transition) {
      if (table_->axis2()->variable() == TableAxisVariable::normalized_voltage) {
        // Null driver_waveform_name_ means it is the default unnamed waveform.
        DriverWaveform *driver_waveform = new DriverWaveform(driver_waveform_name_,
                                                             table_);
        library_->addDriverWaveform(driver_waveform);

      }
      else
        libWarn(1225, group, "normalized_driver_waveform variable_2 must be normalized_voltage");
    }
    else
      libWarn(1226, group, "normalized_driver_waveform variable_1 must be input_net_transition");
  }
  endTableModel();
}

void
LibertyReader::visitDriverWaveformRise(LibertyAttr *attr)
{
  visitDriverWaveformRiseFall(attr, RiseFall::rise());
}

void
LibertyReader::visitDriverWaveformFall(LibertyAttr *attr)
{
  visitDriverWaveformRiseFall(attr, RiseFall::fall());
}

void
LibertyReader::visitDriverWaveformRiseFall(LibertyAttr *attr,
                                           const RiseFall *rf)
{
  if (ports_) {
    const char *driver_waveform_name = getAttrString(attr);
    DriverWaveform *driver_waveform = library_->findDriverWaveform(driver_waveform_name);
    if (driver_waveform) {
      for (LibertyPort *port : *ports_) 
        port->setDriverWaveform(driver_waveform, rf);
    }
  }
}

///////////////////////////////////////////////////////////////

void
LibertyReader::makeInternalPowers(LibertyPort *port,
				  InternalPowerGroup *power_group)
{
  int line = power_group->line();
  StringSeq *related_port_names = power_group->relatedPortNames();
  if (related_port_names) {
    for (const char *related_port_name : *related_port_names) {
      PortNameBitIterator related_port_iter(cell_, related_port_name, this, line);
      if (related_port_iter.hasNext()) {
	debugPrint(debug_, "liberty", 2, "  power %s -> %s",
                   related_port_name, port->name());
	makeInternalPowers(port, related_port_name, related_port_iter, power_group);
      }
    }
  }
  else {
    if (port->hasMembers()) {
      LibertyPortMemberIterator bit_iter(port);
      while (bit_iter.hasNext()) {
	LibertyPort *port_bit = bit_iter.next();
	builder_.makeInternalPower(cell_, port_bit, nullptr, power_group);
      }
    }
    else
      builder_.makeInternalPower(cell_, port, nullptr, power_group);
  }
}

void
LibertyReader::makeInternalPowers(LibertyPort *port,
				  const char *related_port_name,
				  PortNameBitIterator &related_port_iter,
				  InternalPowerGroup *power_group)
{
  if (related_port_iter.size() == 1 && !port->hasMembers()) {
    // one -> one
    if (related_port_iter.hasNext()) {
      LibertyPort *related_port = related_port_iter.next();
      builder_.makeInternalPower(cell_, port, related_port, power_group);
    }
  }
  else if (related_port_iter.size() > 1 && !port->hasMembers()) {
    // bus -> one
    while (related_port_iter.hasNext()) {
      LibertyPort *related_port = related_port_iter.next();
      builder_.makeInternalPower(cell_, port, related_port, power_group);
    }
  }
  else if (related_port_iter.size() == 1 && port->hasMembers()) {
    // one -> bus
    if (related_port_iter.hasNext()) {
      LibertyPort *related_port = related_port_iter.next();
      LibertyPortMemberIterator bit_iter(port);
      while (bit_iter.hasNext()) {
	LibertyPort *port_bit = bit_iter.next();
	builder_.makeInternalPower(cell_, port_bit, related_port, power_group);
      }
    }
  }
  else {
    // bus -> bus
    if (power_group->isOneToOne()) {
      if (static_cast<int>(related_port_iter.size()) == port->size()) {
	LibertyPortMemberIterator to_iter(port);
	while (related_port_iter.hasNext() && to_iter.hasNext()) {
	  LibertyPort *related_port_bit = related_port_iter.next();
	  LibertyPort *port_bit = to_iter.next();
	  builder_.makeInternalPower(cell_, port_bit, related_port_bit, power_group);
	}
      }
      else
	libWarn(1227, power_group->line(),
		"internal_power port %s and related port %s are different sizes.",
		related_port_name,
		port->name());
    }
    else {
      while (related_port_iter.hasNext()) {
	LibertyPort *related_port_bit = related_port_iter.next();
	LibertyPortMemberIterator to_iter(port);
	while (to_iter.hasNext()) {
	  LibertyPort *port_bit = to_iter.next();
	  builder_.makeInternalPower(cell_, port_bit, related_port_bit, power_group);
	}
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::visitArea(LibertyAttr *attr)
{
  if (cell_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      cell_->setArea(value);
  }
  if (wireload_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      wireload_->setArea(value);
  }
}

void
LibertyReader::visitDontUse(LibertyAttr *attr)
{
  if (cell_) {
    bool dont_use, exists;
    getAttrBool(attr, dont_use, exists);
    if (exists)
      cell_->setDontUse(dont_use);
  }
}

void
LibertyReader::visitIsMacro(LibertyAttr *attr)
{
  if (cell_) {
    bool is_macro, exists;
    getAttrBool(attr, is_macro, exists);
    if (exists)
      cell_->setIsMacro(is_macro);
  }
}

void
LibertyReader::visitIsMemory(LibertyAttr *attr)
{
  if (cell_) {
    bool is_memory, exists;
    getAttrBool(attr, is_memory, exists);
    if (exists)
      cell_->setIsMemory(is_memory);
  }
}

void
LibertyReader::visitIsPadCell(LibertyAttr *attr)
{
  if (cell_) {
    bool pad_cell, exists;
    getAttrBool(attr, pad_cell, exists);
    if (exists)
      cell_->setIsPad(pad_cell);
  }
}

void
LibertyReader::visitIsClockCell(LibertyAttr *attr)
{
  if (cell_) {
    bool is_clock_cell, exists;
    getAttrBool(attr, is_clock_cell, exists);
    if (exists)
      cell_->setIsClockCell(is_clock_cell);
  }
}

void
LibertyReader::visitIsLevelShifter(LibertyAttr *attr)
{
  if (cell_) {
    bool is_level_shifter, exists;
    getAttrBool(attr, is_level_shifter, exists);
    if (exists)
      cell_->setIsLevelShifter(is_level_shifter);
  }
}

void
LibertyReader::visitLevelShifterType(LibertyAttr *attr)
{
  if (cell_) {
    const char *level_shifter_type = getAttrString(attr);
    if (stringEq(level_shifter_type, "HL"))
      cell_->setLevelShifterType(LevelShifterType::HL);
    else if (stringEq(level_shifter_type, "LH"))
      cell_->setLevelShifterType(LevelShifterType::LH);
    else if (stringEq(level_shifter_type, "HL_LH"))
      cell_->setLevelShifterType(LevelShifterType::HL_LH);
    else
      libWarn(1228, attr, "level_shifter_type must be HL, LH, or HL_LH");
  }
}

void
LibertyReader::visitIsIsolationCell(LibertyAttr *attr)
{
  if (cell_) {
    bool is_isolation_cell, exists;
    getAttrBool(attr, is_isolation_cell, exists);
    if (exists)
      cell_->setIsIsolationCell(is_isolation_cell);
  }
}

void
LibertyReader::visitAlwaysOn(LibertyAttr *attr)
{
  if (cell_) {
    bool always_on, exists;
    getAttrBool(attr, always_on, exists);
    if (exists)
      cell_->setAlwaysOn(always_on);
  }
}

void
LibertyReader::visitSwitchCellType(LibertyAttr *attr)
{
  if (cell_) {
    const char *switch_cell_type = getAttrString(attr);
    if (stringEq(switch_cell_type, "coarse_grain"))
      cell_->setSwitchCellType(SwitchCellType::coarse_grain);
    else if (stringEq(switch_cell_type, "fine_grain"))
      cell_->setSwitchCellType(SwitchCellType::fine_grain);
    else
      libWarn(1229, attr, "switch_cell_type must be coarse_grain or fine_grain");
  }
}

void
LibertyReader::visitInterfaceTiming(LibertyAttr *attr)
{
  if (cell_) {
    bool value, exists;
    getAttrBool(attr, value, exists);
    if (exists)
      cell_->setInterfaceTiming(value);
  }
}

void
LibertyReader::visitScalingFactors(LibertyAttr *attr)
{
  if (cell_) {
    const char *scale_factors_name = getAttrString(attr);
    ScaleFactors *scales = library_->findScaleFactors(scale_factors_name);
    if (scales)
      cell_->setScaleFactors(scales);
    else
      libWarn(1230, attr, "scaling_factors %s not found.", scale_factors_name);
  }
}

void
LibertyReader::visitClockGatingIntegratedCell(LibertyAttr *attr)
{
  if (cell_) {
    const char *clock_gate_type = getAttrString(attr);
    if (clock_gate_type) {
      if (stringBeginEqual(clock_gate_type, "latch_posedge"))
	cell_->setClockGateType(ClockGateType::latch_posedge);
      else if (stringBeginEqual(clock_gate_type, "latch_negedge"))
	cell_->setClockGateType(ClockGateType::latch_negedge);
      else
	cell_->setClockGateType(ClockGateType::other);
    }
  }
}

void
LibertyReader::visitCellFootprint(LibertyAttr *attr)
{
  if (cell_) {
    const char *footprint = getAttrString(attr);
    if (footprint)
      cell_->setFootprint(footprint);
  }
}

void
LibertyReader::visitCellUserFunctionClass(LibertyAttr *attr)
{
  if (cell_) {
    const char *user_function_class = getAttrString(attr);
    if (user_function_class)
      cell_->setUserFunctionClass(user_function_class);
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginPin(LibertyGroup *group)
{
  if (cell_) {
    if (in_bus_) {
      saved_ports_ = ports_;
      saved_port_group_ = port_group_;
      ports_ = new LibertyPortSeq;
      for (LibertyAttrValue *param : *group->params()) {
	if (param->isString()) {
	  const char *port_name = param->stringValue();
	  debugPrint(debug_, "liberty", 1, " port %s", port_name);
	  PortNameBitIterator port_iter(cell_, port_name, this, group->line());
	  while (port_iter.hasNext()) {
	    LibertyPort *port = port_iter.next();
	    ports_->push_back(port);
	  }
	}
	else
	  libWarn(1231, group, "pin name is not a string.");
      }
    }
    else if (in_bundle_) {
      saved_ports_ = ports_;
      saved_port_group_ = port_group_;
      ports_ = new LibertyPortSeq;
      for (LibertyAttrValue *param : *group->params()) {
	if (param->isString()) {
	  const char *name = param->stringValue();
	  debugPrint(debug_, "liberty", 1, " port %s", name);
	  LibertyPort *port = findPort(name);
	  if (port == nullptr)
	    port = builder_.makePort(cell_, name);
	  ports_->push_back(port);
	}
	else
	  libWarn(1232, group, "pin name is not a string.");
      }
    }
    else {
      ports_ = new LibertyPortSeq;
      // Multiple port names can share group def.
      for (LibertyAttrValue *param : *group->params()) {
	if (param->isString()) {
	  const char *name = param->stringValue();
	  debugPrint(debug_, "liberty", 1, " port %s", name);
	  LibertyPort *port = builder_.makePort(cell_, name);
	  ports_->push_back(port);
	}
	else
	  libWarn(1233, group, "pin name is not a string.");
      }
    }
    port_group_ = new PortGroup(ports_, group->line());
    cell_port_groups_.push_back(port_group_);
  }
  if (test_cell_) {
    const char *pin_name = group->firstName();
    if (pin_name) {
      port_ = findPort(save_cell_, pin_name);
      test_port_ = findPort(test_cell_, pin_name);
    }
  }
}

void
LibertyReader::endPin(LibertyGroup *)
{
  if (cell_) {
    endPorts();
    if (in_bus_ || in_bundle_) {
      ports_ = saved_ports_;
      port_group_ = saved_port_group_;
    }
  }
  port_ = nullptr;
  test_port_ = nullptr;
}

void
LibertyReader::endPorts()
{
  // Capacitances default based on direction so wait until the end
  // of the pin group to set them.
  if (ports_) {
    for (LibertyPort *port : *ports_) {
      if (in_bus_ || in_bundle_) {
        // Do not clobber member port capacitances by setting the capacitance
        // on a bus or bundle.
        LibertyPortMemberIterator member_iter(port);
        while (member_iter.hasNext()) {
          LibertyPort *member = member_iter.next();
          setPortCapDefault(member);
        }
      }
      else
        setPortCapDefault(port);
    }
    ports_ = nullptr;
    port_group_ = nullptr;
  }
}

void
LibertyReader::setPortCapDefault(LibertyPort *port)
{
  for (auto min_max : MinMax::range()) {
    for (auto tr : RiseFall::range()) {
      float cap;
      bool exists;
      port->capacitance(tr, min_max, cap, exists);
      if (!exists)
	port->setCapacitance(tr, min_max, defaultCap(port));
    }
  }
}

void
LibertyReader::beginBus(LibertyGroup *group)
{
  if (cell_) {
    beginBusOrBundle(group);
    in_bus_ = true;
  }
}

void
LibertyReader::endBus(LibertyGroup *group)
{
  if (cell_) {
    if (ports_->empty())
      libWarn(1234, group, "bus %s bus_type not found.", group->firstName());
    endBusOrBundle();
    in_bus_ = false;
  }
}

void
LibertyReader::beginBusOrBundle(LibertyGroup *group)
{
  // Multiple port names can share group def.
  for (LibertyAttrValue *param : *group->params()) {
    if (param->isString()) {
      const char *name = param->stringValue();
      if (name)
	bus_names_.push_back(stringCopy(name));
    }
  }
  ports_ = new LibertyPortSeq;
  port_group_ = new PortGroup(ports_, group->line());
  cell_port_groups_.push_back(port_group_);
}

void
LibertyReader::endBusOrBundle()
{
  endPorts();
  deleteContents(&bus_names_);
  bus_names_.clear();
  ports_ = nullptr;
  port_group_ = nullptr;
}

// Bus port are not made until the bus_type is specified.
void
LibertyReader::visitBusType(LibertyAttr *attr)
{
  if (cell_) {
    const char *bus_type = getAttrString(attr);
    if (bus_type) {
      // Look for bus dcl local to cell first.
      BusDcl *bus_dcl = cell_->findBusDcl(bus_type);
      if (bus_dcl == nullptr)
	bus_dcl = library_->findBusDcl(bus_type);
      if (bus_dcl) {
        for (const char *name : bus_names_) {
	  debugPrint(debug_, "liberty", 1, " bus %s", name);
	  LibertyPort *port = builder_.makeBusPort(cell_, name, bus_dcl->from(),
                                                    bus_dcl->to(), bus_dcl);
	  ports_->push_back(port);
	}
      }
      else
	libWarn(1235, attr, "bus_type %s not found.", bus_type);
    }
    else
      libWarn(1236, attr, "bus_type is not a string.");
  }
}

void
LibertyReader::beginBundle(LibertyGroup *group)
{
  if (cell_) {
    beginBusOrBundle(group);
    in_bundle_ = true;
  }
}

void
LibertyReader::endBundle(LibertyGroup *group)
{
  if (cell_) {
    if (ports_ && ports_->empty())
      libWarn(1237, group, "bundle %s member not found.", group->firstName());
    endBusOrBundle();
    in_bundle_ = false;
  }
}

void
LibertyReader::visitMembers(LibertyAttr *attr)
{
  if (cell_) {
    if (attr->isComplex()) {
      for (const char *name : bus_names_) {
	debugPrint(debug_, "liberty", 1, " bundle %s", name);
	ConcretePortSeq *members = new ConcretePortSeq;
        for (LibertyAttrValue *value : *attr->values()) {
	  if (value->isString()) {
	    const char *port_name = value->stringValue();
	    LibertyPort *port = findPort(port_name);
	    if (port == nullptr)
	      port = builder_.makePort(cell_, port_name);
	    members->push_back(port);
	  }
	  else
	    libWarn(1238, attr, "member is not a string.");
	}
	LibertyPort *port = builder_.makeBundlePort(cell_, name, members);
	ports_->push_back(port);
      }
    }
    else
      libWarn(1239, attr,"members attribute is missing values.");
  }
}

LibertyPort *
LibertyReader::findPort(const char *port_name)
{
  return findPort(cell_, port_name);
}

// Also used by LibExprParser::makeFuncExprPort.
LibertyPort *
libertyReaderFindPort(LibertyCell *cell,
                      const char *port_name)
{
  LibertyPort *port = cell->findLibertyPort(port_name);
  if (port == nullptr) {
    const LibertyLibrary *library = cell->libertyLibrary();
    char brkt_left = library->busBrktLeft();
    char brkt_right = library->busBrktRight();
    const char escape = '\\';
    // Pins at top level with bus names have escaped brackets.
    string escaped_port_name = escapeChars(port_name, brkt_left, brkt_right, escape);
    port = cell->findLibertyPort(escaped_port_name.c_str());
  }
  return port;
}

LibertyPort *
LibertyReader::findPort(LibertyCell *cell,
			const char *port_name)
{
  return libertyReaderFindPort(cell, port_name);
}

void
LibertyReader::visitDirection(LibertyAttr *attr)
{
  if (ports_) {
    const char *dir = getAttrString(attr);
    if (dir) {
      PortDirection *port_dir = PortDirection::unknown();
      if (stringEq(dir, "input"))
	port_dir = PortDirection::input();
      else if (stringEq(dir, "output"))
	port_dir = PortDirection::output();
      else if (stringEq(dir, "inout"))
	port_dir = PortDirection::bidirect();
      else if (stringEq(dir, "internal"))
	port_dir = PortDirection::internal();
      else
	libWarn(1240, attr, "unknown port direction.");

      for (LibertyPort *port : *ports_)  {
	// Tristate enable function sets direction to tristate; don't
	// clobber it.
	if (!port->direction()->isTristate())
	  port->setDirection(port_dir);
      }
    }
  }
}

void
LibertyReader::visitFunction(LibertyAttr *attr)
{
  if (ports_) {
    const char *func = getAttrString(attr);
    if (func) {
      for (LibertyPort *port : *ports_)
        makeLibertyFunc(func, port->functionRef(), false, "function", attr);
    }
  }
}

void
LibertyReader::visitThreeState(LibertyAttr *attr)
{
  if (ports_) {
    const char *three_state = getAttrString(attr);
    if (three_state) {
      for (LibertyPort *port : *ports_)
	makeLibertyFunc(three_state, port->tristateEnableRef(), true,
			"three_state", attr);
    }
  }
}

void
LibertyReader::visitPorts(std::function<void (LibertyPort *port)> func)
{
  for (LibertyPort *port : *ports_) {
    func(port);
    LibertyPortMemberIterator member_iter(port);
    while (member_iter.hasNext()) {
      LibertyPort *member = member_iter.next();
      func(member);
    }
  }
}

void
LibertyReader::visitClock(LibertyAttr *attr)
{
  if (ports_) {
    bool is_clk, exists;
    getAttrBool(attr, is_clk, exists);
    if (exists) {
      for (LibertyPort *port : *ports_)
	port->setIsClock(is_clk);
    }
  }
}

void
LibertyReader::visitIsPad(LibertyAttr *attr)
{
  if (ports_) {
    bool is_pad, exists;
    getAttrBool(attr, is_pad, exists);
    if (exists) {
      for (LibertyPort *port : *ports_)
        port->setIsPad(is_pad);
    }
  }
}

void
LibertyReader::visitCapacitance(LibertyAttr *attr)
{
  if (ports_) {
    float cap;
    bool exists;
    getAttrFloat(attr, cap, exists);
    if (exists) {
      cap *= cap_scale_;
      for (LibertyPort *port : *ports_)
	port->setCapacitance(cap);
    }
  }
  if (wireload_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      wireload_->setCapacitance(value * cap_scale_);
  }
}

void
LibertyReader::visitRiseCap(LibertyAttr *attr)
{
  if (ports_) {
    float cap;
    bool exists;
    getAttrFloat(attr, cap, exists);
    if (exists) {
      cap *= cap_scale_;
      for (LibertyPort *port : *ports_) {
	port->setCapacitance(RiseFall::rise(), MinMax::min(), cap);
	port->setCapacitance(RiseFall::rise(), MinMax::max(), cap);
      }
    }
  }
}

void
LibertyReader::visitFallCap(LibertyAttr *attr)
{
  if (ports_) {
    float cap;
    bool exists;
    getAttrFloat(attr, cap, exists);
    if (exists) {
      cap *= cap_scale_;
      for (LibertyPort *port : *ports_) {
	port->setCapacitance(RiseFall::fall(), MinMax::min(), cap);
	port->setCapacitance(RiseFall::fall(), MinMax::max(), cap);
      }
    }
  }
}

void
LibertyReader::visitRiseCapRange(LibertyAttr *attr)
{
  if (ports_) {
    bool exists;
    float min, max;
    getAttrFloat2(attr, min, max, exists);
    if (exists) {
      min *= cap_scale_;
      max *= cap_scale_;
      for (LibertyPort *port : *ports_) {
	port->setCapacitance(RiseFall::rise(), MinMax::min(), min);
	port->setCapacitance(RiseFall::rise(), MinMax::max(), max);
      }
    }
  }
}

void
LibertyReader::visitFallCapRange(LibertyAttr *attr)
{
  if (ports_) {
    bool exists;
    float min, max;
    getAttrFloat2(attr, min, max, exists);
    if (exists) {
      min *= cap_scale_;
      max *= cap_scale_;
      for (LibertyPort *port : *ports_) {
	port->setCapacitance(RiseFall::fall(), MinMax::min(), min);
	port->setCapacitance(RiseFall::fall(), MinMax::max(), max);
      }
    }
  }
}

float
LibertyReader::defaultCap(LibertyPort *port)
{
  PortDirection *dir = port->direction();
  float cap = 0.0;
  if (dir->isInput())
    cap = library_->defaultInputPinCap();
  else if (dir->isOutput()
	   || dir->isTristate())
    cap = library_->defaultOutputPinCap();
  else if (dir->isBidirect())
    cap = library_->defaultBidirectPinCap();
  return cap;
}

void
LibertyReader::visitFanoutLoad(LibertyAttr *attr)
{
  if (ports_) {
    float fanout;
    bool exists;
    getAttrFloat(attr, fanout, exists);
    if (exists) {
      visitPorts([&] (LibertyPort *port) {
		   port->setFanoutLoad(fanout);
		 });
    }
  }
}

void
LibertyReader::visitMaxFanout(LibertyAttr *attr)
{
  visitFanout(attr, MinMax::max());
}

void
LibertyReader::visitMinFanout(LibertyAttr *attr)
{
  visitFanout(attr, MinMax::min());
}

void
LibertyReader::visitFanout(LibertyAttr *attr,
			   MinMax *min_max)
{
  if (ports_) {
    float fanout;
    bool exists;
    getAttrFloat(attr, fanout, exists);
    if (exists) {
      visitPorts([&] (LibertyPort *port) {
		   port->setFanoutLimit(fanout, min_max);
		 });
    }
  }
}

void
LibertyReader::visitMaxTransition(LibertyAttr *attr)
{
  visitMinMaxTransition(attr, MinMax::max());
}

void
LibertyReader::visitMinTransition(LibertyAttr *attr)
{
  visitMinMaxTransition(attr, MinMax::min());
}

void
LibertyReader::visitMinMaxTransition(LibertyAttr *attr, MinMax *min_max)
{
  if (cell_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists) {
      if (min_max == MinMax::max() && value == 0.0)
	libWarn(1241, attr, "max_transition is 0.0.");
      value *= time_scale_;
      visitPorts([&] (LibertyPort *port) {
		   port->setSlewLimit(value, min_max);
		 });
    }
  }
}

void
LibertyReader::visitMaxCapacitance(LibertyAttr *attr)
{
  visitMinMaxCapacitance(attr, MinMax::max());
}

void
LibertyReader::visitMinCapacitance(LibertyAttr *attr)
{
  visitMinMaxCapacitance(attr, MinMax::min());
}

void
LibertyReader::visitMinMaxCapacitance(LibertyAttr *attr,
				      MinMax *min_max)
{
  if (cell_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists) {
      value *= cap_scale_;
      LibertyPortSeq::Iterator port_iter(ports_);
      visitPorts([&] (LibertyPort *port) {
		   port->setCapacitanceLimit(value, min_max);
		 });
    }
  }
}

void
LibertyReader::visitMinPeriod(LibertyAttr *attr)
{
  if (cell_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists) {
      for (LibertyPort *port : *ports_)
	port->setMinPeriod(value * time_scale_);
    }
  }
}

void
LibertyReader::visitMinPulseWidthLow(LibertyAttr *attr)
{
  visitMinPulseWidth(attr, RiseFall::fall());
}

void
LibertyReader::visitMinPulseWidthHigh(LibertyAttr *attr)
{
  visitMinPulseWidth(attr, RiseFall::rise());
}

void
LibertyReader::visitMinPulseWidth(LibertyAttr *attr,
				  const RiseFall *rf)
{
  if (cell_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists) {
      value *= time_scale_;
      for (LibertyPort *port : *ports_)
	port->setMinPulseWidth(rf, value);
    }
  }
}

void
LibertyReader::visitPulseClock(LibertyAttr *attr)
{
  if (cell_) {
    const char *pulse_clk = getAttrString(attr);
    if (pulse_clk) {
      RiseFall *trigger = nullptr;
      RiseFall *sense = nullptr;
      if (stringEq(pulse_clk, "rise_triggered_high_pulse")) {
	trigger = RiseFall::rise();
	sense = RiseFall::rise();
      }
      else if (stringEq(pulse_clk, "rise_triggered_low_pulse")) {
	trigger = RiseFall::rise();
	sense = RiseFall::fall();
      }
      else if (stringEq(pulse_clk, "fall_triggered_high_pulse")) {
	trigger = RiseFall::fall();
	sense = RiseFall::rise();
      }
      else if (stringEq(pulse_clk, "fall_triggered_low_pulse")) {
	trigger = RiseFall::fall();
	sense = RiseFall::fall();
      }
      else
	libWarn(1242,attr, "pulse_latch unknown pulse type.");
      if (trigger) {
        for (LibertyPort *port : *ports_)
	  port->setPulseClk(trigger, sense);
      }
    }
  }
}

void
LibertyReader::visitClockGateClockPin(LibertyAttr *attr)
{
  visitPortBoolAttr(attr, &LibertyPort::setIsClockGateClock);
}

void
LibertyReader::visitClockGateEnablePin(LibertyAttr *attr)
{
  visitPortBoolAttr(attr, &LibertyPort::setIsClockGateEnable);
}

void
LibertyReader::visitClockGateOutPin(LibertyAttr *attr)
{
  visitPortBoolAttr(attr, &LibertyPort::setIsClockGateOut);
}

void
LibertyReader::visitIsPllFeedbackPin(LibertyAttr *attr)
{
  visitPortBoolAttr(attr, &LibertyPort::setIsPllFeedback);
}

void
LibertyReader::visitSignalType(LibertyAttr *attr)
{
  if (test_cell_ && port_) {
    const char *type = getAttrString(attr);
    if (type) {
      ScanSignalType signal_type = ScanSignalType::none;
      if (stringEq(type, "test_scan_enable"))
        signal_type = ScanSignalType::enable;
      else if (stringEq(type, "test_scan_enable_inverted"))
        signal_type = ScanSignalType::enable_inverted;
      else if (stringEq(type, "test_scan_clock"))
        signal_type = ScanSignalType::clock;
      else if (stringEq(type, "test_scan_clock_a"))
        signal_type = ScanSignalType::clock_a;
      else if (stringEq(type, "test_scan_clock_b"))
        signal_type = ScanSignalType::clock_b;
      else if (stringEq(type, "test_scan_in"))
        signal_type = ScanSignalType::input;
      else if (stringEq(type, "test_scan_in_inverted"))
        signal_type = ScanSignalType::input_inverted;
      else if (stringEq(type, "test_scan_out"))
        signal_type = ScanSignalType::output;
      else if (stringEq(type, "test_scan_out_inverted"))
        signal_type = ScanSignalType::output_inverted;
      else {
        libWarn(1299, attr, "unknown signal_type %s.", type);
        return;
      }
      if (port_)
        port_->setScanSignalType(signal_type);
      if (test_port_)
        test_port_->setScanSignalType(signal_type);
    }
  }
}

void
LibertyReader::visitIsolationCellDataPin(LibertyAttr *attr)
{
  visitPortBoolAttr(attr, &LibertyPort::setIsolationCellData);
}

void
LibertyReader::visitIsolationCellEnablePin(LibertyAttr *attr)
{
  visitPortBoolAttr(attr, &LibertyPort::setIsolationCellEnable);
}

void
LibertyReader::visitLevelShifterDataPin(LibertyAttr *attr)
{
  visitPortBoolAttr(attr, &LibertyPort::setLevelShifterData);
}

void
LibertyReader::visitSwitchPin(LibertyAttr *attr)
{
  visitPortBoolAttr(attr, &LibertyPort::setIsSwitch);
}

void
LibertyReader::visitPortBoolAttr(LibertyAttr *attr,
                                 LibertyPortBoolSetter setter)
{
  if (cell_) {
    bool value, exists;
    getAttrBool(attr, value, exists);
    if (exists) {
      for (LibertyPort *port : *ports_)
	(port->*setter)(value);
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginFF(LibertyGroup *group)
{
  beginSequential(group, true, false);
}

void
LibertyReader::endFF(LibertyGroup *)
{
  sequential_ = nullptr;
}

void
LibertyReader::beginFFBank(LibertyGroup *group)
{
  beginSequential(group, true, true);
}

void
LibertyReader::endFFBank(LibertyGroup *)
{
  sequential_ = nullptr;
}

void
LibertyReader::beginLatch(LibertyGroup *group)
{
  beginSequential(group, false, false);
}

void
LibertyReader::endLatch(LibertyGroup *)
{
  sequential_ = nullptr;
}

void
LibertyReader::beginLatchBank(LibertyGroup *group)
{
  beginSequential(group, false, true);
}

void
LibertyReader::endLatchBank(LibertyGroup *)
{
  sequential_ = nullptr;
}

void
LibertyReader::beginSequential(LibertyGroup *group,
			       bool is_register,
			       bool is_bank)
{
  if (cell_) {
    // Define ff/latch state variables as internal ports.
    const char *out_name, *out_inv_name;
    int size;
    bool has_size;
    seqPortNames(group, out_name, out_inv_name, has_size, size);
    LibertyPort *out_port = nullptr;
    LibertyPort *out_port_inv = nullptr;
    if (out_name) {
      if (has_size)
	out_port = builder_.makeBusPort(cell_, out_name, size - 1, 0, nullptr);
      else
	out_port = builder_.makePort(cell_, out_name);
      out_port->setDirection(PortDirection::internal());
    }
    if (out_inv_name) {
      if (has_size)
	out_port_inv = builder_.makeBusPort(cell_, out_inv_name, size - 1, 0, nullptr);
      else
	out_port_inv = builder_.makePort(cell_, out_inv_name);
      out_port_inv->setDirection(PortDirection::internal());
    }
    sequential_ = new SequentialGroup(is_register, is_bank,
				      out_port, out_port_inv, size,
				      group->line());
    cell_sequentials_.push_back(sequential_);
  }
}

void
LibertyReader::seqPortNames(LibertyGroup *group,
			    const char *&out_name,
			    const char *&out_inv_name,
			    bool &has_size,
			    int &size)
{
  int i = 0;
  out_name = nullptr;
  out_inv_name = nullptr;
  size = 1;
  has_size = false;
  for (LibertyAttrValue *value : *group->params()) {
    if (i == 0)
      out_name = value->stringValue();
    else if (i == 1)
      out_inv_name = value->stringValue();
    else if (i == 2) {
      size = static_cast<int>(value->floatValue());
      has_size = true;
    }
    i++;
  }
}

void
LibertyReader::visitClockedOn(LibertyAttr *attr)
{
  if (sequential_) {
    const char *func = getAttrString(attr);
    if (func)
      sequential_->setClock(stringCopy(func));
  }
}

void
LibertyReader::visitDataIn(LibertyAttr *attr)
{
  if (sequential_) {
    const char *func = getAttrString(attr);
    if (func)
      sequential_->setData(stringCopy(func));
  }
}

void
LibertyReader::visitClear(LibertyAttr *attr)
{
  if (sequential_) {
    const char *func = getAttrString(attr);
    if (func)
      sequential_->setClear(stringCopy(func));
  }
}

void
LibertyReader::visitPreset(LibertyAttr *attr)
{
  if (sequential_) {
    const char *func = getAttrString(attr);
    if (func)
      sequential_->setPreset(stringCopy(func));
  }
}

void
LibertyReader::visitClrPresetVar1(LibertyAttr *attr)
{
  if (sequential_) {
    LogicValue var = getAttrLogicValue(attr);
    sequential_->setClrPresetVar1(var);
  }
}

void
LibertyReader::visitClrPresetVar2(LibertyAttr *attr)
{
  if (sequential_) {
    LogicValue var = getAttrLogicValue(attr);
    sequential_->setClrPresetVar2(var);
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginStatetable(LibertyGroup *group)
{
  if (cell_) {
    const char *input_ports_arg = group->firstName();
    StdStringSeq input_ports;
    if (input_ports_arg)
      input_ports = parseTokenList(input_ports_arg, ' ');

    const char *internal_ports_arg = group->secondName();
    StdStringSeq internal_ports;
    if (internal_ports_arg)
      internal_ports = parseTokenList(internal_ports_arg, ' ');
    statetable_ = new StatetableGroup(input_ports, internal_ports, group->line());
  }
}

void
LibertyReader::visitTable(LibertyAttr *attr)
{
  if (statetable_) {
    const char *table_str = getAttrString(attr);
    StdStringSeq table_rows = parseTokenList(table_str, ',');
    size_t input_count = statetable_->inputPorts().size();
    size_t internal_count = statetable_->internalPorts().size();
    for (string row : table_rows) {
      StdStringSeq row_groups = parseTokenList(row.c_str(), ':');
      if (row_groups.size() != 3) {
        libWarn(1300, attr, "table row must have 3 groups separated by ':'.");
        break;
      }
      StdStringSeq inputs = parseTokenList(row_groups[0].c_str(), ' ');
      if (inputs.size() != input_count) {
        libWarn(1301, attr, "table row has %zu input values but %zu are required.",
                inputs.size(),
                input_count);
        break;
      }
      StdStringSeq currents = parseTokenList(row_groups[1].c_str(), ' ');
      if (currents.size() != internal_count) {
        libWarn(1302, attr, "table row has %zu current values but %zu are required.",
                currents.size(),
                internal_count);
        break;
      }
      StdStringSeq nexts = parseTokenList(row_groups[2].c_str(), ' ');
      if (nexts.size() != internal_count) {
        libWarn(1303, attr, "table row has %zu next values but %zu are required.",
                nexts.size(),
                internal_count);
        break;
      }

      StateInputValues input_values = parseStateInputValues(inputs, attr);
      StateInternalValues current_values=parseStateInternalValues(currents,attr);
      StateInternalValues next_values = parseStateInternalValues(nexts, attr);
      statetable_->addRow(input_values, current_values, next_values);
    }
  }
}

static EnumNameMap<StateInputValue> state_input_value_name_map =
  {{StateInputValue::low, "L"},
   {StateInputValue::high, "H"},
   {StateInputValue::dont_care, "-"},
   {StateInputValue::low_high, "L/H"},
   {StateInputValue::high_low, "H/L"},
   {StateInputValue::rise, "R"},
   {StateInputValue::fall, "F"},
   {StateInputValue::not_rise, "~R"},
   {StateInputValue::not_fall, "~F"}
  };

static EnumNameMap<StateInternalValue> state_internal_value_name_map =
  {{StateInternalValue::low, "L"},
   {StateInternalValue::high, "H"},
   {StateInternalValue::unspecified, "-"},
   {StateInternalValue::low_high, "L/H"},
   {StateInternalValue::high_low, "H/L"},
   {StateInternalValue::unknown, "X"},
   {StateInternalValue::hold, "N"}
  };

StateInputValues
LibertyReader::parseStateInputValues(StdStringSeq &inputs,
                                     LibertyAttr *attr)
{
  StateInputValues input_values;
  for (string input : inputs) {
    bool exists;
    StateInputValue value;
    state_input_value_name_map.find(input.c_str(), value, exists);
    if (!exists) {
      libWarn(1304, attr, "table input value '%s' not recognized.",
              input.c_str());
      value = StateInputValue::dont_care;
    }
    input_values.push_back(value);
  }
  return input_values;
}

StateInternalValues
LibertyReader::parseStateInternalValues(StdStringSeq &states,
                                        LibertyAttr *attr)
{
  StateInternalValues state_values;
  for (string state : states) {
    bool exists;
    StateInternalValue value;
    state_internal_value_name_map.find(state.c_str(), value, exists);
    if (!exists) {
      libWarn(1305, attr, "table internal value '%s' not recognized.",
              state.c_str());
      value = StateInternalValue::unknown;
    }
    state_values.push_back(value);
  }
  return state_values;
}

void
LibertyReader::endStatetable(LibertyGroup *)
{
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginTiming(LibertyGroup *group)
{
  if (port_group_) {
    timing_ = new TimingGroup(group->line());
    port_group_->addTimingGroup(timing_);
  }
}

void
LibertyReader::endTiming(LibertyGroup *group)
{
  if (timing_) {
    // Set scale factor type in constraint tables.
    for (auto rf : RiseFall::range()) {
      TableModel *model = timing_->constraint(rf);
      if (model) {
	ScaleFactorType type=timingTypeScaleFactorType(timing_->attrs()->timingType());
	model->setScaleFactorType(type);
      }
    }
    TimingType timing_type = timing_->attrs()->timingType();
    if (timing_->relatedPortNames() == nullptr
        && !(timing_type == TimingType::min_pulse_width
             || timing_type == TimingType::min_clock_tree_path
             || timing_type == TimingType::max_clock_tree_path))
      libWarn(1243, group, "timing group missing related_pin/related_bus_pin.");
  }
  timing_ = nullptr;
  receiver_model_ = nullptr;
}

void
LibertyReader::visitRelatedPin(LibertyAttr *attr)
{
  if (timing_)
    visitRelatedPin(attr, timing_);
  if (internal_power_)
    visitRelatedPin(attr, internal_power_);
}

void
LibertyReader::visitRelatedPin(LibertyAttr *attr,
			       RelatedPortGroup *group)
{
  const char *port_names = getAttrString(attr);
  if (port_names) {
    group->setRelatedPortNames(parseNameList(port_names));
    group->setIsOneToOne(true);
  }
}

StringSeq *
LibertyReader::parseNameList(const char *name_list)
{
  StringSeq *names = new StringSeq;
  // Parse space separated list of names.
  TokenParser parser(name_list, " ");
  while (parser.hasNext()) {
    char *token = parser.next();
    // Skip extra spaces.
    if (token[0] != '\0') {
      const char *name = token;
      names->push_back(stringCopy(name));
    }
  }
  return names;
}

StdStringSeq
LibertyReader::parseTokenList(const char *token_str,
                              const char separator)
{
  StdStringSeq tokens;
  // Parse space separated list of names.
  char separators[2] = {separator, '\0'};
  TokenParser parser(token_str, separators);
  while (parser.hasNext()) {
    char *token = parser.next();
    // Skip extra spaces.
    if (token[0] != '\0') {
      tokens.push_back(token);
    }
  }
  return tokens;
}

void
LibertyReader::visitRelatedBusPins(LibertyAttr *attr)
{
  if (timing_)
    visitRelatedBusPins(attr, timing_);
  if (internal_power_)
    visitRelatedBusPins(attr, internal_power_);
}

void
LibertyReader::visitRelatedBusPins(LibertyAttr *attr,
				   RelatedPortGroup *group)
{
  const char *port_names = getAttrString(attr);
  if (port_names) {
    group->setRelatedPortNames(parseNameList(port_names));
    group->setIsOneToOne(false);
  }
}

void
LibertyReader::visitRelatedOutputPin(LibertyAttr *attr)
{
  if (timing_) {
    const char *pin_name = getAttrString(attr);
    if (pin_name)
      timing_->setRelatedOutputPortName(pin_name);
  }
}

void
LibertyReader::visitTimingType(LibertyAttr *attr)
{
  if (timing_) {
    const char *type_name = getAttrString(attr);
    if (type_name) {
      TimingType type = findTimingType(type_name);
      if (type == TimingType::unknown)
	libWarn(1244, attr, "unknown timing_type %s.", type_name);
      else
	timing_->attrs()->setTimingType(type);
    }
  }
}

void
LibertyReader::visitTimingSense(LibertyAttr *attr)
{
  if (timing_) {
    const char *sense_name = getAttrString(attr);
    if (sense_name) {
      if (stringEq(sense_name, "non_unate"))
	timing_->attrs()->setTimingSense(TimingSense::non_unate);
      else if (stringEq(sense_name, "positive_unate"))
	timing_->attrs()->setTimingSense(TimingSense::positive_unate);
      else if (stringEq(sense_name, "negative_unate"))
	timing_->attrs()->setTimingSense(TimingSense::negative_unate);
      else
	libWarn(1245, attr, "unknown timing_sense %s.", sense_name);
    }
  }
}

void
LibertyReader::visitSdfCondStart(LibertyAttr *attr)
{
  if (timing_) {
    const char *cond = getAttrString(attr);
    if (cond)
      timing_->attrs()->setSdfCondStart(cond);
  }
}

void
LibertyReader::visitSdfCondEnd(LibertyAttr *attr)
{
  if (timing_) {
    const char *cond = getAttrString(attr);
    if (cond)
      timing_->attrs()->setSdfCondEnd(cond);
  }
}

void
LibertyReader::visitMode(LibertyAttr *attr)
{
  if (timing_) {
    if (attr->isComplex()) {
      LibertyAttrValueIterator value_iter(attr->values());
      if (value_iter.hasNext()) {
	LibertyAttrValue *value = value_iter.next();
	if (value->isString()) {
	  timing_->attrs()->setModeName(value->stringValue());
	  if (value_iter.hasNext()) {
	    value = value_iter.next();
	    if (value->isString())
	      timing_->attrs()->setModeValue(value->stringValue());
	    else
	      libWarn(1246, attr, "mode value is not a string.");
	  }
	  else
	    libWarn(1247, attr, "missing mode value.");
	}
	else
	  libWarn(1248, attr, "mode name is not a string.");
      }
      else
	libWarn(1249, attr, "mode missing values.");
    }
    else
      libWarn(1250, attr, "mode missing mode name and value.");
  }
}

void
LibertyReader::visitIntrinsicRise(LibertyAttr *attr)
{
  visitIntrinsic(attr, RiseFall::rise());
}

void
LibertyReader::visitIntrinsicFall(LibertyAttr *attr)
{
  visitIntrinsic(attr, RiseFall::fall());
}

void
LibertyReader::visitIntrinsic(LibertyAttr *attr,
			      RiseFall *rf)
{
  if (timing_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      timing_->setIntrinsic(rf, value * time_scale_);
  }
}

void
LibertyReader::visitRiseResistance(LibertyAttr *attr)
{
  visitRiseFallResistance(attr, RiseFall::rise());
}

void
LibertyReader::visitFallResistance(LibertyAttr *attr)
{
  visitRiseFallResistance(attr, RiseFall::fall());
}

void
LibertyReader::visitRiseFallResistance(LibertyAttr *attr,
				       RiseFall *rf)
{
  if (timing_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      timing_->setResistance(rf, value * res_scale_);
  }
}

void
LibertyReader::beginCellRise(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::rise(), ScaleFactorType::cell);
}

void
LibertyReader::beginCellFall(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::fall(), ScaleFactorType::cell);
}

void
LibertyReader::endCellRiseFall(LibertyGroup *group)
{
  if (table_) {
    if (GateTableModel::checkAxes(table_)) {
      TableModel *table_model = new TableModel(table_, tbl_template_,
                                               scale_factor_type_, rf_);
      timing_->setCell(rf_, table_model);
    }
    else
      libWarn(1251, group, "unsupported model axis.");
  }
  endTableModel();
}

void
LibertyReader::beginRiseTransition(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::rise(), ScaleFactorType::transition);
}

void
LibertyReader::beginFallTransition(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::fall(), ScaleFactorType::transition);
}

void
LibertyReader::endRiseFallTransition(LibertyGroup *group)
{
  if (table_) {
    if (GateTableModel::checkAxes(table_)) {
      TableModel *table_model = new TableModel(table_, tbl_template_,
                                               scale_factor_type_, rf_);
      timing_->setTransition(rf_, table_model);
    }
    else
      libWarn(1252, group, "unsupported model axis.");
  }
  endTableModel();
}

void
LibertyReader::beginRiseConstraint(LibertyGroup *group)
{
  // Scale factor depends on timing_type, which may follow this stmt.
  beginTimingTableModel(group, RiseFall::rise(), ScaleFactorType::unknown);
}

void
LibertyReader::beginFallConstraint(LibertyGroup *group)
{
  // Scale factor depends on timing_type, which may follow this stmt.
  beginTimingTableModel(group, RiseFall::fall(), ScaleFactorType::unknown);
}

void
LibertyReader::endRiseFallConstraint(LibertyGroup *group)
{
  if (table_) {
    if (CheckTableModel::checkAxes(table_)) {
      TableModel *table_model = new TableModel(table_, tbl_template_,
                                               scale_factor_type_, rf_);
      timing_->setConstraint(rf_, table_model);
    }
    else
      libWarn(1253, group, "unsupported model axis.");
  }
  endTableModel();
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginRiseTransitionDegredation(LibertyGroup *group)
{
  if (library_)
    beginTableModel(group, TableTemplateType::delay,
		    RiseFall::rise(), time_scale_,
		    ScaleFactorType::transition);
}

void
LibertyReader::beginFallTransitionDegredation(LibertyGroup *group)
{
  if (library_)
    beginTableModel(group, TableTemplateType::delay,
		    RiseFall::fall(), time_scale_,
		    ScaleFactorType::transition);
}

void
LibertyReader::endRiseFallTransitionDegredation(LibertyGroup *group)
{
  if (table_) {
    if (LibertyLibrary::checkSlewDegradationAxes(table_)) {
      TableModel *table_model = new TableModel(table_, tbl_template_,
                                               scale_factor_type_, rf_);
      library_->setWireSlewDegradationTable(table_model, rf_);
    }
    else
      libWarn(1254, group, "unsupported model axis.");
  }
  endTableModel();
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginTimingTableModel(LibertyGroup *group,
				     RiseFall *rf,
				     ScaleFactorType scale_factor_type)
{
  if (timing_)
    beginTableModel(group, TableTemplateType::delay, rf,
		    time_scale_, scale_factor_type);
  else
    libWarn(1255, group, "%s group not in timing group.", group->firstName());
}

void
LibertyReader::beginTableModel(LibertyGroup *group,
			       TableTemplateType type,
			       RiseFall *rf,
			       float scale,
			       ScaleFactorType scale_factor_type)
{
  beginTable(group, type, scale);
  rf_ = rf;
  scale_factor_type_ = scale_factor_type;
  sigma_type_ = EarlyLateAll::all();
}

void
LibertyReader::endTableModel()
{
  endTable();
  scale_factor_type_ = ScaleFactorType::unknown;
  sigma_type_ = nullptr;
  index_ = 0;
}

void
LibertyReader::beginTable(LibertyGroup *group,
			  TableTemplateType type,
			  float scale)
{
  const char *template_name = group->firstName();
  if (library_ && template_name) {
    tbl_template_ = library_->findTableTemplate(template_name, type);
    if (tbl_template_) {
      axis_[0] = tbl_template_->axis1ptr();
      axis_[1] = tbl_template_->axis2ptr();
      axis_[2] = tbl_template_->axis3ptr();
    }
    else {
      libWarn(1256, group, "table template %s not found.", template_name);
      axis_[0] = nullptr;
      axis_[1] = nullptr;
      axis_[2] = nullptr;
    }
    clearAxisValues();
    table_ = nullptr;
    table_model_scale_ = scale;
  }
}

void
LibertyReader::endTable()
{
  table_ = nullptr;
  tbl_template_ = nullptr;
  axis_[0] = nullptr;
  axis_[1] = nullptr;
  axis_[2] = nullptr;
}

void
LibertyReader::visitValue(LibertyAttr *attr)
{
  if (leakage_power_) {
    float value;
    bool valid;
    getAttrFloat(attr, value, valid);
    if (valid)
      leakage_power_->setPower(value * power_scale_);
  }
}

void
LibertyReader::visitValues(LibertyAttr *attr)
{
  if (tbl_template_
      // Ignore values in ecsm_waveform groups.
      && !stringEq(libertyGroup()->type(), "ecsm_waveform"))
    makeTable(attr, table_model_scale_);
}

void
LibertyReader::makeTable(LibertyAttr *attr,
                         float scale)
{
  if (attr->isComplex()) {
    makeTableAxis(0);
    makeTableAxis(1);
    makeTableAxis(2);
    if (axis_[0] && axis_[1] && axis_[2]) {
      // 3D table
      // Column index1*size(index2) + index2
      // Row    index3
      FloatTable *table = makeFloatTable(attr,
					 axis_[0]->size()*axis_[1]->size(),
					 axis_[2]->size(), scale);
      table_ = make_shared<Table3>(table, axis_[0], axis_[1], axis_[2]);
    }
    else if (axis_[0] && axis_[1]) {
      // 2D table
      // Row    variable1/axis[0]
      // Column variable2/axis[1]
      FloatTable *table = makeFloatTable(attr, axis_[0]->size(),
					 axis_[1]->size(), scale);
      table_ = make_shared<Table2>(table, axis_[0], axis_[1]);
    }
    else if (axis_[0]) {
      // 1D table
      FloatTable *table = makeFloatTable(attr, 1, axis_[0]->size(), scale);
      FloatSeq *values = (*table)[0];
      delete table;
      table_ = make_shared<Table1>(values, axis_[0]);
    }
    else {
      // scalar
      FloatTable *table = makeFloatTable(attr, 1, 1, scale);
      float value = (*(*table)[0])[0];
      delete (*table)[0];
      delete table;
      table_ = make_shared<Table0>(value);
    }
  }
  else
    libWarn(1257, attr, "%s is missing values.", attr->name());
}

FloatTable *
LibertyReader::makeFloatTable(LibertyAttr *attr,
			      size_t rows,
			      size_t cols,
			      float scale)
{
  FloatTable *table = new FloatTable;
  table->reserve(rows);
  for (LibertyAttrValue *value : *attr->values()) {
    FloatSeq *row = new FloatSeq;
    row->reserve(cols);
    table->push_back(row);
    if (value->isString()) {
      const char *values_list = value->stringValue();
      parseStringFloatList(values_list, scale, row, attr);
    }
    else if (value->isFloat())
      // Scalar value.
      row->push_back(value->floatValue() * scale);
    else
      libWarn(1258, attr, "%s is not a list of floats.", attr->name());
    if (row->size() != cols) {
      libWarn(1259, attr, "table row has %u columns but axis has %d.",
	      // size_t is long on 64 bit ports.
	      static_cast<unsigned>(row->size()),
	      static_cast<unsigned>(cols));
      // Fill out row columns with zeros.
      for (size_t c = row->size(); c < cols; c++)
	row->push_back(0.0);
    }
  }
  if (table->size() != rows) {
    libWarn(1260, attr, "table has %u rows but axis has %d.",
	    // size_t is long on 64 bit ports.
	    static_cast<unsigned>(table->size()),
	    static_cast<unsigned>(rows));
    // Fill with zero'd rows.
    for (size_t r = table->size(); r < rows; r++) {
      FloatSeq *row = new FloatSeq;
      table->push_back(row);
      // Fill out row with zeros.
      for (size_t c = row->size(); c < cols; c++)
	row->push_back(0.0);
    }
  }
  return table;
}

void
LibertyReader::makeTableAxis(int index)
{
  if (axis_values_[index]) {
    TableAxisVariable var = axis_[index]->variable();
    FloatSeq *values = axis_values_[index];
    const Units *units = library_->units();
    float scale = tableVariableUnit(var, units)->scale();
    scaleFloats(values, scale);
    axis_[index] = make_shared<TableAxis>(var, values);
  }
}

////////////////////////////////////////////////////////////////

// Define lut output variables as internal ports.
// I can't find any documentation for this group.
void
LibertyReader::beginLut(LibertyGroup *group)
{
  if (cell_) {
    for (LibertyAttrValue *param : *group->params()) {
      if (param->isString()) {
	const char *names = param->stringValue();
	// Parse space separated list of related port names.
	TokenParser parser(names, " ");
	while (parser.hasNext()) {
	  char *name = parser.next();
	  if (name[0] != '\0') {
	    LibertyPort *port = builder_.makePort(cell_, name);
	    port->setDirection(PortDirection::internal());
	  }
	}
      }
      else
	libWarn(1261, group, "lut output is not a string.");
    }
  }
}

void
LibertyReader::endLut(LibertyGroup *)
{
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginTestCell(LibertyGroup *group)
{
  if (cell_ && cell_->testCell())
    libWarn(1262, group, "cell %s test_cell redefinition.", cell_->name());
  else {
    string name = cell_->name();
    name += "/test_cell";
    test_cell_ = new TestCell(cell_->libertyLibrary(), name.c_str(),
                              cell_->filename());
    cell_->setTestCell(test_cell_);

    // Do a recursive parse of cell into the test_cell because it has
    // pins, buses, bundles, and sequentials just like a cell.
    save_cell_ = cell_;
    save_cell_port_groups_ = std::move(cell_port_groups_);
    save_statetable_ = statetable_;
    statetable_ = nullptr;
    save_cell_sequentials_ = std::move(cell_sequentials_);
    save_cell_funcs_ = std::move(cell_funcs_);
    cell_ = test_cell_;
  }
}

void
LibertyReader::endTestCell(LibertyGroup *)
{
  makeCellSequentials();
  makeStatetable();
  parseCellFuncs();
  finishPortGroups();

  // Restore reader state to enclosing cell.
  cell_port_groups_ = std::move(save_cell_port_groups_);
  statetable_ = save_statetable_;
  cell_sequentials_ = std::move(save_cell_sequentials_);
  cell_funcs_= std::move(save_cell_funcs_);
  cell_ = save_cell_;

  test_cell_ = nullptr;
  save_statetable_ = nullptr;
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginModeDef(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name)
    mode_def_ = cell_->makeModeDef(name);
  else
    libWarn(1263, group, "mode definition missing name.");
}

void
LibertyReader::endModeDef(LibertyGroup *)
{
  mode_def_ = nullptr;
}

void
LibertyReader::beginModeValue(LibertyGroup *group)
{
  if (mode_def_) {
    const char *name = group->firstName();
    if (name)
      mode_value_ = mode_def_->defineValue(name, nullptr, nullptr);
    else
      libWarn(1264, group, "mode value missing name.");
  }
}

void
 LibertyReader::endModeValue(LibertyGroup *)
{
  mode_value_ = nullptr;
}

void
LibertyReader::visitWhen(LibertyAttr *attr)
{
  if (tbl_template_)
    libWarn(1265, attr, "when attribute inside table model.");
  if (mode_value_) {
    const char *func = getAttrString(attr);
    if (func)
      makeLibertyFunc(func, mode_value_->condRef(), false, "when", attr);
  }
  if (timing_) {
    const char *func = getAttrString(attr);
    if (func)
      makeLibertyFunc(func, timing_->attrs()->condRef(), false, "when", attr);
  }
  if (internal_power_) {
    const char *func = getAttrString(attr);
    if (func)
      makeLibertyFunc(func, internal_power_->whenRef(), false, "when", attr);
  }
  if (leakage_power_) {
    const char *func = getAttrString(attr);
    if (func)
      makeLibertyFunc(func, leakage_power_->whenRef(), false, "when", attr);
  }
}

void
LibertyReader::visitSdfCond(LibertyAttr *attr)
{
  if (mode_value_) {
    const char *cond = getAttrString(attr);
    if (cond)
      mode_value_->setSdfCond(cond);
  }
  else if (timing_) {
    const char *cond = getAttrString(attr);
    if (cond)
      timing_->attrs()->setSdfCond(cond);
  }
  // sdf_cond can also appear inside minimum_period groups.
}

////////////////////////////////////////////////////////////////

const char *
LibertyReader::getAttrString(LibertyAttr *attr)
{
  if (attr->isSimple()) {
    LibertyAttrValue *value = attr->firstValue();
    if (value->isString())
      return value->stringValue();
    else
      libWarn(1266, attr, "%s attribute is not a string.", attr->name());
  }
  else
    libWarn(1267, attr, "%s is not a simple attribute.", attr->name());
  return nullptr;
}

void
LibertyReader::getAttrInt(LibertyAttr *attr,
			  // Return values.
			  int &value,
			  bool &exists)
{
  value = 0;
  exists = false;
  if (attr->isSimple()) {
    LibertyAttrValue *attr_value = attr->firstValue();
    if (attr_value->isFloat()) {
      float float_val = attr_value->floatValue();
      value = static_cast<int>(float_val);
      exists = true;
    }
    else
      libWarn(1268, attr, "%s attribute is not an integer.",attr->name());
  }
  else
    libWarn(1269, attr, "%s is not a simple attribute.", attr->name());
}

void
LibertyReader::getAttrFloat(LibertyAttr *attr,
			    // Return values.
			    float &value,
			    bool &valid)
{
  valid = false;
  if (attr->isSimple()) 
    getAttrFloat(attr, attr->firstValue(), value, valid);
  else
    libWarn(1270, attr, "%s is not a simple attribute.", attr->name());
}

void
LibertyReader::getAttrFloat(LibertyAttr *attr,
			    LibertyAttrValue *attr_value,
			    // Return values.
			    float &value,
			    bool &valid)
{
  if (attr_value->isFloat()) {
    valid = true;
    value = attr_value->floatValue();
  }
  else if (attr_value->isString()) {
    const char *string = attr_value->stringValue();
    // See if attribute string is a variable.
    variableValue(string, value, valid);
    if (!valid) {
      // For some reason area attributes for pads are quoted floats.
      // Check that the string is a valid double.
      char *end;
      value = strtof(string, &end);
      if ((*end && !isspace(*end))
          // strtof support INF as a valid float.
          || stringEqual(string, "inf"))
	libWarn(1271, attr, "%s value %s is not a float.",
		attr->name(),
		string);
      valid = true;
    }
  }
}

// Get two floats in a complex attribute.
//  attr(float1, float2);
void
LibertyReader::getAttrFloat2(LibertyAttr *attr,
			     // Return values.
			     float &value1,
			     float &value2,
			     bool &exists)
{
  exists = false;
  if (attr->isComplex()) {
    LibertyAttrValueIterator value_iter(attr->values());
    if (value_iter.hasNext()) {
      LibertyAttrValue *value = value_iter.next();
      getAttrFloat(attr, value, value1, exists);
      if (exists) {
	if (value_iter.hasNext()) {
	  value = value_iter.next();
	  getAttrFloat(attr, value, value2, exists);
	}
	else
	  libWarn(1272, attr, "%s missing values.", attr->name());
      }
    }
    else
      libWarn(1273, attr, "%s missing values.", attr->name());
  }
  else
    libWarn(1274, attr, "%s is not a complex attribute.", attr->name());
}

// Parse string of comma separated floats.
// Note that some brain damaged vendors (that used to "Think") are not
// consistent about including the delimiters.
void
LibertyReader::parseStringFloatList(const char *float_list,
				    float scale,
				    FloatSeq *values,
				    LibertyAttr *attr)
{
  const char *delimiters = ", ";
  TokenParser parser(float_list, delimiters);
  while (parser.hasNext()) {
    char *token = parser.next();
    // Some (brain dead) libraries enclose floats in brackets.
    if (*token == '{')
      token++;
    char *end;
    float value = strtof(token, &end) * scale;
    if (end == token
	|| (end && !(*end == '\0'
		     || isspace(*end)
		     || strchr(delimiters, *end) != nullptr
		     || *end == '}')))
      libWarn(1275, attr, "%s is not a float.", token);
    values->push_back(value);
  }
}

FloatSeq *
LibertyReader::readFloatSeq(LibertyAttr *attr,
			    float scale)
{
  FloatSeq *values = nullptr;
  if (attr->isComplex()) {
    LibertyAttrValueIterator value_iter(attr->values());
    if (value_iter.hasNext()) {
      LibertyAttrValue *value = value_iter.next();
      if (value->isString()) {
	values = new FloatSeq;
	parseStringFloatList(value->stringValue(), scale, values, attr);
      }
      else if (value->isFloat()) {
	values = new FloatSeq;
        values->push_back(value->floatValue());
      }
      else
	libWarn(1276, attr, "%s is missing values.", attr->name());
    }
    if (value_iter.hasNext())
      libWarn(1277, attr, "%s has more than one string.", attr->name());
  }
  else {
    LibertyAttrValue *value = attr->firstValue();
    if (value->isString()) {
      values = new FloatSeq;
      parseStringFloatList(value->stringValue(), scale, values, attr);
    }
    else
      libWarn(1278, attr, "%s is missing values.", attr->name());
  }
  return values;
}

void
LibertyReader::getAttrBool(LibertyAttr *attr,
			   // Return values.
			   bool &value,
			   bool &exists)
{
  exists = false;
  if (attr->isSimple()) {
    LibertyAttrValue *val = attr->firstValue();
    if (val->isString()) {
      const char *str = val->stringValue();
      if (stringEqual(str, "true")) {
	value = true;
	exists = true;
      }
      else if (stringEqual(str, "false")) {
	value = false;
	exists = true;
      }
      else
	libWarn(1279, attr, "%s attribute is not boolean.", attr->name());
    }
    else
      libWarn(1280, attr, "%s attribute is not boolean.", attr->name());
  }
  else
    libWarn(1281, attr, "%s is not a simple attribute.", attr->name());
}

// Read L/H/X string attribute values as bool.
LogicValue
LibertyReader::getAttrLogicValue(LibertyAttr *attr)
{
  const char *str = getAttrString(attr);
  if (str) {
    if (stringEq(str, "L"))
      return LogicValue::zero;
    else if (stringEq(str, "H"))
      return LogicValue::one;
    else if (stringEq(str, "X"))
      return LogicValue::unknown;
    else
      libWarn(1282, attr, "attribute %s value %s not recognized.",
	      attr->name(), str);
    // fall thru
  }
  return LogicValue::unknown;
}

FuncExpr *
LibertyReader::parseFunc(const char *func,
			 const char *attr_name,
			 int line)
{
  string error_msg;
  stringPrint(error_msg, "%s, line %d %s",
              filename_,		
              line,
              attr_name);
  return parseFuncExpr(func, cell_, error_msg.c_str(), report_);
}

EarlyLateAll *
LibertyReader::getAttrEarlyLate(LibertyAttr *attr)
{
  const char *value = getAttrString(attr);
  if (stringEq(value, "early"))
    return EarlyLateAll::early();
  else if (stringEq(value, "late"))
    return EarlyLateAll::late();
  else if (stringEq(value, "early_and_late"))
    return EarlyLateAll::all();
  else {
    libWarn(1283, attr, "unknown early/late value.");
    return EarlyLateAll::all();
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::visitVariable(LibertyVariable *var)
{
  if (var_map_ == nullptr)
    var_map_ = new LibertyVariableMap;
  const char *var_name = var->variable();
  const char *key;
  float value;
  bool exists;
  var_map_->findKey(var_name, key, value, exists);
  if (exists) {
    // Duplicate variable name.
    (*var_map_)[key] = var->value();
  }
  else
    (*var_map_)[stringCopy(var_name)] = var->value();
}

void
LibertyReader::variableValue(const char *var,
			     float &value,
			     bool &exists)
{
  if (var_map_)
    var_map_->findKey(var, value, exists);
  else
    exists = false;
}

////////////////////////////////////////////////////////////////

void
LibertyReader::libWarn(int id,
                       LibertyStmt *stmt,
		       const char *fmt,
		       ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(id, filename_, stmt->line(), fmt, args);
  va_end(args);
}

void
LibertyReader::libWarn(int id,
                       int line,
		       const char *fmt,
		       ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(id, filename_, line, fmt, args);
  va_end(args);
}

void
LibertyReader::libError(int id,
                        LibertyStmt *stmt,
			const char *fmt,
			...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileError(id, filename_, stmt->line(), fmt, args);
  va_end(args);
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginTableTemplatePower(LibertyGroup *group)
{
  beginTableTemplate(group, TableTemplateType::power);
}

void
LibertyReader::beginLeakagePower(LibertyGroup *group)
{
  if (cell_) {
    leakage_power_ = new LeakagePowerGroup(group->line());
    leakage_powers_.push_back(leakage_power_);
  }
}

void
LibertyReader::endLeakagePower(LibertyGroup *)
{
  leakage_power_ = nullptr;
}

void
LibertyReader::beginInternalPower(LibertyGroup *group)
{
  if (port_group_) {
    internal_power_ = makeInternalPowerGroup(group->line());
    port_group_->addInternalPowerGroup(internal_power_);
  }
}

InternalPowerGroup *
LibertyReader::makeInternalPowerGroup(int line)
{
  return new InternalPowerGroup(line);
}

void
LibertyReader::endInternalPower(LibertyGroup *)
{
  internal_power_ = nullptr;
}

void
LibertyReader::beginFallPower(LibertyGroup *group)
{
  if (internal_power_)
    beginTableModel(group, TableTemplateType::power,
		    RiseFall::fall(), energy_scale_,
		    ScaleFactorType::internal_power);
}

void
LibertyReader::beginRisePower(LibertyGroup *group)
{
  if (internal_power_)
    beginTableModel(group, TableTemplateType::power,
		    RiseFall::rise(), energy_scale_,
		    ScaleFactorType::internal_power);
}

void
LibertyReader::endRiseFallPower(LibertyGroup *)
{
  if (table_) {
    TableModel *table_model = new TableModel(table_, tbl_template_,
                                             scale_factor_type_, rf_);
    internal_power_->setModel(rf_, new InternalPowerModel(table_model));
  }
  endTableModel();
}

void
LibertyReader::endPower(LibertyGroup *)
{
  if (table_) {
    TableModel *table_model = new TableModel(table_, tbl_template_,
                                             scale_factor_type_, rf_);
    // Share the model for rise/fall.
    InternalPowerModel *power_model = new InternalPowerModel(table_model);
    internal_power_->setModel(RiseFall::rise(), power_model);
    internal_power_->setModel(RiseFall::fall(), power_model);
  }
  endTableModel();
}

void
LibertyReader::visitRelatedGroundPin(LibertyAttr *attr)
{
  if (ports_) {
    const char *related_ground_pin = getAttrString(attr);
    for (LibertyPort *port : *ports_)
      port->setRelatedGroundPin(related_ground_pin);
  }
}

void
LibertyReader::visitRelatedPowerPin(LibertyAttr *attr)
{
  if (ports_) {
    const char *related_power_pin = getAttrString(attr);
    for (LibertyPort *port : *ports_)
      port->setRelatedPowerPin(related_power_pin);
  }
}

void
LibertyReader::visitRelatedPgPin(LibertyAttr *attr)
{
  if (internal_power_)
    internal_power_->setRelatedPgPin(getAttrString(attr));
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginTableTemplateOcv(LibertyGroup *group)
{
  beginTableTemplate(group, TableTemplateType::ocv);
}

void
LibertyReader::visitOcvArcDepth(LibertyAttr *attr)
{
  float value;
  bool exists;
  getAttrFloat(attr, value, exists);
  if (exists) {
    if (timing_)
      timing_->attrs()->setOcvArcDepth(value);
    else if (cell_)
      cell_->setOcvArcDepth(value);
    else
      library_->setOcvArcDepth(value);
  }
}

void
LibertyReader::visitDefaultOcvDerateGroup(LibertyAttr *attr)
{
  const char *derate_name = getAttrString(attr);
  OcvDerate *derate = library_->findOcvDerate(derate_name);
  if (derate)
    library_->setDefaultOcvDerate(derate);
  else
    libWarn(1284, attr, "OCV derate group named %s not found.", derate_name);
}

void
LibertyReader::visitOcvDerateGroup(LibertyAttr *attr)
{
  ocv_derate_name_ = stringCopy(getAttrString(attr));
}

void
LibertyReader::beginOcvDerate(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name)
    ocv_derate_ = new OcvDerate(stringCopy(name));
  else
    libWarn(1285, group, "ocv_derate missing name.");
}

void
LibertyReader::endOcvDerate(LibertyGroup *)
{
  if (cell_)
    library_->addOcvDerate(ocv_derate_);
  else if (library_)
    library_->addOcvDerate(ocv_derate_);
  ocv_derate_ = nullptr;
}

void
LibertyReader::beginOcvDerateFactors(LibertyGroup *group)
{
  if (ocv_derate_) {
    rf_type_ = RiseFallBoth::riseFall();
    derate_type_ = EarlyLateAll::all();
    path_type_ = PathType::clk_and_data;
    beginTable(group, TableTemplateType::ocv, 1.0);
  }
}

void
LibertyReader::endOcvDerateFactors(LibertyGroup *)
{
  if (ocv_derate_) {
    for (auto early_late : derate_type_->range()) {
      for (auto rf : rf_type_->range()) {
	if (path_type_ == PathType::clk_and_data) {
	  ocv_derate_->setDerateTable(rf, early_late, PathType::clk, table_);
	  ocv_derate_->setDerateTable(rf, early_late, PathType::data, table_);
	}
	else
	  ocv_derate_->setDerateTable(rf, early_late, path_type_, table_);
      }
    }
  }
  endTable();
}

void
LibertyReader::visitRfType(LibertyAttr *attr)
{
  const char *rf_name = getAttrString(attr);
  if (stringEq(rf_name, "rise"))
    rf_type_ = RiseFallBoth::rise();
  else if (stringEq(rf_name, "fall"))
    rf_type_ = RiseFallBoth::fall();
  else if (stringEq(rf_name, "rise_and_fall"))
    rf_type_ = RiseFallBoth::riseFall();
  else
    libError(1286, attr, "unknown rise/fall.");
}

void
LibertyReader::visitDerateType(LibertyAttr *attr)
{
  derate_type_ = getAttrEarlyLate(attr);
}

void
LibertyReader::visitPathType(LibertyAttr *attr)
{
  const char *path_type = getAttrString(attr);
  if (stringEq(path_type, "clock"))
    path_type_ = PathType::clk;
  else if (stringEq(path_type, "data"))
    path_type_ = PathType::data;
  else if (stringEq(path_type, "clock_and_data"))
    path_type_ = PathType::clk_and_data;
  else
    libWarn(1287, attr, "unknown derate type.");
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginOcvSigmaCellRise(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::rise(), ScaleFactorType::unknown);
}

void
LibertyReader::beginOcvSigmaCellFall(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::fall(), ScaleFactorType::unknown);
}

void
LibertyReader::endOcvSigmaCell(LibertyGroup *group)
{
  if (table_) {
    if (GateTableModel::checkAxes(table_)) {
      TableModel *table_model = new TableModel(table_, tbl_template_,
                                               scale_factor_type_, rf_);
      if (sigma_type_ == EarlyLateAll::all()) {
	timing_->setDelaySigma(rf_, EarlyLate::min(), table_model);
	timing_->setDelaySigma(rf_, EarlyLate::max(), table_model);
      }
      else
	timing_->setDelaySigma(rf_, sigma_type_->asMinMax(), table_model);
    }
    else
      libWarn(1288, group, "unsupported model axis.");
  }
  endTableModel();
}

void
LibertyReader::beginOcvSigmaRiseTransition(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::rise(), ScaleFactorType::unknown);
}

void
LibertyReader::beginOcvSigmaFallTransition(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::fall(), ScaleFactorType::unknown);
}

void
LibertyReader::endOcvSigmaTransition(LibertyGroup *group)
{
  if (table_) {
    if (GateTableModel::checkAxes(table_)) {
      TableModel *table_model = new TableModel(table_, tbl_template_,
                                               scale_factor_type_, rf_);
      if (sigma_type_ == EarlyLateAll::all()) {
	timing_->setSlewSigma(rf_, EarlyLate::min(), table_model);
	timing_->setSlewSigma(rf_, EarlyLate::max(), table_model);
      }
      else
	timing_->setSlewSigma(rf_, sigma_type_->asMinMax(), table_model);
    }
    else
      libWarn(1289, group, "unsupported model axis.");
  }
  endTableModel();
}

void
LibertyReader::beginOcvSigmaRiseConstraint(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::rise(), ScaleFactorType::unknown);
}

void
LibertyReader::beginOcvSigmaFallConstraint(LibertyGroup *group)
{
  beginTimingTableModel(group, RiseFall::fall(), ScaleFactorType::unknown);
}

void
LibertyReader::endOcvSigmaConstraint(LibertyGroup *group)
{
  if (table_) {
    if (CheckTableModel::checkAxes(table_)) {
      TableModel *table_model = new TableModel(table_, tbl_template_,
                                               scale_factor_type_, rf_);
      if (sigma_type_ == EarlyLateAll::all()) {
	timing_->setConstraintSigma(rf_, EarlyLate::min(), table_model);
	timing_->setConstraintSigma(rf_, EarlyLate::max(), table_model);
      }
      else
	timing_->setConstraintSigma(rf_, sigma_type_->asMinMax(), table_model);
    }
    else
      libWarn(1290, group, "unsupported model axis.");
  }
  endTableModel();
}

void
LibertyReader::visitSigmaType(LibertyAttr *attr)
{
  sigma_type_ = getAttrEarlyLate(attr);
}

void
LibertyReader::visitCellLeakagePower(LibertyAttr *attr)
{
  if (cell_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      cell_->setLeakagePower(value * power_scale_);
  }
}

void
LibertyReader::beginPgPin(LibertyGroup *group)
{
  if (cell_) {
    const char *name = group->firstName();
    pg_port_ = new LibertyPgPort(name, cell_);
    cell_->addPgPort(pg_port_);
  }
}

void
LibertyReader::endPgPin(LibertyGroup *)
{
  pg_port_ = nullptr;
}

void
LibertyReader::visitPgType(LibertyAttr *attr)
{
  if (pg_port_) {
    const char *type_name = getAttrString(attr);
    LibertyPgPort::PgType type = LibertyPgPort::PgType::unknown;
    if (stringEqual(type_name, "primary_ground"))
      type = LibertyPgPort::PgType::primary_ground;
    else if (stringEqual(type_name, "primary_power"))
      type = LibertyPgPort::PgType::primary_power;

    else if (stringEqual(type_name, "backup_ground"))
      type = LibertyPgPort::PgType::backup_ground;
    else if (stringEqual(type_name, "backup_power"))
      type = LibertyPgPort::PgType::backup_power;

    else if (stringEqual(type_name, "internal_ground"))
      type = LibertyPgPort::PgType::internal_ground;
    else if (stringEqual(type_name, "internal_power"))
      type = LibertyPgPort::PgType::internal_power;

    else if (stringEqual(type_name, "nwell"))
      type = LibertyPgPort::PgType::nwell;
    else if (stringEqual(type_name, "pwell"))
      type = LibertyPgPort::PgType::pwell;

    else if (stringEqual(type_name, "deepnwell"))
      type = LibertyPgPort::PgType::deepnwell;
    else if (stringEqual(type_name, "deeppwell"))
      type = LibertyPgPort::PgType::deeppwell;

    else
      libError(1291, attr, "unknown pg_type.");
    pg_port_->setPgType(type);
  }
}

void
LibertyReader::visitVoltageName(LibertyAttr *attr)
{
  if (pg_port_) {
    const char *voltage_name = getAttrString(attr);
    pg_port_->setVoltageName(voltage_name);
  }
}

////////////////////////////////////////////////////////////////

LibertyFunc::LibertyFunc(const char *expr,
			 FuncExpr *&func_ref,
			 bool invert,
			 const char *attr_name,
			 int line) :
  expr_(stringCopy(expr)),
  func_ref_(func_ref),
  invert_(invert),
  attr_name_(stringCopy(attr_name)),
  line_(line)
{
}

LibertyFunc::~LibertyFunc()
{
  stringDelete(expr_);
  stringDelete(attr_name_);
}

////////////////////////////////////////////////////////////////

PortGroup::PortGroup(LibertyPortSeq *ports,
		     int line) :
  ports_(ports),
  line_(line)
{
}

PortGroup::~PortGroup()
{
  timings_.deleteContents();
  delete ports_;
}

void
PortGroup::addTimingGroup(TimingGroup *timing)
{
  timings_.push_back(timing);
}

void
PortGroup::addInternalPowerGroup(InternalPowerGroup *internal_power)
{
  internal_power_groups_.push_back(internal_power);
}

////////////////////////////////////////////////////////////////

SequentialGroup::SequentialGroup(bool is_register,
				 bool is_bank,
				 LibertyPort *out_port,
				 LibertyPort *out_inv_port,
				 int size,
				 int line) :
  is_register_(is_register),
  is_bank_(is_bank),
  out_port_(out_port),
  out_inv_port_(out_inv_port),
  size_(size),
  clk_(nullptr),
  data_(nullptr),
  preset_(nullptr),
  clear_(nullptr),
  clr_preset_var1_(LogicValue::unknown),
  clr_preset_var2_(LogicValue::unknown),
  line_(line)
{
}

SequentialGroup::~SequentialGroup()
{
  if (clk_)
    stringDelete(clk_);
  if (data_)
    stringDelete(data_);
  if (preset_)
    stringDelete(preset_);
  if (clear_)
    stringDelete(clear_);
}

void
SequentialGroup::setClock(const char *clk)
{
  clk_ = clk;
}

void
SequentialGroup::setData(const char *data)
{
  data_ = data;
}

void
SequentialGroup::setClear(const char *clr)
{
  clear_ = clr;
}

void
SequentialGroup::setPreset(const char *preset)
{
  preset_ = preset;
}

void
SequentialGroup::setClrPresetVar1(LogicValue var)
{
  clr_preset_var1_ = var;
}

void
SequentialGroup::setClrPresetVar2(LogicValue var)
{
  clr_preset_var2_ = var;
}

////////////////////////////////////////////////////////////////

StatetableGroup::StatetableGroup(StdStringSeq &input_ports,
                                 StdStringSeq &internal_ports,
                                 int line) :
  input_ports_(input_ports),
  internal_ports_(internal_ports),
  line_(line)
{
}

void
StatetableGroup::addRow(StateInputValues &input_values,
                        StateInternalValues &current_values,
                        StateInternalValues &next_values)
{
  table_.emplace_back(input_values, current_values, next_values);
}

////////////////////////////////////////////////////////////////

RelatedPortGroup::RelatedPortGroup(int line) :
  related_port_names_(nullptr),
  line_(line)
{
}

RelatedPortGroup::~RelatedPortGroup()
{
  if (related_port_names_) {
    deleteContents(related_port_names_);
    delete related_port_names_;
  }
}

void
RelatedPortGroup::setRelatedPortNames(StringSeq *names)
{
  related_port_names_ = names;
}

void
RelatedPortGroup::setIsOneToOne(bool one)
{
  is_one_to_one_ = one;
}

////////////////////////////////////////////////////////////////

TimingGroup::TimingGroup(int line) :
  RelatedPortGroup(line),
  attrs_(make_shared<TimingArcAttrs>()),
  related_output_port_name_(nullptr),
  receiver_model_(nullptr)
{
  for (auto rf_index : RiseFall::rangeIndex()) {
    cell_[rf_index] = nullptr;
    constraint_[rf_index] = nullptr;
    transition_[rf_index] = nullptr;
    intrinsic_[rf_index] = 0.0F;
    intrinsic_exists_[rf_index] = false;
    resistance_[rf_index] = 0.0F;
    resistance_exists_[rf_index] = false;
    output_waveforms_[rf_index] = nullptr;

    for (auto el_index : EarlyLate::rangeIndex()) {
      delay_sigma_[rf_index][el_index] = nullptr;
      slew_sigma_[rf_index][el_index] = nullptr;
      constraint_sigma_[rf_index][el_index] = nullptr;
    }
  }
}

TimingGroup::~TimingGroup()
{
  if (related_output_port_name_)
    stringDelete(related_output_port_name_);
}

void
TimingGroup::setRelatedOutputPortName(const char *name)
{
  related_output_port_name_ = stringCopy(name);
}

void
TimingGroup::setIntrinsic(RiseFall *rf,
			  float value)
{
  int rf_index = rf->index();
  intrinsic_[rf_index] = value;
  intrinsic_exists_[rf_index] = true;
}

void
TimingGroup::intrinsic(RiseFall *rf,
		       // Return values.
		       float &value,
		       bool &exists)
{
  int rf_index = rf->index();
  value = intrinsic_[rf_index];
  exists = intrinsic_exists_[rf_index];
}

void
TimingGroup::setResistance(RiseFall *rf,
			   float value)
{
  int rf_index = rf->index();
  resistance_[rf_index] = value;
  resistance_exists_[rf_index] = true;
}

void
TimingGroup::resistance(RiseFall *rf,
			// Return values.
			float &value,
			bool &exists)
{
  int rf_index = rf->index();
  value = resistance_[rf_index];
  exists = resistance_exists_[rf_index];
}

TableModel *
TimingGroup::cell(RiseFall *rf)
{
  return cell_[rf->index()];
}

void
TimingGroup::setCell(RiseFall *rf,
		     TableModel *model)
{
  cell_[rf->index()] = model;
}

TableModel *
TimingGroup::constraint(RiseFall *rf)
{
  return constraint_[rf->index()];
}

void
TimingGroup::setConstraint(RiseFall *rf,
			   TableModel *model)
{
  constraint_[rf->index()] = model;
}

TableModel *
TimingGroup::transition(RiseFall *rf)
{
  return transition_[rf->index()];
}

void
TimingGroup::setTransition(RiseFall *rf,
			   TableModel *model)
{
  transition_[rf->index()] = model;
}

void
TimingGroup::setDelaySigma(RiseFall *rf,
			   EarlyLate *early_late,
			   TableModel *model)
{
  delay_sigma_[rf->index()][early_late->index()] = model;
}

void
TimingGroup::setSlewSigma(RiseFall *rf,
			  EarlyLate *early_late,
			  TableModel *model)
{
  slew_sigma_[rf->index()][early_late->index()] = model;
}

void
TimingGroup::setConstraintSigma(RiseFall *rf,
				EarlyLate *early_late,
				TableModel *model)
{
  constraint_sigma_[rf->index()][early_late->index()] = model;
}

void
TimingGroup::setReceiverModel(ReceiverModelPtr receiver_model)
{
  receiver_model_ = receiver_model;
}

OutputWaveforms *
TimingGroup::outputWaveforms(RiseFall *rf)
{
  return output_waveforms_[rf->index()];
}

void
TimingGroup::setOutputWaveforms(RiseFall *rf,
                                OutputWaveforms *output_waveforms)
{
  output_waveforms_[rf->index()] = output_waveforms;
}

////////////////////////////////////////////////////////////////

InternalPowerGroup::InternalPowerGroup(int line) :
  InternalPowerAttrs(),
  RelatedPortGroup(line)
{
}

InternalPowerGroup::~InternalPowerGroup()
{
}

////////////////////////////////////////////////////////////////

LeakagePowerGroup::LeakagePowerGroup(int line) :
  LeakagePowerAttrs(),
  line_(line)
{
}

LeakagePowerGroup::~LeakagePowerGroup()
{
}

////////////////////////////////////////////////////////////////

PortNameBitIterator::PortNameBitIterator(LibertyCell *cell,
					 const char *port_name,
					 LibertyReader *visitor,
					 int line) :
  cell_(cell),
  visitor_(visitor),
  line_(line),
  port_(nullptr),
  bit_iterator_(nullptr),
  range_bus_port_(nullptr),
  range_name_next_(nullptr),
  size_(0)
{
  init(port_name);
}

void
PortNameBitIterator::init(const char *port_name)
{
  LibertyPort *port = visitor_->findPort(port_name);
  if (port) {
    if (port->isBus())
      bit_iterator_ = new LibertyPortMemberIterator(port);
    else
      port_ = port;
    size_ = port->size();
  }
  else {
    // Check for bus range.
    LibertyLibrary *library = visitor_->library();
    bool is_bus, is_range, subscript_wild;
    string bus_name;
    int from, to;
    parseBusName(port_name, library->busBrktLeft(),
                 library->busBrktRight(), '\\',
                 is_bus, is_range, bus_name, from, to, subscript_wild);
    if (is_range) {
      port = visitor_->findPort(port_name);
      if (port) {
	if (port->isBus()) {
	  if (port->busIndexInRange(from)
	      && port->busIndexInRange(to)) {
	    range_bus_port_ = port;
	    range_from_ = from;
	    range_to_ = to;
	    range_bit_ = from;
	  }
	  else
	    visitor_->libWarn(1292, line_, "port %s subscript out of range.",
			      port_name);
	}
	else
	  visitor_->libWarn(1293, line_, "port range %s of non-bus port %s.",
			    port_name,
			    bus_name.c_str());
      }
      else {
	range_bus_name_ = bus_name;
	range_from_ = from;
	range_to_ = to;
	range_bit_ = from;
	findRangeBusNameNext();
      }
      size_ = abs(from - to) + 1;
    }
    else
      visitor_->libWarn(1294, line_, "port %s not found.", port_name);
  }
}

PortNameBitIterator::~PortNameBitIterator()
{
  delete bit_iterator_;
}

bool
PortNameBitIterator::hasNext()
{
  return port_
    || (bit_iterator_ && bit_iterator_->hasNext())
    || (range_bus_port_
	&& ((range_from_ > range_to_)
	    ? range_bit_ >= range_to_
	    : range_bit_ <= range_from_))
    || (!range_bus_name_.empty()
	&& range_name_next_);
}

LibertyPort *
PortNameBitIterator::next()
{
  if (port_) {
    LibertyPort *next = port_;
    port_ = nullptr;
    return next;
  }
  else if (bit_iterator_)
    return bit_iterator_->next();
  else if (range_bus_port_) {
    LibertyPort *next = range_bus_port_->findLibertyBusBit(range_bit_);
    if (range_from_ > range_to_)
      range_bit_--;
    else
      range_bit_++;
    return next;
  }
  else if (!range_bus_name_.empty()) {
    LibertyPort *next = range_name_next_;
    findRangeBusNameNext();
    return next;
  }
  else
    return nullptr;
}

void
PortNameBitIterator::findRangeBusNameNext()
{
  if ((range_from_ > range_to_)
      ? range_bit_ >= range_to_
      : range_bit_ <= range_to_) {
    LibertyLibrary *library = visitor_->library();
    string bus_bit_name;
    stringPrint(bus_bit_name, "%s%c%d%c",
                range_bus_name_.c_str(),
                library->busBrktLeft(),
                range_bit_,
                library->busBrktRight());
    range_name_next_ = visitor_->findPort(bus_bit_name.c_str());
    if (range_name_next_) {
      if (range_from_ > range_to_)
	range_bit_--;
      else
	range_bit_++;
    }
    else
      visitor_->libWarn(1295, line_, "port %s not found.", bus_bit_name.c_str());
  }
  else
    range_name_next_ = nullptr;
}

////////////////////////////////////////////////////////////////

OutputWaveform::OutputWaveform(float slew,
                               float cap,
                               Table1 *currents,
                               float reference_time) :
  slew_(slew),
  cap_(cap),
  currents_(currents),
  reference_time_(reference_time)
{
}

OutputWaveform::~OutputWaveform()
{
  delete currents_;
}

Table1 *
OutputWaveform::stealCurrents()
{
  Table1 *currents = currents_;
  currents_ = nullptr;
  return currents;
}

} // namespace
