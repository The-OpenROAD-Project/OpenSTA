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

#include <ctype.h>
#include <stdlib.h>
#include "Machine.hh"
#include "Report.hh"
#include "Debug.hh"
#include "TokenParser.hh"
#include "Network.hh"
#include "Units.hh"
#include "PortDirection.hh"
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
#include "ParseBus.hh"
#include "Liberty.hh"
#include "LibertyBuilder.hh"
#include "LibertyReader.hh"
#include "LibertyReaderPvt.hh"

namespace sta {

static void
scaleFloats(FloatSeq *floats,
	    float scale);

LibertyLibrary *
readLibertyFile(const char *filename,
		bool infer_latches,
		Network *network)
{
  LibertyBuilder builder;
  LibertyReader reader(&builder);
  return reader.readLibertyFile(filename, infer_latches, network);
}

LibertyReader::LibertyReader(LibertyBuilder *builder) :
  LibertyGroupVisitor(),
  builder_(builder)
{
  defineVisitors();
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
LibertyReader::readLibertyFile(const char *filename,
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
  port_group_ = nullptr;
  saved_ports_ = nullptr;
  saved_port_group_ = nullptr;
  in_bus_ = false;
  in_bundle_ = false;
  sequential_ = nullptr;
  timing_ = nullptr;
  internal_power_ = nullptr;
  leakage_power_ = nullptr;
  table_ = nullptr;
  table_model_scale_ = 1.0;
  mode_def_ = nullptr;
  mode_value_ = nullptr;
  ocv_derate_ = nullptr;
  pg_port_ = nullptr;
  have_resistance_unit_ = false;
  
  for (auto rf_index : RiseFall::rangeIndex()) {
    have_input_threshold_[rf_index] = false;
    have_output_threshold_[rf_index] = false;
    have_slew_lower_threshold_[rf_index] = false;
    have_slew_upper_threshold_[rf_index] = false;
  }

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

  defineGroupVisitor("cell", &LibertyReader::beginCell,
		     &LibertyReader::endCell);
  defineGroupVisitor("scaled_cell", &LibertyReader::beginScaledCell,
		     &LibertyReader::endScaledCell);
  defineAttrVisitor("clock_gating_integrated_cell",
		    &LibertyReader::visitClockGatingIntegratedCell);
  defineAttrVisitor("area", &LibertyReader::visitArea);
  defineAttrVisitor("dont_use", &LibertyReader::visitDontUse);
  defineAttrVisitor("is_macro", &LibertyReader::visitIsMacro);
  defineAttrVisitor("is_pad", &LibertyReader::visitIsPad);
  defineAttrVisitor("interface_timing", &LibertyReader::visitInterfaceTiming);
  defineAttrVisitor("scaling_factors", &LibertyReader::visitScalingFactors);

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
}

void
LibertyReader::defineScalingFactorVisitors()
{
  for (int type_index = 0; type_index < scale_factor_type_count; type_index++) {
    ScaleFactorType type = static_cast<ScaleFactorType>(type_index);
    const char *type_name = scaleFactorTypeName(type);
    for (int pvt_index = 0; pvt_index < int(ScaleFactorPvt::count); pvt_index++) {
      ScaleFactorPvt pvt = static_cast<ScaleFactorPvt>(pvt_index);
      const char *pvt_name = scaleFactorPvtName(pvt);
      if (scaleFactorTypeRiseFallSuffix(type)) {
	for (auto tr : RiseFall::range()) {
	  const char *tr_name = (tr == RiseFall::rise()) ? "rise":"fall";
	  const char *attr_name = stringPrintTmp("k_%s_%s_%s",
						 pvt_name,
						 type_name,
						 tr_name);
	  defineAttrVisitor(attr_name,&LibertyReader::visitScaleFactorSuffix);
	}
      }
      else if (scaleFactorTypeRiseFallPrefix(type)) {
	for (auto tr : RiseFall::range()) {
	  const char *tr_name = (tr == RiseFall::rise()) ? "rise":"fall";
	  const char *attr_name = stringPrintTmp("k_%s_%s_%s",
						 pvt_name,
						 tr_name,
						 type_name);
	  defineAttrVisitor(attr_name,&LibertyReader::visitScaleFactorPrefix);
	}
      }
      else if (scaleFactorTypeLowHighSuffix(type)) {
	for (auto tr : RiseFall::range()) {
	  const char *tr_name = (tr == RiseFall::rise()) ? "high":"low";
	  const char *attr_name = stringPrintTmp("k_%s_%s_%s",
						 pvt_name,
						 tr_name,
						 type_name);
	  defineAttrVisitor(attr_name,&LibertyReader::visitScaleFactorHiLow);
	}
      }
      else {
	const char *attr_name = stringPrintTmp("k_%s_%s",
					       pvt_name,
					       type_name);
	defineAttrVisitor(attr_name,&LibertyReader::visitScaleFactor);
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
      libWarn(group, "library %s already exists.\n", name);
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


    library_->setDelayModelType(DelayModelType::cmos_linear);
    scale_factors_ = new ScaleFactors("");
    library_->setScaleFactors(scale_factors_);
  }
  else
    libError(group, "library does not have a name.\n");
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
  // Default resistance_unit to pulling_resistance_unit.
  if (!have_resistance_unit_) {
    Units *units = library_->units();
    *units->resistanceUnit() = *units->pullingResistanceUnit();
  }

  // These attributes reference named groups in the library so
  // wait until the end of the library to resolve them.
  if (default_wireload_) {
    Wireload *wireload = library_->findWireload(default_wireload_);
    if (wireload)
      library_->setDefaultWireload(wireload);
    else
      libWarn(group, "default_wire_load %s not found.\n", default_wireload_);
    stringDelete(default_wireload_);
  }

  if (default_wireload_selection_) {
    WireloadSelection *selection =
      library_->findWireloadSelection(default_wireload_selection_);
    if (selection)
      library_->setDefaultWireloadSelection(selection);
    else
      libWarn(group, "default_wire_selection %s not found.\n",
	      default_wireload_selection_);
    stringDelete(default_wireload_selection_);
  }

  bool missing_threshold = false;
  for (auto tr : RiseFall::range()) {
    int tr_index = tr->index();
    if (!have_input_threshold_[tr_index]) {
      libWarn(group, "input_threshold_pct_%s not found.\n", tr->name());
      missing_threshold = true;
    }
    if (!have_output_threshold_[tr_index]) {
      libWarn(group, "output_threshold_pct_%s not found.\n", tr->name());
      missing_threshold = true;
    }
    if (!have_slew_lower_threshold_[tr_index]) {
      libWarn(group, "slew_lower_threshold_pct_%s not found.\n", tr->name());
      missing_threshold = true;
    }
    if (!have_slew_upper_threshold_[tr_index]) {
      libWarn(group, "slew_upper_threshold_pct_%s not found.\n", tr->name());
      missing_threshold = true;
    }
  }
  if (missing_threshold)
    libError(group, "Library %s is missing one or more thresholds.\n",
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
	       library_->units()->pullingResistanceUnit());
}

void
LibertyReader::visitResistanceUnit(LibertyAttr *attr)
{
  if (library_) {
    parseUnits(attr, "ohm", res_scale_, library_->units()->resistanceUnit());
    have_resistance_unit_ = true;
  }
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
  const char *unit_str = getAttrString(attr);
  unsigned unit_str_length = strlen(unit_str);

  // Unit format is <multipler_digits><scale_suffix_char><unit_suffix>.
  // Find the multiplier digits.
  char *mult_str = makeTmpString(unit_str_length);
  const char *s = unit_str;
  char *m = mult_str;
  for (; *s; s++) {
    char ch = *s;
    if (isdigit(ch))
      *m++ = ch;
    else
      break;
  }
  *m = '\0';

  float mult = 1.0F;
  if (*mult_str != '\0') {
    if (stringEq(mult_str, "1"))
      mult = 1.0F;
    else if (stringEq(mult_str, "10"))
      mult = 10.0F;
    else if (stringEq(mult_str, "100"))
      mult = 100.0F;
    else
      libWarn(attr, "unknown unit multiplier %s.\n", mult_str);
  }

  float scale_mult = 1.0F;
  if (*s && stringEqual(s + 1, unit_suffix)) {
    char scale_char = tolower(*s);
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
      libWarn(attr, "unknown unit scale %c.\n", scale_char);
  }
  else if (!stringEqual(s, unit_suffix))
    libWarn(attr, "unknown unit suffix %s.\n", s + 1);

  scale_var = scale_mult * mult;
  unit->setScale(scale_var);
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
		libWarn(attr, "capacitive_load_units are not ff or pf.\n");
	    }
	    else
	      libWarn(attr, "capacitive_load_units are not a string.\n");
	  }
	  else
	    libWarn(attr, "capacitive_load_units missing suffix.\n");
	}
	else
	  libWarn(attr, "capacitive_load_units scale is not a float.\n");
      }
      else
	libWarn(attr, "capacitive_load_units missing scale and suffix.\n");
    }
    else
      libWarn(attr, "capacitive_load_unit missing values suffix.\n");
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
	libWarn(attr, "delay_model %s not supported.\n.", type_name);
      }
      else if (stringEq(type_name, "cmos2")) {
	library_->setDelayModelType(DelayModelType::cmos2);
	libWarn(attr, "delay_model %s not supported.\n.", type_name);
      }
      else if (stringEq(type_name, "polynomial")) {
	library_->setDelayModelType(DelayModelType::polynomial);
	libWarn(attr, "delay_model %s not supported.\n.", type_name);
      }
      // Evil IBM garbage.
      else if (stringEq(type_name, "dcm")) {
	library_->setDelayModelType(DelayModelType::dcm);
	libWarn(attr, "delay_model %s not supported.\n.", type_name);
      }
      else
	libWarn(attr, "unknown delay_model %s\n.", type_name);
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
      libWarn(attr, "unknown bus_naming_style format.\n");
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
	      libWarn(attr, "voltage_map voltage is not a float.\n");
	  }
	  else
	    libWarn(attr, "voltage_map missing voltage.\n");
	}
	else
	  libWarn(attr, "voltage_map supply name is not a string.\n");
      }
      else
	libWarn(attr, "voltage_map missing supply name and voltage.\n");
    }
    else
      libWarn(attr, "voltage_map missing values suffix.\n");
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
    if (exists)
      library_->setDefaultMaxSlew(value * time_scale_);
  }
}

