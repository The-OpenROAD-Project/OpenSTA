// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#include "LibertyWriter.hh"

#include <cstdlib>
#include <algorithm>

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

using std::abs;

class LibertyWriter
{
public:
  LibertyWriter(const LibertyLibrary *lib,
                const char *filename,
                FILE *stream,
                Report *report);
  void writeLibrary();

protected:
  void writeHeader();
  void writeFooter();
  void writeTableTemplates();
  void writeTableTemplate(TableTemplate *tbl_template);
  void writeBusDcls();
  void writeCells();
  void writeCell(const LibertyCell *cell);
  void writePort(const LibertyPort *port);
  void writeBusPort(const LibertyPort *port);
  void writePortAttrs(const LibertyPort *port);
  void writeTimingArcSet(const TimingArcSet *arc_set);
  void writeTimingModels(const TimingArc *arc,
                         RiseFall *rf);
  void writeTableModel(const TableModel *model);
  void writeTableModel0(const TableModel *model);
  void writeTableModel1(const TableModel *model);
  void writeTableModel2(const TableModel *model);
  void writeTableAxis(TableAxisPtr axis,
                      int index);

  const char *asString(bool value);
  const char *asString(const PortDirection *dir);
  const char *timingTypeString(const TimingArcSet *arc_set);

  const LibertyLibrary *library_;
  const char *filename_;
  FILE *stream_;
  Report *report_;
  const Unit *time_unit_;
  const Unit *cap_unit_;
};

void
writeLiberty(LibertyLibrary *lib,
             const char *filename,
             StaState *sta)
{
  FILE *stream = fopen(filename, "w");
  if (stream) {
    LibertyWriter writer(lib, filename, stream, sta->report());
    writer.writeLibrary();
    fclose(stream);
  }
  else
    throw FileNotWritable(filename);
}

LibertyWriter::LibertyWriter(const LibertyLibrary *lib,
                             const char *filename,
                             FILE *stream,
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
  fprintf(stream_, "\n");
  writeTableTemplates();
  writeBusDcls();
  fprintf(stream_, "\n");
  writeCells();
  writeFooter();
}
  
void
LibertyWriter::writeHeader()
{
  fprintf(stream_, "library (%s) {\n", library_->name());
  fprintf(stream_, "  comment                        : \"\";\n");
  fprintf(stream_, "  delay_model                    : table_lookup;\n");
  fprintf(stream_, "  simulation                     : false;\n");
  const Unit *cap_unit = library_->units()->capacitanceUnit();
  fprintf(stream_, "  capacitive_load_unit (1,%s);\n",
          cap_unit->scaledSuffix());
  fprintf(stream_, "  leakage_power_unit             : 1pW;\n");
  const Unit *current_unit = library_->units()->currentUnit();
  fprintf(stream_, "  current_unit                   : \"1%s\";\n",
          current_unit->scaledSuffix());
  const Unit *res_unit = library_->units()->resistanceUnit();
  fprintf(stream_, "  pulling_resistance_unit        : \"1%s\";\n",
          res_unit->scaledSuffix());
  const Unit *time_unit = library_->units()->timeUnit();
  fprintf(stream_, "  time_unit                      : \"1%s\";\n",
          time_unit->scaledSuffix());
  const Unit *volt_unit = library_->units()->voltageUnit();
  fprintf(stream_, "  voltage_unit                   : \"1%s\";\n",
          volt_unit->scaledSuffix());
  fprintf(stream_, "  library_features(report_delay_calculation);\n");
  fprintf(stream_, "\n");

  fprintf(stream_, "  input_threshold_pct_rise : %.0f;\n",
          library_->inputThreshold(RiseFall::rise()) * 100);
  fprintf(stream_, "  input_threshold_pct_fall : %.0f;\n",
          library_->inputThreshold(RiseFall::fall()) * 100);
  fprintf(stream_, "  output_threshold_pct_rise : %.0f;\n",
          library_->inputThreshold(RiseFall::rise()) * 100);
  fprintf(stream_, "  output_threshold_pct_fall : %.0f;\n",
          library_->inputThreshold(RiseFall::fall()) * 100);
  fprintf(stream_, "  slew_lower_threshold_pct_rise : %.0f;\n",
          library_->slewLowerThreshold(RiseFall::rise()) * 100);
  fprintf(stream_, "  slew_lower_threshold_pct_fall : %.0f;\n",
          library_->slewLowerThreshold(RiseFall::fall()) * 100);
  fprintf(stream_, "  slew_upper_threshold_pct_rise : %.0f;\n",
          library_->slewUpperThreshold(RiseFall::rise()) * 100);
  fprintf(stream_, "  slew_upper_threshold_pct_fall : %.0f;\n",
          library_->slewUpperThreshold(RiseFall::rise()) * 100);
  fprintf(stream_, "  slew_derate_from_library : %.1f;\n",
          library_->slewDerateFromLibrary());
  fprintf(stream_, "\n");

  bool exists;
  float max_fanout;
  library_->defaultFanoutLoad(max_fanout, exists);
  if (exists)
    fprintf(stream_, "  default_max_fanout             : %.0f;\n", max_fanout);
  float max_slew;
  library_->defaultMaxSlew(max_slew, exists);
  if (exists)
    fprintf(stream_, "  default_max_transition         : %s;\n",
            time_unit_->asString(max_slew, 3));
  float max_cap;
  library_->defaultMaxCapacitance(max_cap, exists);
  if (exists)
    fprintf(stream_, "  default_max_capacitance         : %s;\n",
            cap_unit_->asString(max_cap, 3));
  float fanout_load;
  library_->defaultFanoutLoad(fanout_load, exists);
  if (exists)
    fprintf(stream_, "  default_fanout_load            : %.2f;\n", fanout_load);
  fprintf(stream_, "\n");
  fprintf(stream_, "  nom_process                    : %.1f;\n",
          library_->nominalProcess());
  fprintf(stream_, "  nom_temperature                : %.1f;\n",
          library_->nominalTemperature());
  fprintf(stream_, "  nom_voltage                    : %.2f;\n",
          library_->nominalVoltage());
}

