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

#pragma once

#include "LibertyClass.hh"
#include "SearchClass.hh"
#include "StaState.hh"

namespace sta {

class Sta;
class LibertyBuilder;

class MakeTimingModel : public StaState
{
public:
  MakeTimingModel(const Corner *corner,
                   Sta *sta);
  ~MakeTimingModel();
  void writeTimingModel(const char *cell_name,
                        const char *filename);
  void makeTimingModel(const char *cell_name,
                       const char *filename);
  void writeLibertyFile(const char *filename);

private:
  void makeLibrary(const char *cell_name,
                   const char *filename);
  void makeCell(const char *cell_name,
                 const char *filename);
  void makePorts();
  void findInputToOutputPaths();
  void findInputSetupHolds();
  void findClkedOutputPaths();

  Sta *sta_;
  LibertyLibrary *library_;
  LibertyCell *cell_;
  const Corner *corner_;
  MinMax *min_max_;
  LibertyBuilder *lib_builder_;
};

void
writeTimingModel(const char *cell_name,
                 const char *filename,
                 const Corner *corner,
                 Sta *sta);

} // namespace