void
LibertyReader::visitDefaultMaxFanout(LibertyAttr *attr)
{
  if (library_){
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists)
      library_->setDefaultMaxFanout(value);
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
    if (exists)
      library_->setDefaultFanoutLoad(value);
  }
}

void
LibertyReader::visitDefaultWireLoad(LibertyAttr *attr)
{
  if (library_) {
    const char *value = getAttrString(attr);
    if (value)
      default_wireload_ = stringCopy(value);
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
	libWarn(attr, "default_wire_load_mode %s not found.\n",
		wire_load_mode);
    }
  }
}

void
LibertyReader::visitDefaultWireLoadSelection(LibertyAttr *attr)
{
  if (library_) {
    const char *value = getAttrString(attr);
    if (value)
      default_wireload_selection_ = stringCopy(value);
  }
}

void
LibertyReader::visitDefaultOperatingConditions(LibertyAttr *attr)
{
  if (library_) {
    const char *op_cond_name = getAttrString(attr);
    OperatingConditions *op_cond =
      library_->findOperatingConditions(op_cond_name);
    if (op_cond)
      library_->setDefaultOperatingConditions(op_cond);
    else
      libWarn(attr, "default_operating_condition %s not found.\n",
	      op_cond_name);
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
      libWarn(group, "table template does not have a name.\n");
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
    TableAxis *axis;
    makeAxis(0, group, axis);
    if (axis)
      tbl_template_->setAxis1(axis);
    makeAxis(1, group, axis);
    if (axis)
      tbl_template_->setAxis2(axis);
    makeAxis(2, group, axis);
    if (axis)
      tbl_template_->setAxis3(axis);
    tbl_template_ = nullptr;
  }
}

void
LibertyReader::makeAxis(int index,
			LibertyGroup *group,
			TableAxis *&axis)
{
  axis = nullptr;
  TableAxisVariable axis_var = axis_var_[index];
  FloatSeq *axis_values = axis_values_[index];
  if (axis_var != TableAxisVariable::unknown && axis_values) {
    const Units *units = library_->units();
    float scale = tableVariableUnit(axis_var, units)->scale();
    scaleFloats(axis_values, scale);
    axis = new TableAxis(axis_var, axis_values);
  }
  else if (axis_var == TableAxisVariable::unknown && axis_values) {
    libWarn(group, "missing variable_%d attribute.\n", index + 1);
    delete axis_values;
    axis_values_[index] = nullptr;
  }
  // No warning for missing index_xx attributes because they are
  // not required by ic_shell.
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
      libWarn(attr, "axis type %s not supported.\n", type);
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
    if (axis_values)
      axis_values_[index] = axis_values;
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
	libWarn(group, "bus type %s missing bit_from.\n", name);
      if (!type_bit_to_exists_)
	libWarn(group, "bus type %s missing bit_to.\n", name);
    }
  }
  else
    libWarn(group, "type does not have a name.\n");
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
    libWarn(group, "scaling_factors do not have a name.\n");
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
  const char *name = group->firstName();
  if (name) {
    op_cond_ = new OperatingConditions(name);
    library_->addOperatingConditions(op_cond_);
  }
  else
    libWarn(group, "operating_conditions does not have a name.\n");
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
    libWarn(group, "wire_load does not have a name.\n");
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
      libWarn(attr, "fanout_length is missing length and fanout.\n");
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
    libWarn(group, "wire_load_selection does not have a name.\n");
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
		libWarn(attr, "wireload %s not found.\n", wireload_name);
	    }
	    else
	      libWarn(attr,
		      "wire_load_from_area wireload name not a string.\n");
	  }
	  else
	    libWarn(attr, "wire_load_from_area min not a float.\n");
	}
	else
	  libWarn(attr, "wire_load_from_area max not a float.\n");
      }
      else
	libWarn(attr, "wire_load_from_area missing parameters.\n");
    }
    else
      libWarn(attr, "wire_load_from_area missing parameters.\n");
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginCell(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name) {
    debugPrint1(debug_, "liberty", 1, "cell %s\n", name);
    cell_ = builder_->makeCell(library_, name, filename_);
    in_bus_ = false;
    in_bundle_ = false;
  }
  else
    libWarn(group, "cell does not have a name.\n");
}

