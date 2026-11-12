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

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "TableModel.hh"

namespace sta {

using WaveformSeq = std::vector<Table>;

class Gnuplot
{
public:
  void append(std::string_view title,
              Waveform waveform);
  void clear();
  void writeFiles(std::string_view gnuplot_filename,
                  std::string_view csv_filename);

private:
  void writeCsvFile(std::string_view csv_filename);
  void writeGnuplotFile(std::string_view gnuplot_filename,
                        std::string_view csv_filename);

  std::vector<std::string> titles_;
  WaveformSeq waveforms_;
};

} // namespace sta
