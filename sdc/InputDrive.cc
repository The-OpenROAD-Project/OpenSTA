// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

#include "Machine.hh"
#include "InputDrive.hh"

namespace sta {

InputDrive::InputDrive()
{
  for (auto tr_index : TransRiseFall::rangeIndex()) {
    for (auto mm_index : MinMax::rangeIndex())
      drive_cells_[tr_index][mm_index] = nullptr;
  }
}

InputDrive::~InputDrive()
{
  for (auto tr_index : TransRiseFall::rangeIndex()) {
    for (auto mm_index : MinMax::rangeIndex()) {
      InputDriveCell *drive_cell = drive_cells_[tr_index][mm_index];
      delete drive_cell;
    }
  }
}

void
InputDrive::setSlew(const TransRiseFallBoth *tr,
		    const MinMaxAll *min_max,
		    float slew)
{
  slews_.setValue(tr, min_max, slew);
}

void
InputDrive::setDriveResistance(const TransRiseFallBoth *tr,
			       const MinMaxAll *min_max,
			       float res)
{
  drive_resistances_.setValue(tr, min_max, res);
}

void
InputDrive::driveResistance(const TransRiseFall *tr,
			    const MinMax *min_max,
			    float &res,
			    bool &exists)
{
  drive_resistances_.value(tr, min_max, res, exists);
}

bool
InputDrive::hasDriveResistance(const TransRiseFall *tr, const MinMax *min_max)
{
  return drive_resistances_.hasValue(tr, min_max);
}

bool
InputDrive::driveResistanceMinMaxEqual(const TransRiseFall *tr)
{
  float min_res, max_res;
  bool min_exists, max_exists;
  drive_resistances_.value(tr, MinMax::min(), min_res, min_exists);
  drive_resistances_.value(tr, MinMax::max(), max_res, max_exists);
  return min_exists && max_exists && min_res == max_res;
}

void
InputDrive::setDriveCell(LibertyLibrary *library,
			 LibertyCell *cell,
			 LibertyPort *from_port,
			 float *from_slews,
			 LibertyPort *to_port,
			 const TransRiseFallBoth *tr,
			 const MinMaxAll *min_max)
{
  for (auto tr_index : tr->rangeIndex()) {
    for (auto mm_index : min_max->rangeIndex()) {
      InputDriveCell *drive = drive_cells_[tr_index][mm_index];
      if (drive) {
	drive->setLibrary(library);
	drive->setCell(cell);
	drive->setFromPort(from_port);
	drive->setFromSlews(from_slews);
	drive->setToPort(to_port);
      }
      else {
	drive = new InputDriveCell(library, cell, from_port,
				   from_slews, to_port);
	drive_cells_[tr_index][mm_index] = drive;
      }
    }
  }
}

void
InputDrive::driveCell(const TransRiseFall *tr,
		      const MinMax *min_max,
		      LibertyCell *&cell,
		      LibertyPort *&from_port,
		      float *&from_slews,
		      LibertyPort *&to_port)
{
  InputDriveCell *drive = drive_cells_[tr->index()][min_max->index()];
  if (drive) {
    cell = drive->cell();
    from_port = drive->fromPort();
    from_slews = drive->fromSlews();
    to_port = drive->toPort();
  }
  else
    cell = nullptr;
}

InputDriveCell *
InputDrive::driveCell(const TransRiseFall *tr,
		      const MinMax *min_max)
{
  return drive_cells_[tr->index()][min_max->index()];
}

bool
InputDrive::hasDriveCell(const TransRiseFall *tr,
			 const MinMax *min_max)
{
  return drive_cells_[tr->index()][min_max->index()] != nullptr;
}

bool
InputDrive::driveCellsEqual()
{
  int rise_index = TransRiseFall::riseIndex();
  int fall_index = TransRiseFall::fallIndex();
  int min_index = MinMax::minIndex();
  int max_index = MinMax::maxIndex();
  InputDriveCell *drive1 = drive_cells_[rise_index][min_index];
  InputDriveCell *drive2 = drive_cells_[rise_index][max_index];
  InputDriveCell *drive3 = drive_cells_[fall_index][min_index];
  InputDriveCell *drive4 = drive_cells_[fall_index][max_index];
  return drive1->equal(drive2)
    && drive1->equal(drive3)
    && drive1->equal(drive4);
}

void
InputDrive::slew(const TransRiseFall *tr,
		 const MinMax *min_max,
		 float &slew,
		 bool &exists)
{
  slews_.value(tr, min_max, slew, exists);
}

////////////////////////////////////////////////////////////////

InputDriveCell::InputDriveCell(LibertyLibrary *library,
			       LibertyCell *cell,
			       LibertyPort *from_port,
			       float *from_slews,
			       LibertyPort *to_port) :
  library_(library),
  cell_(cell),
  from_port_(from_port),
  to_port_(to_port)
{
  setFromSlews(from_slews);
}

void
InputDriveCell::setLibrary(LibertyLibrary *library)
{
  library_ = library;
}

void
InputDriveCell::setCell(LibertyCell *cell)
{
  cell_ = cell;
}

void
InputDriveCell::setFromPort(LibertyPort *from_port)
{
  from_port_ = from_port;
}

void
InputDriveCell::setToPort(LibertyPort *to_port)
{
  to_port_ = to_port;
}

void
InputDriveCell::setFromSlews(float *from_slews)
{
  for (auto tr_index : TransRiseFall::rangeIndex())
    from_slews_[tr_index] = from_slews[tr_index];
}

bool
InputDriveCell::equal(InputDriveCell *drive) const
{
  int rise_index = TransRiseFall::riseIndex();
  int fall_index = TransRiseFall::fallIndex();
  return cell_ == drive->cell_
    && from_port_ == drive->from_port_
    && from_slews_[rise_index] == drive->from_slews_[rise_index]
    && from_slews_[fall_index] == drive->from_slews_[fall_index]
    && to_port_ == drive->to_port_;
}

} // namespace