void
LibertyReader::endCell(LibertyGroup *group)
{
  if (cell_) {
    // Sequentials and leakage powers reference expressions outside of port definitions
    // so they do not require LibertyFunc's.
    makeCellSequentials();
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
	libWarn(group, "cell %s ocv_derate_group %s not found.\n",
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
  PortGroupSeq::Iterator group_iter(cell_port_groups_);
  while (group_iter.hasNext()) {
    PortGroup *port_group = group_iter.next();
    int line = port_group->line();
    LibertyPortSeq::Iterator port_iter(port_group->ports());
    while (port_iter.hasNext()) {
      LibertyPort *port = port_iter.next();
      checkPort(port, line);
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
      libWarn(line, "port %s function size does not match port size.\n",
	      port->name());
    }
  }
  if (port->tristateEnable()
      && port->direction() == PortDirection::output())
    port->setDirection(PortDirection::tristate());
}

void
LibertyReader::makeTimingArcs(PortGroup *port_group)
{
  TimingGroupSeq::Iterator timing_iter(port_group->timingGroups());
  while (timing_iter.hasNext()) {
    TimingGroup *timing = timing_iter.next();
    timing->makeTimingModels(library_, this);

    LibertyPortSeq::Iterator port_iter(port_group->ports());
    while (port_iter.hasNext()) {
      LibertyPort *port = port_iter.next();
      makeTimingArcs(port, timing);
    }
    cell_->addTimingArcAttrs(timing);
  }
}

void
LibertyReader::makeInternalPowers(PortGroup *port_group)
{
  InternalPowerGroupSeq::Iterator power_iter(port_group->internalPowerGroups());
  while (power_iter.hasNext()) {
    InternalPowerGroup *power_group = power_iter.next();
    LibertyPortSeq::Iterator port_iter(port_group->ports());
    while (port_iter.hasNext()) {
      LibertyPort *port = port_iter.next();
      makeInternalPowers(port, power_group);
    }
    cell_->addInternalPowerAttrs(power_group);
  }
}

void
LibertyReader::makeCellSequentials()
{
  SequentialGroupSeq::Iterator seq_iter(cell_sequentials_);
  while (seq_iter.hasNext()) {
    SequentialGroup *seq = seq_iter.next();
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
      libWarn(line, "%s %s bus width mismatch.\n", type, clk_attr);
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
      libWarn(line, "%s %s bus width mismatch.\n", type, data_attr);
      data_expr->deleteSubexprs();
      data_expr = nullptr;
    }
  }
  const char *clr = seq->clear();
  FuncExpr *clr_expr = nullptr;
  if (clr) {
    clr_expr = parseFunc(clr, "clear", line);
    if (clr_expr && clr_expr->checkSize(size)) {
      libWarn(line, "%s %s bus width mismatch.\n", type, "clear");
      clr_expr->deleteSubexprs();
      clr_expr = nullptr;
    }
  }
  const char *preset = seq->preset();
  FuncExpr *preset_expr = nullptr;
  if (preset) {
    preset_expr = parseFunc(preset, "preset", line);
    if (preset_expr && preset_expr->checkSize(size)) {
      libWarn(line, "%s %s bus width mismatch.\n", type, "preset");
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
      libWarn(line, "latch enable function is non-unate for port %s.\n",
	      enable_port->name());
      break;
    case TimingSense::none:
    case TimingSense::unknown:
      libWarn(line, "latch enable function is unknown for port %s.\n",
	      enable_port->name());
      break;
    }
  }
}

void
LibertyReader::makeLeakagePowers()
{
  LeakagePowerGroupSeq::Iterator power_iter(leakage_powers_);
  while (power_iter.hasNext()) {
    LeakagePowerGroup *power_group = power_iter.next();
    builder_->makeLeakagePower(cell_, power_group);
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
  LibertyFuncSeq::Iterator func_iter(cell_funcs_);
  while (func_iter.hasNext()) {
    LibertyFunc *func = func_iter.next();
    FuncExpr *expr = parseFunc(func->expr(), func->attrName(), func->line());
    if (func->invert()) {
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
	  debugPrint2(debug_, "liberty", 1, "scaled cell %s %s\n",
		      name, op_cond_name);
	  cell_ = library_->makeScaledCell(name, filename_);
	}
	else
	  libWarn(group, "operating conditions %s not found.\n", op_cond_name);
      }
      else
	libWarn(group, "scaled_cell does not have an operating condition.\n");
    }
    else
      libWarn(group, "scaled_cell cell %s has not been defined.\n", name);
  }
  else
    libWarn(group, "scaled_cell does not have a name.\n");
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
      libWarn(group, "scaled_cell %s, %s port functions do not match cell port functions.\n",
	      cell_->name(),
	      op_cond_->name());
  }
  else
    libWarn(group, "scaled_cell ports do not match cell ports.\n");
  if (!equivCellTimingArcSets(cell_, scaled_cell_owner_))
    libWarn(group, "scaled_cell %s, %s timing does not match cell timing.\n",
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
  // Should be more comprehensive (timing checks on inputs, etc).
  TimingType type = timing->timingType();
  if (type == TimingType::combinational &&
      to_port_dir->isInput())
    libWarn(line, "combinational timing to an input port.\n");
  StringSeq::Iterator related_port_iter(timing->relatedPortNames());
  while (related_port_iter.hasNext()) {
    const char *from_port_name = related_port_iter.next();
    PortNameBitIterator from_port_iter(cell_, from_port_name, this, line);
    if (from_port_iter.hasNext()) {
      debugPrint2(debug_, "liberty", 2, "  timing %s -> %s\n",
		  from_port_name, to_port->name());
      makeTimingArcs(from_port_name, from_port_iter, to_port,
		     related_out_port, timing);
    }
  }
}

void
TimingGroup::makeTimingModels(LibertyLibrary *library,
			      LibertyReader *visitor)
{
  switch (library->delayModelType()) {
  case DelayModelType::cmos_linear:
    makeLinearModels(library);
    break;
  case DelayModelType::table:
    makeTableModels(visitor);
    break;
  case DelayModelType::cmos_pwl:
  case DelayModelType::cmos2:
  case DelayModelType::polynomial:
  case DelayModelType::dcm:
    break;
  }
}

