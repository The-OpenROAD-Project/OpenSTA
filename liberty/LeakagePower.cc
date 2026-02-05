// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "LeakagePower.hh"

#include "FuncExpr.hh"
#include "TableModel.hh"
#include "Liberty.hh"

namespace sta {

LeakagePower::LeakagePower(LibertyCell *cell,
                           LibertyPort *related_pg_port,
                           FuncExpr *when,
                           float power) :
  cell_(cell),
  related_pg_port_(related_pg_port),
  when_(when),
  power_(power)
{
}

LeakagePower::LeakagePower(LeakagePower &&other) noexcept
{
  cell_ = other.cell_;
  related_pg_port_ = other.related_pg_port_;
  when_ = other.when_;
  other.when_ = nullptr;
  power_ = other.power_;
}

LeakagePower::~LeakagePower()
{
  delete when_;
}

} // namespace
>>>>>>>

namespace sta {

LeakagePower::LeakagePower(LibertyCell *cell,
                           LibertyPort *related_pg_port,
                           FuncExpr *when,
                           float power) :
  cell_(cell),
  related_pg_port_(related_pg_port),
  when_(when),
  power_(power)
{
}

LeakagePower::~LeakagePower()
{
  if (when_)
    when_->deleteSubexprs();
}

} // namespace
