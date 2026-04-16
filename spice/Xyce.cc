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


#include "Xyce.hh"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include "Error.hh"
#include "StringUtil.hh"

namespace sta {

void
readXyceCsv(const char *csv_filename,
            // Return values.
            StringSeq &titles,
            WaveformSeq &waveforms)
{
  std::ifstream file(csv_filename);
  if (file.is_open()) {
    std::string line;

    // Read the header line.
    std::getline(file, line);
    std::stringstream ss(line);
    std::string field;
    size_t col = 0;
    while (std::getline(ss, field, ',')) {
      // Skip TIME title.
      if (col > 0)
        titles.push_back(field);
      col++;
    }

    std::vector<FloatSeq> values(titles.size() + 1);
    while (std::getline(file, line)) {
      std::stringstream ss(line);
      size_t col = 0;
      while (std::getline(ss, field, ',')) {
        auto [value, valid] = stringFloat(field);
        values[col].push_back(value);
        col++;
      }
    }
    file.close();
    TableAxisPtr time_axis = std::make_shared<TableAxis>(TableAxisVariable::time,
                                                         std::move(values[0]));
    for (size_t var = 1; var < values.size(); var++)
      waveforms.emplace_back(new FloatSeq(values[var]), time_axis);
  }
  else
    throw FileNotReadable(csv_filename);
}

} // namespace sta