void
TimingGroup::makeLinearModels(LibertyLibrary *library)
{
  for (auto tr : RiseFall::range()) {
    int tr_index = tr->index();
    float intr = intrinsic_[tr_index];
    bool intr_exists = intrinsic_exists_[tr_index];
    if (!intr_exists)
      library->defaultIntrinsic(tr, intr, intr_exists);
    TimingModel *model = nullptr;
    if (timingTypeIsCheck(timing_type_)) {
      if (intr_exists)
	model = new CheckLinearModel(intr);
    }
    else {
      float res = resistance_[tr_index];
      bool res_exists = resistance_exists_[tr_index];
      if (!res_exists)
	library->defaultPinResistance(tr, PortDirection::output(),
				      res, res_exists);
      if (!res_exists)
	res = 0.0F;
      if (intr_exists)
	model = new GateLinearModel(intr, res);
    }
    models_[tr_index] = model;
  }
}

void
TimingGroup::makeTableModels(LibertyReader *visitor)
{
  for (auto tr : RiseFall::range()) {
    int tr_index = tr->index();
    TableModel *cell = cell_[tr_index];
    TableModel *constraint = constraint_[tr_index];
    TableModel *transition = transition_[tr_index];
    if (cell || transition) {
      models_[tr_index] = new GateTableModel(cell, delay_sigma_[tr_index],
					     transition, slew_sigma_[tr_index]);
      if (timing_type_ == TimingType::clear
	  || timing_type_ == TimingType::combinational
	  || timing_type_ == TimingType::combinational_fall
	  || timing_type_ == TimingType::combinational_rise
	  || timing_type_ == TimingType::falling_edge
	  || timing_type_ == TimingType::preset
	  || timing_type_ == TimingType::rising_edge
	  || timing_type_ == TimingType::three_state_disable
	  || timing_type_ == TimingType::three_state_disable_rise
	  || timing_type_ == TimingType::three_state_disable_fall
	  || timing_type_ == TimingType::three_state_enable
	  || timing_type_ == TimingType::three_state_enable_fall
	  || timing_type_ == TimingType::three_state_enable_rise) {
	if (transition == nullptr)
	  visitor->libWarn(line_, "missing %s_transition.\n", tr->name());
	if (cell == nullptr)
	  visitor->libWarn(line_, "missing cell_%s.\n", tr->name());
      }
    }
    if (constraint)
      models_[tr_index] = new CheckTableModel(constraint,
					      constraint_sigma_[tr_index]);
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
      builder_->makeTimingArcs(cell_, from_port, to_port,
			       related_out_port, timing);
    }
  }
  else if (from_port_iter.size() > 1 && !to_port->hasMembers()) {
    // bus -> one
    while (from_port_iter.hasNext()) {
      LibertyPort *from_port = from_port_iter.next();
      builder_->makeTimingArcs(cell_, from_port, to_port,
			       related_out_port, timing);
    }
  }
  else if (from_port_iter.size() == 1 && to_port->hasMembers()) {
    // one -> bus
    if (from_port_iter.hasNext()) {
      LibertyPort *from_port = from_port_iter.next();
      LibertyPortMemberIterator bit_iter(to_port);
      while (bit_iter.hasNext()) {
	LibertyPort *to_port_bit = bit_iter.next();
	builder_->makeTimingArcs(cell_, from_port, to_port_bit,
				 related_out_port, timing);
      }
    }
  }
  else {
    // bus -> bus
    if (timing->isOneToOne()) {
      if (static_cast<int>(from_port_iter.size()) == to_port->size()) {
	LibertyPortMemberIterator to_iter(to_port);
	while (from_port_iter.hasNext() && to_iter.hasNext()) {
	  LibertyPort *from_port_bit = from_port_iter.next();
	  LibertyPort *to_port_bit = to_iter.next();
	  builder_->makeTimingArcs(cell_, from_port_bit, to_port_bit,
				   related_out_port, timing);
	}
      }
      else
	libWarn(timing->line(),
		"timing port %s and related port %s are different sizes.\n",
		from_port_name,
		to_port->name());
    }
    else {
      while (from_port_iter.hasNext()) {
	LibertyPort *from_port_bit = from_port_iter.next();
	LibertyPortMemberIterator to_iter(to_port);
	while (to_iter.hasNext()) {
	  LibertyPort *to_port_bit = to_iter.next();
	  builder_->makeTimingArcs(cell_, from_port_bit, to_port_bit,
				   related_out_port, timing);
	}
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::makeInternalPowers(LibertyPort *port,
				  InternalPowerGroup *power_group)
{
  int line = power_group->line();
  StringSeq *related_port_names = power_group->relatedPortNames();
  if (related_port_names) {
    StringSeq::Iterator related_port_iter(related_port_names);
    while (related_port_iter.hasNext()) {
      const char *related_port_name = related_port_iter.next();
      PortNameBitIterator related_port_iter(cell_, related_port_name, this, line);
      if (related_port_iter.hasNext()) {
	debugPrint2(debug_, "liberty", 2, "  power %s -> %s\n",
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
	builder_->makeInternalPower(cell_, port_bit, nullptr, power_group);
      }
    }
    else
      builder_->makeInternalPower(cell_, port, nullptr, power_group);
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
      builder_->makeInternalPower(cell_, port, related_port, power_group);
    }
  }
  else if (related_port_iter.size() > 1 && !port->hasMembers()) {
    // bus -> one
    while (related_port_iter.hasNext()) {
      LibertyPort *related_port = related_port_iter.next();
      builder_->makeInternalPower(cell_, port, related_port, power_group);
    }
  }
  else if (related_port_iter.size() == 1 && port->hasMembers()) {
    // one -> bus
    if (related_port_iter.hasNext()) {
      LibertyPort *related_port = related_port_iter.next();
      LibertyPortMemberIterator bit_iter(port);
      while (bit_iter.hasNext()) {
	LibertyPort *port_bit = bit_iter.next();
	builder_->makeInternalPower(cell_, port_bit, related_port, power_group);
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
	  builder_->makeInternalPower(cell_, port_bit, related_port_bit, power_group);
	}
      }
      else
	libWarn(power_group->line(),
		"internal_power port %s and related port %s are different sizes.\n",
		related_port_name,
		port->name());
    }
    else {
      while (related_port_iter.hasNext()) {
	LibertyPort *related_port_bit = related_port_iter.next();
	LibertyPortMemberIterator to_iter(port);
	while (to_iter.hasNext()) {
	  LibertyPort *port_bit = to_iter.next();
	  builder_->makeInternalPower(cell_, port_bit, related_port_bit, power_group);
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
LibertyReader::visitIsPad(LibertyAttr *attr)
{
  if (cell_) {
    bool is_pad, exists;
    getAttrBool(attr, is_pad, exists);
    if (exists)
      cell_->setIsPad(is_pad);
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
      libWarn(attr, "scaling_factors %s not found.\n", scale_factors_name);
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

////////////////////////////////////////////////////////////////

void
LibertyReader::beginPin(LibertyGroup *group)
{
  if (cell_) {
    if (in_bus_) {
      saved_ports_ = ports_;
      saved_port_group_ = port_group_;
      ports_ = new LibertyPortSeq;
      LibertyAttrValueIterator param_iter(group->params());
      while (param_iter.hasNext()) {
	LibertyAttrValue *param = param_iter.next();
	if (param->isString()) {
	  const char *name = param->stringValue();
	  debugPrint1(debug_, "liberty", 1, " port %s\n", name);
	  PortNameBitIterator port_iter(cell_, name, this, group->line());
	  while (port_iter.hasNext()) {
	    LibertyPort *port = port_iter.next();
	    ports_->push_back(port);
	    setPortDefaults(port);
	  }
	}
	else
	  libWarn(group, "pin name is not a string.\n");
      }
    }
    else if (in_bundle_) {
      saved_ports_ = ports_;
      saved_port_group_ = port_group_;
      ports_ = new LibertyPortSeq;
      LibertyAttrValueIterator param_iter(group->params());
      while (param_iter.hasNext()) {
	LibertyAttrValue *param = param_iter.next();
	if (param->isString()) {
	  const char *name = param->stringValue();
	  debugPrint1(debug_, "liberty", 1, " port %s\n", name);
	  LibertyPort *port = findPort(name);
	  if (port == nullptr)
	    port = builder_->makePort(cell_, name);
	  ports_->push_back(port);
	  setPortDefaults(port);
	}
	else
	  libWarn(group, "pin name is not a string.\n");
      }
    }
    else {
      ports_ = new LibertyPortSeq;
      char brkt_left = library_->busBrktLeft();
      char brkt_right = library_->busBrktRight();
      // Multiple port names can share group def.
      LibertyAttrValueIterator param_iter(group->params());
      while (param_iter.hasNext()) {
	LibertyAttrValue *param = param_iter.next();
	if (param->isString()) {
	  const char *name = param->stringValue();
	  debugPrint1(debug_, "liberty", 1, " port %s\n", name);
	  if (isBusName(name, brkt_left, brkt_right, escape_))
	    // Pins not inside a bus group with bus names are not really
	    // busses, so escape the brackets.
	    name = escapeChars(name, brkt_left, brkt_right, escape_);
	  LibertyPort *port = builder_->makePort(cell_, name);
	  ports_->push_back(port);
	  setPortDefaults(port);
	}
	else
	  libWarn(group, "pin name is not a string.\n");
      }
    }
    port_group_ = new PortGroup(ports_, group->line());
    cell_port_groups_.push_back(port_group_);
  }
  if (test_cell_) {
    const char *pin_name = group->firstName();
    if (pin_name)
      port_ = findPort(save_cell_, pin_name);
  }
}

void
LibertyReader::setPortDefaults(LibertyPort *port)
{
  float fanout;
  bool exists;
  library_->defaultMaxFanout(fanout, exists);
  if (exists)
    port->setFanoutLimit(fanout, MinMax::max());
  float slew;
  library_->defaultMaxSlew(slew, exists);
  if (exists)
    port->setSlewLimit(slew, MinMax::max());
  float max_cap;
  library_->defaultMaxCapacitance(max_cap, exists);
  if (exists)
    port->setCapacitanceLimit(slew, MinMax::max());
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
}

void
LibertyReader::endPorts()
{
  // Capacitances default based on direction so wait until the end
  // of the pin group to set them.
  LibertyPortSeq::Iterator port_iter(ports_);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
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
      libWarn(group, "bus %s bus_type not found.\n", group->firstName());
    endBusOrBundle();
    in_bus_ = false;
  }
}

void
LibertyReader::beginBusOrBundle(LibertyGroup *group)
{
  // Multiple port names can share group def.
  LibertyAttrValueIterator param_iter(group->params());
  while (param_iter.hasNext()) {
    LibertyAttrValue *param = param_iter.next();
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
	StringSeq::Iterator name_iter(bus_names_);
	while (name_iter.hasNext()) {
	  const char *name = name_iter.next();
	  debugPrint1(debug_, "liberty", 1, " bus %s\n", name);
	  LibertyPort *port = builder_->makeBusPort(cell_, name,
						    bus_dcl->from(),
						    bus_dcl->to());
	  ports_->push_back(port);
	}
      }
      else
	libWarn(attr, "bus_type %s not found.\n", bus_type);
    }
    else
      libWarn(attr, "bus_type is not a string.\n");
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
    if (ports_->empty())
      libWarn(group, "bundle %s member not found.\n", group->firstName());
    endBusOrBundle();
    in_bundle_ = false;
  }
}

void
LibertyReader::visitMembers(LibertyAttr *attr)
{
  if (cell_) {
    if (attr->isComplex()) {
      StringSeq::Iterator name_iter(bus_names_);
      while (name_iter.hasNext()) {
	const char *name = name_iter.next();
	debugPrint1(debug_, "liberty", 1, " bundle %s\n", name);
	ConcretePortSeq *members = new ConcretePortSeq;
	LibertyAttrValueIterator value_iter(attr->values());
	while (value_iter.hasNext()) {
	  LibertyAttrValue *value = value_iter.next();
	  if (value->isString()) {
	    const char *port_name = value->stringValue();
	    LibertyPort *port = findPort(port_name);
	    if (port == nullptr)
	      port = builder_->makePort(cell_, port_name);
	    members->push_back(port);
	  }
	  else
	    libWarn(attr, "member is not a string.\n");
	}
	LibertyPort *port = builder_->makeBundlePort(cell_, name, members);
	ports_->push_back(port);
      }
    }
    else
      libWarn(attr,"members attribute is missing values.\n");
  }
}

LibertyPort *
LibertyReader::findPort(const char *port_name)
{
  return findPort(cell_, port_name);
}

LibertyPort *
LibertyReader::findPort(LibertyCell *cell,
			const char *port_name)
{
  LibertyPort *port = cell->findLibertyPort(port_name);
  if (port == nullptr) {
    char brkt_left = library_->busBrktLeft();
    char brkt_right = library_->busBrktRight();
    if (isBusName(port_name, brkt_left, brkt_right, escape_)) {
      // Pins at top level with bus names have escaped brackets.
      port_name = escapeChars(port_name, brkt_left, brkt_right, escape_);
      port = cell->findLibertyPort(port_name);
    }
  }
  return port;
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
	libWarn(attr, "unknown port direction.\n");

      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	if (!port->direction()->isTristate())
	  // Tristate enable function sets direction to tristate; don't
	  // clobber it.
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	makeLibertyFunc(func, port->functionRef(), false, "function", attr);
      }
    }
  }
}

void
LibertyReader::visitThreeState(LibertyAttr *attr)
{
  if (ports_) {
    const char *three_state = getAttrString(attr);
    if (three_state) {
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	makeLibertyFunc(three_state, port->tristateEnableRef(), true,
			"three_state", attr);
      }
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setIsClock(is_clk);
      }
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setCapacitance(cap);
      }
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setFanoutLimit(fanout, min_max);
      }
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
      value *= time_scale_;
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setSlewLimit(value, min_max);
      }
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
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setCapacitanceLimit(value, min_max);
      }
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
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setMinPeriod(value * time_scale_);
      }
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
				  RiseFall *rf)
{
  if (cell_) {
    float value;
    bool exists;
    getAttrFloat(attr, value, exists);
    if (exists) {
      value *= time_scale_;
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setMinPulseWidth(rf, value);
      }
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
	libWarn(attr, "pulse_latch unknown pulse type.\n");
      if (trigger) {
	LibertyPortSeq::Iterator port_iter(ports_);
	while (port_iter.hasNext()) {
	  LibertyPort *port = port_iter.next();
	  port->setPulseClk(trigger, sense);
	}
      }
    }
  }
}