void
LibertyWriter::writeTableTemplates()
{
  for (TableTemplate *tbl_template : library_->tableTemplates())
    writeTableTemplate(tbl_template);
}

void
LibertyWriter::writeTableTemplate(TableTemplate *tbl_template)
{
  TableAxisPtr axis1 = tbl_template->axis1();
  TableAxisPtr axis2 = tbl_template->axis2();
  TableAxisPtr axis3 = tbl_template->axis3();
  // skip scalar templates
  if (axis1) {
    fprintf(stream_, "  lu_table_template(%s) {\n", tbl_template->name());
    fprintf(stream_, "    variable_1 : %s;\n",
            tableVariableString(axis1->variable()));
    if (axis2)
      fprintf(stream_, "    variable_2 : %s;\n",
              tableVariableString(axis2->variable()));
    if (axis3)
      fprintf(stream_, "    variable_3 : %s;\n",
              tableVariableString(axis3->variable()));
    if (axis1)
      writeTableAxis(axis1, 1);
    if (axis2)
      writeTableAxis(axis2, 2);
    if (axis3)
      writeTableAxis(axis3, 3);
    fprintf(stream_, "  }\n");
  }
}

void
LibertyWriter::writeTableAxis(TableAxisPtr axis,
                              int index)
{
  fprintf(stream_, "    index_%d (\"", index);
  const Unit *unit = tableVariableUnit(axis->variable(), library_->units());
  bool first = true;
  for (size_t i = 0; i < axis->size(); i++) {
    if (!first)
      fprintf(stream_, ",  ");      
    fprintf(stream_, "%s", unit->asString(axis->axisValue(i), 5));
    first = false;
  }
  fprintf(stream_, "\");\n");
}

