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

#include "SdcClass.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"

namespace sta {

typedef Set<FuncExpr*> FuncExprSet;

class GatedClk : public StaState
{
public:
  GatedClk(const StaState *sta);

  bool isGatedClkEnable(Vertex *vertex) const;
  void isGatedClkEnable(Vertex *enable_vertex,
			bool &is_gated_clk_enable,
			const Pin *&clk_pin,
			LogicValue &logic_active_value) const;
  void gatedClkEnables(Vertex *clk_vertex,
		       // Return value.
		       PinSet &enable_pins);
  RiseFall *gatedClkActiveTrans(LogicValue active_value,
				     const MinMax *min_max) const;

protected:
  void isClkGatingFunc(FuncExpr *func,
		       LibertyPort *enable_port,
		       LibertyPort *clk_port,
		       bool &is_clk_gate,
		       LogicValue &logic_value) const;
  void functionClkOperands(FuncExpr *root_expr,
			   FuncExpr *curr_expr,
			   FuncExprSet &funcs) const;
};

} // namespace
