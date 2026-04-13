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

#include "LibertyWriter.hh"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>

#include "Error.hh"
#include "Format.hh"
#include "LibertyClass.hh"
#include "MinMax.hh"
#include "Transition.hh"
#include "Units.hh"
#include "FuncExpr.hh"
#include "PortDirection.hh"
#include "Liberty.hh"
#include "TimingRole.hh"
#include "TimingArc.hh"
#include "TimingModel.hh"
#include "TableModel.hh"
#include "StaState.hh"

namespace sta {

class LibertyWriter
{
public:
  LibertyWriter(const LibertyLibrary *lib,
                const char *filename,
                std::ofstream &stream,
                Report *report);
  void writeLibrary();

protected:
  void writeHeader();
  void writeFooter();
  void writeTableTemplates();
  void writeTableTemplate(const TableTemplate *tbl_template);
  void writeBusDcls();
  void writeCells();
  void writeCell(const LibertyCell *cell);
  void writePort(const LibertyPort *port);
  void writePwrGndPort(const LibertyPort *port);
  void writeBusPort(const LibertyPort *port);
  void writePortAttrs(const LibertyPort *port);
  void writeTimingArcSet(const TimingArcSet *arc_set);
  void writeTimingModels(const TimingArc *arc,
                         const RiseFall *rf);
  void writeTableModel(const TableModel *model);
  void writeTableModel0(const TableModel *model);
  void writeTableModel1(const TableModel *model);
  void writeTableModel2(const TableModel *model);
  void writeTableAxis4(const TableAxis *axis,
                       int index);
  void writeTableAxis10(const TableAxis *axis,
                        int index);

  const char *asString(bool value);
  const char *asString(const PortDirection *dir);
  const char *timingTypeString(const TimingArcSet *arc_set);
  bool isAutoWidthArc(const LibertyPort *port,
                      const TimingArcSet *arc_set);

