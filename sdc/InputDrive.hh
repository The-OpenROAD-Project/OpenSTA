// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include "DisallowCopyAssign.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "MinMax.hh"
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
  void setDriveCell(LibertyLibrary *library,
		    LibertyCell *cell,
		    LibertyPort *from_port,
		    float *from_slews,
		    LibertyPort *to_port,
		    const RiseFallBoth *rf,
		    const MinMaxAll *min_max);
  void driveCell(const RiseFall *rf,
		 const MinMax *min_max,
		 LibertyCell *&cell,
		 LibertyPort *&from_port,
		 float *&from_slews,
		 LibertyPort *&to_port);
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
  DISALLOW_COPY_AND_ASSIGN(InputDrive);

  RiseFallMinMax slews_;
  RiseFallMinMax drive_resistances_;
  // Separate rise/fall/min/max drive cells.
  InputDriveCell *drive_cells_[RiseFall::index_count][MinMax::index_count];
};

class InputDriveCell
{
public:
  InputDriveCell(LibertyLibrary *library,
		 LibertyCell *cell,
		 LibertyPort *from_port,
		 float *from_slews,
		 LibertyPort *to_port);
  LibertyLibrary *library() const { return library_; }
  void setLibrary(LibertyLibrary *library);
  LibertyCell *cell() const { return cell_; }
  void setCell(LibertyCell *cell);
  LibertyPort *fromPort() const { return from_port_; }
  void setFromPort(LibertyPort *from_port);
  float *fromSlews() { return from_slews_; }
  void setFromSlews(float *from_slews);
  LibertyPort *toPort() const { return to_port_; }
  void setToPort(LibertyPort *to_port);
  bool equal(InputDriveCell *drive) const;

private:
  DISALLOW_COPY_AND_ASSIGN(InputDriveCell);

  LibertyLibrary *library_;
  LibertyCell *cell_;
  LibertyPort *from_port_;
  float from_slews_[RiseFall::index_count];
  LibertyPort *to_port_;
};

} // namespace
