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

#pragma once

#include "MinMax.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "RiseFallMinMax.hh"

namespace sta {

class InputDriveCell;

// Input drive description from
//  set_driving_cell
//  set_drive
//  set_input_transition
class InputDrive
{
public:
  explicit InputDrive();
  ~InputDrive();
  void setSlew(const RiseFallBoth *rf,
	       const MinMaxAll *min_max,
	       float slew);
  void setDriveResistance(const RiseFallBoth *rf,
			  const MinMaxAll *min_max,
			  float res);
  void driveResistance(const RiseFall *rf,
		       const MinMax *min_max,
		       float &res,
		       bool &exists);
  bool hasDriveResistance(const RiseFall *rf,
			  const MinMax *min_max);
  bool driveResistanceMinMaxEqual(const RiseFall *rf);
  void setDriveCell(const LibertyLibrary *library,
		    const LibertyCell *cell,
		    const LibertyPort *from_port,
		    float *from_slews,
		    const LibertyPort *to_port,
		    const RiseFallBoth *rf,
		    const MinMaxAll *min_max);
  void driveCell(const RiseFall *rf,
		 const MinMax *min_max,
                 // Return values.
		 const LibertyCell *&cell,
		 const LibertyPort *&from_port,
		 float *&from_slews,
		 const LibertyPort *&to_port);
  InputDriveCell *driveCell(const RiseFall *rf,
			    const MinMax *min_max);
  bool hasDriveCell(const RiseFall *rf,
		    const MinMax *min_max);
  // True if rise/fall/min/max drive cells are equal.
  bool driveCellsEqual();
  void slew(const RiseFall *rf,
	    const MinMax *min_max,
	    float &slew,
	    bool &exists);
  RiseFallMinMax *slews() { return &slews_; }

private:
  RiseFallMinMax slews_;
  RiseFallMinMax drive_resistances_;
  // Separate rise/fall/min/max drive cells.
  InputDriveCell *drive_cells_[RiseFall::index_count][MinMax::index_count];
};

class InputDriveCell
{
public:
  InputDriveCell(const LibertyLibrary *library,
		 const LibertyCell *cell,
		 const LibertyPort *from_port,
		 float *from_slews,
		 const LibertyPort *to_port);
  const LibertyLibrary *library() const { return library_; }
  void setLibrary(const LibertyLibrary *library);
  const LibertyCell *cell() const { return cell_; }
  void setCell(const LibertyCell *cell);
  const LibertyPort *fromPort() const { return from_port_; }
  void setFromPort(const LibertyPort *from_port);
  float *fromSlews() { return from_slews_; }
  void setFromSlews(float *from_slews);
  const LibertyPort *toPort() const { return to_port_; }
  void setToPort(const LibertyPort *to_port);
  bool equal(const InputDriveCell *drive) const;

private:
  const LibertyLibrary *library_;
  const LibertyCell *cell_;
  const LibertyPort *from_port_;
  float from_slews_[RiseFall::index_count];
  const LibertyPort *to_port_;
};

} // namespace
