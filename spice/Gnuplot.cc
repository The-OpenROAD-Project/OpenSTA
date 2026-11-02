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

#include "Gnuplot.hh"

#include <cstdio>
#include <string>

#include "Format.hh"

namespace sta {

void
Gnuplot::append(std::string_view title,
                Waveform waveform)
{
  titles_.emplace_back(title);
  waveforms_.push_back(std::move(waveform));
}

void
Gnuplot::clear()
{
  titles_.clear();
  waveforms_.clear();
}

void
Gnuplot::writeFiles(std::string_view gnuplot_filename,
                    std::string_view csv_filename)
{
  writeCsvFile(csv_filename);
  writeGnuplotFile(gnuplot_filename, csv_filename);
}

std::string
replaceAll(std::string str,
           const std::string &from,
           const std::string &to)
{
  size_t start_pos = 0;
  while((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
  return str;
}

void
Gnuplot::writeCsvFile(std::string_view csv_filename)
{
  FILE *stream = fopen(std::string(csv_filename).c_str(), "w");
  if (stream) {
    bool first = true;
    for (std::string &title : titles_) {
      if (!first)
        sta::print(stream, ",");
      // Escape underscores because gnuplot thinks they mean subscript.
      std::string gnuplot_title = replaceAll(title, "_", "\\\\_");
      sta::print(stream, "time,{}", gnuplot_title);
      first = false;
    }
    sta::print(stream, "\n");

    bool row_values = true;
    for (size_t row = 0; row_values; row++) {
      row_values = false;
      bool first = true;
      for (const Waveform &waveform : waveforms_) {
        const TableAxis *axis = waveform.axis1();
        const FloatSeq &time_values = axis->values();
        if (!first)
          sta::print(stream, ",");
        if (row < time_values.size()) {
          float time = time_values[row];
          FloatSeq *values = waveform.values();
          float value = (*values)[row];
          sta::print(stream, "{:.5e},{:.5e}", time, value);
          row_values = true;
        }
        else
          sta::print(stream, ",");
        first = false;
      }
      sta::print(stream, "\n");
    }
    fclose(stream);
  }
}

void
Gnuplot::writeGnuplotFile(std::string_view gnuplot_filename,
                          std::string_view csv_filename)
{
  FILE *stream = fopen(std::string(gnuplot_filename).c_str(), "w");
  if (stream) {
    sta::print(stream, "set datafile separator ','\n");
    sta::print(stream, "set key autotitle columnhead\n");
    sta::print(stream, "set terminal qt font \"Sans,12\"\n");
    sta::print(stream, "plot\\\n");
    bool first = true;
    for (size_t i = 0; i < waveforms_.size(); i++) {
      if (first)
        sta::print(stream, "\"{}\" ", csv_filename);
      else {
        sta::print(stream, ",\\\n");
        sta::print(stream, "'' ");
      }
      sta::print(stream, "using {}:{} with lines", i * 2 + 1, i * 2 + 2);
      first = false;
    }
    sta::print(stream, "\n");
    sta::print(stream, "pause mouse close\n");
    fclose(stream);
  }
}

} // namespace sta
