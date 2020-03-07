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

// Translate from spf/spef namespace to sta namespace.
// Caller owns the result string.
char *
spefToSta(const char *token, char spef_divider,
	  char path_escape, char path_divider);
// Translate from sta namespace to spf/spef namespace.
// Caller owns the result string.
char *
staToSpef(const char *token, char spef_divider,
	  char path_divider, char path_escape);


} // namespace
