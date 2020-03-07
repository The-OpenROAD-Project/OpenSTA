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
#include "Sequential.hh"

namespace sta {

Sequential::Sequential(bool is_register,
		       FuncExpr *clock,
		       FuncExpr *data,
		       FuncExpr *clear,
		       FuncExpr *preset,
		       LogicValue clr_preset_out,
		       LogicValue clr_preset_out_inv,
		       LibertyPort *output,
		       LibertyPort *output_inv) :
  is_register_(is_register),
  clock_(clock),
  data_(data),
  clear_(clear),
  preset_(preset),
  clr_preset_out_(clr_preset_out),
  clr_preset_out_inv_(clr_preset_out_inv),
  output_(output),
  output_inv_(output_inv)
{
}

Sequential::~Sequential()
{
  if (clock_)
    clock_->deleteSubexprs();
  if (data_)
    data_->deleteSubexprs();
  if (clear_)
    clear_->deleteSubexprs();
  if (preset_)
    preset_->deleteSubexprs();
}

} // namespace