  const LibertyLibrary *library_;
  const char *filename_;
  std::ofstream &stream_;
  Report *report_;
  const Unit *time_unit_;
  const Unit *cap_unit_;
};

void
writeLiberty(LibertyLibrary *lib,
             const char *filename,
             StaState *sta)
{
  std::ofstream stream(filename);
  if (stream.is_open()) {
    LibertyWriter writer(lib, filename, stream, sta->report());
    writer.writeLibrary();
  }
  else
    throw FileNotWritable(filename);
}

LibertyWriter::LibertyWriter(const LibertyLibrary *lib,
                             const char *filename,
                             std::ofstream &stream,
                             Report *report) :
  library_(lib),
  filename_(filename),
  stream_(stream),
  report_(report),
  time_unit_(lib->units()->timeUnit()),
  cap_unit_(lib->units()->capacitanceUnit())
{
}

void
LibertyWriter::writeLibrary()
{
  writeHeader();
  sta::print(stream_, "\n");
  writeTableTemplates();
  writeBusDcls();
  sta::print(stream_, "\n");
  writeCells();
  writeFooter();
}

void
LibertyWriter::writeHeader()
{
  sta::print(stream_, "library ({}) {{\n", library_->name());
  sta::print(stream_, "  comment                        : \"\";\n");
  sta::print(stream_, "  delay_model                    : table_lookup;\n");
  sta::print(stream_, "  simulation                     : false;\n");
  const Unit *cap_unit = library_->units()->capacitanceUnit();
  sta::print(stream_, "  capacitive_load_unit (1,{});\n",
             cap_unit->scaleAbbrevSuffix());
  sta::print(stream_, "  leakage_power_unit             : 1pW;\n");
  const Unit *current_unit = library_->units()->currentUnit();
  sta::print(stream_, "  current_unit                   : \"1{}\";\n",
             current_unit->scaleAbbrevSuffix());
  const Unit *res_unit = library_->units()->resistanceUnit();
  sta::print(stream_, "  pulling_resistance_unit        : \"1{}\";\n",
             res_unit->scaleAbbrevSuffix());
  const Unit *time_unit = library_->units()->timeUnit();
  sta::print(stream_, "  time_unit                      : \"1{}\";\n",
             time_unit->scaleAbbrevSuffix());
  const Unit *volt_unit = library_->units()->voltageUnit();
  sta::print(stream_, "  voltage_unit                   : \"1{}\";\n",
             volt_unit->scaleAbbrevSuffix());
  sta::print(stream_, "  library_features(report_delay_calculation);\n");
  sta::print(stream_, "\n");

  sta::print(stream_, "  input_threshold_pct_rise : {:.0f};\n",
             library_->inputThreshold(RiseFall::rise()) * 100);
  sta::print(stream_, "  input_threshold_pct_fall : {:.0f};\n",
             library_->inputThreshold(RiseFall::fall()) * 100);
  sta::print(stream_, "  output_threshold_pct_rise : {:.0f};\n",
             library_->inputThreshold(RiseFall::rise()) * 100);
  sta::print(stream_, "  output_threshold_pct_fall : {:.0f};\n",
             library_->inputThreshold(RiseFall::fall()) * 100);
  sta::print(stream_, "  slew_lower_threshold_pct_rise : {:.0f};\n",
             library_->slewLowerThreshold(RiseFall::rise()) * 100);
  sta::print(stream_, "  slew_lower_threshold_pct_fall : {:.0f};\n",
             library_->slewLowerThreshold(RiseFall::fall()) * 100);
  sta::print(stream_, "  slew_upper_threshold_pct_rise : {:.0f};\n",
             library_->slewUpperThreshold(RiseFall::rise()) * 100);
  sta::print(stream_, "  slew_upper_threshold_pct_fall : {:.0f};\n",
             library_->slewUpperThreshold(RiseFall::rise()) * 100);
  sta::print(stream_, "  slew_derate_from_library : {:.1f};\n",
             library_->slewDerateFromLibrary());
  sta::print(stream_, "\n");

  bool exists;
  float max_fanout;
  library_->defaultFanoutLoad(max_fanout, exists);
  if (exists)
    sta::print(stream_, "  default_max_fanout             : {:.0f};\n", max_fanout);
  float max_slew;
  library_->defaultMaxSlew(max_slew, exists);
  if (exists)
    sta::print(stream_, "  default_max_transition         : {};\n",
               time_unit_->asString(max_slew, 3));
  float max_cap;
  library_->defaultMaxCapacitance(max_cap, exists);
  if (exists)
    sta::print(stream_, "  default_max_capacitance         : {};\n",
               cap_unit_->asString(max_cap, 3));
  float fanout_load;
  library_->defaultFanoutLoad(fanout_load, exists);
  if (exists)
    sta::print(stream_, "  default_fanout_load            : {:.2f};\n", fanout_load);
  sta::print(stream_, "\n");

  sta::print(stream_, "  nom_process                    : {:.1f};\n",
             library_->nominalProcess());
  sta::print(stream_, "  nom_temperature                : {:.1f};\n",
             library_->nominalTemperature());
  sta::print(stream_, "  nom_voltage                    : {:.2f};\n",
             library_->nominalVoltage());
}

void
LibertyWriter::writeTableTemplates()
{
  for (TableTemplate *tbl_template : library_->tableTemplates())
    writeTableTemplate(tbl_template);
}

void
LibertyWriter::writeTableTemplate(const TableTemplate *tbl_template)
{
  const TableAxis *axis1 = tbl_template->axis1();
  const TableAxis *axis2 = tbl_template->axis2();
  const TableAxis *axis3 = tbl_template->axis3();
  // skip scalar templates
  if (axis1) {
    sta::print(stream_, "  lu_table_template({}) {{\n", tbl_template->name());
    sta::print(stream_, "    variable_1 : {};\n",
               tableVariableString(axis1->variable()));
    if (axis2)
      sta::print(stream_, "    variable_2 : {};\n",
                 tableVariableString(axis2->variable()));
    if (axis3)
      sta::print(stream_, "    variable_3 : {};\n",
                 tableVariableString(axis3->variable()));
    if (axis1 && !axis1->values().empty())
      writeTableAxis4(axis1, 1);
    if (axis2 && !axis2->values().empty())
      writeTableAxis4(axis2, 2);
    if (axis3 && !axis3->values().empty())
      writeTableAxis4(axis3, 3);
    sta::print(stream_, "  }}\n");
  }
}

// indent 4
void
LibertyWriter::writeTableAxis4(const TableAxis *axis,
                               int index)
{
  sta::print(stream_, "    index_{}(\"", index);
  const Unit *unit = tableVariableUnit(axis->variable(), library_->units());
  bool first = true;
  for (size_t i = 0; i < axis->size(); i++) {
    if (!first)
      sta::print(stream_, ",  ");
    sta::print(stream_, "{}", unit->asString(axis->axisValue(i), 5));
    first = false;
  }
  sta::print(stream_, "\");\n");
}

// indent 10
void
LibertyWriter::writeTableAxis10(const TableAxis *axis,
                                int index)
{
  sta::print(stream_, "      ");
  writeTableAxis4(axis, index);
}

void
LibertyWriter::writeBusDcls()
{
  BusDclSeq dcls = library_->busDcls();
  for (BusDcl *dcl : dcls) {
    sta::print(stream_, "  type (\"{}\") {{\n", dcl->name());
    sta::print(stream_, "    base_type : array;\n");
    sta::print(stream_, "    data_type : bit;\n");
    sta::print(stream_, "    bit_width : {};\n", std::abs(dcl->from() - dcl->to() + 1));
    sta::print(stream_, "    bit_from : {};\n", dcl->from());
    sta::print(stream_, "    bit_to : {};\n", dcl->to());
    sta::print(stream_, "  }}\n");
  }
}

void
LibertyWriter::writeCells()
{
  LibertyCellIterator cell_iter(library_);
  while (cell_iter.hasNext()) {
    const LibertyCell *cell = cell_iter.next();
    writeCell(cell);
  }
}

void
LibertyWriter::writeCell(const LibertyCell *cell)
{
  sta::print(stream_, "  cell (\"{}\") {{\n", cell->name());
  float area = cell->area();
  if (area > 0.0)
    sta::print(stream_, "    area : {:.3f} \n", area);
  if (cell->isMacro())
    sta::print(stream_, "    is_macro_cell : true;\n");
  if (cell->interfaceTiming())
    sta::print(stream_, "    interface_timing : true;\n");
  const std::string &footprint = cell->footprint();
  if (!footprint.empty())
    sta::print(stream_, "    cell_footprint : \"{}\";\n", footprint);
  const std::string &user_function_class = cell->userFunctionClass();
  if (!user_function_class.empty())
    sta::print(stream_, "    user_function_class : \"{}\";\n", user_function_class);

  LibertyCellPortIterator port_iter(cell);
  while (port_iter.hasNext()) {
    const LibertyPort *port = port_iter.next();
    if (!port->direction()->isInternal()) {
      if (port->isPwrGnd())
        writePwrGndPort(port);
      else if (port->isBus())
        writeBusPort(port);
      else if (port->isBundle())
        report_->error(1340, "{}/{} bundled ports not supported.", library_->name(),
                       cell->name());
      else
        writePort(port);
    }
  }

  sta::print(stream_, "  }}\n");
  sta::print(stream_, "\n");
}

void
LibertyWriter::writeBusPort(const LibertyPort *port)
{
  sta::print(stream_, "    bus(\"{}\") {{\n", port->name());
  if (port->busDcl())
    sta::print(stream_, "      bus_type : {};\n", port->busDcl()->name());
  writePortAttrs(port);

  LibertyPortMemberIterator member_iter(port);
  while (member_iter.hasNext()) {
    LibertyPort *member = member_iter.next();
    writePort(member);
  }
  sta::print(stream_, "    }}\n");
}

void
LibertyWriter::writePort(const LibertyPort *port)
{
  sta::print(stream_, "    pin(\"{}\") {{\n", port->name());
  writePortAttrs(port);
  sta::print(stream_, "    }}\n");
}

void
LibertyWriter::writePortAttrs(const LibertyPort *port)
{
  sta::print(stream_, "      direction : {};\n", asString(port->direction()));
  auto func = port->function();
  if (func
      // cannot ref internal ports until sequentials are written
      && !(func->port() && func->port()->direction()->isInternal()))
    sta::print(stream_, "      function : \"{}\";\n", func->to_string());
  auto tristate_enable = port->tristateEnable();
  if (tristate_enable) {
    if (tristate_enable->op() == FuncExpr::Op::not_) {
      FuncExpr *three_state = tristate_enable->left();
      sta::print(stream_, "      three_state : \"{}\";\n",
                 three_state->to_string());
    }
    else {
      FuncExpr *three_state = tristate_enable->copy()->invert();
      sta::print(stream_, "      three_state : \"{}\";\n",
                 three_state->to_string());
      delete three_state;
    }
  }
  if (port->isClock())
    sta::print(stream_, "      clock : true;\n");
  sta::print(stream_, "      capacitance : {};\n",
             cap_unit_->asString(port->capacitance(), 4));

  float limit;
  bool exists;
  port->slewLimit(MinMax::max(), limit, exists);
  if (exists)
    sta::print(stream_, "      max_transition : {};\n", time_unit_->asString(limit, 3));
  port->capacitanceLimit(MinMax::max(), limit, exists);
  if (exists)
    sta::print(stream_, "      max_capacitance : {};\n", cap_unit_->asString(limit, 3));

  for (TimingArcSet *arc_set : port->libertyCell()->timingArcSetsTo(port)) {
    if (!isAutoWidthArc(port, arc_set))
      writeTimingArcSet(arc_set);
  }
}

void
LibertyWriter::writePwrGndPort(const LibertyPort *port)
{
  sta::print(stream_, "    pg_pin(\"{}\") {{\n", port->name());
  sta::print(stream_, "      pg_type : \"{}\";\n", pwrGndTypeName(port->pwrGndType()));
  sta::print(stream_, "      voltage_name : \"{}\";\n", port->voltageName());
  sta::print(stream_, "    }}\n");
}

// Check if arc is added for port min_pulse_width_high/low attribute.
bool
LibertyWriter::isAutoWidthArc(const LibertyPort *port,
                              const TimingArcSet *arc_set)
{
  if (arc_set->role() == TimingRole::width()) {
    float min_width;
    bool exists1, exists2;
    port->minPulseWidth(RiseFall::rise(), min_width, exists1);
    port->minPulseWidth(RiseFall::fall(), min_width, exists2);
    return exists1 || exists2;
  }
  return false;
}

void
LibertyWriter::writeTimingArcSet(const TimingArcSet *arc_set)
{
  sta::print(stream_, "      timing() {{\n");
  if (arc_set->from())
    sta::print(stream_, "        related_pin : \"{}\";\n", arc_set->from()->name());
  TimingSense sense = arc_set->sense();
  if (sense != TimingSense::unknown && sense != TimingSense::non_unate)
    sta::print(stream_, "        timing_sense : {};\n", to_string(sense));
  const char *timing_type = timingTypeString(arc_set);
  if (timing_type)
    sta::print(stream_, "        timing_type : {};\n", timing_type);

  for (const RiseFall *rf : RiseFall::range()) {
    TimingArc *arc = arc_set->arcTo(rf);
    if (arc) {
      // Min pulse width arcs are wrt to the leading edge of the pulse.
      const RiseFall *model_rf =
          (arc_set->role() == TimingRole::width()) ? rf->opposite() : rf;
      writeTimingModels(arc, model_rf);
    }
  }

  sta::print(stream_, "      }}\n");
}

void
LibertyWriter::writeTimingModels(const TimingArc *arc,
                                 const RiseFall *rf)
{
  TimingModel *model = arc->model();
  const GateTableModel *gate_model = dynamic_cast<GateTableModel *>(model);
  const CheckTableModel *check_model = dynamic_cast<CheckTableModel *>(model);
  if (gate_model) {
    const TableModel *delay_model = gate_model->delayModel();
    const std::string &template_name = delay_model->tblTemplate()->name();
    sta::print(stream_, "        cell_{}({}) {{\n", rf->name(), template_name);
    writeTableModel(delay_model);
    sta::print(stream_, "        }}\n");

    const TableModel *slew_model = gate_model->slewModel();
    if (slew_model) {
      const std::string &slew_template_name = slew_model->tblTemplate()->name();
      sta::print(stream_, "        {}_transition({}) {{\n", rf->name(),
                 slew_template_name);
      writeTableModel(slew_model);
      sta::print(stream_, "        }}\n");
    }
  }
  else if (check_model) {
    const TableModel *model = check_model->checkModel();
    const std::string &template_name = model->tblTemplate()->name();
    sta::print(stream_, "        {}_constraint({}) {{\n", rf->name(),
               template_name);
    writeTableModel(model);
    sta::print(stream_, "        }}\n");
  }
  else
    report_->error(1341, "{}/{}/{} timing model not supported.", library_->name(),
                   arc->from()->libertyCell()->name(), arc->from()->name());
}

void
LibertyWriter::writeTableModel(const TableModel *model)
{
  switch (model->order()) {
    case 0:
      writeTableModel0(model);
      break;
    case 1:
      writeTableModel1(model);
      break;
    case 2:
      writeTableModel2(model);
      break;
    case 3:
      report_->error(1342, "3 axis table models not supported.");
      break;
    default:
      break;
  }
}

void
LibertyWriter::writeTableModel0(const TableModel *model)
{
  float value = model->value(0, 0, 0);
  sta::print(stream_, "          values(\"{}\");\n", time_unit_->asString(value, 5));
}

void
LibertyWriter::writeTableModel1(const TableModel *model)
{
  writeTableAxis10(model->axis1(), 1);
  sta::print(stream_, "          values(\"");
  bool first_col = true;
  for (size_t index1 = 0; index1 < model->axis1()->size(); index1++) {
    float value = model->value(index1, 0, 0);
    if (!first_col)
      sta::print(stream_, ",");
    sta::print(stream_, "{}", time_unit_->asString(value, 5));
    first_col = false;
  }
  sta::print(stream_, "\");\n");
}

void
LibertyWriter::writeTableModel2(const TableModel *model)
{
  writeTableAxis10(model->axis1(), 1);
  writeTableAxis10(model->axis2(), 2);
  sta::print(stream_, "          values(\"");
  bool first_row = true;
  for (size_t index1 = 0; index1 < model->axis1()->size(); index1++) {
    if (!first_row) {
      sta::print(stream_, "\\\n");
      sta::print(stream_, "                 \"");
    }
    bool first_col = true;
    for (size_t index2 = 0; index2 < model->axis2()->size(); index2++) {
      float value = model->value(index1, index2, 0);
      if (!first_col)
        sta::print(stream_, ",");
      sta::print(stream_, "{}", time_unit_->asString(value, 5));
      first_col = false;
    }
    sta::print(stream_, "\"");
    first_row = false;
  }
  sta::print(stream_, ");\n");
}

void
LibertyWriter::writeFooter()
{
  sta::print(stream_, "}}\n");
}

const char *
LibertyWriter::asString(bool value)
{
  return value ? "true" : "false";
}

const char *
LibertyWriter::asString(const PortDirection *dir)
{
  if (dir == PortDirection::input())
    return "input";
  else if (dir == PortDirection::output() || (dir == PortDirection::tristate()))
    return "output";
  else if (dir == PortDirection::internal())
    return "internal";
  else if (dir == PortDirection::bidirect())
    return "inout";
  else if (dir == PortDirection::ground()
           || dir == PortDirection::power()
           || dir == PortDirection::well())
    return "input";
  return "unknown";
}

const char *
LibertyWriter::timingTypeString(const TimingArcSet *arc_set)
{
  const TimingRole *role = arc_set->role();
  if (role == TimingRole::combinational())
    return "combinational";
  else if (role == TimingRole::tristateDisable())
    return "three_state_disable";
  else if (role == TimingRole::tristateEnable())
    return "three_state_enable";
  else if (role == TimingRole::regClkToQ() || role == TimingRole::latchEnToQ()) {
    const TimingArc *arc = arc_set->arcs()[0];
    if (arc->fromEdge()->asRiseFall() == RiseFall::rise())
      return "rising_edge";
    else
      return "falling_edge";
  }
  else if (role == TimingRole::latchDtoQ())
    return nullptr;
  else if (role == TimingRole::regSetClr()) {
    const TimingArc *arc = arc_set->arcs()[0];
    if (arc->toEdge()->asRiseFall() == RiseFall::rise())
      return "preset";
    else
      return "clear";
  }
  else if (role == TimingRole::setup() || role == TimingRole::recovery()) {
    const TimingArc *arc = arc_set->arcs()[0];
    if (arc->fromEdge()->asRiseFall() == RiseFall::rise())
      return "setup_rising";
    else
      return "setup_falling";
  }
  else if (role == TimingRole::hold() || role == TimingRole::removal()) {
    const TimingArc *arc = arc_set->arcs()[0];
    if (arc->fromEdge()->asRiseFall() == RiseFall::rise())
      return "hold_rising";
    else
      return "hold_falling";
  }
  else if (role == TimingRole::nonSeqSetup()) {
    const TimingArc *arc = arc_set->arcs()[0];
    if (arc->fromEdge()->asRiseFall() == RiseFall::rise())
      return "non_seq_setup_rising";
    else
      return "non_seq_setup_falling";
  }
  else if (role == TimingRole::nonSeqHold()) {
    const TimingArc *arc = arc_set->arcs()[0];
    if (arc->fromEdge()->asRiseFall() == RiseFall::rise())
      return "non_seq_hold_rising";
    else
      return "non_seq_hold_falling";
  }
  else if (role == TimingRole::clockTreePathMin())
    return "min_clock_tree_path";
  else if (role == TimingRole::clockTreePathMax())
    return "max_clock_tree_path";
  else if (role == TimingRole::width())
    return "min_pulse_width";
  else {
    report_->error(1343, "{}/{}/{} timing arc type {} not supported.",
                   library_->name(), arc_set->to()->libertyCell()->name(),
                   arc_set->to()->name(), role->to_string());
    return nullptr;
  }
}

}  // namespace sta