void
LibertyReader::visitClockGateClockPin(LibertyAttr *attr)
{
  if (cell_) {
    bool value, exists;
    getAttrBool(attr, value, exists);
    if (exists) {
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setIsClockGateClockPin(value);
      }
    }
  }
}

void
LibertyReader::visitClockGateEnablePin(LibertyAttr *attr)
{
  if (cell_) {
    bool value, exists;
    getAttrBool(attr, value, exists);
    if (exists) {
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setIsClockGateEnablePin(value);
      }
    }
  }
}

void
LibertyReader::visitClockGateOutPin(LibertyAttr *attr)
{
  if (cell_) {
    bool value, exists;
    getAttrBool(attr, value, exists);
    if (exists) {
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setIsClockGateOutPin(value);
      }
    }
  }
}

void
LibertyReader::visitIsPllFeedbackPin(LibertyAttr *attr)
{
  if (cell_) {
    bool value, exists;
    getAttrBool(attr, value, exists);
    if (exists) {
      LibertyPortSeq::Iterator port_iter(ports_);
      while (port_iter.hasNext()) {
	LibertyPort *port = port_iter.next();
	port->setIsPllFeedbackPin(value);
      }
    }
  }
}

void
LibertyReader::visitSignalType(LibertyAttr *attr)
{
  if (test_cell_) {
    const char *type = getAttrString(attr);
    if (type) {
      if (stringEq(type, "test_scan_enable"))
	test_cell_->setScanEnable(port_);
      if (stringEq(type, "test_scan_in"))
	test_cell_->setScanIn(port_);
      if (stringEq(type, "test_scan_out"))
	test_cell_->setScanOut(port_);
      if (stringEq(type, "test_scan_out_inverted"))
	test_cell_->setScanOutInv(port_);
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
	out_port = builder_->makeBusPort(cell_, out_name, size - 1, 0);
      else
	out_port = builder_->makePort(cell_,out_name);
      out_port->setDirection(PortDirection::internal());
    }
    if (out_inv_name) {
      if (has_size)
	out_port_inv = builder_->makeBusPort(cell_, out_inv_name, size - 1, 0);
      else
	out_port_inv = builder_->makePort(cell_, out_inv_name);
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
  LibertyAttrValueIterator param_iter(group->params());
  while (param_iter.hasNext()) {
    LibertyAttrValue *value = param_iter.next();
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
  if (test_cell_) {
    const char *next_state = getAttrString(attr);
    LibertyPort *port = findPort(save_cell_, next_state);
    if (port)
      test_cell_->setDataIn(port);
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
LibertyReader::beginTiming(LibertyGroup *group)
{
  if (port_group_) {
    timing_ = makeTimingGroup(group->line());
    port_group_->addTimingGroup(timing_);
  }
}

TimingGroup *
LibertyReader::makeTimingGroup(int line)
{
  return new TimingGroup(line);
}

void
LibertyReader::endTiming(LibertyGroup *)
{
  if (timing_) {
    // Set scale factor type in constraint tables.
    for (auto tr : RiseFall::range()) {
      TableModel *model = timing_->constraint(tr);
      if (model) {
	ScaleFactorType type=timingTypeScaleFactorType(timing_->timingType());
	model->setScaleFactorType(type);
      }
    }
    timing_ = nullptr;
  }
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
	libWarn(attr, "unknown timing_type %s.\n", type_name);
      else
	timing_->setTimingType(type);
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
	timing_->setTimingSense(TimingSense::non_unate);
      else if (stringEq(sense_name, "positive_unate"))
	timing_->setTimingSense(TimingSense::positive_unate);
      else if (stringEq(sense_name, "negative_unate"))
	timing_->setTimingSense(TimingSense::negative_unate);
      else
	libWarn(attr, "unknown timing_sense %s.\n", sense_name);
    }
  }
}

void
LibertyReader::visitSdfCondStart(LibertyAttr *attr)
{
  if (timing_) {
    const char *cond = getAttrString(attr);
    if (cond)
      timing_->setSdfCondStart(cond);
  }
}

void
LibertyReader::visitSdfCondEnd(LibertyAttr *attr)
{
  if (timing_) {
    const char *cond = getAttrString(attr);
    if (cond)
      timing_->setSdfCondEnd(cond);
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
	  timing_->setModeName(value->stringValue());
	  if (value_iter.hasNext()) {
	    value = value_iter.next();
	    if (value->isString())
	      timing_->setModeValue(value->stringValue());
	    else
	      libWarn(attr, "mode value is not a string.\n");
	  }
	  else
	    libWarn(attr, "missing mode value.\n");
	}
	else
	  libWarn(attr, "mode name is not a string.\n");
      }
      else
	libWarn(attr, "mode missing values.\n");
    }
    else
      libWarn(attr, "mode missing mode name and value.\n");
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
      TableModel *table_model = new TableModel(table_, scale_factor_type_, rf_);
      timing_->setCell(rf_, table_model);
    }
    else {
      libWarn(group, "unsupported model axis.\n");
      delete table_;
    }
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
      TableModel *table_model = new TableModel(table_, scale_factor_type_, rf_);
      timing_->setTransition(rf_, table_model);
    }
    else {
      libWarn(group, "unsupported model axis.\n");
      delete table_;
    }
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
      TableModel *table_model = new TableModel(table_, scale_factor_type_, rf_);
      timing_->setConstraint(rf_, table_model);
    }
    else {
      libWarn(group, "unsupported model axis.\n");
      delete table_;
    }
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
      TableModel *table_model = new TableModel(table_, scale_factor_type_, rf_);
      library_->setWireSlewDegradationTable(table_model, rf_);
    }
    else {
      libWarn(group, "unsupported model axis.\n");
      delete table_;
    }
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
}

