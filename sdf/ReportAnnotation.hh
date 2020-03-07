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

namespace sta {

void
reportAnnotatedDelay(bool report_cells,
		     bool report_nets,
		     bool report_in_ports,
		     bool report_out_ports,
		     int max_lines,
		     bool list_annotated,
		     bool list_unannotated,
		     bool report_constant_arcs,
		     StaState *sta);
void
reportAnnotatedCheck(bool report_setup,
		     bool report_hold,
		     bool report_recovery,
		     bool report_removal,
		     bool report_nochange,
		     bool report_width,
		     bool report_period,
		     bool report_max_skew,
		     int max_lines,
		     bool list_annotated,
		     bool list_unannotated,
		     bool report_constant_arcs,
		     StaState *sta);

} // namespace