void
LibertyWriter::writeBusDcls()
{
  BusDclSeq dcls = library_->busDcls();
  for (BusDcl *dcl : dcls) {
    fprintf(stream_, "  type (\"%s\") {\n", dcl->name());
    fprintf(stream_, "    base_type : array;\n");
    fprintf(stream_, "    data_type : bit;\n");
    fprintf(stream_, "    bit_width : %d;\n", abs(dcl->from() - dcl->to() + 1));
    fprintf(stream_, "    bit_from : %d;\n", dcl->from());
    fprintf(stream_, "    bit_to : %d;\n", dcl->to());
    fprintf(stream_, "  }\n");
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
  fprintf(stream_, "  cell (\"%s\") {\n", cell->name());
  float area = cell->area();
  if (area > 0.0)
    fprintf(stream_, "    area : %.3f \n", area);
  if (cell->isMacro())
    fprintf(stream_, "    is_macro : true;\n");

  LibertyCellPortIterator port_iter(cell);
  while (port_iter.hasNext()) {
    const LibertyPort *port = port_iter.next();
    if (!port->direction()->isInternal()) {
      if (port->isBus())
        writeBusPort(port);
      else if (port->isBundle())
        report_->error(704, "%s/%s bundled ports not supported.",
                       library_->name(),
                       cell->name());
      else
        writePort(port);
    }
  }

  fprintf(stream_, "  }\n");
  fprintf(stream_, "\n");
}

void
LibertyWriter::writeBusPort(const LibertyPort *port)
{
  fprintf(stream_, "    bus(\"%s\") {\n", port->name());
  if (port->busDcl())
    fprintf(stream_, "      bus_type : %s;\n", port->busDcl()->name());
  writePortAttrs(port);

  LibertyPortMemberIterator member_iter(port);
  while (member_iter.hasNext()) {
    LibertyPort *member = member_iter.next();
    writePort(member);
  }
  fprintf(stream_, "    }\n");
}

void
LibertyWriter::writePort(const LibertyPort *port)
{
  fprintf(stream_, "    pin(\"%s\") {\n", port->name());
  writePortAttrs(port);
  fprintf(stream_, "    }\n");
}

void
LibertyWriter::writePortAttrs(const LibertyPort *port)
{
  fprintf(stream_, "      direction : %s;\n" , asString(port->direction()));
  auto func = port->function();
  if (func
      // cannot ref internal ports until sequentials are written
      && !(func->port()
           && func->port()->direction()->isInternal()))
    fprintf(stream_, "      function : \"%s\";\n", func->asString());
  auto tristate_enable = port->tristateEnable();
  if (tristate_enable) {
    if (tristate_enable->op() == FuncExpr::op_not) {
      FuncExpr *three_state = tristate_enable->left();
      fprintf(stream_, "      three_state : \"%s\";\n", three_state->asString());
    }
    else {
      FuncExpr three_state(FuncExpr::op_not, tristate_enable, nullptr, nullptr);
      fprintf(stream_, "      three_state : \"%s\";\n", three_state.asString());
    }
  }
  if (port->isClock())
    fprintf(stream_, "      clock : true;\n");
  fprintf(stream_, "      capacitance : %s;\n",
          cap_unit_->asString(port->capacitance(), 4));
  
  float limit;
  bool exists;
  port->slewLimit(MinMax::max(), limit, exists);
  if (exists)
    fprintf(stream_, "      max_transition : %s;\n",
            time_unit_->asString(limit, 3));
  port->capacitanceLimit(MinMax::max(), limit, exists);
  if (exists)
    fprintf(stream_, "      max_capacitance : %s;\n",
            cap_unit_->asString(limit, 3));

  for (TimingArcSet *arc_set : port->libertyCell()->timingArcSets(nullptr,port))
    writeTimingArcSet(arc_set);
}

void
LibertyWriter::writeTimingArcSet(const TimingArcSet *arc_set)
{
  fprintf(stream_, "      timing() {\n");
  fprintf(stream_, "        related_pin : \"%s\";\n", arc_set->from()->name());
  TimingSense sense = arc_set->sense();
  if (sense != TimingSense::unknown
      && sense != TimingSense::non_unate)
    fprintf(stream_, "        timing_sense : %s;\n",
            timingSenseString(sense));
  const char *timing_type = timingTypeString(arc_set);
  if (timing_type)
    fprintf(stream_, "        timing_type : %s;\n", timing_type);

  for (RiseFall *rf : RiseFall::range()) {
    TimingArc *arc = arc_set->arcTo(rf);
    if (arc)
      writeTimingModels(arc, rf);
  }

  fprintf(stream_, "      }\n");
}

void
LibertyWriter::writeTimingModels(const TimingArc *arc,
                                 RiseFall *rf)
{
  TimingModel *model = arc->model();
  const GateTableModel *gate_model = dynamic_cast<GateTableModel*>(model);
  const CheckTableModel *check_model = dynamic_cast<CheckTableModel*>(model);
  if (gate_model) {
    const TableModel *delay_model = gate_model->delayModel();
    const char *template_name = delay_model->tblTemplate()->name();
    fprintf(stream_, "	cell_%s(%s) {\n", rf->name(), template_name);
    writeTableModel(delay_model);
    fprintf(stream_, "	}\n");

    const TableModel *slew_model = gate_model->slewModel();
    template_name = slew_model->tblTemplate()->name();
    fprintf(stream_, "	%s_transition(%s) {\n", rf->name(), template_name);
    writeTableModel(slew_model);
    fprintf(stream_, "	}\n");
  } else if (check_model) {
    const TableModel *model = check_model->model();
    const char *template_name = model->tblTemplate()->name();
    fprintf(stream_, "	%s_constraint(%s) {\n", rf->name(), template_name);
    writeTableModel(model);
    fprintf(stream_, "	}\n");
  }
  else
    report_->error(701, "%s/%s/%s timing model not supported.",
                   library_->name(),
                   arc->from()->libertyCell()->name(),
                   arc->from()->name());
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
    report_->error(702, "3 axis table models not supported.");  
    break;
  }
}

