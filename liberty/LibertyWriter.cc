// OpenSTA, Static Timing Analyzer
// Copyright (c) 2022, Parallax Software, Inc.
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

#include <stdlib.h>

#include "Liberty.hh"
#include "Units.hh"

namespace sta {

class LibertyWriter
{
public:
  LibertyWriter(LibertyLibrary *lib,
                const char *filename,
                FILE *stream);
  void writeLibrary();

protected:
  void writeHeader();
  void writeFooter();

  LibertyLibrary *library_;
  const char *filename_;
  FILE *stream_;
};

void
writeLiberty(LibertyLibrary *lib,
             const char *filename)
{
  FILE *stream = fopen(filename, "w");
  if (stream) {
    LibertyWriter writer(lib, filename, stream);
    writer.writeLibrary();
    fclose(stream);
  }
  else
    throw FileNotWritable(filename);
}

LibertyWriter::LibertyWriter(LibertyLibrary *lib,
                             const char *filename,
                             FILE *stream) :
  library_(lib),
  filename_(filename),
  stream_(stream)
{
}

void
LibertyWriter::writeLibrary()
{
  writeHeader();
  writeFooter();
}
  
void
LibertyWriter::writeHeader()
{
  fprintf(stream_, "library (%s) {\n", library_->name());
  fprintf(stream_, "  comment                        : \"\";\n");
  fprintf(stream_, "  delay_model                    : table_lookup;\n");
  fprintf(stream_, "  simulation                     : false;\n");
  Unit *cap_unit = library_->units()->capacitanceUnit();
  fprintf(stream_, "  capacitive_load_unit (1,%s%s);\n",
          cap_unit->scaleAbreviation(),
          cap_unit->suffix());
  fprintf(stream_, "  leakage_power_unit             : 1pW;\n");
  Unit *current_unit = library_->units()->currentUnit();
  fprintf(stream_, "  current_unit                   : \"1%s%s\";\n",
          current_unit->scaleAbreviation(),
          current_unit->suffix());
  Unit *res_unit = library_->units()->resistanceUnit();
  fprintf(stream_, "  pulling_resistance_unit        : \"1%s%s\";\n",
          res_unit->scaleAbreviation(),
          res_unit->suffix());
  Unit *time_unit = library_->units()->timeUnit();
  fprintf(stream_, "  time_unit                      : \"1%s%s\";\n",
          time_unit->scaleAbreviation(),
          time_unit->suffix());
  Unit *volt_unit = library_->units()->voltageUnit();
  fprintf(stream_, "  voltage_unit                   : \"1%s%s\";\n",
          volt_unit->scaleAbreviation(),
          volt_unit->suffix());
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

  fprintf(stream_, "  default_max_fanout             : 40;\n");
  fprintf(stream_, "  default_max_transition         : 2.00;\n");
  fprintf(stream_, "  default_cell_leakage_power     : 100;\n");
  fprintf(stream_, "  default_fanout_load            : 1.0;\n");
  fprintf(stream_, "  default_inout_pin_cap          : 0.0;\n");
  fprintf(stream_, "  default_input_pin_cap          : 0.0;\n");
  fprintf(stream_, "  default_output_pin_cap         : 0.0;\n");
  fprintf(stream_, "\n");
  fprintf(stream_, "  nom_process                    : 1.0;\n");
  fprintf(stream_, "  nom_temperature                : 125.00;\n");
  fprintf(stream_, "  nom_voltage                    : 1.62;\n");
}

void
LibertyWriter::writeFooter()
{
  fprintf(stream_, "}\n");
}

} // namespace
