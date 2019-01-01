%module sta

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

%include "../tcl/StaException.i"
%include "../tcl/StaTcl.i"
%include "../verilog/Verilog.i"
%include "../tcl/NetworkEdit.i"
%include "../sdf/Sdf.i"
%include "../dcalc/DelayCalc.i"
%include "../parasitics/Parasitics.i"
