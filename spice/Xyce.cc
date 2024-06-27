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


#include "Xyce.hh"

#include <fstream>
#include <sstream>
#include <memory>

#include "Error.hh"

namespace sta {

using std::string;
using std::ifstream;
using std::getline;
using std::stringstream;
using std::vector;
using std::make_shared;

void
readXyceCsv(const char *csv_filename,
            // Return values.
            StdStringSeq &titles,
            WaveformSeq &waveforms)
{
  ifstream file(csv_filename);
  if (file.is_open()) {
    string line;

    // Read the header line.
    getline(file, line);
    stringstream ss(line);
    string field;
    size_t col = 0;
    while (getline(ss, field, ',')) {
      // Skip TIME title.
      if (col > 0)
        titles.push_back(field);
      col++;
    }

    vector<FloatSeq> values(titles.size() + 1);
    while (getline(file, line)) {
      stringstream ss(line);
      size_t col = 0;
      while (getline(ss, field, ',')) {
        float value = std::stof(field);
        values[col].push_back(value);
        col++;
      }
    }
    file.close();
    TableAxisPtr time_axis = make_shared<TableAxis>(TableAxisVariable::time,
                                                    new FloatSeq(values[0]));
    for (size_t var = 1; var < values.size(); var++)
      waveforms.emplace_back(new FloatSeq(values[var]), time_axis);
  }
  else
    throw FileNotReadable(csv_filename);
}

} // namespace
