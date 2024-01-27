// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#pragma once

#include <functional>

namespace sta {

typedef const std::function<void (double x,
                                  // Return values.
                                  double &y,
                                  double &dy)> FindRootFunc;

double
findRoot(FindRootFunc func,
	 double x1,
	 double x2,
	 double x_tol,
	 int max_iter,
         // Return value.
         bool &fail);

double
findRoot(FindRootFunc func,
	 double x1,
	 double y1,
         double x2,
	 double y2,
	 double x_tol,
	 int max_iter,
         // Return value.
         bool &fail);

} // namespace
