// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include "LibertyReader.hh"

#include <cctype>
#include <cstdlib>
#include <functional>
#include <string>
#include <string_view>

#include "ContainerHelpers.hh"
#include "Format.hh"
#include "Report.hh"
#include "Debug.hh"
#include "EnumNameMap.hh"
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
#include "LibExprReader.hh"
#include "Liberty.hh"
#include "LibertyBuilder.hh"
#include "LibertyReaderPvt.hh"
#include "PortDirection.hh"
#include "ParseBus.hh"
#include "Network.hh"

extern int LibertyParse_debug;

namespace sta {

static void
scaleFloats(FloatSeq &floats,
            float scale);

LibertyLibrary *
readLibertyFile(std::string_view filename,
                bool infer_latches,
                Network *network)
{
  LibertyReader reader(filename, infer_latches, network);
  return reader.readLibertyFile(filename);
}

LibertyReader::LibertyReader(std::string_view filename,
                             bool infer_latches,
                             Network *network) :
  LibertyGroupVisitor(),
  filename_(filename),
  infer_latches_(infer_latches),
  report_(network->report()),
  debug_(network->debug()),
  network_(network),
  builder_(debug_, report_),
  library_(nullptr)
{
  defineVisitors();
}

LibertyLibrary *
LibertyReader::readLibertyFile(std::string_view filename)
{
  //::LibertyParse_debug = 1;
  parseLibertyFile(filename, this, report_);
  return library_;
}

void
LibertyReader::defineGroupVisitor(std::string_view type,
                                  LibraryGroupVisitor begin_visitor,
                                  LibraryGroupVisitor end_visitor)
{
  std::string type_str(type);
  if (begin_visitor)
    group_begin_map_[type_str] = begin_visitor;
  if (end_visitor)
    group_end_map_[type_str] = end_visitor;
}

void
LibertyReader::defineVisitors()
{
  defineGroupVisitor("library", &LibertyReader::beginLibrary,
                     &LibertyReader::endLibrary);
  defineGroupVisitor("cell", nullptr, &LibertyReader::endCell);
  defineGroupVisitor("scaled_cell", nullptr, &LibertyReader::endScaledCell);
}

void
LibertyReader::visitAttr(const LibertySimpleAttr *)
{
}

void
LibertyReader::visitAttr(const LibertyComplexAttr *)
{
}

void
LibertyReader::begin(const LibertyGroup *group,
                     LibertyGroup *parent_group)
{
  LibraryGroupVisitor *visitor = findKeyValuePtr(group_begin_map_, group->type());
  if (visitor)
    (this->**visitor)(group, parent_group);
}

void
LibertyReader::end(const LibertyGroup *group,
                   LibertyGroup *parent_group)
{
  LibraryGroupVisitor *visitor = findKeyValuePtr(group_end_map_, group->type());
  if (visitor)
    (this->**visitor)(group, parent_group);
}

void
LibertyReader::beginLibrary(const LibertyGroup *library_group,
                            LibertyGroup *)
{
  makeLibrary(library_group);
}

void
LibertyReader::endLibrary(const LibertyGroup *library_group,
                          LibertyGroup *)
{
  // If a library has no cells endCell is not called.
  if (!library_group->empty())
    readLibraryAttributes(library_group);
  checkThresholds(library_group);
  delete library_group;
}

////////////////////////////////////////////////////////////////

void
LibertyReader::endCell(const LibertyGroup *cell_group,
                       LibertyGroup *library_group)
{
  // Read library groups defined since the last cell was read.
  // Normally they are all defined by the first cell, but there
  // are libraries that define table templates and bus tyupes
  // between cells.
  if (!library_group->oneGroupOnly())
    readLibraryAttributes(library_group);

  if (cell_group->hasFirstParam()) {
    const std::string &name = cell_group->firstParam();
    debugPrint(debug_, "liberty", 1, "cell {}", name);
    LibertyCell *cell = builder_.makeCell(library_, name, filename_);
    readCell(cell, cell_group);
  }
  else
    warn(1193, cell_group, "cell missing name.");

  // Delete the cell group and preceding library attributes
  // and groups so they are not revisited and reduce memory peak.
  library_group->clear();
}

void
LibertyReader::endScaledCell(const LibertyGroup *scaled_cell_group,
                             LibertyGroup *library_group)
{
  readLibraryAttributes(library_group);
  readScaledCell(scaled_cell_group);
  library_group->deleteSubgroup(scaled_cell_group);
}

////////////////////////////////////////////////////////////////

void
LibertyReader::readLibraryAttributes(const LibertyGroup *library_group)
{
  readTechnology(library_group);
  readLibraryUnits(library_group);
  readThresholds(library_group);
  readDelayModel(library_group);
  readBusStyle(library_group);
  readBusTypes(nullptr, library_group);
  readTableTemplates(library_group);
  readVoltateMaps(library_group);
  readWireloads(library_group);
  readWireloadSelection(library_group);
  readDefaultWireLoad(library_group);
  readDefaultWireLoadMode(library_group);
  readDefaultWireLoadSelection(library_group);
  readOperatingConds(library_group);
  readScaleFactors(library_group);
  readOcvDerateFactors(nullptr, library_group);
  readDefaultOcvDerateGroup(library_group);
  readGroupAttrFloat("ocv_arc_depth", library_group,
                    [this](float v) { library_->setOcvArcDepth(v); });
  readNormalizedDriverWaveform(library_group);
  readSlewDegradations(library_group);

  readLibAttrFloat(library_group, "nom_temperature",
                   &LibertyLibrary::setNominalTemperature, 1.0F);
  readLibAttrFloat(library_group, "nom_voltage", &LibertyLibrary::setNominalVoltage,
                   volt_scale_);
  readLibAttrFloat(library_group, "nom_process",
                   &LibertyLibrary::setNominalProcess, 1.0F);
  readLibAttrFloat(library_group, "default_inout_pin_cap",
                   &LibertyLibrary::setDefaultBidirectPinCap, cap_scale_);
  readLibAttrFloat(library_group, "default_input_pin_cap",
                   &LibertyLibrary::setDefaultInputPinCap, cap_scale_);
  readLibAttrFloat(library_group, "default_output_pin_cap",
                   &LibertyLibrary::setDefaultOutputPinCap, cap_scale_);
  readLibAttrFloatWarnZero(library_group, "default_max_transition",
                           &LibertyLibrary::setDefaultMaxSlew, time_scale_);
  readLibAttrFloatWarnZero(library_group, "default_max_fanout",
                           &LibertyLibrary::setDefaultMaxFanout, 1.0F);
  readLibAttrFloat(library_group, "default_intrinsic_rise",
                   &LibertyLibrary::setDefaultIntrinsic, RiseFall::rise(),
                   time_scale_);
  readLibAttrFloat(library_group, "default_intrinsic_fall",
                   &LibertyLibrary::setDefaultIntrinsic, RiseFall::fall(),
                   time_scale_);
  readLibAttrFloat(library_group, "default_inout_pin_rise_res",
                   &LibertyLibrary::setDefaultBidirectPinRes, RiseFall::rise(),
                   res_scale_);
  readLibAttrFloat(library_group, "default_inout_pin_fall_res",
                   &LibertyLibrary::setDefaultBidirectPinRes, RiseFall::fall(),
                   res_scale_);
  readLibAttrFloat(library_group, "default_output_pin_rise_res",
                   &LibertyLibrary::setDefaultOutputPinRes, RiseFall::rise(),
                   res_scale_);
  readLibAttrFloat(library_group, "default_output_pin_fall_res",
                   &LibertyLibrary::setDefaultOutputPinRes, RiseFall::fall(),
                   res_scale_);
  readLibAttrFloatWarnZero(library_group, "default_fanout_load",
                           &LibertyLibrary::setDefaultFanoutLoad, 1.0F);
  readLibAttrFloat(library_group, "slew_derate_from_library",
                   &LibertyLibrary::setSlewDerateFromLibrary, 1.0F);
}

void
LibertyReader::makeLibrary(const LibertyGroup *library_group)
{
  if (library_group->hasFirstParam()) {
    const std::string &lib_name = library_group->firstParam();
    LibertyLibrary *library = network_->findLiberty(lib_name);
    if (library)
      warn(1140, library_group, "library {} already exists.", lib_name);
    // Make a new library even if a library with the same name exists.
    // Both libraries may be accessed by min/max analysis points.
    library_ = network_->makeLibertyLibrary(lib_name, filename_);
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
    // Default is 1 micron.
    distance_scale_ = 1e-6;

    library_->units()->timeUnit()->setScale(time_scale_);
    library_->units()->capacitanceUnit()->setScale(cap_scale_);
    library_->units()->resistanceUnit()->setScale(res_scale_);
    library_->units()->voltageUnit()->setScale(volt_scale_);
    library_->units()->currentUnit()->setScale(current_scale_);
    library_->units()->distanceUnit()->setScale(distance_scale_);

    library_->setDelayModelType(DelayModelType::cmos_linear);
  }
  else
    error(1141, library_group, "library missing name.");
}

// Energy scale is derived from other units.
float
LibertyReader::energyScale()
{
  return volt_scale_ * volt_scale_ * cap_scale_;
}

void
LibertyReader::readTechnology(const LibertyGroup *library_group)
{
  const LibertyComplexAttr *tech_attr = library_group->findComplexAttr("technology");
  if (tech_attr) {
    const LibertyAttrValue *tech_value = tech_attr->firstValue();
    if (tech_value) {
      const std::string &tech = tech_value->stringValue();
      if (tech == "fpga")
        library_->setDelayModelType(DelayModelType::cmos_linear);
    }
  }
}

void
LibertyReader::readLibraryUnits(const LibertyGroup *library_group)
{
  readUnit("time_unit", "s", time_scale_, library_->units()->timeUnit(), library_group);
  readUnit("pulling_resistance_unit", "ohm", res_scale_,
            library_->units()->resistanceUnit(), library_group);
  readUnit("voltage_unit", "V", volt_scale_, library_->units()->voltageUnit(),
           library_group);
  readUnit("current_unit", "A", current_scale_, library_->units()->currentUnit(),
           library_group);
  readUnit("leakage_power_unit", "W", power_scale_, library_->units()->powerUnit(),
           library_group);
  readUnit("distance_unit", "m", distance_scale_, library_->units()->distanceUnit(),
           library_group);

  const LibertyComplexAttr *cap_attr =
    library_group->findComplexAttr("capacitive_load_unit");
  if (cap_attr) {
    const LibertyAttrValueSeq &values = cap_attr->values();
    if (values.size() == 2) {
      LibertyAttrValue *value = values[0];
      auto [scale, valid] = value->floatValue();
      if (valid) {
        value = values[1];
        if (value->isString()) {
          const std::string suffix = value->stringValue();
          if (stringEqual(suffix, "ff"))
            cap_scale_ = scale * 1E-15F;
          else if (stringEqual(suffix, "pf"))
            cap_scale_ = scale * 1E-12F;
          else
            warn(1154, cap_attr, "capacitive_load_units are not ff or pf.");
        }
        else
          warn(1155, cap_attr, "capacitive_load_units are not a string.");
      }
      else
        warn(1157, cap_attr, "capacitive_load_units scale is not a float.");
    }
    else if (values.size() == 1)
      warn(1156, cap_attr, "capacitive_load_units missing suffix.");
    else
      warn(1158, cap_attr, "capacitive_load_units missing scale and suffix.");
    library_->units()->capacitanceUnit()->setScale(cap_scale_);
  }
}

void
LibertyReader::readUnit(std::string_view unit_attr_name,
                        std::string_view unit_suffix,
                        float &scale_var,
                        Unit *unit,
                        const LibertyGroup *library_group)
{
  const LibertySimpleAttr *unit_attr = library_group->findSimpleAttr(unit_attr_name);
  if (unit_attr) {
    const std::string &units = unit_attr->stringValue();
    if (!units.empty()) {
      // Unit format is <multipler_digits><scale_suffix_char><unit_suffix>.
      // Find the multiplier digits.
      size_t mult_end = units.find_first_not_of("0123456789");
      float mult = 1.0F;
      std::string scale_suffix;
      if (mult_end != units.npos) {
        std::string unit_mult = units.substr(0, mult_end);
        scale_suffix = units.substr(mult_end);
        if (unit_mult == "1")
          mult = 1.0F;
        else if (unit_mult == "10")
          mult = 10.0F;
        else if (unit_mult == "100")
          mult = 100.0F;
        else
          warn(1150, unit_attr, "unknown unit multiplier {}.", unit_mult);
      }
      else
        scale_suffix = units;

      float scale_mult = 1.0F;
      if (scale_suffix.size() == unit_suffix.size() + 1) {
        std::string suffix = scale_suffix.substr(1);
        if (stringEqual(suffix, unit_suffix)) {
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
            warn(1151, unit_attr, "unknown unit scale {}.", scale_char);
        }
        else
          warn(1152, unit_attr, "unknown unit suffix {}.", suffix);
      }
      else if (!stringEqual(scale_suffix, unit_suffix))
        warn(1153, unit_attr, "unknown unit suffix {}.", scale_suffix);
      scale_var = scale_mult * mult;
      unit->setScale(scale_var);
    }
  }
}

void
LibertyReader::readDelayModel(const LibertyGroup *library_group)
{
  const std::string &type_name = library_group->findAttrString("delay_model");
  if (!type_name.empty()) {
    if (type_name == "table_lookup")
      library_->setDelayModelType(DelayModelType::table);
    else if (type_name == "generic_cmos")
      library_->setDelayModelType(DelayModelType::cmos_linear);
    else if (type_name == "piecewise_cmos") {
      library_->setDelayModelType(DelayModelType::cmos_pwl);
      warn(1160, library_group, "delay_model {} not supported.", type_name);
    }
    else if (type_name == "cmos2") {
      library_->setDelayModelType(DelayModelType::cmos2);
      warn(1161, library_group, "delay_model {} not supported.", type_name);
    }
    else if (type_name == "polynomial") {
      library_->setDelayModelType(DelayModelType::polynomial);
      warn(1162, library_group, "delay_model {} not supported.", type_name);
    }
    // Evil IBM garbage.
    else if (type_name == "dcm") {
      library_->setDelayModelType(DelayModelType::dcm);
      warn(1163, library_group, "delay_model {} not supported..", type_name);
    }
    else
      warn(1164, library_group, "unknown delay_model {}.", type_name);
  }
}

void
LibertyReader::readBusStyle(const LibertyGroup *library_group)
{
  const std::string &bus_style = library_group->findAttrString("bus_naming_style");
  if (!bus_style.empty()) {
    // Assume bus style is of the form "%s[%d]".
    if (bus_style.size() == 6
        && bus_style[0] == '%'
        && bus_style[1] == 's'
        && bus_style[3] == '%'
        && bus_style[4] == 'd')
      library_->setBusBrkts(bus_style[2], bus_style[5]);
    else
      warn(1165, library_group, "unknown bus_naming_style format.");
  }
}

void
LibertyReader::readBusTypes(LibertyCell *cell,
                            const LibertyGroup *group)
{
  for (const LibertyGroup *type_group : group->findSubgroups("type")) {
    if (type_group->hasFirstParam()) {
      const std::string &name = type_group->firstParam();
      int from, to;
      bool from_exists, to_exists;
      type_group->findAttrInt("bit_from", from, from_exists);
      type_group->findAttrInt("bit_to", to, to_exists);
      if (from_exists && to_exists) {
        if (cell)
          cell->makeBusDcl(name, from, to);
        else
          library_->makeBusDcl(name, from, to);
      }
      else if (!from_exists)
        warn(1179, type_group, "bus type missing bit_from.");
      else if (!to_exists)
        warn(1180, type_group, "bus type missing bit_to.");
    }
  }
}

void
LibertyReader::readThresholds(const LibertyGroup *library_group)
{
  for (const RiseFall *rf : RiseFall::range()) {
    std::string suffix(rf->to_string());
    readLibAttrFloat(library_group, "input_threshold_pct_" + suffix,
                     &LibertyLibrary::setInputThreshold, rf, 0.01F);
    readLibAttrFloat(library_group, "output_threshold_pct_" + suffix,
                     &LibertyLibrary::setOutputThreshold, rf, 0.01F);
    readLibAttrFloat(library_group, "slew_lower_threshold_pct_" + suffix,
                     &LibertyLibrary::setSlewLowerThreshold, rf, 0.01F);
    readLibAttrFloat(library_group, "slew_upper_threshold_pct_" + suffix,
                     &LibertyLibrary::setSlewUpperThreshold, rf, 0.01F);
  }
}

void
LibertyReader::checkThresholds(const LibertyGroup *library_group) const
{
  for (const RiseFall *rf : RiseFall::range()) {
    if (library_->inputThreshold(rf) == 0.0)
      warn(1145, library_group, "input_threshold_pct_{} not found.", rf->name());
    if (library_->outputThreshold(rf) == 0.0)
      warn(1146, library_group, "output_threshold_pct_{} not found.", rf->name());
    if (library_->slewLowerThreshold(rf) == 0.0)
      warn(1147, library_group, "slew_lower_threshold_pct_{} not found.", rf->name());
    if (library_->slewUpperThreshold(rf) == 0.0)
      warn(1148, library_group, "slew_upper_threshold_pct_{} not found.", rf->name());
  }
}

void
LibertyReader::readTableTemplates(const LibertyGroup *library_group)
{
  readTableTemplates(library_group, "lu_table_template", TableTemplateType::delay);
  readTableTemplates(library_group, "output_current_template",
                     TableTemplateType::output_current);
  readTableTemplates(library_group, "power_lut_template", TableTemplateType::power);
  readTableTemplates(library_group, "ocv_table_template", TableTemplateType::ocv);
}

void
LibertyReader::readTableTemplates(const LibertyGroup *library_group,
                                  std::string_view group_name,
                                  TableTemplateType type)
{
  for (const LibertyGroup *template_group :
       library_group->findSubgroups(group_name)) {
    if (template_group->hasFirstParam()) {
      const std::string &name = template_group->firstParam();
      TableTemplate *tbl_template = library_->makeTableTemplate(name, type);
      TableAxisPtr axis1 = makeTableTemplateAxis(template_group, 1);
      if (axis1)
        tbl_template->setAxis1(axis1);
      TableAxisPtr axis2 = makeTableTemplateAxis(template_group, 2);
      if (axis2)
        tbl_template->setAxis2(axis2);
      TableAxisPtr axis3 = makeTableTemplateAxis(template_group, 3);
      if (axis3)
        tbl_template->setAxis3(axis3);
    }
    else
      warn(1175, template_group, "table template missing name.");
  }
}

TableAxisPtr
LibertyReader::makeTableTemplateAxis(const LibertyGroup *template_group,
                                     int axis_index)
{
  std::string var_attr_name = sta::format("variable_{}", axis_index);
  const std::string &var_name = template_group->findAttrString(var_attr_name);
  if (!var_name.empty()) {
    TableAxisVariable axis_var = stringTableAxisVariable(var_name);
    if (axis_var == TableAxisVariable::unknown)
      warn(1297, template_group, "axis type {} not supported.", var_name);
    else {
      std::string index_attr_name = sta::format("index_{}", axis_index);
      const LibertyComplexAttr *index_attr =
        template_group->findComplexAttr(index_attr_name);
      FloatSeq axis_values;
      if (index_attr) {
        axis_values = readFloatSeq(index_attr, 1.0F);
        if (!axis_values.empty()) {
          float prev = axis_values[0];
          for (size_t i = 1; i < axis_values.size(); i++) {
            float value = axis_values[i];
            if (value <= prev) {
              warn(1178, template_group, "non-increasing table index values.");
              break;
            }
            prev = value;
          }
        }
      }
      const Units *units = library_->units();
      float scale = tableVariableUnit(axis_var, units)->scale();
      scaleFloats(axis_values, scale);
      return make_shared<TableAxis>(axis_var, std::move(axis_values));
    }
  }
  return nullptr;
}

static void
scaleFloats(FloatSeq &floats,
            float scale)
{
  size_t count = floats.size();
  for (size_t i = 0; i < count; i++)
    floats[i] *= scale;
}

void
LibertyReader::readVoltateMaps(const LibertyGroup *library_group)
{
  for (const LibertyComplexAttr *volt_attr :
         library_group->findComplexAttrs("voltage_map")) {
    const LibertyAttrValueSeq &values = volt_attr->values();
    if (values.size() == 2) {
      const std::string &volt_name = values[0]->stringValue();
      auto [volt, valid] = values[1]->floatValue();
      if (valid)
        library_->addSupplyVoltage(volt_name, volt);
      else
        warn(1166, volt_attr, "voltage_map voltage is not a float.");
    }
  }
}

void
LibertyReader::readOperatingConds(const LibertyGroup *library_group)
{
  for (const LibertyGroup *opcond_group :
         library_group->findSubgroups("operating_conditions")) {
    if (opcond_group->hasFirstParam()) {
      const std::string &name = opcond_group->firstParam();
      OperatingConditions *op_cond = library_->makeOperatingConditions(name);
      float value;
      bool exists;
      opcond_group->findAttrFloat("process", value, exists);
      if (exists)
        op_cond->setProcess(value);
      opcond_group->findAttrFloat("temperature", value, exists);
      if (exists)
        op_cond->setTemperature(value);
      opcond_group->findAttrFloat("voltage", value, exists);
      if (exists)
        op_cond->setVoltage(value);
      const std::string &tree_type = opcond_group->findAttrString("tree_type");
      if (!tree_type.empty()) {
        WireloadTree wireload_tree = stringWireloadTree(tree_type);
        op_cond->setWireloadTree(wireload_tree);
      }
    }
  }

  const std::string &default_op_cond =
    library_group->findAttrString("default_operating_conditions");
  if (!default_op_cond.empty()) {
    OperatingConditions *op_cond =
      library_->findOperatingConditions(default_op_cond);
    if (op_cond)
      library_->setDefaultOperatingConditions(op_cond);
    else
      warn(1144, library_group, "default_operating_condition {} not found.",
           default_op_cond);
  }
}

void
LibertyReader::readScaleFactors(const LibertyGroup *library_group)
{
  // Top level scale factors.
  ScaleFactors *scale_factors = library_->makeScaleFactors("");
  library_->setScaleFactors(scale_factors);
  readScaleFactors(library_group, scale_factors);

  // Named scale factors.
  for (const LibertyGroup *scale_group : library_group->findSubgroups("scaling_factors")){
    if (scale_group->hasFirstParam()) {
      const std::string &name = scale_group->firstParam();
      ScaleFactors *scale_factors = library_->makeScaleFactors(name);
      readScaleFactors(scale_group, scale_factors);
    }
  }
}

void
LibertyReader::readScaleFactors(const LibertyGroup *scale_group,
                                ScaleFactors *scale_factors)
{
  // Skip unknown type.
  for (int type_index = 0; type_index < scale_factor_type_count - 1; type_index++) {
    ScaleFactorType type = static_cast<ScaleFactorType>(type_index);
    const std::string &type_name = scaleFactorTypeName(type);
    // Skip unknown pvt.
    for (int pvt_index = 0; pvt_index < scale_factor_pvt_count - 1; pvt_index++) {
      ScaleFactorPvt pvt = static_cast<ScaleFactorPvt>(pvt_index);
      const std::string pvt_name = scaleFactorPvtName(pvt);
      std::string attr_name;
      for (const RiseFall *rf : RiseFall::range()) {
        if (scaleFactorTypeRiseFallSuffix(type)) {
          const std::string rf_name = (rf == RiseFall::rise()) ? "rise" : "fall";
          attr_name = sta::format("k_{}_{}_{}", pvt_name, type_name, rf_name);
        }
        else if (scaleFactorTypeRiseFallPrefix(type)) {
          const std::string rf_name = (rf == RiseFall::rise()) ? "rise" : "fall";
          attr_name = sta::format("k_{}_{}_{}", pvt_name, rf_name, type_name);
        }
        else if (scaleFactorTypeLowHighSuffix(type)) {
          const std::string rf_name = (rf == RiseFall::rise()) ? "high" : "low";
          attr_name = sta::format("k_{}_{}_{}", pvt_name, type_name, rf_name);
        }
        else
          attr_name = sta::format("k_{}_{}", pvt_name, type_name);
        float value;
        bool exists;
        scale_group->findAttrFloat(attr_name, value, exists);
        if (exists)
          scale_factors->setScale(type, pvt, rf, value);
      }
    }
  }
}

void
LibertyReader::readWireloads(const LibertyGroup *library_group)
{
  for (const LibertyGroup *wl_group : library_group->findSubgroups("wire_load")) {
    if (wl_group->hasFirstParam()) {
      const std::string &name = wl_group->firstParam();
      Wireload *wireload = library_->makeWireload(name);
      float value;
      bool exists;
      wl_group->findAttrFloat("resistance", value, exists);
      if (exists)
        wireload->setResistance(value * res_scale_);

      wl_group->findAttrFloat("capacitance", value, exists);
      if (exists)
        wireload->setCapacitance(value * cap_scale_);

      wl_group->findAttrFloat("slope", value, exists);
      if (exists)
        wireload->setSlope(value);

      for (const LibertyComplexAttr *fanout_attr :
             wl_group->findComplexAttrs("fanout_length")) {
        float fanout, length;
        bool exists;
        getAttrFloat2(fanout_attr, fanout, length, exists);
        if (exists)
          wireload->addFanoutLength(fanout, length);
        else
          warn(1185, fanout_attr, "fanout_length is missing length and fanout.");
      }
    }
    else
      warn(1184, wl_group, "wire_load missing name.");
  }
}

void
LibertyReader::readWireloadSelection(const LibertyGroup *library_group)
{
  const LibertyGroup *sel_group = library_group->findSubgroup("wire_load_selection");
  if (sel_group) {
    std::string name;
    if (sel_group->hasFirstParam())
      name = sel_group->firstParam();
    WireloadSelection *wireload_selection = library_->makeWireloadSelection(name);
    for (const LibertyComplexAttr *area_attr :
           sel_group->findComplexAttrs("wire_load_from_area")) {
      const LibertyAttrValueSeq &values = area_attr->values();
      if (values.size() == 3) {
        auto [min_area, min_valid] = values[0]->floatValue();
        if (min_valid) {
          auto [max_area, max_valid] = values[1]->floatValue();
          if (max_valid) {
            LibertyAttrValue *value = values[2];
            if (value->isString()) {
              const std::string &wireload_name = value->stringValue();
              const Wireload *wireload =
                library_->findWireload(wireload_name);
              if (wireload)
                wireload_selection->addWireloadFromArea(min_area, max_area,
                                                        wireload);
              else
                warn(1187, area_attr, "wireload {} not found.", wireload_name);
            }
            else
              warn(1188, area_attr,
                      "wire_load_from_area wireload name not a string.");
          }
          else
            warn(1189, area_attr, "wire_load_from_area min not a float.");
        }
        else
          warn(1190, area_attr, "wire_load_from_area max not a float.");
      }
      else
        warn(1191, area_attr, "wire_load_from_area missing parameters.");
    }
  }
}

void
LibertyReader::readDefaultWireLoad(const LibertyGroup *library_group)
{
  const std::string &wireload_name =
    library_group->findAttrString("default_wire_load");
  if (!wireload_name.empty()) {
    const Wireload *wireload = library_->findWireload(wireload_name);
    if (wireload)
      library_->setDefaultWireload(wireload);
    else
      warn(1142, library_group, "default_wire_load {} not found.",
              wireload_name);
  }
}

void
LibertyReader::readDefaultWireLoadMode(const LibertyGroup *library_group)
{
  const std::string &wire_load_mode =
    library_group->findAttrString("default_wire_load_mode");
  if (!wire_load_mode.empty()) {
    WireloadMode mode = stringWireloadMode(wire_load_mode);
    if (mode != WireloadMode::unknown)
      library_->setDefaultWireloadMode(mode);
    else
      warn(1174, library_group, "default_wire_load_mode {} not found.",
              wire_load_mode);
  }
}

void
LibertyReader::readDefaultWireLoadSelection(const LibertyGroup *library_group)
{
  const std::string &selection_name =
    library_group->findAttrString("default_wire_load_selection");
  if (!selection_name.empty()) {
    const WireloadSelection *selection =
      library_->findWireloadSelection(selection_name);
    if (selection)
      library_->setDefaultWireloadSelection(selection);
    else
      warn(1143, library_group, "default_wire_selection {} not found.",
              selection_name);
  }
}

void
LibertyReader::readModeDefs(LibertyCell *cell,
                            const LibertyGroup *cell_group)
{
  for (const LibertyGroup *mode_group : cell_group->findSubgroups("mode_definition")) {
    if (mode_group->hasFirstParam()) {
      const std::string &name = mode_group->firstParam();
      ModeDef *mode_def = cell->makeModeDef(name);
      for (const LibertyGroup *value_group : mode_group->findSubgroups("mode_value")) {
        if (value_group->hasFirstParam()) {
          const std::string &value_name = value_group->firstParam();
          ModeValueDef *mode_value = mode_def->defineValue(value_name);
          const std::string &sdf_cond = value_group->findAttrString("sdf_cond");
          if (!sdf_cond.empty())
            mode_value->setSdfCond(sdf_cond);
          const std::string &when = value_group->findAttrString("when");
          if (!when.empty()) {
            // line
            FuncExpr *when_expr = parseFunc(when, "when", cell,
                                            value_group->line());
            mode_value->setCond(when_expr);
          }
        }
        else
          warn(1264, value_group, "mode value missing name.");
      }
    }
    else
      warn(1263, mode_group, "mode definition missing name.");
  }
}

void
LibertyReader::readSlewDegradations(const LibertyGroup *library_group)
{
  for (const RiseFall *rf : RiseFall::range()) {
    const std::string group_name = sta::format("{}_transition_degradation",
                                               rf->to_string());
    const LibertyGroup *degradation_group =
      library_group->findSubgroup(group_name);
    if (degradation_group) {
      TableModel *table_model = readTableModel(degradation_group, rf,
                                               TableTemplateType::delay,
                                               time_scale_,
                                               ScaleFactorType::transition);
      if (LibertyLibrary::checkSlewDegradationAxes(table_model))
        library_->setWireSlewDegradationTable(table_model, rf);
      else
        warn(1254, degradation_group, "unsupported model axis.");
    }
  }
}

void
LibertyReader::readLibAttrFloat(const LibertyGroup *library_group,
                                std::string_view attr_name,
                                void (LibertyLibrary::*set_func)(float value),
                                float scale)
{
  float value;
  bool exists;
  library_group->findAttrFloat(attr_name, value, exists);
  if (exists)
    (library_->*set_func)(value * scale);
}

void
LibertyReader::readLibAttrFloat(const LibertyGroup *library_group,
                                std::string_view attr_name,
                                void (LibertyLibrary::*set_func)(const RiseFall *rf,
                                                                 float value),
                                const RiseFall *rf,
                                float scale)
{
  float value;
  bool exists;
  library_group->findAttrFloat(attr_name, value, exists);
  if (exists)
    (library_->*set_func)(rf, value * scale);
}

void
LibertyReader::readLibAttrFloatWarnZero(const LibertyGroup *library_group,
                                        std::string_view attr_name,
                                        void (LibertyLibrary::*set_func)(float value),
                                        float scale)
{
  float value;
  bool exists;
  library_group->findAttrFloat(attr_name, value, exists);
  if (exists) {
    if (value == 0.0F) {
      const LibertySimpleAttr *attr = library_group->findSimpleAttr(attr_name);
      if (attr)
        warn(1171, attr, "{} is 0.0.", attr_name);
      else
        warn(1172, library_group, "{} is 0.0.", attr_name);
    }
    (library_->*set_func)(value * scale);
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::readCell(LibertyCell *cell,
                        const LibertyGroup *cell_group)
{
  readBusTypes(cell, cell_group);
  // Make ports first because they are referenced by functions, timing arcs, etc.
  LibertyPortGroupMap port_group_map = makeCellPorts(cell, cell_group);

  // Make ff/latch output ports.
  makeSequentials(cell, cell_group);
  // Test cell ports may be referenced by a statetable.
  readTestCell(cell, cell_group);

  readCellAttributes(cell, cell_group);

  // Set port directions before making timing arcs etc.
  for (auto const &[port_group, ports] : port_group_map)
    readPortDir(ports, port_group);

  for (auto const &[port_group, ports] : port_group_map) {
    readPortAttributes(cell, ports, port_group);
    makePortFuncs(cell, ports, port_group);
    makeTimingArcs(cell, ports, port_group);
    readInternalPowerGroups(cell, ports, port_group);
  }

  cell->finish(infer_latches_, report_, debug_);
}

void
LibertyReader::readScaledCell(const LibertyGroup *scaled_cell_group)
{
  if (scaled_cell_group->hasFirstParam()) {
    const std::string &name = scaled_cell_group->firstParam();
    LibertyCell *owner = library_->findLibertyCell(name);
    if (owner) {
      if (scaled_cell_group->hasSecondParam()) {
        const std::string &op_cond_name = scaled_cell_group->secondParam();
        OperatingConditions *op_cond = library_->findOperatingConditions(op_cond_name);
        if (op_cond) {
          debugPrint(debug_, "liberty", 1, "scaled cell {} {}",
                     name, op_cond_name);
          LibertyCell *scaled_cell = library_->makeScaledCell(name, filename_);
          readCell(scaled_cell, scaled_cell_group);
          checkScaledCell(scaled_cell, owner, scaled_cell_group, op_cond_name);
          // Add scaled cell AFTER ports and timing arcs are defined.
          owner->addScaledCell(op_cond, scaled_cell);
        }
        else
          warn(1202, scaled_cell_group, "operating conditions {} not found.",
                  op_cond_name);
      }
      else
        warn(1203, scaled_cell_group, "scaled_cell missing operating condition.");
    }
    else
      warn(1204, scaled_cell_group, "scaled_cell cell {} has not been defined.", name);
  }
  else
    warn(1205, scaled_cell_group, "scaled_cell missing name.");
}

// Minimal check that is not very specific about where the discrepancies are.
void
LibertyReader::checkScaledCell(LibertyCell *scaled_cell,
                               LibertyCell *owner,
                               const LibertyGroup *scaled_cell_group,
                               std::string_view op_cond_name)
{
  if (equivCellPorts(scaled_cell, owner)) {
    if (!equivCellPorts(scaled_cell, owner))
      warn(1206, scaled_cell_group, "scaled_cell {}, {} ports do not match cell ports",
              scaled_cell->name(),
              op_cond_name);
    if (!equivCellFuncs(scaled_cell, owner))
      warn(1206, scaled_cell_group,
              "scaled_cell {}, {} port functions do not match cell port functions.",
              scaled_cell->name(),
              op_cond_name);
  }
  else
    warn(1207, scaled_cell_group, "scaled_cell ports do not match cell ports.");
  if (!equivCellTimingArcSets(scaled_cell, owner))
    warn(1208, scaled_cell_group,
            "scaled_cell {}, {} timing does not match cell timing.",
            scaled_cell->name(),
            op_cond_name);
}

LibertyPortGroupMap
LibertyReader::makeCellPorts(LibertyCell *cell,
                             const LibertyGroup *cell_group)
{
  LibertyPortGroupMap port_group_map;
  for (const LibertyGroup *subgroup : cell_group->subgroups()) {
    const std::string &type = subgroup->type();
    if (type == "pin")
      makePinPort(cell, subgroup, port_group_map);
    else if (type == "bus")
      makeBusPort(cell, subgroup, port_group_map);
    else if (type == "bundle")
      makeBundlePort(cell, subgroup, port_group_map);
    else if (type == "pg_pin")
      makePgPinPort(cell, subgroup);
  }
  return port_group_map;
}

void
LibertyReader::makePinPort(LibertyCell *cell,
                           const LibertyGroup *pin_group,
                           LibertyPortGroupMap &port_group_map)
{
  for (const LibertyAttrValue *port_value : pin_group->params()) {
    const std::string &port_name = port_value->stringValue();
    LibertyPort *port = makePort(cell, port_name);
    port_group_map[pin_group].push_back(port);
  }
}

void
LibertyReader::makeBusPort(LibertyCell *cell,
                           const LibertyGroup *bus_group,
                           LibertyPortGroupMap &port_group_map)
{
  for (const LibertyAttrValue *port_value : bus_group->params()) {
    const std::string &port_name = port_value->stringValue();
    const LibertySimpleAttr *bus_type_attr = bus_group->findSimpleAttr("bus_type");
    if (bus_type_attr) {
      const std::string &bus_type = bus_type_attr->stringValue();
      if (!bus_type.empty()) {
        // Look for bus dcl local to cell first.
        BusDcl *bus_dcl = cell->findBusDcl(bus_type);
        if (bus_dcl == nullptr)
          bus_dcl = library_->findBusDcl(bus_type);
        if (bus_dcl) {
          debugPrint(debug_, "liberty", 1, " bus {}", port_name);
          LibertyPort *bus_port = makeBusPort(cell, port_name,
                                              bus_dcl->from(), bus_dcl->to(),
                                              bus_dcl);
          port_group_map[bus_group].push_back(bus_port);
          // Make ports for pin groups inside the bus group.
          makeBusPinPorts(cell, bus_group, port_group_map);
        }
        else
          warn(1235, bus_type_attr, "bus_type {} not found.", bus_type);
      }
    }
    else
      warn(1236, bus_type_attr, "bus_type not found.");
  }
}

void
LibertyReader::makeBusPinPorts(LibertyCell *cell,
                               const LibertyGroup *bus_group,
                               LibertyPortGroupMap &port_group_map)
{
  for (const LibertyGroup *pin_group : bus_group->findSubgroups("pin")) {
    for (const LibertyAttrValue *param : pin_group->params()) {
      if (param->isString()) {
        const std::string pin_name = param->stringValue();
        debugPrint(debug_, "liberty", 1, " bus pin port {}", pin_name);
        // Expand foo[3:0] port names.
        PortNameBitIterator name_iter(cell, pin_name, this, pin_group->line());
        while (name_iter.hasNext()) {
          LibertyPort *pin_port = name_iter.next();
          if (pin_port) {
            port_group_map[pin_group].push_back(pin_port);
          }
          else
            warn(1232, pin_group, "pin {} not found.", pin_name);
        }
      }
      else
        warn(1233, pin_group, "pin name is not a string.");
    }
  }
}

void
LibertyReader::makeBundlePort(LibertyCell *cell,
                              const LibertyGroup *bundle_group,
                              LibertyPortGroupMap &port_group_map)
{
  if (bundle_group->hasFirstParam()) {
    const std::string &bundle_name = bundle_group->firstParam();
    debugPrint(debug_, "liberty", 1, " bundle {}", bundle_name);
    
    const LibertyComplexAttr *member_attr = bundle_group->findComplexAttr("members");
    ConcretePortSeq *members = new ConcretePortSeq;
    for (const LibertyAttrValue *member_value : member_attr->values()) {
      if (member_value->isString()) {
        const std::string &member_name = member_value->stringValue();
        LibertyPort *member = cell->findLibertyPort(member_name);
        if (member == nullptr)
          member = makePort(cell, member_name);
        members->push_back(member);
      }
    }
    LibertyPort *bundle_port = builder_.makeBundlePort(cell, bundle_name, members);
    port_group_map[bundle_group].push_back(bundle_port);
    // Make ports for pin groups inside the bundle group.
    makeBundlePinPorts(cell, bundle_group, port_group_map);
  }
  else
    warn(1313, bundle_group, "bundle missing name.");
}

void
LibertyReader::makeBundlePinPorts(LibertyCell *cell,
                                  const LibertyGroup *bundle_group,
                                  LibertyPortGroupMap &port_group_map)
{
  for (const LibertyGroup *pin_group : bundle_group->findSubgroups("pin")) {
    for (LibertyAttrValue *param : pin_group->params()) {
      if (param->isString()) {
        const std::string pin_name = param->stringValue();
        debugPrint(debug_, "liberty", 1, " bundle pin port {}", pin_name);
        LibertyPort *pin_port = cell->findLibertyPort(pin_name);
        if (pin_port == nullptr)
          pin_port = makePort(cell, pin_name);
        port_group_map[pin_group].push_back(pin_port);
      }
      else
        warn(1234, pin_group, "pin name is not a string.");
    }
  }
}

void
LibertyReader::makePgPinPort(LibertyCell *cell,
                             const LibertyGroup *pg_pin_group)
{
  if (pg_pin_group->hasFirstParam()) {
    const std::string &port_name = pg_pin_group->firstParam();
    LibertyPort *pg_port = makePort(cell, port_name);

    const std::string &type_name = pg_pin_group->findAttrString("pg_type");
    if (!type_name.empty()) {
      PwrGndType type = findPwrGndType(type_name);
      PortDirection *dir = PortDirection::unknown();
      switch (type) {
      case PwrGndType::primary_ground:
      case PwrGndType::backup_ground:
      case PwrGndType::internal_ground:
        dir = PortDirection::ground();
        break;
      case PwrGndType::primary_power:
      case PwrGndType::backup_power:
      case PwrGndType::internal_power:
        dir = PortDirection::power();
        break;
      case PwrGndType::nwell:
      case PwrGndType::pwell:
      case PwrGndType::deepnwell:
      case PwrGndType::deeppwell:
        dir = PortDirection::bias();
        break;
      case PwrGndType::none:
        error(1291, pg_pin_group, "unknown pg_type.");
        break;
      default:
        break;
      }
      pg_port->setPwrGndType(type);
      pg_port->setDirection(dir);
    }

    const std::string &voltate_name = pg_pin_group->findAttrString("voltage_name");
    if (!voltate_name.empty())
      pg_port->setVoltageName(voltate_name);
  }
  else
    warn(1314, pg_pin_group, "pg_pin missing name.");
}

////////////////////////////////////////////////////////////////

void
LibertyReader::readPortAttributes(LibertyCell *cell,
                                  const LibertyPortSeq &ports,
                                  const LibertyGroup *port_group)
{
  readCapacitance(ports, port_group);
  readMinPulseWidth(cell, ports, port_group);
  readPortAttrFloat("min_period", &LibertyPort::setMinPeriod, ports,
                    port_group, time_scale_);
  readPortAttrBool("clock", &LibertyPort::setIsClock, ports, port_group);
  readPortAttrFloat("fanout_load", &LibertyPort::setFanoutLoad, ports,
                    port_group, 1.0F);
  readPortAttrFloatMinMax("max_fanout", &LibertyPort::setFanoutLimit, ports,
                         port_group, MinMax::max(), 1.0F);
  readPortAttrFloatMinMax("min_fanout", &LibertyPort::setFanoutLimit, ports,
                         port_group, MinMax::min(), 1.0F);
  readPulseClock(ports, port_group);
  readPortAttrBool("clock_gate_clock_pin", &LibertyPort::setIsClockGateClock,
                   ports, port_group);
  readPortAttrBool("clock_gate_enable_pin", &LibertyPort::setIsClockGateEnable,
                   ports, port_group);
  readPortAttrBool("clock_gate_out_pin", &LibertyPort::setIsClockGateOut,
                   ports, port_group);
  readPortAttrBool("is_pll_feedback_pin", &LibertyPort::setIsPllFeedback,
                   ports, port_group);
  readSignalType(cell, ports, port_group);
  readPortAttrBool("isolation_cell_data_pin",
                   &LibertyPort::setIsolationCellData, ports, port_group);
  readPortAttrBool("isolation_cell_enable_pin",
                   &LibertyPort::setIsolationCellEnable, ports, port_group);
  readPortAttrBool("level_shifter_data_pin",
                   &LibertyPort::setLevelShifterData, ports, port_group);
  readPortAttrBool("switch_pin", &LibertyPort::setIsSwitch, ports, port_group);
  readPortAttrLibertyPort("related_ground_pin",
                          &LibertyPort::setRelatedGroundPort,
                          cell, ports, port_group);
  readPortAttrLibertyPort("related_power_pin",
                          &LibertyPort::setRelatedPowerPort,
                          cell, ports, port_group);
  readDriverWaveform(ports, port_group);
}

void
LibertyReader::readDriverWaveform(const LibertyPortSeq &ports,
                                  const LibertyGroup *port_group)
{
  for (const RiseFall *rf : RiseFall::range()) {
    const std::string_view attr_name = (rf == RiseFall::rise())
      ? "driver_waveform_rise"
      : "driver_waveform_fall";
    const std::string &name = port_group->findAttrString(attr_name);
    if (!name.empty()) {
      DriverWaveform *waveform = library_->findDriverWaveform(name);
      if (waveform) {
        for (LibertyPort *port : ports)
          port->setDriverWaveform(waveform, rf);
      }
    }
  }
}

void
LibertyReader::readPortAttrString(std::string_view attr_name,
                                  void (LibertyPort::*set_func)(std::string value),
                                  const LibertyPortSeq &ports,
                                  const LibertyGroup *group)
{
  const std::string &value = group->findAttrString(attr_name);
  if (!value.empty()) {
    for (LibertyPort *port : ports)
      (port->*set_func)(value);
  }
}

void
LibertyReader::readPortAttrLibertyPort(std::string_view attr_name,
                                       void (LibertyPort::*set_func)(LibertyPort *port),
                                       LibertyCell *cell,
                                       const LibertyPortSeq &ports,
                                       const LibertyGroup *group)
{
  const std::string &value = group->findAttrString(attr_name);
  if (!value.empty()) {
    LibertyPort *related = cell->findLibertyPort(value);
    for (LibertyPort *port : ports)
      (port->*set_func)(related);
  }
}

void
LibertyReader::readPortAttrFloat(std::string_view attr_name,
                                 void (LibertyPort::*set_func)(float value),
                                 const LibertyPortSeq &ports,
                                 const LibertyGroup *group,
                                 float scale)
{
  float value;
  bool exists;
  group->findAttrFloat(attr_name, value, exists);
  if (exists) {
    for (LibertyPort *port : ports)
      (port->*set_func)(value * scale);
  }
}

void
LibertyReader::readPortAttrBool(std::string_view attr_name,
                                void (LibertyPort::*set_func)(bool value),
                                const LibertyPortSeq &ports,
                                const LibertyGroup *group)
{
  const LibertySimpleAttr *attr = group->findSimpleAttr(attr_name);
  if (attr) {
    const LibertyAttrValue &attr_value = attr->value();
    if (attr_value.isString()) {
      const std::string &value = attr_value.stringValue();
      if (stringEqual(value, "true")) {
        for (LibertyPort *port : ports)
          (port->*set_func)(true);
      }
      else if (stringEqual(value, "false")) {
        for (LibertyPort *port : ports)
          (port->*set_func)(false);
      }
      else
        warn(1238, attr, "{} attribute is not boolean.", attr_name);
    }
    else
      warn(1239, attr, "{} attribute is not boolean.", attr_name);
  }
}

void
LibertyReader::readPortAttrFloatMinMax(std::string_view attr_name,
                                       void (LibertyPort::*set_func)(float value,
                                                                     const MinMax *min_max),
                                       const LibertyPortSeq &ports,
                                       const LibertyGroup *group,
                                       const MinMax *min_max,
                                       float scale)
{
  float value;
  bool exists;
  group->findAttrFloat(attr_name, value, exists);
  if (exists) {
    for (LibertyPort *port : ports)
      (port->*set_func)(value * scale, min_max);
  }
}

void
LibertyReader::readPulseClock(const LibertyPortSeq &ports,
                              const LibertyGroup *port_group)
{
  const std::string &pulse_clk = port_group->findAttrString("pulse_clock");
  if (!pulse_clk.empty()) {
    const RiseFall *trigger = nullptr;
    const RiseFall *sense = nullptr;
    if (pulse_clk == "rise_triggered_high_pulse") {
      trigger = RiseFall::rise();
      sense = RiseFall::rise();
    }
    else if (pulse_clk == "rise_triggered_low_pulse") {
      trigger = RiseFall::rise();
      sense = RiseFall::fall();
    }
    else if (pulse_clk == "fall_triggered_high_pulse") {
      trigger = RiseFall::fall();
      sense = RiseFall::rise();
    }
    else if (pulse_clk == "fall_triggered_low_pulse") {
      trigger = RiseFall::fall();
      sense = RiseFall::fall();
    }
    else
      warn(1242, port_group, "pulse_latch unknown pulse type.");
    if (trigger) {
      for (LibertyPort *port : ports)
        port->setPulseClk(trigger, sense);
    }
  }
}

void
LibertyReader::readSignalType(LibertyCell *cell,
                              const LibertyPortSeq &ports,
                              const LibertyGroup *port_group)
{
  if (!dynamic_cast<TestCell *>(cell))
    return;
  const std::string &type = port_group->findAttrString("signal_type");
  if (type.empty())
    return;
  ScanSignalType signal_type = ScanSignalType::none;
  if (type == "test_scan_enable")
    signal_type = ScanSignalType::enable;
  else if (type == "test_scan_enable_inverted")
    signal_type = ScanSignalType::enable_inverted;
  else if (type == "test_scan_clock")
    signal_type = ScanSignalType::clock;
  else if (type == "test_scan_clock_a")
    signal_type = ScanSignalType::clock_a;
  else if (type == "test_scan_clock_b")
    signal_type = ScanSignalType::clock_b;
  else if (type == "test_scan_in")
    signal_type = ScanSignalType::input;
  else if (type == "test_scan_in_inverted")
    signal_type = ScanSignalType::input_inverted;
  else if (type == "test_scan_out")
    signal_type = ScanSignalType::output;
  else if (type == "test_scan_out_inverted")
    signal_type = ScanSignalType::output_inverted;
  else {
    warn(1299, port_group, "unknown signal_type {}.", type);
    return;
  }
  for (LibertyPort *port : ports)
    port->setScanSignalType(signal_type);
}

void
LibertyReader::readPortDir(const LibertyPortSeq &ports,
                          const LibertyGroup *port_group)
{
  const LibertySimpleAttr *dir_attr = port_group->findSimpleAttr("direction");
  // Note missing direction attribute is not an error because a bus group
  // can have pin groups for the bus bits that have direcitons.
  if (dir_attr) {
    const std::string &dir = dir_attr->stringValue();
    if (!dir.empty()) {
      PortDirection *port_dir = PortDirection::unknown();
      if (dir == "input")
        port_dir = PortDirection::input();
      else if (dir == "output")
        port_dir = PortDirection::output();
      else if (dir == "inout")
        port_dir = PortDirection::bidirect();
      else if (dir == "internal")
        port_dir = PortDirection::internal();
      else
        warn(1240, dir_attr, "unknown port direction.");
      for (LibertyPort *port : ports)
        port->setDirection(port_dir);
    }
  }
}

void
LibertyReader::readCapacitance(const LibertyPortSeq &ports,
                               const LibertyGroup *port_group)
{
  // capacitance
  readPortAttrFloat("capacitance", &LibertyPort::setCapacitance, ports,
                    port_group, cap_scale_);

  for (LibertyPort *port : ports) {
    // rise/fall_capacitance
    for (const RiseFall *rf : RiseFall::range()) {
      std::string attr_name = sta::format("{}_capacitance", rf->to_string());
      float cap;
      bool exists;
      port_group->findAttrFloat(attr_name, cap, exists);
      if (exists) {
        for (const MinMax *min_max : MinMax::range())
          port->setCapacitance(rf, min_max, cap * cap_scale_);
      }

      // rise/fall_capacitance_range(min_cap, max_cap);
      attr_name = sta::format("{}_capacitance_range", rf->to_string());
      const LibertyComplexAttrSeq &range_attrs = port_group->findComplexAttrs(attr_name);
      if (!range_attrs.empty()) {
        const LibertyComplexAttr *attr = range_attrs[0];
        const LibertyAttrValueSeq &values = attr->values();
        if (values.size() == 2) {
          auto [cap_min, min_valid] = values[0]->floatValue();
          if (min_valid)
            port->setCapacitance(rf, MinMax::min(), cap_min * cap_scale_);
          auto [cap_max, max_valid] = values[1]->floatValue();
          if (max_valid)
            port->setCapacitance(rf, MinMax::max(), cap_max * cap_scale_);
        }
      }
    }
    if (!(port->isBus() || port->isBundle()))
      setPortCapDefault(port);

    for (const MinMax *min_max : MinMax::range()) {
      // min/max_capacitance
      std::string attr_name = sta::format("{}_capacitance", min_max->to_string());
      float limit;
      bool exists;
      port_group->findAttrFloat(attr_name, limit, exists);
      if (exists)
        port->setCapacitanceLimit(limit * cap_scale_, min_max);

      // min/max_transition
      attr_name = sta::format("{}_transition", min_max->to_string());
      port_group->findAttrFloat(attr_name, limit, exists);
      if (exists) {
        if (min_max == MinMax::max() && limit == 0.0)
          warn(1241, port_group, "max_transition is 0.0.");
        port->setSlewLimit(limit * time_scale_, min_max);
      }
    }

    // Default capacitance.
    if (port->isBus() || port->isBundle()) {
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
}

void
LibertyReader::setPortCapDefault(LibertyPort *port)
{
  for (const MinMax *min_max : MinMax::range()) {
    for (const RiseFall *rf : RiseFall::range()) {
      float cap;
      bool exists;
      port->capacitance(rf, min_max, cap, exists);
      if (!exists)
        port->setCapacitance(rf, min_max, defaultCap(port));
    }
  }
}

void
LibertyReader::readMinPulseWidth(LibertyCell *cell,
                                 const LibertyPortSeq &ports,
                                 const LibertyGroup *port_group)
{
  for (LibertyPort *port : ports) {
    TimingArcAttrsPtr timing_attrs = nullptr;
    for (const RiseFall *rf : RiseFall::range()) {
      const std::string mpw_attr_name = rf == RiseFall::rise()
        ? "min_pulse_width_high"
        : "min_pulse_width_low";
      float mpw;
      bool exists;
      port_group->findAttrFloat(mpw_attr_name, mpw, exists);
      if (exists) {
        mpw *= time_scale_;
        port->setMinPulseWidth(rf, mpw);

        // Make timing arcs for the port min_pulse_width_low/high attributes.
        // This is redundant but makes sdf annotation consistent.
        if (timing_attrs == nullptr) {
          timing_attrs = std::make_shared<TimingArcAttrs>();
          timing_attrs->setTimingType(TimingType::min_pulse_width);
        }
        TimingModel *check_model =
          makeScalarCheckModel(cell, mpw, ScaleFactorType::min_pulse_width, rf);
        timing_attrs->setModel(rf, check_model);
      }
    }
    if (timing_attrs)
      builder_.makeTimingArcs(cell, port, port, nullptr, timing_attrs,
                              port_group->line());
  }
}

void
LibertyReader::makePortFuncs(LibertyCell *cell,
                             const LibertyPortSeq &ports,
                             const LibertyGroup *port_group)
{
  const LibertySimpleAttr *func_attr = port_group->findSimpleAttr("function");
  if (func_attr) {
    const std::string &func = func_attr->stringValue();
    if (!func.empty()) {
      FuncExpr *func_expr = parseFunc(func, "function", cell, func_attr->line());
      for (LibertyPort *port : ports) {
        port->setFunction(func_expr);
        if (func_expr->checkSize(port)) {
          warn(1195, func_attr->line(),
                  "port {} function size does not match port size.",
                  port->name());
        }
      }
    }
  }

  const LibertySimpleAttr *tri_attr = port_group->findSimpleAttr("three_state");
  if (tri_attr) {
    const std::string tri_disable = tri_attr->stringValue();
    if (!tri_disable.empty()) {
      FuncExpr *tri_disable_expr = parseFunc(tri_disable,
                                             "three_state", cell,
                                             tri_attr->line());
      FuncExpr *tri_enable_expr = tri_disable_expr->invert();
      for (LibertyPort *port : ports) {
        port->setTristateEnable(tri_enable_expr);
        if (port->direction() == PortDirection::output())
          port->setDirection(PortDirection::tristate());
      }
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::makeSequentials(LibertyCell *cell,
                               const LibertyGroup *cell_group)
{
  makeSequentials(cell, cell_group, true, "ff", "clocked_on", "next_state");
  makeSequentials(cell, cell_group, true, "ff_bank", "clocked_on", "next_state");
  makeSequentials(cell, cell_group, false, "latch", "enable", "data_in");
  makeSequentials(cell, cell_group, false, "latch_bank", "enable", "data_in");

  const LibertyGroup *lut_group = cell_group->findSubgroup("lut");;
  if (lut_group) {
    LibertyPort *out_port = nullptr;
    LibertyPort *out_port_inv = nullptr;
    size_t size;
    makeSeqPorts(cell, lut_group, out_port, out_port_inv, size);
  }
}

void
LibertyReader::makeSequentials(LibertyCell *cell,
                               const LibertyGroup *cell_group,
                               bool is_register,
                               std::string_view seq_group_name,
                               std::string_view clk_attr_name,
                               std::string_view data_attr_name)
{
  for (const LibertyGroup *seq_group :
       cell_group->findSubgroups(seq_group_name)) {
    LibertyPort *out_port = nullptr;
    LibertyPort *out_port_inv = nullptr;
    size_t size;
    makeSeqPorts(cell, seq_group, out_port, out_port_inv, size);
    FuncExpr *clk_expr = makeSeqFunc(cell, seq_group, clk_attr_name, size);
    FuncExpr *data_expr = makeSeqFunc(cell, seq_group, data_attr_name, size);
    FuncExpr *clr_expr = makeSeqFunc(cell, seq_group, "clear", size);
    FuncExpr *preset_expr = makeSeqFunc(cell, seq_group, "preset", size);

    LogicValue clr_preset_var1 = LogicValue::unknown;
    const LibertySimpleAttr *var1 = seq_group->findSimpleAttr("clear_preset_var1");
    if (var1)
      clr_preset_var1 = getAttrLogicValue(var1);

    LogicValue clr_preset_var2 = LogicValue::unknown;
    const LibertySimpleAttr *var2 = seq_group->findSimpleAttr("clear_preset_var2");
    if (var2)
      clr_preset_var2 = getAttrLogicValue(var2);

    cell->makeSequential(size, is_register, clk_expr, data_expr, clr_expr,
                         preset_expr, clr_preset_var1, clr_preset_var2,
                         out_port, out_port_inv);
  }
}

FuncExpr *
LibertyReader::makeSeqFunc(LibertyCell *cell,
                           const LibertyGroup *seq_group,
                           std::string_view attr_name,
                           int size)
{
  FuncExpr *expr = nullptr;
  const std::string &attr = seq_group->findAttrString(attr_name);
  if (!attr.empty()) {
    expr = parseFunc(attr, attr_name, cell, seq_group->line());
    if (expr && expr->checkSize(size)) {
      warn(1196, seq_group, "{} {} bus width mismatch.",
              seq_group->type(), attr_name);
      delete expr;
      expr = nullptr;
    }
  }
  return expr;
}

void
LibertyReader::makeSeqPorts(LibertyCell *cell,
                            const LibertyGroup *seq_group,
                            // Return values.
                            LibertyPort *&out_port,
                            LibertyPort *&out_port_inv,
                            size_t &size)
{
  std::string out_name, out_inv_name;
  bool has_size;
  seqPortNames(seq_group, out_name, out_inv_name, has_size, size);
  if (!out_name.empty()) {
    if (has_size)
      out_port = makeBusPort(cell, out_name, size - 1, 0, nullptr);
    else
      out_port = makePort(cell, out_name);
    out_port->setDirection(PortDirection::internal());
  }
  if (!out_inv_name.empty()) {
    if (has_size)
      out_port_inv = makeBusPort(cell, out_inv_name, size - 1, 0, nullptr);
    else
      out_port_inv = makePort(cell, out_inv_name);
    out_port_inv->setDirection(PortDirection::internal());
  }
}

void
LibertyReader::seqPortNames(const LibertyGroup *group,
                            // Return values.
                            std::string &out_name,
                            std::string &out_inv_name,
                            bool &has_size,
                            size_t &size)
{
  if (group->params().size() == 1) {
    // out_port
    out_name = group->firstParam();
    size = 1;
    has_size = false;
  }
  if (group->params().size() == 2) {
    // out_port, out_port_inv
    out_name = group->firstParam();
    out_inv_name = group->secondParam();
    size = 1;
    has_size = false;
  }
  else if (group->params().size() == 3) {
    LibertyAttrValue *third_value = group->params()[2];
    auto [size_flt, size_valid] = third_value->floatValue();
    if (size_valid) {
      // out_port, out_port_inv, bus_size
      out_name = group->firstParam();
      out_inv_name = group->secondParam();
      size = static_cast<int>(size_flt);
      has_size = true;
    }
    else {
      // in_port (ignored), out_port, out_port_inv
      out_name = group->secondParam();
      out_inv_name = third_value->stringValue();
      has_size = true;
      size = 1;
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::readCellAttributes(LibertyCell *cell,
                                  const LibertyGroup *cell_group)
{
  readCellAttrFloat("area", &LibertyCell::setArea, cell, cell_group, 1.0);
  readCellAttrString("cell_footprint", &LibertyCell::setFootprint, cell, cell_group);
  readCellAttrBool("dont_use", &LibertyCell::setDontUse, cell, cell_group);
  readCellAttrBool("is_macro_cell", &LibertyCell::setIsMacro, cell, cell_group);
  readCellAttrBool("is_pad", &LibertyCell::setIsPad, cell, cell_group);
  readCellAttrBool("is_level_shifter", &LibertyCell::setIsLevelShifter, cell, cell_group);
  readCellAttrBool("is_clock_cell", &LibertyCell::setIsClockCell, cell, cell_group);
  readCellAttrBool("is_isolation_cell", &LibertyCell::setIsIsolationCell,cell,cell_group);
  readCellAttrBool("always_on", &LibertyCell::setAlwaysOn,cell,cell_group);
  readCellAttrBool("interface_timing", &LibertyCell::setInterfaceTiming,cell,cell_group);
  readCellAttrFloat("cell_leakage_power", &LibertyCell::setLeakagePower, cell,
                    cell_group, power_scale_);

  readCellAttrBool("is_memory", &LibertyCell::setIsMemory, cell, cell_group);
  if (cell_group->findSubgroup("memory"))
    cell->setIsMemory(true);

  readCellAttrBool("pad_cell", &LibertyCell::setIsPad, cell, cell_group);
  readLevelShifterType(cell, cell_group);
  readSwitchCellType(cell, cell_group);
  readCellAttrString("user_function_class", &LibertyCell::setUserFunctionClass,
                    cell, cell_group);

  readOcvDerateFactors(cell, cell_group);
  readCellOcvDerateGroup(cell, cell_group);
  readGroupAttrFloat("ocv_arc_depth", cell_group,
                    [cell](float v) { cell->setOcvArcDepth(v); });

  const std::string &clock_gate_type =
    cell_group->findAttrString("clock_gating_integrated_cell");
  if (!clock_gate_type.empty()) {
    if (stringBeginEqual(clock_gate_type, "latch_posedge"))
      cell->setClockGateType(ClockGateType::latch_posedge);
    else if (stringBeginEqual(clock_gate_type, "latch_negedge"))
      cell->setClockGateType(ClockGateType::latch_negedge);
    else
      cell->setClockGateType(ClockGateType::other);
  }

  readScaleFactors(cell, cell_group);
  readLeagageGrouops(cell, cell_group);
  readStatetable(cell, cell_group);
  readModeDefs(cell, cell_group);
}

void
LibertyReader::readScaleFactors(LibertyCell *cell,
                                const LibertyGroup *cell_group)
{
  const std::string &scale_factors_name =
    cell_group->findAttrString("scaling_factors");
  if (!scale_factors_name.empty()) {
    ScaleFactors *scale_factors =
      library_->findScaleFactors(scale_factors_name);
    if (scale_factors)
      cell->setScaleFactors(scale_factors);
    else
      warn(1230, cell_group, "scaling_factors {} not found.", scale_factors_name);
  }
}

void
LibertyReader::readCellAttrString(std::string_view attr_name,
                                  void (LibertyCell::*set_func)(std::string_view value),
                                  LibertyCell *cell,
                                  const LibertyGroup *group)
{
  const std::string &value = group->findAttrString(attr_name);
  if (!value.empty())
    (cell->*set_func)(value);
}

void
LibertyReader::readCellAttrFloat(std::string_view attr_name,
                                 void (LibertyCell::*set_func)(float value),
                                 LibertyCell *cell,
                                 const LibertyGroup *group,
                                 float scale)
{
  float value;
  bool exists;
  group->findAttrFloat(attr_name, value, exists);
  if (exists)
    (cell->*set_func)(value * scale);
}

void
LibertyReader::readCellAttrBool(std::string_view attr_name,
                                void (LibertyCell::*set_func)(bool value),
                                LibertyCell *cell,
                                const LibertyGroup *group)
{
  const LibertySimpleAttr *attr = group->findSimpleAttr(attr_name);
  if (attr) {
    const LibertyAttrValue &attr_value = attr->value();
    if (attr_value.isString()) {
      const std::string &value = attr_value.stringValue();
      if (stringEqual(value, "true"))
        (cell->*set_func)(true);
      else if (stringEqual(value, "false"))
        (cell->*set_func)(false);
      else
        warn(1279, attr, "{} attribute is not boolean.", attr_name);
    }
    else
      warn(1280, attr, "{} attribute is not boolean.", attr_name);
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::makeTimingArcs(LibertyCell *cell,
                              const LibertyPortSeq &ports,
                              const LibertyGroup *port_group)
{
  for (const LibertyGroup *timing_group : port_group->findSubgroups("timing")) {
    TimingArcAttrsPtr timing_attrs = std::make_shared<TimingArcAttrs>();
    readTimingArcAttrs(cell, timing_group, timing_attrs);
    makeTimingModels(cell, timing_group, timing_attrs);

    LibertyPort *related_output_port = findLibertyPort(cell, timing_group,
                                                       "related_output_pin");
    StringSeq related_port_names = findAttributStrings(timing_group, "related_pin");
    StringSeq related_bus_names=findAttributStrings(timing_group,"related_bus_pins");
    TimingType timing_type = timing_attrs->timingType();

    for (LibertyPort *to_port : ports) {
      if (timing_type == TimingType::combinational &&
          to_port->direction()->isInput())
        warn(1209, timing_group, "combinational timing to an input port.");

      if (related_port_names.size() || related_bus_names.size()) {
        for (const std::string &from_port_name : related_port_names) {
          debugPrint(debug_, "liberty", 2, "  timing {} -> {}",
                     from_port_name, to_port->name());
          makeTimingArcs(cell, from_port_name, to_port, related_output_port, true,
                         timing_attrs, timing_group->line());
        }
        for (const std::string &from_port_name : related_bus_names) {
          debugPrint(debug_, "liberty", 2, "  timing {} -> {}",
                     from_port_name, to_port->name());
          makeTimingArcs(cell, from_port_name, to_port, related_output_port, false,
                         timing_attrs, timing_group->line());
        }
      }
      else if (!(timing_type == TimingType::min_pulse_width
                 || timing_type == TimingType::minimum_period
                 || timing_type == TimingType::min_clock_tree_path
                 || timing_type == TimingType::max_clock_tree_path))
        warn(1243, timing_group, "timing group missing related_pin/related_bus_pin.");
      else
        makeTimingArcs(cell, to_port, related_output_port,
                       timing_attrs, timing_group->line());
    }
  }
}

void
LibertyReader::readTimingArcAttrs(LibertyCell *cell,
                                  const LibertyGroup *timing_group,
                                  TimingArcAttrsPtr timing_attrs)
{
  readTimingSense(timing_group, timing_attrs);
  readTimingType(timing_group, timing_attrs);
  readTimingWhen(cell, timing_group, timing_attrs);
  readTimingMode(timing_group, timing_attrs);
  readGroupAttrFloat("ocv_arc_depth", timing_group,
                    [timing_attrs](float v) { timing_attrs->setOcvArcDepth(v); });
}

void
LibertyReader::readGroupAttrFloat(std::string_view attr_name,
                                  const LibertyGroup *group,
                                  const std::function<void(float)> &set_func,
                                  float scale)
{
  float value;
  bool exists;
  group->findAttrFloat(attr_name, value, exists);
  if (exists)
    set_func(value * scale);
}

void
LibertyReader::readTimingSense(const LibertyGroup *timing_group,
                               TimingArcAttrsPtr timing_attrs)
{
  const LibertySimpleAttr *sense_attr = timing_group->findSimpleAttr("timing_sense");
  if (sense_attr) {
    const std::string &sense_name = sense_attr->stringValue();
    if (sense_name == "non_unate")
      timing_attrs->setTimingSense(TimingSense::non_unate);
    else if (sense_name == "positive_unate")
      timing_attrs->setTimingSense(TimingSense::positive_unate);
    else if (sense_name == "negative_unate")
      timing_attrs->setTimingSense(TimingSense::negative_unate);
    else
      warn(1245, timing_group, "unknown timing_sense {}.", sense_name);
  }
}

void
LibertyReader::readTimingType(const LibertyGroup *timing_group,
                              TimingArcAttrsPtr timing_attrs)
{
  TimingType type = TimingType::combinational;
  const LibertySimpleAttr *type_attr = timing_group->findSimpleAttr("timing_type");
  if (type_attr) {
    const std::string type_name = type_attr->stringValue();
    type = findTimingType(type_name);
    if (type == TimingType::unknown) {
      warn(1244, type_attr, "unknown timing_type {}.", type_name);
      type = TimingType::combinational;
    }
  }
  timing_attrs->setTimingType(type);
}

void
LibertyReader::readTimingWhen(const LibertyCell *cell,
                              const LibertyGroup *timing_group,
                              TimingArcAttrsPtr timing_attrs)
{
  const LibertySimpleAttr *when_attr = timing_group->findSimpleAttr("when");
  if (when_attr) {
    const std::string &when = when_attr->stringValue();
    if (!when.empty()) {
      FuncExpr *when_expr = parseFunc(when, "when", cell, when_attr->line());
      timing_attrs->setCond(when_expr);
    }
  }

  const LibertySimpleAttr *cond_attr = timing_group->findSimpleAttr("sdf_cond");
  if (cond_attr) {
    const std::string &cond = cond_attr->stringValue();
    timing_attrs->setSdfCond(cond);
  }
  cond_attr = timing_group->findSimpleAttr("sdf_cond_start");
  if (cond_attr) {
    const std::string &cond = cond_attr->stringValue();
    timing_attrs->setSdfCondStart(cond);
  }
  cond_attr = timing_group->findSimpleAttr("sdf_cond_end");
  if (cond_attr) {
    const std::string &cond = cond_attr->stringValue();
    timing_attrs->setSdfCondEnd(cond);
  }
}

void
LibertyReader::readTimingMode(const LibertyGroup *timing_group,
                              TimingArcAttrsPtr timing_attrs)
{
  const LibertyComplexAttrSeq &mode_attrs = timing_group->findComplexAttrs("mode");
  if (!mode_attrs.empty()) {
    const LibertyComplexAttr *mode_attr = mode_attrs[0];
    const LibertyAttrValueSeq &mode_values = mode_attr->values();
    if (mode_values.size() == 2) {
      LibertyAttrValue *value = mode_values[0];
      if (value->isString())
        timing_attrs->setModeName(value->stringValue());
      else
        warn(1248, mode_attr, "mode name is not a string.");

      value = mode_values[1];
      if (value->isString())
        timing_attrs->setModeValue(value->stringValue());
      else
        warn(1246, mode_attr, "mode value is not a string.");
    }
    else
      warn(1249, mode_attr, "mode requirees 2 values.");
  }
}

void
LibertyReader::makeTimingModels(LibertyCell *cell,
                                const LibertyGroup *timing_group,
                                TimingArcAttrsPtr timing_attrs)
{
   switch (cell->libertyLibrary()->delayModelType()) {
  case DelayModelType::cmos_linear:
    makeLinearModels(cell, timing_group, timing_attrs);
    break;
  case DelayModelType::table:
    makeTableModels(cell, timing_group, timing_attrs);
    break;
  case DelayModelType::cmos_pwl:
  case DelayModelType::cmos2:
  case DelayModelType::polynomial:
  case DelayModelType::dcm:
    break;
  }
}

void
LibertyReader::makeLinearModels(LibertyCell *cell,
                                const LibertyGroup *timing_group,
                                TimingArcAttrsPtr timing_attrs)
{
  LibertyLibrary *library = cell->libertyLibrary();
  for (const RiseFall *rf : RiseFall::range()) {
    std::string intr_attr_name = sta::format("intrinsic_{}", rf->to_string());
    float intr = 0.0;
    bool intr_exists;
    timing_group->findAttrFloat(intr_attr_name, intr, intr_exists);
    if (intr_exists)
      intr *= time_scale_;
    else
      library->defaultIntrinsic(rf, intr, intr_exists);
    TimingModel *model = nullptr;
    if (intr_exists) {
      if (timingTypeIsCheck(timing_attrs->timingType()))
        model = new CheckLinearModel(cell, intr);
      else {
        std::string res_attr_name = sta::format("{}_resistance", rf->to_string());
        float res = 0.0;
        bool res_exists;
        timing_group->findAttrFloat(res_attr_name, res, res_exists);
        if (res_exists)
          res *= res_scale_;
        else
          library->defaultPinResistance(rf, PortDirection::output(),
                                        res, res_exists);
        model = new GateLinearModel(cell, intr, res);
      }
      timing_attrs->setModel(rf, model);
    }
  }
}

void
LibertyReader::makeTableModels(LibertyCell *cell,
                               const LibertyGroup *timing_group,
                               TimingArcAttrsPtr timing_attrs)
{
  bool found_model = false;
  for (const RiseFall *rf : RiseFall::range()) {
    TableModel *delay_model = readTableModel(timing_group,
                                             sta::format("cell_{}", rf->to_string()),
                                             rf, TableTemplateType::delay,
                                             time_scale_,
                                             ScaleFactorType::cell,
                                             GateTableModel::checkAxes);
    TableModel *slew_model = readTableModel(timing_group,
                                            sta::format("{}_transition",
                                                        rf->to_string()),
                                            rf, TableTemplateType::delay,
                                            time_scale_,
                                            ScaleFactorType::transition,
                                            GateTableModel::checkAxes);
    if (delay_model || slew_model) {
      TableModels *delay_models = new TableModels(delay_model);
      readLvfModels(timing_group,
                    sta::format("ocv_sigma_cell_{}", rf->to_string()),
                    sta::format("ocv_std_dev_cell_{}", rf->to_string()),
                    sta::format("ocv_mean_shift_cell_{}", rf->to_string()),
                    sta::format("ocv_skewness_cell_{}", rf->to_string()),
                    rf, delay_models, GateTableModel::checkAxes);

      TableModels *slew_models = new TableModels(slew_model);
      readLvfModels(timing_group,
                    sta::format("ocv_sigma_{}_transition", rf->to_string()),
                    sta::format("ocv_std_dev_{}_transition", rf->to_string()),
                    sta::format("ocv_mean_shift_{}_transition", rf->to_string()),
                    sta::format("ocv_skewness_{}_transition", rf->to_string()),
                    rf, slew_models, GateTableModel::checkAxes);

      ReceiverModelPtr receiver_model = readReceiverCapacitance(timing_group, rf);
      OutputWaveforms *output_waveforms = readOutputWaveforms(timing_group, rf);

      timing_attrs->setModel(rf, new GateTableModel(cell, delay_models,
                                                    slew_models,
                                                    receiver_model,
                                                    output_waveforms));
      TimingType timing_type = timing_attrs->timingType();
      if (isGateTimingType(timing_type)) {
        if (slew_model == nullptr)
          warn(1210, timing_group, "missing {}_transition.", rf->name());
        if (delay_model == nullptr)
          warn(1211, timing_group, "missing cell_{}.", rf->name());
      }
      found_model = true;
    }

    std::string constraint_attr_name  = sta::format("{}_constraint", rf->to_string());
    ScaleFactorType scale_factor_type = 
      timingTypeScaleFactorType(timing_attrs->timingType());
    TableModel *check_model = readTableModel(timing_group,
                                             constraint_attr_name,
                                             rf, TableTemplateType::delay,
                                             time_scale_, scale_factor_type,
                                             CheckTableModel::checkAxes);
    if (check_model) {
      TableModels *check_models = new TableModels(check_model);
      readLvfModels(timing_group,
                    sta::format("ocv_sigma_{}_constraiint", rf->to_string()),
                    sta::format("ocv_std_dev_{}_constraiint", rf->to_string()),
                    sta::format("ocv_mean_shift_{}_constraiint", rf->to_string()),
                    sta::format("ocv_skewness_{}_constraiint", rf->to_string()),
                    rf, check_models, CheckTableModel::checkAxes);
      timing_attrs->setModel(rf, new CheckTableModel(cell, check_models));
      found_model = true;
    }
  }
  if (!found_model)
    warn(1311, timing_group, "no table models found in timing group.");
}

bool
LibertyReader::isGateTimingType(TimingType timing_type)
{
  return timing_type == TimingType::clear
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
    || timing_type == TimingType::three_state_enable_rise;
}

TableModel *
LibertyReader::readTableModel(const LibertyGroup *timing_group,
                              const std::string &table_group_name,
                              const RiseFall *rf,
                              TableTemplateType template_type,
                              float scale,
                              ScaleFactorType scale_factor_type,
                              const std::function<bool(TableModel *model)> check_axes)
{
  const LibertyGroup *table_group = timing_group->findSubgroup(table_group_name);
  if (table_group)
    return readTableModel(table_group, rf, template_type, scale,
                         scale_factor_type, check_axes);
  return nullptr;
}

void
LibertyReader::readLvfModels(const LibertyGroup *timing_group,
                             const std::string &sigma_group_name,
                             const std::string &std_dev_group_name,
                             const std::string &mean_shift_group_name,
                             const std::string &skewness_group_name,
                             const RiseFall *rf,
                             TableModels *table_models,
                             const std::function<bool(TableModel *model)> check_axes)
{
  TableModelsEarlyLate sigmas =
    readEarlyLateTableModels(timing_group,
                             sigma_group_name,
                             rf, TableTemplateType::delay,
                             time_scale_,
                             ScaleFactorType::unknown,
                             check_axes);
  for (const EarlyLate *early_late : EarlyLate::range())
    table_models->setSigma(sigmas[early_late->index()], early_late);

  const LibertyGroup *std_dev_group = timing_group->findSubgroup(std_dev_group_name);
  if (std_dev_group) {
    TableModel *std_dev = readTableModel(std_dev_group, rf, TableTemplateType::delay,
                                        time_scale_, ScaleFactorType::unknown,
                                        check_axes);
    table_models->setStdDev(std_dev);
  }

  const LibertyGroup *mean_shift_group=timing_group->findSubgroup(mean_shift_group_name);
  if (mean_shift_group) {
    TableModel *mean_shift = readTableModel(mean_shift_group, rf,
                                            TableTemplateType::delay,
                                            time_scale_, ScaleFactorType::unknown,
                                            check_axes);
    table_models->setMeanShift(mean_shift);
  }

  const LibertyGroup *skewness_group = timing_group->findSubgroup(skewness_group_name);
  if (skewness_group) {
    TableModel *skewness = readTableModel(skewness_group, rf,
                                          TableTemplateType::delay,
                                          1.0, ScaleFactorType::unknown,
                                          check_axes);
    table_models->setSkewness(skewness);
  }
}

TableModelsEarlyLate
LibertyReader::readEarlyLateTableModels(const LibertyGroup *timing_group,
                                        std::string_view table_group_name,
                                        const RiseFall *rf,
                                        TableTemplateType template_type,
                                        float scale,
                                        ScaleFactorType scale_factor_type,
                                        const std::function<bool(TableModel *model)> check_axes)
{
  TableModelsEarlyLate models{};
  for (const LibertyGroup *table_group : timing_group->findSubgroups(table_group_name)){
    TableModel *model = readTableModel(table_group, rf, template_type, scale,
                                       scale_factor_type, check_axes);
    const std::string &early_late = table_group->findAttrString("sigma_type");
    if (early_late.empty()
        || early_late == "early_and_late") {
      models[EarlyLate::early()->index()] = model;
      models[EarlyLate::late()->index()] = model;
    }
    else if (early_late == "early")
      models[EarlyLate::early()->index()] = model;
    else if (early_late == "late")
      models[EarlyLate::late()->index()] = model;
  }
  return models;
}

ReceiverModelPtr
LibertyReader::readReceiverCapacitance(const LibertyGroup *timing_group,
                                       const RiseFall *rf)
{
  ReceiverModelPtr receiver_model = nullptr;
  readReceiverCapacitance(timing_group, "receiver_capacitance", 0, rf,
                          receiver_model);
  readReceiverCapacitance(timing_group, "receiver_capacitance1", 0, rf,
                          receiver_model);
  readReceiverCapacitance(timing_group, "receiver_capacitance2", 1, rf,
                          receiver_model);
  return receiver_model;
}

void
LibertyReader::readReceiverCapacitance(const LibertyGroup *timing_group,
                                       std::string_view cap_group_name,
                                       int index,
                                       const RiseFall *rf,
                                       ReceiverModelPtr &receiver_model)
{
  std::string cap_group_name1 = sta::format("{}_{}", cap_group_name, rf->to_string());
  const LibertyGroup *cap_group = timing_group->findSubgroup(cap_group_name1);
  if (cap_group) {
    const LibertySimpleAttr *segment_attr = cap_group->findSimpleAttr("segment");
    if (segment_attr) {
      // For receiver_capacitance groups with mulitiple segments this
      // overrides the index passed in beginReceiverCapacitance1Rise/Fall.
      int segment;
      bool exists;
      getAttrInt(segment_attr, segment, exists);
      if (exists)
        index = segment;
    }
    TableModel *model = readTableModel(cap_group, rf, TableTemplateType::delay,
                                       cap_scale_, ScaleFactorType::pin_cap);
    if (ReceiverModel::checkAxes(model)) {
      if (receiver_model == nullptr)
        receiver_model = std::make_shared<ReceiverModel>();
      receiver_model->setCapacitanceModel(std::move(*model), index, rf);
    }
    else
      warn(1219, cap_group, "unsupported model axis.");
    delete model;
  }
}

OutputWaveforms *
LibertyReader::readOutputWaveforms(const LibertyGroup *timing_group,
                                   const RiseFall *rf)
{
  const std::string current_group_name=sta::format("output_current_{}",rf->to_string());
  const LibertyGroup *current_group = timing_group->findSubgroup(current_group_name);
  if (current_group) {
    OutputWaveformSeq output_currents;
    for (const LibertyGroup *vector_group : current_group->findSubgroups("vector")) {
      float ref_time;
      bool ref_time_exists;
      vector_group->findAttrFloat("reference_time", ref_time, ref_time_exists);
      if (ref_time_exists) {
        ref_time *= time_scale_;
        TableModel *table = readTableModel(vector_group, rf,
                                           TableTemplateType::output_current,
                                           current_scale_, ScaleFactorType::unknown);
        if (table) {
          TableTemplate *tbl_template = table->tblTemplate();
          const TableAxis *slew_axis, *cap_axis;
          // Canonicalize axis order.
          if (tbl_template->axis1()->variable()==TableAxisVariable::input_net_transition){
            slew_axis = table->axis1();
            cap_axis = table->axis2();
          }
          else {
            slew_axis = table->axis2();
            cap_axis = table->axis1();
          }

          if (slew_axis->size() == 1 && cap_axis->size() == 1) {
            // Convert 1x1xN Table (order 3) to 1D Table.
            float slew = slew_axis->axisValue(0);
            float cap = cap_axis->axisValue(0);
            TablePtr table_ptr = table->table();
            FloatTable *values3 = table_ptr->values3();
            FloatSeq row = std::move((*values3)[0]);
            values3->erase(values3->begin());
            Table *table1 = new Table(std::move(row), table->table()->axis3ptr());
            output_currents.emplace_back(slew, cap, table1, ref_time);
          }
          else
            warn(1223, vector_group,
                    "vector index_1 and index_2 must have exactly one value.");
        }
        delete table;
      }
      else
        warn(1224, vector_group, "vector reference_time not found.");
    }
    if (!output_currents.empty())
      return makeOutputWaveforms(current_group, output_currents, rf);
  }
  return nullptr;
}

OutputWaveforms *
LibertyReader::makeOutputWaveforms(const LibertyGroup *current_group,
                                   OutputWaveformSeq &output_currents,
                                   const RiseFall *rf)
{
  std::set<float> slew_set, cap_set;
  FloatSeq slew_values;
  FloatSeq cap_values;
  for (const OutputWaveform &waveform : output_currents) {
    float slew = waveform.slew();
    // Filter duplilcate slews and capacitances.
    if (!slew_set.contains(slew)) {
      slew_set.insert(slew);
      slew_values.push_back(slew);
    }
    float cap = waveform.cap();
    if (!cap_set.contains(cap)) {
      cap_set.insert(cap);
      cap_values.push_back(cap);
    }
  }
  sort(slew_values, std::less<float>());
  sort(cap_values, std::less<float>());
  size_t slew_size = slew_values.size();
  size_t cap_size = cap_values.size();
  TableAxisPtr slew_axis =
    make_shared<TableAxis>(TableAxisVariable::input_net_transition,
                           std::move(slew_values));
  TableAxisPtr cap_axis =
    make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance,
                           std::move(cap_values));
  FloatSeq ref_times(slew_size);
  Table1Seq current_waveforms(slew_size * cap_size);
  for (OutputWaveform &waveform : output_currents) {
    size_t slew_index, cap_index;
    bool slew_exists, cap_exists;
    slew_axis->findAxisIndex(waveform.slew(), slew_index, slew_exists);
    cap_axis->findAxisIndex(waveform.cap(), cap_index, cap_exists);
    if (slew_exists && cap_exists) {
      size_t index = slew_index * cap_axis->size() + cap_index;
      current_waveforms[index] = waveform.releaseCurrents();
      ref_times[slew_index] = waveform.referenceTime();
    }
    else
      warn(1221, current_group, "output current waveform {:.2e} {:.2e} not found.",
              waveform.slew(),
              waveform.cap());
  }
  Table ref_time_tbl(std::move(ref_times), slew_axis);
  OutputWaveforms *output_current = new OutputWaveforms(slew_axis, cap_axis, rf,
                                                        current_waveforms,
                                                        std::move(ref_time_tbl));
  return output_current;
}

TableModel *
LibertyReader::readTableModel(const LibertyGroup *table_group,
                              const RiseFall *rf,
                              TableTemplateType template_type,
                              float scale,
                              ScaleFactorType scale_factor_type,
                              const std::function<bool(TableModel *model)> &check_axes)
{
  if (library_ && table_group->hasFirstParam()) {
    const std::string &template_name = table_group->firstParam();
    TableTemplate *tbl_template = library_->findTableTemplate(template_name,
                                                              template_type);
    if (tbl_template) {
      TablePtr table = readTableModel(table_group, tbl_template, scale);
      if (table) {
        TableModel *table_model = new TableModel(table, tbl_template,
                                                 scale_factor_type, rf);
        if (!check_axes(table_model)) {
          warn(1251, table_group, "unsupported model axis.");
        }
        return table_model;
      }
    }
    else
      warn(1253, table_group, "table template {} not found.", template_name);
  }
  return nullptr;
}

TablePtr
LibertyReader::readTableModel(const LibertyGroup *table_group,
                              const TableTemplate *tbl_template,
                              float scale)
{
  const LibertyComplexAttr *values_attr = table_group->findComplexAttr("values");
  if (values_attr) {
    TableAxisPtr axis1 = makeTableAxis(table_group, "index_1", tbl_template->axis1ptr());
    TableAxisPtr axis2 = makeTableAxis(table_group, "index_2", tbl_template->axis2ptr());
    TableAxisPtr axis3 = makeTableAxis(table_group, "index_3", tbl_template->axis3ptr());
    if (axis1 && axis2 && axis3) {
      // 3D table
      FloatTable float_table = makeFloatTable(values_attr, table_group,
                                              axis1->size() * axis2->size(),
                                              axis3->size(), scale);
      return make_shared<Table>(std::move(float_table), axis1, axis2, axis3);
    }
    else if (axis1 && axis2) {
      FloatTable float_table = makeFloatTable(values_attr, table_group,
                                              axis1->size(), axis2->size(), scale);
      return make_shared<Table>(std::move(float_table), axis1, axis2);
    }
    else if (axis1) {
      FloatTable table = makeFloatTable(values_attr, table_group, 1,
                                        axis1->size(), scale);
      return make_shared<Table>(std::move(table[0]), axis1);
    }
    else if (axis1 == nullptr && axis2 == nullptr && axis3 == nullptr) {
      FloatTable table = makeFloatTable(values_attr, table_group, 1, 1, scale);
      float value = table[0][0];
      return std::make_shared<Table>(value);
    }
  }
  else
    warn(1257, table_group, "{} is missing values.", table_group->type());
  return nullptr;
}

TableAxisPtr
LibertyReader::makeTableAxis(const LibertyGroup *table_group,
                             std::string_view index_attr_name,
                             TableAxisPtr template_axis)
{
  const LibertyComplexAttr *index_attr = table_group->findComplexAttr(index_attr_name);
  if (index_attr) {
    FloatSeq axis_values = readFloatSeq(index_attr, 1.0F);
    if (axis_values.empty())
      warn(1177, index_attr, "missing table index values.");
    else {
      // Check monotonicity of the values.
      float prev = axis_values[0];
      for (size_t i = 1; i < axis_values.size(); i++) {
        float value = axis_values[i];
        if (value <= prev)
          warn(1173, index_attr, "non-increasing table index values.");
        prev = value;
      }

      TableAxisVariable axis_var = template_axis->variable();
      const Units *units = library_->units();
      float scale = tableVariableUnit(axis_var, units)->scale();
      scaleFloats(axis_values, scale);
      return make_shared<TableAxis>(axis_var, std::move(axis_values));
    }
  }
  return template_axis;
}

////////////////////////////////////////////////////////////////

void
LibertyReader::makeTimingArcs(LibertyCell *cell,
                              const std::string &from_port_name,
                              LibertyPort *to_port,
                              LibertyPort *related_out_port,
                              bool one_to_one,
                              TimingArcAttrsPtr timing_attrs,
                              int timing_line)
{
  PortNameBitIterator from_port_iter(cell, from_port_name, this, timing_line);
  if (from_port_iter.size() == 1 && !to_port->hasMembers()) {
    // one -> one
    if (from_port_iter.hasNext()) {
      LibertyPort *from_port = from_port_iter.next();
      if (from_port->direction()->isOutput())
        warn(1212, timing_line, "timing group from output port.");
      builder_.makeTimingArcs(cell, from_port, to_port, related_out_port,
                              timing_attrs, timing_line);
    }
  }
  else if (from_port_iter.size() > 1 && !to_port->hasMembers()) {
    // bus -> one
    while (from_port_iter.hasNext()) {
      LibertyPort *from_port = from_port_iter.next();
      if (from_port->direction()->isOutput())
        warn(1213, timing_line, "timing group from output port.");
      builder_.makeTimingArcs(cell, from_port, to_port, related_out_port,
                              timing_attrs, timing_line);
    }
  }
  else if (from_port_iter.size() == 1 && to_port->hasMembers()) {
    // one -> bus
    if (from_port_iter.hasNext()) {
      LibertyPort *from_port = from_port_iter.next();
      if (from_port->direction()->isOutput())
        warn(1214, timing_line, "timing group from output port.");
      LibertyPortMemberIterator bit_iter(to_port);
      while (bit_iter.hasNext()) {
        LibertyPort *to_port_bit = bit_iter.next();
        builder_.makeTimingArcs(cell, from_port, to_port_bit, related_out_port,
                                timing_attrs, timing_line);
      }
    }
  }
  else {
    // bus -> bus
    if (one_to_one) {
      int from_size = from_port_iter.size();
      int to_size = to_port->size();
      LibertyPortMemberIterator to_port_iter(to_port);
      // warn about different sizes
      if (from_size != to_size)
        warn(1216, timing_line,
                "timing port {} and related port {} are different sizes.",
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
          warn(1215, timing_line, "timing group from output port.");
        builder_.makeTimingArcs(cell, from_port_bit, to_port_bit,
                                related_out_port, timing_attrs,
                                timing_line);
      }
    }
    else {
      // cross product
      while (from_port_iter.hasNext()) {
        LibertyPort *from_port_bit = from_port_iter.next();
        LibertyPortMemberIterator to_port_iter(to_port);
        while (to_port_iter.hasNext()) {
          LibertyPort *to_port_bit = to_port_iter.next();
          builder_.makeTimingArcs(cell, from_port_bit, to_port_bit,
                                  related_out_port, timing_attrs,
                                  timing_line);
        }
      }
    }
  }
}

void
LibertyReader::makeTimingArcs(LibertyCell *cell,
                              LibertyPort *to_port,
                              LibertyPort *related_out_port,
                              TimingArcAttrsPtr timing_attrs,
                              int timing_line)
{
  if (to_port->hasMembers()) {
    LibertyPortMemberIterator bit_iter(to_port);
    while (bit_iter.hasNext()) {
      LibertyPort *to_port_bit = bit_iter.next();
      builder_.makeTimingArcs(cell, nullptr, to_port_bit,
                              related_out_port, timing_attrs,
                              timing_line);
    }
  }
  else
    builder_.makeTimingArcs(cell, nullptr, to_port,
                            related_out_port, timing_attrs,
                            timing_line);
}

////////////////////////////////////////////////////////////////

void
LibertyReader::readLeagageGrouops(LibertyCell *cell,
                                  const LibertyGroup *cell_group)
{
  for (const LibertyGroup *leak_group : cell_group->findSubgroups("leakage_power")) {
    FuncExpr *when = readFuncExpr(cell, leak_group, "when");
    float power;
    bool exists;
    leak_group->findAttrFloat("value", power, exists);
    if (exists) {
      LibertyPort *related_pg_port = findLibertyPort(cell, leak_group, "related_pg_pin");
      cell->makeLeakagePower(related_pg_port, when, power * power_scale_);
    }
    else
      warn(1307, leak_group, "leakage_power missing value.");
  }
}

void
LibertyReader::readInternalPowerGroups(LibertyCell *cell,
                                       const LibertyPortSeq &ports,
                                       const LibertyGroup *port_group)
{
  for (LibertyPort *port : ports) {
    for (const LibertyGroup *ipwr_group : port_group->findSubgroups("internal_power")) {
      LibertyPortSeq related_ports = findLibertyPorts(cell, ipwr_group, "related_pin");
      LibertyPort *related_pg_port = findLibertyPort(cell, ipwr_group, "related_pg_pin");
      std::shared_ptr<FuncExpr> when;
      FuncExpr *when1 = readFuncExpr(cell, ipwr_group, "when");
      if (when1)
        when = std::shared_ptr<FuncExpr>(when1);
      InternalPowerModels models{};
      // rise/fall_power group
      for (const RiseFall *rf : RiseFall::range()) {
        std::string pwr_attr_name = sta::format("{}_power", rf->to_string());
        const LibertyGroup *pwr_group = ipwr_group->findSubgroup(pwr_attr_name);
        if (pwr_group) {
          TableModel *model = readTableModel(pwr_group, rf, TableTemplateType::power,
                                             energyScale(),
                                             ScaleFactorType::internal_power);
          std::shared_ptr<TableModel> table_model(model);
          InternalPowerModel pwr_model(table_model);
          models[rf->index()] = pwr_model;
        }
      }
      // power group (rise/fall power are the same)
      const LibertyGroup *pwr_group = ipwr_group->findSubgroup("power");
      if (pwr_group) {
        TableModel *model = readTableModel(pwr_group, RiseFall::rise(),
                                           TableTemplateType::power,
                                           energyScale(),
                                           ScaleFactorType::internal_power);
        std::shared_ptr<TableModel> table_model(model);
        for (const RiseFall *rf : RiseFall::range()) {
          InternalPowerModel pwr_model(table_model);
          models[rf->index()] = pwr_model;
        }
      }
      if (related_ports.empty())
        cell->makeInternalPower(port, nullptr, related_pg_port, when, models);
      else {
        for (LibertyPort *related_port : related_ports)
          cell->makeInternalPower(port, related_port, related_pg_port, when, models);
      }
    }
  }
}

////////////////////////////////////////////////////////////////

FuncExpr *
LibertyReader::readFuncExpr(LibertyCell *cell,
                            const LibertyGroup *group,
                            std::string_view attr_name)
{
  const std::string &attr = group->findAttrString(attr_name);
  if (!attr.empty())
    return parseFunc(attr, attr_name, cell, group->line());
  else
    return nullptr;
}

LibertyPort *
LibertyReader::findLibertyPort(LibertyCell *cell,
                               const LibertyGroup *group,
                               std::string_view port_name_attr)
{
  const LibertySimpleAttr *attr = group->findSimpleAttr(port_name_attr);
  if (attr) {
    const std::string &port_name = attr->stringValue();
    LibertyPort *port = cell->findLibertyPort(port_name);
    if (port)
      return port;
    else
      warn(1290, attr, "port {} not found.", port_name);
  }
  return nullptr;
}

StringSeq
LibertyReader::findAttributStrings(const LibertyGroup *group,
                                   std::string_view name_attr)
{
  const LibertySimpleAttr *attr = group->findSimpleAttr(name_attr);
  if (attr) {
    const std::string &strings = attr->stringValue();
    return parseTokens(strings);
  }
  return StringSeq();
}

LibertyPortSeq
LibertyReader::findLibertyPorts(LibertyCell *cell,
                                const LibertyGroup *group,
                                std::string_view port_name_attr)
{
  LibertyPortSeq ports;
  StringSeq port_names = findAttributStrings(group, port_name_attr);
  for (const std::string &port_name : port_names) {
    LibertyPort *port = findPort(cell, port_name);
    if (port)
      ports.push_back(port);
    else
      warn(1306, group, "port {} not found.", port_name);
  }
  return ports;
}

////////////////////////////////////////////////////////////////

TimingModel *
LibertyReader::makeScalarCheckModel(LibertyCell *cell,
                                    float value,
                                    ScaleFactorType scale_factor_type,
                                    const RiseFall *rf)
{
  TablePtr table = std::make_shared<Table>(value);
  TableTemplate *tbl_template =
    library_->findTableTemplate("scalar", TableTemplateType::delay);
  TableModel *table_model = new TableModel(table, tbl_template,
                                           scale_factor_type, rf);

  TableModels *check_models = new TableModels(table_model);
  TimingModel *check_model = new CheckTableModel(cell, check_models);
  return check_model;
}

void
LibertyReader::checkLatchEnableSense(FuncExpr *enable_func,
                                     int line)
{
  LibertyPortSet enable_ports = enable_func->ports();
  for (LibertyPort *enable_port : enable_ports) {
    TimingSense enable_sense = enable_func->portTimingSense(enable_port);
    switch (enable_sense) {
    case TimingSense::positive_unate:
    case TimingSense::negative_unate:
      break;
    case TimingSense::non_unate:
      warn(1200, line, "latch enable function is non-unate for port {}.",
              enable_port->name());
      break;
    case TimingSense::none:
    case TimingSense::unknown:
      warn(1201, line, "latch enable function is unknown for port {}.",
              enable_port->name());
      break;
    }
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::readNormalizedDriverWaveform(const LibertyGroup *library_group)
{
  for (const LibertyGroup *waveform_group :
       library_group->findSubgroups("normalized_driver_waveform")) {
    if (waveform_group->hasFirstParam()) {
      const std::string &template_name = waveform_group->firstParam();
      TableTemplate *tbl_template = library_->findTableTemplate(template_name,
                                                                TableTemplateType::delay);
      if (!tbl_template) {
        warn(1256, waveform_group, "table template {} not found.", template_name);
        continue;
      }
      TablePtr table = readTableModel(waveform_group, tbl_template, time_scale_);
      if (!table)
        continue;
      if (table->axis1()->variable() != TableAxisVariable::input_net_transition) {
        warn(1265, waveform_group,
                "normalized_driver_waveform variable_1 must be input_net_transition");
        continue;
      }
      if (table->axis2()->variable() != TableAxisVariable::normalized_voltage) {
        warn(1225, waveform_group,
                "normalized_driver_waveform variable_2 must be normalized_voltage");
        continue;
      }
      std::string driver_waveform_name;
      const std::string &name_attr =
        waveform_group->findAttrString("driver_waveform_name");
      if (!name_attr.empty())
        driver_waveform_name = name_attr;
      library_->makeDriverWaveform(driver_waveform_name, table);
    }
    else
      warn(1227, waveform_group, "normalized_driver_waveform missing template.");
  }
}

////////////////////////////////////////////////////////////////

void
LibertyReader::readLevelShifterType(LibertyCell *cell,
                                    const LibertyGroup *cell_group)
{
  const std::string &level_shifter_type =
    cell_group->findAttrString("level_shifter_type");
  if (!level_shifter_type.empty()) {
    if (level_shifter_type == "HL")
      cell->setLevelShifterType(LevelShifterType::HL);
    else if (level_shifter_type == "LH")
      cell->setLevelShifterType(LevelShifterType::LH);
    else if (level_shifter_type == "HL_LH")
      cell->setLevelShifterType(LevelShifterType::HL_LH);
    else
      warn(1228, cell_group, "level_shifter_type must be HL, LH, or HL_LH");
  }
}

void
LibertyReader::readSwitchCellType(LibertyCell *cell,
                                  const LibertyGroup *cell_group)
{
  const std::string &switch_cell_type =
    cell_group->findAttrString("switch_cell_type");
  if (!switch_cell_type.empty()) {
    if (switch_cell_type == "coarse_grain")
      cell->setSwitchCellType(SwitchCellType::coarse_grain);
    else if (switch_cell_type == "fine_grain")
      cell->setSwitchCellType(SwitchCellType::fine_grain);
    else
      warn(1229, cell_group, "switch_cell_type must be coarse_grain or fine_grain");
  }
}

void
LibertyReader::readCellOcvDerateGroup(LibertyCell *cell,
                                      const LibertyGroup *cell_group)
{
  const std::string &derate_name = cell_group->findAttrString("ocv_derate_group");
  if (!derate_name.empty()) {
    OcvDerate *derate = cell->findOcvDerate(derate_name);
    if (derate == nullptr)
      derate = library_->findOcvDerate(derate_name);
    if (derate)
      cell->setOcvDerate(derate);
    else
      warn(1237, cell_group, "OCV derate group named {} not found.",
              derate_name);
  }
}

void
LibertyReader::readStatetable(LibertyCell *cell,
                              const LibertyGroup *cell_group)
{
  for (const LibertyGroup *statetable_group : cell_group->findSubgroups("statetable")) {
    StringSeq input_ports;
    if (statetable_group->hasFirstParam()) {
      const std::string &input_ports_arg = statetable_group->firstParam();
      input_ports = parseTokens(input_ports_arg);
    }

    StringSeq internal_ports;
    if (statetable_group->hasSecondParam()) {
      const std::string &internal_ports_arg = statetable_group->secondParam();
      internal_ports = parseTokens(internal_ports_arg);
    }

    const LibertySimpleAttr *table_attr = statetable_group->findSimpleAttr("table");
    if (table_attr) {
      const std::string &table_str = table_attr->stringValue();
      StringSeq table_rows = parseTokens(table_str, ",");
      size_t input_count = input_ports.size();
      size_t internal_count = internal_ports.size();
      StatetableRows table;
      for (const std::string &row : table_rows) {
        const StringSeq row_groups = parseTokens(row, ":");
        if (row_groups.size() != 3) {
          warn(1300, table_attr, "table row must have 3 groups separated by ':'.");
          break;
        }
        StringSeq inputs = parseTokens(row_groups[0]);
        if (inputs.size() != input_count) {
          warn(1301, table_attr, "table row has {} input values but {} are required.",
                  inputs.size(), input_count);
          break;
        }
        StringSeq currents = parseTokens(row_groups[1]);
        if (currents.size() != internal_count) {
          warn(1302,table_attr,
                  "table row has {} current values but {} are required.",
                  currents.size(), internal_count);
          break;
        }
        StringSeq nexts = parseTokens(row_groups[2]);
        if (nexts.size() != internal_count) {
          warn(1303, table_attr, "table row has {} next values but {} are required.",
                  nexts.size(), internal_count);
          break;
        }

        StateInputValues input_values = parseStateInputValues(inputs, table_attr);
        StateInternalValues current_values=parseStateInternalValues(currents,table_attr);
        StateInternalValues next_values = parseStateInternalValues(nexts, table_attr);
        table.emplace_back(input_values, current_values, next_values);
      }

      LibertyPortSeq input_port_ptrs;
      for (const std::string &input : input_ports) {
        LibertyPort *port = cell->findLibertyPort(input);
        if (port == nullptr
            && cell->testCell())
          port = cell->testCell()->findLibertyPort(input);
        if (port)
          input_port_ptrs.push_back(port);
        else
          warn(1298, statetable_group, "statetable input port {} not found.",
                  input);
      }
      LibertyPortSeq internal_port_ptrs;
      for (const std::string &internal : internal_ports) {
        LibertyPort *port = cell->findLibertyPort(internal);
        if (port == nullptr)
          port = makePort(cell, internal);
        internal_port_ptrs.push_back(port);
      }
      cell->makeStatetable(input_port_ptrs, internal_port_ptrs, table);
    }
  }
}

void
LibertyReader::readTestCell(LibertyCell *cell,
                            const LibertyGroup *cell_group)
{
  const LibertyGroup *test_cell_group = cell_group->findSubgroup("test_cell");
  if (test_cell_group) {
    if (cell->testCell())
      warn(1262, test_cell_group, "cell {} test_cell redefinition.", cell->name());
    else {
      std::string test_cell_name = std::string(cell->name()) + "/test_cell";
      TestCell *test_cell = new TestCell(cell->libertyLibrary(),
                                         std::move(test_cell_name),
                                         cell->filename());
      cell->setTestCell(test_cell);
      readCell(test_cell, test_cell_group);
    }
  }
}

////////////////////////////////////////////////////////////////

LibertyPort *
LibertyReader::makePort(LibertyCell *cell,
                        std::string_view port_name)
{
  std::string sta_name = portLibertyToSta(port_name);
  return builder_.makePort(cell, sta_name);
}

LibertyPort *
LibertyReader::makeBusPort(LibertyCell *cell,
                           std::string_view bus_name,
                           int from_index,
                           int to_index,
                           BusDcl *bus_dcl)
{
  std::string sta_name = portLibertyToSta(bus_name);
  return builder_.makeBusPort(cell, sta_name, from_index, to_index, bus_dcl);
}

// Also used by LibExprReader::makeFuncExprPort.
LibertyPort *
libertyReaderFindPort(const LibertyCell *cell,
                      std::string_view port_name)
{
  LibertyPort *port = cell->findLibertyPort(port_name);
  if (port == nullptr) {
    const LibertyLibrary *library = cell->libertyLibrary();
    char brkt_left = library->busBrktLeft();
    char brkt_right = library->busBrktRight();
    const char escape = '\\';
    // Pins at top level with bus names have escaped brackets.
    std::string escaped_port_name = escapeChars(port_name, brkt_left, brkt_right, escape);
    port = cell->findLibertyPort(escaped_port_name);
  }
  return port;
}

LibertyPort *
LibertyReader::findPort(LibertyCell *cell,
                        std::string_view port_name)
{
  return libertyReaderFindPort(cell, port_name);
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

////////////////////////////////////////////////////////////////

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
LibertyReader::parseStateInputValues(StringSeq &inputs,
                                     const LibertySimpleAttr *attr)
{
  StateInputValues input_values;
  for (std::string input : inputs) {
    bool exists;
    StateInputValue value;
    state_input_value_name_map.find(input, value, exists);
    if (!exists) {
      warn(1304, attr, "table input value '{}' not recognized.", input);
      value = StateInputValue::dont_care;
    }
    input_values.push_back(value);
  }
  return input_values;
}

StateInternalValues
LibertyReader::parseStateInternalValues(StringSeq &states,
                                        const LibertySimpleAttr *attr)
{
  StateInternalValues state_values;
  for (std::string state : states) {
    bool exists;
    StateInternalValue value;
    state_internal_value_name_map.find(state, value, exists);
    if (!exists) {
      warn(1305, attr, "table internal value '{}' not recognized.", state);
      value = StateInternalValue::unknown;
    }
    state_values.push_back(value);
  }
  return state_values;
}

////////////////////////////////////////////////////////////////

FloatTable
LibertyReader::makeFloatTable(const LibertyComplexAttr *values_attr,
                              const LibertyGroup *table_group,
                              size_t rows,
                              size_t cols,
                              float scale)
{
  FloatTable table;
  table.reserve(rows);
  for (const LibertyAttrValue *value : values_attr->values()) {
    FloatSeq row;
    row.reserve(cols);
    if (value->isString())
      row = parseFloatList(value->stringValue(), scale, values_attr->line());
    else if (value->isFloat()) {
      auto [entry, valid] = value->floatValue();
      row.push_back(entry * scale);
    }
    else
      warn(1258, values_attr, "{} is not a list of floats.",
           values_attr->name());
    if (row.size() != cols) {
      warn(1259, values_attr, "{} row has {} columns but axis has {}.",
           table_group->type(),
           row.size(),
           cols);
      for (size_t c = row.size(); c < cols; c++)
        row.push_back(0.0);
    }
    table.push_back(std::move(row));
  }
  if (table.size() != rows) {
    if (rows == 0)
      warn(1260, values_attr, "{} missing axis values.",
           table_group->type());
    else
      warn(1261, values_attr, "{} has {} rows but axis has {}.",
           table_group->type(),
           table.size(),
           rows);
    for (size_t r = table.size(); r < rows; r++) {
      FloatSeq row(cols, 0.0);
      table.push_back(std::move(row));
    }
  }
  return table;
}

////////////////////////////////////////////////////////////////

void
LibertyReader::getAttrInt(const LibertySimpleAttr *attr,
                          // Return values.
                          int &value,
                          bool &exists)
{
  value = 0;
  exists = false;
  const LibertyAttrValue &attr_value = attr->value();
  if (attr_value.isFloat()) {
    auto [float_val, valid] = attr_value.floatValue();
    value = static_cast<int>(float_val);
    exists = true;
  }
  else
    warn(1268, attr, "{} attribute is not an integer.", attr->name());
}

// Get two floats in a complex attribute.
//  attr(float1, float2);
void
LibertyReader::getAttrFloat2(const LibertyComplexAttr *attr,
                             // Return values.
                             float &value1,
                             float &value2,
                             bool &exists)
{
  exists = false;
  const LibertyAttrValueSeq &values = attr->values();
  if (values.size() == 2) {
    LibertyAttrValue *value = values[0];
    getAttrFloat(attr, value, value1, exists);
    if (!exists)
      warn(1272, attr, "{} is not a float.", attr->name());

    value = values[1];
    getAttrFloat(attr, value, value2, exists);
    if (!exists)
      warn(1273, attr, "{} is not a float.", attr->name());
  }
  else
    warn(1274, attr, "{} requires 2 valules.", attr->name());
}

void
LibertyReader::getAttrFloat(const LibertyComplexAttr *attr,
                            const LibertyAttrValue *attr_value,
                            // Return values.
                            float &value,
                            bool &valid)
{
  if (attr_value->isFloat()) {
    auto [value1, valid1] = attr_value->floatValue();
    value = value1;
    valid = valid1;
  }
  else if (attr_value->isString()) {
    const std::string &str = attr_value->stringValue();
    variableValue(str, value, valid);
    if (!valid) {
      auto [value1, valid1] = stringFloat(str);
      value = value1;
      valid = valid1;
      if (!valid1)
        warn(1183, attr->line(), "{} value {} is not a float.", attr->name(), str);
    }
  }
}

// Parse string of comma separated floats.
// Note that some brain damaged vendors (that used to "Think") are not
// consistent about including the delimiters.
FloatSeq
LibertyReader::parseFloatList(const std::string &float_list,
                              float scale,
                              int line)
{
  StringSeq tokens = parseTokens(float_list, " ,{}");
  FloatSeq values;
  values.reserve(tokens.size());
  for (const std::string &token : tokens) {
    auto [value, valid] = stringFloat(token);
    if (!valid)
      warn(1310, line, "{} is not a float.", token);
    else
      values.push_back(value * scale);
  }
  return values;
}

FloatSeq
LibertyReader::readFloatSeq(const LibertyComplexAttr *attr,
                            float scale)
{
  FloatSeq values;
  const LibertyAttrValueSeq &attr_values = attr->values();
  if (attr_values.size() == 1) {
    LibertyAttrValue *value = attr_values[0];
    if (value->isString())
      values = parseFloatList(value->stringValue(), scale, attr->line());
    else {
      auto [entry, valid] = value->floatValue();
      if (valid)
        values.push_back(entry * scale);
    }
  }
  else if (attr_values.size() > 1) {
    for (LibertyAttrValue *value : attr_values) {
      if (value->isString()) {
        FloatSeq parsed = parseFloatList(value->stringValue(), scale, attr->line());
        values.insert(values.end(), parsed.begin(), parsed.end());
      }
      else {
        auto [entry, valid] = value->floatValue();
        if (valid)
          values.push_back(entry * scale);
      }
    }
  }
  else
    warn(1277, attr, "{} has no values.", attr->name());
  return values;
}

////////////////////////////////////////////////////////////////

void
LibertyReader::getAttrBool(const LibertySimpleAttr *attr,
                           // Return values.
                           bool &value,
                           bool &exists)
{
  exists = false;
  const LibertyAttrValue &val = attr->value();
  if (val.isString()) {
    const std::string &str = val.stringValue();
    if (stringEqual(str, "true")) {
      value = true;
      exists = true;
    }
    else if (stringEqual(str, "false")) {
      value = false;
      exists = true;
    }
    else
      warn(1288, attr, "{} attribute is not boolean.", attr->name());
  }
  else
    warn(1289, attr, "{} attribute is not boolean.", attr->name());
}

// Read L/H/X string attribute values as bool.
LogicValue
LibertyReader::getAttrLogicValue(const LibertySimpleAttr *attr)
{
  const std::string &str = attr->stringValue();
  if (str == "L")
    return LogicValue::zero;
  else if (str == "H")
    return LogicValue::one;
  else if (str == "X")
    return LogicValue::unknown;
  else
    warn(1282, attr, "attribute {} value {} not recognized.", attr->name(), str);
  // fall thru
  return LogicValue::unknown;
}

const EarlyLateAll *
LibertyReader::getAttrEarlyLate(const LibertySimpleAttr *attr)
{
  const std::string &value = attr->stringValue();
  if (value == "early")
    return EarlyLateAll::early();
  else if (value == "late")
    return EarlyLateAll::late();
  else if (value == "early_and_late")
    return EarlyLateAll::all();
  else {
    warn(1283, attr, "unknown early/late value.");
    return EarlyLateAll::all();
  }
}

////////////////////////////////////////////////////////////////

FuncExpr *
LibertyReader::parseFunc(std::string_view func,
                         std::string_view attr_name,
                         const LibertyCell *cell,
                         int line)
{
  std::string error_msg = sta::format("{} line {}, {}",
                                      filename_,
                                      line,
                                      attr_name);
  return parseFuncExpr(func, cell, error_msg, report_);
}

////////////////////////////////////////////////////////////////

void
LibertyReader::visitVariable(LibertyVariable *var)
{
  const std::string &var_name = var->variable();
  float value;
  bool exists;
  findKeyValue(var_map_, var_name, value, exists);
  var_map_[var_name] = var->value();
}

void
LibertyReader::variableValue(std::string_view var,
                             float &value,
                             bool &exists)
{
  findKeyValue(var_map_, std::string(var), value, exists);
}

////////////////////////////////////////////////////////////////

void
LibertyReader::readDefaultOcvDerateGroup(const LibertyGroup *library_group)
{
  const std::string &derate_name =
    library_group->findAttrString("default_ocv_derate_group");
  if (!derate_name.empty()) {
    OcvDerate *derate = library_->findOcvDerate(derate_name);
    if (derate)
      library_->setDefaultOcvDerate(derate);
    else
      warn(1284, library_group, "OCV derate group named {} not found.", derate_name);
  }
}

// Read cell or library level ocv_derate groups.
void
LibertyReader::readOcvDerateFactors(LibertyCell *cell,
                                    const LibertyGroup *parent_group)
{
  for (const LibertyGroup *ocv_derate_group :
         parent_group->findSubgroups("ocv_derate")) {
    if (ocv_derate_group->hasFirstParam()) {
      const std::string &name = ocv_derate_group->firstParam();
      OcvDerate *ocv_derate = cell
        ? cell->makeOcvDerate(name)
        : library_->makeOcvDerate(name);
      for (const LibertyGroup *factors_group :
             ocv_derate_group->findSubgroups("ocv_derate_factors")) {
        const RiseFallBoth *rf_type = RiseFallBoth::riseFall();
        const std::string &rf_attr = factors_group->findAttrString("rf_type");
        if (!rf_attr.empty()) {
          if (rf_attr == "rise")
            rf_type = RiseFallBoth::rise();
          else if (rf_attr == "fall")
            rf_type = RiseFallBoth::fall();
          else if (rf_attr == "rise_and_fall")
            rf_type = RiseFallBoth::riseFall();
          else
            error(1286, factors_group, "unknown rise/fall.");
        }

        const EarlyLateAll *derate_type = EarlyLateAll::all();
        const std::string &derate_attr =
          factors_group->findAttrString("derate_type");
        if (!derate_attr.empty()) {
          if (derate_attr == "early")
            derate_type = EarlyLateAll::early();
          else if (derate_attr == "late")
            derate_type = EarlyLateAll::late();
          else if (derate_attr == "early_and_late")
            derate_type = EarlyLateAll::all();
          else {
            warn(1309, factors_group, "unknown early/late value.");
          }
        }

        PathType path_type = PathType::clk_and_data;
        const std::string &path_attr = factors_group->findAttrString("path_type");
        if (!path_attr.empty()) {
          if (path_attr == "clock")
            path_type = PathType::clk;
          else if (path_attr == "data")
            path_type = PathType::data;
          else if (path_attr == "clock_and_data")
            path_type = PathType::clk_and_data;
          else
            warn(1287, factors_group, "unknown derate type.");
        }

        if (factors_group->hasFirstParam()) {
          const std::string &template_name = factors_group->firstParam();
          TableTemplate *tbl_template =
            library_->findTableTemplate(template_name, TableTemplateType::ocv);
          if (tbl_template) {
            TablePtr table = readTableModel(factors_group, tbl_template, 1.0F);
            if (table) {
              for (const EarlyLate *early_late : derate_type->range()) {
                for (const RiseFall *rf : rf_type->range()) {
                  if (path_type == PathType::clk_and_data) {
                    ocv_derate->setDerateTable(rf, early_late, PathType::clk, table);
                    ocv_derate->setDerateTable(rf, early_late, PathType::data, table);
                  }
                  else
                    ocv_derate->setDerateTable(rf, early_late, path_type, table);
                }
              }
            }
          }
          else
            warn(1308, factors_group, "table template {} not found.", template_name);
        }
        else
          warn(1312, factors_group, "ocv_derate_factors missing template.");
      }
    }
    else
      warn(1285, ocv_derate_group, "ocv_derate missing name.");
  }
}

////////////////////////////////////////////////////////////////

PortNameBitIterator::PortNameBitIterator(LibertyCell *cell,
                                         std::string_view port_name,
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
PortNameBitIterator::init(std::string_view port_name)
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
    bool is_bus, is_range, subscript_wild;
    std::string bus_name;
    int from, to;
    parseBusName(port_name, library->busBrktLeft(),
                 library->busBrktRight(), '\\',
                 is_bus, is_range, bus_name, from, to, subscript_wild);
    if (is_range) {
      port = visitor_->findPort(cell_, port_name);
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
            visitor_->warn(1292, line_, "port {} subscript out of range.", port_name);
        }
        else
          visitor_->warn(1293, line_, "port range {} of non-bus port {}.",
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
      size_ = std::abs(from - to) + 1;
    }
    else
      visitor_->warn(1294, line_, "port {} not found.", port_name);
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
    std::string bus_bit_name = range_bus_name_ + library->busBrktLeft()
      + std::to_string(range_bit_) + library->busBrktRight();
    range_name_next_ = visitor_->findPort(cell_, bus_bit_name);
    if (range_name_next_) {
      if (range_from_ > range_to_)
        range_bit_--;
      else
        range_bit_++;
    }
    else
      visitor_->warn(1295, line_, "port {} not found.", bus_bit_name);
  }
  else
    range_name_next_ = nullptr;
}

////////////////////////////////////////////////////////////////

OutputWaveform::OutputWaveform(float slew,
                               float cap,
                               Table *currents,
                               float reference_time) :
  slew_(slew),
  cap_(cap),
  currents_(currents),
  reference_time_(reference_time)
{
}

Table *
OutputWaveform::releaseCurrents()
{
  return currents_.release();
}

} // namespace