void
LibertyWriter::writeTableModel0(const TableModel *model)
{
  float value = model->value(0, 0, 0);
  fprintf(stream_, "          values(\"%s\");\n",
          time_unit_->asString(value, 5));
}

void
LibertyWriter::writeTableModel1(const TableModel *model)
{
  fprintf(stream_, "          values(\"");
  bool first_col = true;
  for (size_t index1 = 0; index1 < model->axis1()->size(); index1++) {
    float value = model->value(index1, 0, 0);
    if (!first_col)
      fprintf(stream_, ",");
    fprintf(stream_, "%s", time_unit_->asString(value, 5));
    first_col = false;
  }
  fprintf(stream_, "\");\n");
}

void
LibertyWriter::writeTableModel2(const TableModel *model)
{
  fprintf(stream_, "          values(\"");
  bool first_row = true;
  for (size_t index1 = 0; index1 < model->axis1()->size(); index1++) {
    if (!first_row) {
      fprintf(stream_, "\\\n");
      fprintf(stream_, "                 \"");
    }
    bool first_col = true;
    for (size_t index2 = 0; index2 < model->axis2()->size(); index2++) {
      float value = model->value(index1, index2, 0);
      if (!first_col)
        fprintf(stream_, ",");
      fprintf(stream_, "%s", time_unit_->asString(value, 5));
      first_col = false;
    }
    fprintf(stream_, "\"");
    first_row = false;
  }
  fprintf(stream_, ");\n");
}

void
LibertyWriter::writeFooter()
{
  fprintf(stream_, "}\n");
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
  else if (dir == PortDirection::output()
           || (dir == PortDirection::tristate()))
    return "output";
  else if (dir == PortDirection::internal())
    return "internal";
  else if (dir == PortDirection::bidirect())
    return "inout";
  else if (dir == PortDirection::ground()
           || dir == PortDirection::power())
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
  else if (role == TimingRole::regClkToQ()
           || role == TimingRole::latchEnToQ()) {
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
  else if (role == TimingRole::setup()
           || role == TimingRole::recovery()) {
    const TimingArc *arc = arc_set->arcs()[0];
    if (arc->fromEdge()->asRiseFall() == RiseFall::rise())
      return "setup_rising";
    else
      return "setup_falling";
  }
  else if (role == TimingRole::hold()
           || role == TimingRole::removal()) {
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
  else {
    report_->error(703, "%s/%s/%s timing arc type %s not supported.",
                   library_->name(),
                   arc_set->from()->libertyCell()->name(),
                   arc_set->from()->name(),
                   role->asString());
    return nullptr;
  }
}

} // namespace
