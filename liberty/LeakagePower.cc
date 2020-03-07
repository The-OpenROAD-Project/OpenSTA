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

#include "Machine.hh"
#include "FuncExpr.hh"
#include "TableModel.hh"
#include "Liberty.hh"
#include "LeakagePower.hh"

namespace sta {

LeakagePowerAttrs::LeakagePowerAttrs() :
  when_(nullptr),
  power_(0.0)
{
}

void
LeakagePowerAttrs::setPower(float power)
{
  power_ = power;
}

////////////////////////////////////////////////////////////////

LeakagePower::LeakagePower(LibertyCell *cell,
			   LeakagePowerAttrs *attrs) :
  cell_(cell),
  when_(attrs->when()),
  power_(attrs->power())
{
  cell->addLeakagePower(this);
}

LeakagePower::~LeakagePower()
{
  if (when_)
    when_->deleteSubexprs();
}

} // namespace