void
LibertyReader::beginTable(LibertyGroup *group,
			  TableTemplateType type,
			  float scale)
{
  const char *template_name = group->firstName();
  if (template_name) {
    tbl_template_ = library_->findTableTemplate(template_name, type);
    if (tbl_template_) {
      axis_[0] = tbl_template_->axis1();
      axis_[1] = tbl_template_->axis2();
      axis_[2] = tbl_template_->axis3();
    }
    else {
      libWarn(group, "table template %s not found.\n", template_name);
      axis_[0] = axis_[1] = axis_[2] = nullptr;
    }
    clearAxisValues();
    own_axis_[0] = own_axis_[1] = own_axis_[2] = false;
    table_ = nullptr;
    table_model_scale_ = scale;
  }
}

void
LibertyReader::endTable()
{
  table_ = nullptr;
  tbl_template_ = nullptr;
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
      // Column index1*size(index2) + index2
      // Row    index3
      FloatTable *table = makeFloatTable(attr,
					 axis_[0]->size()*axis_[1]->size(),
					 axis_[2]->size(), scale);
      if (table)
	table_ = new Table3(table,
			    axis_[0], own_axis_[0],
			    axis_[1], own_axis_[1],
			    axis_[2], own_axis_[2]);
    }
    else if (axis_[0] && axis_[1]) {
      // Row    variable1/axis[0]
      // Column variable2/axis[1]
      FloatTable *table = makeFloatTable(attr, axis_[0]->size(),
					 axis_[1]->size(), scale);
      if (table)
	table_ = new Table2(table,
			    axis_[0], own_axis_[0],
			    axis_[1], own_axis_[1]);
    }
    else if (axis_[0]) {
      FloatTable *table = makeFloatTable(attr, 1, axis_[0]->size(), scale);
      if (table) {
	FloatSeq *values = (*table)[0];
	delete table;
	table_ = new Table1(values, axis_[0], own_axis_[0]);
      }
    }
    else {
      FloatTable *table = makeFloatTable(attr, 1, 1, scale);
      if (table) {
	float value = (*(*table)[0])[0];
	delete (*table)[0];
	delete table;
	table_ = new Table0(value);
      }
    }
  }
  else
    libWarn(attr, "%s is missing values.\n", attr->name());
}

