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
#include "NetworkClass.hh"
#include "LibertyClass.hh"

namespace sta {

// Register/Latch
class Sequential
{
public:
  ~Sequential();
  bool isLatch() const { return !is_register_; }
  bool isRegister() const { return is_register_; }
  FuncExpr *clock() const { return clock_; }
  FuncExpr *data() const { return data_; }
  FuncExpr *clear() const { return clear_; }
  FuncExpr *preset() const { return preset_; }
  // State of output when clear and preset are both true.
  LogicValue clearPresetOutput() const { return clr_preset_out_; }
  // State of outputInv when clear and preset are both true.
  LogicValue clearPresetOutputInv() const {return clr_preset_out_inv_;}
  LibertyPort *output() const { return output_; }
  LibertyPort *outputInv() const { return output_inv_; }

protected:
  // clock/data are:
  //   clocked_on/next_state for registers
  //   enable/data for latches
  Sequential(bool is_register,
	     FuncExpr *clock,
	     FuncExpr *data,
	     FuncExpr *clear,
	     FuncExpr *preset,
	     LogicValue clr_preset_out,
	     LogicValue clr_preset_out_inv,
	     LibertyPort *output,
	     LibertyPort *output_inv);

  bool is_register_;
  FuncExpr *clock_;
  FuncExpr *data_;
  FuncExpr *clear_;
  FuncExpr *preset_;
  LogicValue clr_preset_out_;
  LogicValue clr_preset_out_inv_;
  LibertyPort *output_;
  LibertyPort *output_inv_;

private:
  DISALLOW_COPY_AND_ASSIGN(Sequential);

  friend class LibertyCell;
};

} // namespace
