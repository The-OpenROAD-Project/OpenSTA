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

#include "FindRoot.hh"

#include <algorithm> // abs

namespace sta {

using std::abs;

double
findRoot(FindRootFunc func,
	 double x1,
	 double x2,
	 double x_tol,
	 int max_iter,
         // Return value.
         bool &fail)
{
  double y1, y2, dy1;
  func(x1, y1, dy1);
  func(x2, y2, dy1);
  return findRoot(func, x1, y1, x2, y2, x_tol, max_iter, fail);
}

double
findRoot(FindRootFunc func,
	 double x1,
	 double y1,
         double x2,
	 double y2,
         double x_tol,
	 int max_iter,
         // Return value.
         bool &fail)
{
  if ((y1 > 0.0 && y2 > 0.0) || (y1 < 0.0 && y2 < 0.0)) {
    // Initial bounds do not surround a root.
    fail = true;
    return 0.0;
  }

  if (y1 == 0.0) {
    fail = false;
    return x1;
  }

  if (y2 == 0.0) {
    fail = false;
    return x2;
  }

  if (y1 > 0.0)
    // Swap x1/x2 so func(x1) < 0.
    std::swap(x1, x2);
  double root = (x1 + x2) * 0.5;
  double dx_prev = abs(x2 - x1);
  double dx = dx_prev;
  double y, dy;
  func(root, y, dy);
  for (int iter = 0; iter < max_iter; iter++) {
    // Newton/raphson out of range.
    if ((((root - x2) * dy - y) * ((root - x1) * dy - y) > 0.0)
	// Not decreasing fast enough.
	|| (abs(2.0 * y) > abs(dx_prev * dy))) {
      // Bisect x1/x2 interval.
      dx_prev = dx;
      dx = (x2 - x1) * 0.5;
      root = x1 + dx;
    }
    else {
      dx_prev = dx;
      dx = y / dy;
      root -= dx;
    }
    if (abs(dx) <= x_tol * abs(root)) {
      // Converged.
      fail = false;
      return root;
    }

    func(root, y, dy);
    if (y < 0.0)
      x1 = root;
    else
      x2 = root;
  }
  fail = true;
  return root;
}

} // namespace