FloatTable *
LibertyReader::makeFloatTable(LibertyAttr *attr,
			      size_t rows,
			      size_t cols,
			      float scale)
{
  FloatTable *table = new FloatTable;
  table->reserve(rows);
  LibertyAttrValueIterator value_iter(attr->values());
  while (value_iter.hasNext()) {
    LibertyAttrValue *value = value_iter.next();
    FloatSeq *row = new FloatSeq;
    row->reserve(cols);
    table->push_back(row);
    if (value->isString()) {
      const char *values_list = value->stringValue();
      parseStringFloatList(values_list, scale, row, attr);
    }
    else if (value->isFloat())
      // Scalar value.
      row->push_back(value->floatValue());
    else
      libWarn(attr, "%s is not a list of floats.\n", attr->name());
    if (row->size() != cols) {
      libWarn(attr, "table row has %u columns but axis has %d.\n",
	      // size_t is long on 64 bit ports.
	      static_cast<unsigned>(row->size()),
	      static_cast<unsigned>(cols));
      // Fill out row columns with zeros.
      for (size_t c = row->size(); c < cols; c++)
	row->push_back(0.0);
    }
  }
  if (table->size() != rows) {
    libWarn(attr, "table has %u rows but axis has %d.\n",
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
    axis_[index] = new TableAxis(var, values);
    own_axis_[index] = true;
  }
}

////////////////////////////////////////////////////////////////

// Define lut output variables as internal ports.
// I can't find any documentation for this group.
void
LibertyReader::beginLut(LibertyGroup *group)
{
  if (cell_) {
    LibertyAttrValueIterator param_iter(group->params());
    while (param_iter.hasNext()) {
      LibertyAttrValue *param = param_iter.next();
      if (param->isString()) {
	const char *names = param->stringValue();
	// Parse space separated list of related port names.
	TokenParser parser(names, " ");
	while (parser.hasNext()) {
	  char *name = parser.next();
	  if (name[0] != '\0') {
	    LibertyPort *port = builder_->makePort(cell_, name);
	    port->setDirection(PortDirection::internal());
	  }
	}
      }
      else
	libWarn(group, "lut output is not a string.\n");
    }
  }
}

void
LibertyReader::endLut(LibertyGroup *)
{
}

////////////////////////////////////////////////////////////////

// Find scan ports in test_cell group.
void
LibertyReader::beginTestCell(LibertyGroup *)
{
  test_cell_ = new TestCell;
  cell_->setTestCell(test_cell_);
  save_cell_ = cell_;
  cell_ = nullptr;
}

void
LibertyReader::endTestCell(LibertyGroup *)
{
  cell_ = save_cell_;
  test_cell_ = nullptr;
}

////////////////////////////////////////////////////////////////

void
LibertyReader::beginModeDef(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name)
    mode_def_ = cell_->makeModeDef(name);
  else
    libWarn(group, "mode definition does not have a name.\n");
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
      libWarn(group, "mode value does not have a name.\n");
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
    libWarn(attr, "when attribute inside table model.\n");
  if (mode_value_) {
    const char *func = getAttrString(attr);
    if (func)
      makeLibertyFunc(func, mode_value_->condRef(), false, "when", attr);
  }
  if (timing_) {
    const char *func = getAttrString(attr);
    if (func)
      makeLibertyFunc(func, timing_->condRef(), false, "when", attr);
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
      timing_->setSdfCond(cond);
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
      libWarn(attr, "%s attribute is not a string.\n", attr->name());
  }
  else
    libWarn(attr, "%s is not a simple attribute.\n", attr->name());
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
      libWarn(attr, "%s attribute is not an integer.\n",attr->name());
  }
  else
    libWarn(attr, "%s is not a simple attribute.\n", attr->name());
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
    libWarn(attr, "%s is not a simple attribute.\n", attr->name());
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
      if (*end && !isspace(*end))
	libWarn(attr, "%s value %s is not a float.\n",
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
	  libWarn(attr, "%s missing values.\n", attr->name());
      }
    }
    else
      libWarn(attr, "%s missing values.\n", attr->name());
  }
  else
    libWarn(attr, "%s is not a complex attribute.\n", attr->name());
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
      libWarn(attr, "%s is not a float.\n", token);
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
      else
	libWarn(attr, "%s is missing values.\n", attr->name());
    }
    if (value_iter.hasNext())
      libWarn(attr, "%s has more than one string.\n", attr->name());
  }
  else {
    LibertyAttrValue *value = attr->firstValue();
    if (value->isString()) {
      values = new FloatSeq;
      parseStringFloatList(value->stringValue(), scale, values, attr);
    }
    else
      libWarn(attr, "%s is missing values.\n", attr->name());
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
	libWarn(attr, "%s attribute is not boolean.\n", attr->name());
    }
    else
      libWarn(attr, "%s attribute is not boolean.\n", attr->name());
  }
  else
    libWarn(attr, "%s is not a simple attribute.\n", attr->name());
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
      libWarn(attr, "attribute %s value %s not recognized.\n",
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
  const char *error_msg = stringPrintTmp("%s, line %d %s",
					 filename_,		
					 line,
					 attr_name);
  return parseFuncExpr(func, cell_, error_msg, report_);
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
    libWarn(attr, "unknown early/late value.\n");
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
LibertyReader::libWarn(LibertyStmt *stmt,
		       const char *fmt,
		       ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(filename_, stmt->line(), fmt, args);
  va_end(args);
}

void
LibertyReader::libWarn(int line,
		       const char *fmt,
		       ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(filename_, line, fmt, args);
  va_end(args);
}

void
LibertyReader::libError(LibertyStmt *stmt,
			const char *fmt,
			...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileError(filename_, stmt->line(), fmt, args);
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
    TableModel *table_model = new TableModel(table_, scale_factor_type_, rf_);
    internal_power_->setModel(rf_, new InternalPowerModel(table_model));
  }
  endTableModel();
}

