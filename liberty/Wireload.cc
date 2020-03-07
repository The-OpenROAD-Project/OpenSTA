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

#include <algorithm>
#include "Machine.hh"
#include "DisallowCopyAssign.hh"
#include "StringUtil.hh"
#include "Liberty.hh"
#include "Wireload.hh"

namespace sta {

Wireload::Wireload(const char *name,
		   LibertyLibrary *library) :
  name_(stringCopy(name)),
  library_(library),
  area_(0.0F),
  resistance_(0.0F),
  capacitance_(0.0F),
  slope_(0.0F)
{
}

Wireload::Wireload(const char *name,
		   LibertyLibrary *library,
		   float area,
		   float resistance,
		   float capacitance,
		   float slope) :
  name_(stringCopy(name)),
  library_(library),
  area_(area),
  resistance_(resistance),
  capacitance_(capacitance),
  slope_(slope)
{
}

Wireload::~Wireload()
{
  fanout_lengths_.deleteContents();
  stringDelete(name_);
}

void
Wireload::setArea(float area)
{
  area_ = area;
}

void
Wireload::setResistance(float res)
{
  resistance_ = res;
}

void
Wireload::setCapacitance(float cap)
{
  capacitance_ = cap;
}

void
Wireload::setSlope(float slope)
{
  slope_ = slope;
}

struct FanoutLess
{
  bool operator()(FanoutLength *fanout1,
		  FanoutLength *fanout2) const
  {
    return fanout1->first < fanout2->first;
  }
};

void
Wireload::addFanoutLength(float fanout,
			  float length)
{
  FanoutLength *fanout_length = new FanoutLength(fanout, length);
  fanout_lengths_.push_back(fanout_length);
  // Keep fanouts sorted for lookup.
  if (fanout_lengths_.size() > 1
      && fanout < (fanout_lengths_[fanout_lengths_.size() - 2])->first)
    sort(fanout_lengths_, FanoutLess());
}

void
Wireload::findWireload(float fanout,
		       const OperatingConditions *op_cond,
		       float &cap,
		       float &res) const
{
  size_t size = fanout_lengths_.size();
  float length;
  if (size == 0)
    length = 0;
  else {
    size_t max = size - 1;
    float fanout0 = fanout_lengths_[0]->first;
    float fanout_max = fanout_lengths_[max]->first;
    if (fanout < fanout0) {
      // Extrapolate from lowest fanout entry.
      length = fanout_lengths_[0]->second - (fanout0 - fanout) * slope_;
      if (length < 0)
	length = 0;
    }
    else if (fanout == fanout0)
      length = fanout_lengths_[0]->second;
    else if (fanout >= fanout_max)
      // Extrapolate from max fanout entry.
      length = fanout_lengths_[max]->second + (fanout - fanout_max) * slope_;
    else {
      // Bisection search.
      int lower = -1;
      int upper = size;
      while (upper - lower > 1) {
	int mid = (upper + lower) >> 1;
	if (fanout >= fanout_lengths_[mid]->first)
	  lower = mid;
	else
	  upper = mid;
      }
      // Interpolate between lower and lower+1 entries.
      float fanout1 = fanout_lengths_[lower]->first;
      float fanout2 = fanout_lengths_[lower+1]->first;
      float l1 = fanout_lengths_[lower]->second;
      float l2 = fanout_lengths_[lower+1]->second;
      length = l1 + (l2 - l1) * (fanout - fanout1) / (fanout2 - fanout1);
    }
  }
  // Scale resistance and capacitance.
  cap = length * capacitance_
    * library_->scaleFactor(ScaleFactorType::wire_cap, op_cond);
  res = length * resistance_
    * library_->scaleFactor(ScaleFactorType::wire_res, op_cond);
}

////////////////////////////////////////////////////////////////

class WireloadForArea
{
public:
  WireloadForArea(float min_area,
		  float max_area,
		  const Wireload *wireload);
  float minArea() const { return min_area_; }
  float maxArea() const { return max_area_; }
  const Wireload *wireload() const { return wireload_; }

private:
  DISALLOW_COPY_AND_ASSIGN(WireloadForArea);

  float min_area_;
  float max_area_;
  const Wireload *wireload_;
};

WireloadForArea::WireloadForArea(float min_area,
				 float max_area,
				 const Wireload *wireload) :
  min_area_(min_area),
  max_area_(max_area),
  wireload_(wireload)
{
}

WireloadSelection::WireloadSelection(const char *name) :
  name_(stringCopy(name))
{
}

WireloadSelection::~WireloadSelection()
{
  wireloads_.deleteContents();
  stringDelete(name_);
}

struct WireloadForAreaMinLess
{
  bool operator()(WireloadForArea *wireload1,
		  WireloadForArea *wireload2) const
  {
    return wireload1->minArea() < wireload2->minArea();
  }
};

void
WireloadSelection::addWireloadFromArea(float min_area,
				       float max_area,
				       const Wireload *wireload)
{
  WireloadForArea *wireload_area = new WireloadForArea(min_area, max_area,
						       wireload);
  wireloads_.push_back(wireload_area);
  // Keep wireloads sorted by area for lookup.
  if (wireloads_.size() > 1
      && min_area < (wireloads_[wireloads_.size() - 2])->minArea())
    sort(wireloads_, WireloadForAreaMinLess());
}

// Bisection search.
const Wireload *
WireloadSelection::findWireload(float area) const
{
  int max = static_cast<int>(wireloads_.size()) - 1;
  int lower = -1;
  int upper = max + 1;
  while (upper - lower > 1) {
    int mid = (upper + lower) >> 1;
    if (area >= wireloads_[mid]->minArea())
      lower = mid;
    else
      upper = mid;
  }
  float area0 = wireloads_[0]->minArea();
  float area_max = wireloads_[max]->minArea();
  if (area <= area0)
    return wireloads_[0]->wireload();
  else if (area >= area_max)
    return wireloads_[max]->wireload();
  else
    return wireloads_[lower]->wireload();
}

////////////////////////////////////////////////////////////////

const char *
wireloadTreeString(WireloadTree tree)
{
  switch (tree) {
  case WireloadTree::worst_case:
    return "worst_case_tree";
  case WireloadTree::best_case:
    return "best_case_tree";
  case WireloadTree::balanced:
    return "balanced_tree";
  case WireloadTree::unknown:
    return "unknown";
  }
  // Prevent warnings from lame compilers.
  return "?";
}

WireloadTree
stringWireloadTree(const char *wire_load_type)
{
  if (stringEq(wire_load_type, "worst_case_tree"))
    return WireloadTree::worst_case;
  else if (stringEq(wire_load_type, "best_case_tree"))
    return WireloadTree::best_case;
  else if (stringEq(wire_load_type, "balanced_tree"))
    return WireloadTree::balanced;
  else
    return WireloadTree::unknown;
}

const char *
wireloadModeString(WireloadMode wire_load_mode)
{
  switch (wire_load_mode) {
  case WireloadMode::top:
    return "top";
  case WireloadMode::enclosed:
    return "enclosed";
  case WireloadMode::segmented:
    return "segmented";
  case WireloadMode::unknown:
    return "unknown";
  }
  // Prevent warnings from lame compilers.
  return "?";
}

WireloadMode
stringWireloadMode(const char *wire_load_mode)
{
  if (stringEq(wire_load_mode, "top"))
    return WireloadMode::top;
  else if (stringEq(wire_load_mode, "enclosed"))
    return WireloadMode::enclosed;
  else if (stringEq(wire_load_mode, "segmented"))
    return WireloadMode::segmented;
  else
    return WireloadMode::unknown;
}

} // namespace