void
LibertyReader::visitRelatedGroundPin(LibertyAttr *attr)
{
  if (ports_) {
    const char *related_ground_pin = getAttrString(attr);
    LibertyPortSeq::Iterator port_iter(ports_);
    while (port_iter.hasNext()) {
      LibertyPort *port = port_iter.next();
      port->setRelatedGroundPin(related_ground_pin);
    }
  }
}

void
LibertyReader::visitRelatedPowerPin(LibertyAttr *attr)
{
  if (ports_) {
    const char *related_power_pin = getAttrString(attr);
    LibertyPortSeq::Iterator port_iter(ports_);
    while (port_iter.hasNext()) {
      LibertyPort *port = port_iter.next();
      port->setRelatedPowerPin(related_power_pin);
    }
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
      timing_->setOcvArcDepth(value);
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
    libWarn(attr, "OCV derate group named %s not found.\n", derate_name);
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
    libWarn(group, "ocv_derate does not have a name.\n");
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
      for (auto tr : rf_type_->range()) {
	if (path_type_ == PathType::clk_and_data) {
	  ocv_derate_->setDerateTable(tr, early_late, PathType::clk, table_);
	  ocv_derate_->setDerateTable(tr, early_late, PathType::data, table_);
	}
	else
	  ocv_derate_->setDerateTable(tr, early_late, path_type_, table_);
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
    libError(attr, "unknown rf_type.\n");
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
    libWarn(attr, "unknown derate type.\n");
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
      TableModel *table_model = new TableModel(table_, scale_factor_type_, rf_);
      if (sigma_type_ == EarlyLateAll::all()) {
	timing_->setDelaySigma(rf_, EarlyLate::min(), table_model);
	timing_->setDelaySigma(rf_, EarlyLate::max(), table_model);
      }
      else
	timing_->setDelaySigma(rf_, sigma_type_->asMinMax(), table_model);
    }
    else {
      libWarn(group, "unsupported model axis.\n");
      delete table_;
    }
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
      TableModel *table_model = new TableModel(table_, scale_factor_type_, rf_);
      if (sigma_type_ == EarlyLateAll::all()) {
	timing_->setSlewSigma(rf_, EarlyLate::min(), table_model);
	timing_->setSlewSigma(rf_, EarlyLate::max(), table_model);
      }
      else
	timing_->setSlewSigma(rf_, sigma_type_->asMinMax(), table_model);
    }
    else {
      libWarn(group, "unsupported model axis.\n");
      delete table_;
    }
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
      TableModel *table_model = new TableModel(table_, scale_factor_type_, rf_);
      if (sigma_type_ == EarlyLateAll::all()) {
	timing_->setConstraintSigma(rf_, EarlyLate::min(), table_model);
	timing_->setConstraintSigma(rf_, EarlyLate::max(), table_model);
      }
      else
	timing_->setConstraintSigma(rf_, sigma_type_->asMinMax(), table_model);
    }
    else {
      libWarn(group, "unsupported model axis.\n");
      delete table_;
    }
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
      libError(attr, "unknown pg_type.\n");
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
  // TimingGroups and IntternalPower are NOT deleted because ownership is transfered
  // to LibertyCell::timing_arc_attrs_ by LibertyReader::makeTimingArcs.
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
  TimingArcAttrs(),
  RelatedPortGroup(line),
  related_output_port_name_(nullptr)
{
  for (auto rf_index : RiseFall::rangeIndex()) {
    cell_[rf_index] = nullptr;
    constraint_[rf_index] = nullptr;
    transition_[rf_index] = nullptr;
    intrinsic_[rf_index] = 0.0F;
    intrinsic_exists_[rf_index] = false;
    resistance_[rf_index] = 0.0F;
    resistance_exists_[rf_index] = false;

    for (auto el_index : EarlyLate::rangeIndex()) {
      delay_sigma_[rf_index][el_index] = nullptr;
      slew_sigma_[rf_index][el_index] = nullptr;
      constraint_sigma_[rf_index][el_index] = nullptr;
    }
  }
}

TimingGroup::~TimingGroup()
{
  // TimingAttrs contents are not deleted because they are referenced
  // by TimingArcSets.
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
  range_bus_name_(nullptr),
  range_name_next_(nullptr),
  size_(0)
{
  init(port_name);
}

void
PortNameBitIterator::init(const char *port_name)
{
  LibertyPort *port = visitor_->findPort(cell_, port_name);
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
    int from, to;
    char *bus_name;
    parseBusRange(port_name, library->busBrktLeft(), library->busBrktRight(),
		  '\\', bus_name, from, to);
    if (bus_name) {
      port = visitor_->findPort(cell_, port_name);
      if (port) {
	if (port->isBus()) {
	  if (port->busIndexInRange(from)
	      && port->busIndexInRange(to)) {
	    range_bus_port_ = port;
	    range_from_ = from;
	    range_to_ = to;
	    range_bit_ = from;
	    delete [] bus_name;
	  }
	  else
	    visitor_->libWarn(line_, "port %s subscript out of range.\n",
			      port_name);
	}
	else
	  visitor_->libWarn(line_, "port range %s of non-bus port %s.\n",
			    port_name,
			    bus_name);
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
      visitor_->libWarn(line_, "port %s not found.\n", port_name);
  }
}

PortNameBitIterator::~PortNameBitIterator()
{
  stringDelete(range_bus_name_);
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
    || (range_bus_name_
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
  else if (range_bus_name_) {
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
    const char *bus_bit_name = stringPrintTmp("%s%c%d%c",
					      range_bus_name_,
					      library->busBrktLeft(),
					      range_bit_,
					      library->busBrktRight());
    range_name_next_ = visitor_->findPort(cell_, bus_bit_name);
    if (range_name_next_) {
      if (range_from_ > range_to_)
	range_bit_--;
      else
	range_bit_++;
    }
    else
      visitor_->libWarn(line_, "port %s not found.\n", bus_bit_name);
  }
  else
    range_name_next_ = nullptr;
}

} // namespace
