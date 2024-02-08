// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#include "Corner.hh"

#include "Sdc.hh"
#include "Parasitics.hh"
#include "DcalcAnalysisPt.hh"
#include "PathAnalysisPt.hh"

namespace sta {

Corners::Corners(StaState *sta) :
  StaState(sta)
{
}

Corners::~Corners()
{
  clear();
}

void
Corners::clear()
{
  corners_.deleteContentsClear();
  corner_map_.clear();
  dcalc_analysis_pts_.deleteContentsClear();
  path_analysis_pts_.deleteContentsClear();
  parasitic_analysis_pts_.deleteContentsClear();
}

int
Corners::count() const
{
  return corners_.size();
}

bool
Corners::multiCorner() const
{
  return corners_.size() > 1;
}

Corner *
Corners::findCorner(const char *corner_name)
{
  return corner_map_.findKey(corner_name);
}

Corner *
Corners::findCorner(int corner_index)
{
  return corners_[corner_index];
}

void
Corners::analysisTypeChanged()
{
  makeAnalysisPts();
}

void
Corners::operatingConditionsChanged()
{
  for (DcalcAnalysisPt *dcalc_ap : dcalc_analysis_pts_) {
    const MinMax *min_max = dcalc_ap->constraintMinMax();
    const OperatingConditions *op_cond =
      sdc_->operatingConditions(min_max);
    dcalc_ap->setOperatingConditions(op_cond);
  }
}

void
Corners::makeCorners(StringSet *corner_names)
{
  clear();
  int index = 0;
  for (const char *name : *corner_names) {
    Corner *corner = new Corner(name, index);
    corners_.push_back(corner);
    // Use the copied name in the map.
    corner_map_[corner->name()] = corner;
    index++;
  }
  makeAnalysisPts();
}

void
Corners::copy(Corners *corners)
{
  clear();
  int index = 0;
  for (Corner *orig : corners->corners_) {
    Corner *corner = new Corner(orig->name(), index);
    corners_.push_back(corner);
    // Use the copied name in the map.
    corner_map_[corner->name()] = corner;
    index++;
  }
  makeAnalysisPts();
  
  for (ParasiticAnalysisPt *orig_ap : corners->parasitic_analysis_pts_) {
    ParasiticAnalysisPt *ap = new ParasiticAnalysisPt(orig_ap->name(),
                                                      orig_ap->index(),
                                                      orig_ap->indexMax());
    parasitic_analysis_pts_.push_back(ap);
  }

  for (size_t i = 0; i < corners->corners_.size(); i++) {
    Corner *orig = corners->corners_[i];
    Corner *corner = corners_[i];
    corner->parasitic_analysis_pts_ = orig->parasitic_analysis_pts_;
  }
}

void
Corners::makeParasiticAnalysisPts(bool per_corner)
{
  parasitic_analysis_pts_.deleteContentsClear();
  if (per_corner) {
    // per corner, per min/max
    parasitic_analysis_pts_.resize(corners_.size() * MinMax::index_count);
    for (Corner *corner : corners_) {
      corner->setParasiticAnalysisPtcount(MinMax::index_count);
      for (MinMax *min_max : MinMax::range()) {
        int mm_index = min_max->index();
        int ap_index = corner->index() * MinMax::index_count + mm_index;
        int ap_index_max = corner->index() * MinMax::index_count
          + MinMax::max()->index();
        string ap_name = corner->name();
        ap_name += "_";
        ap_name += min_max->asString();
        ParasiticAnalysisPt *ap = new ParasiticAnalysisPt(ap_name.c_str(),
                                                          ap_index, ap_index_max);
        parasitic_analysis_pts_[ap_index] = ap;
        corner->setParasiticAP(ap, mm_index);
      }
    }
  }
  else {
    // shared corner, per min/max
    parasitic_analysis_pts_.resize(MinMax::index_count);
    int ap_index_max = MinMax::max()->index();
    for (MinMax *min_max : MinMax::range()) {
      int mm_index = min_max->index();
      int ap_index = mm_index;
      ParasiticAnalysisPt *ap = new ParasiticAnalysisPt(min_max->asString(),
							ap_index,
                                                        ap_index_max);
      parasitic_analysis_pts_[ap_index] = ap;
      for (Corner *corner : corners_) {
        corner->setParasiticAnalysisPtcount(MinMax::index_count);
        corner->setParasiticAP(ap, mm_index);
      }
    }
  }
}

void
Corners::makeAnalysisPts()
{
  dcalc_analysis_pts_.deleteContentsClear();
  path_analysis_pts_.deleteContentsClear();

  for (Corner *corner : corners_) {
    makeDcalcAnalysisPts(corner);
    makePathAnalysisPts(corner);
  }
}

void
Corners::makeDcalcAnalysisPts(Corner *corner)
{
  DcalcAnalysisPt *min_ap, *max_ap;
  switch (sdc_->analysisType()) {
  case AnalysisType::single:
    corner->setDcalcAnalysisPtcount(1);
    makeDcalcAnalysisPt(corner, MinMax::max(), MinMax::min());
    break;
  case AnalysisType::bc_wc:
    corner->setDcalcAnalysisPtcount(2);
    min_ap = makeDcalcAnalysisPt(corner, MinMax::min(), MinMax::min());
    max_ap = makeDcalcAnalysisPt(corner, MinMax::max(), MinMax::max());
    min_ap->setCheckClkSlewIndex(min_ap->index());
    max_ap->setCheckClkSlewIndex(max_ap->index());
    break;
  case AnalysisType::ocv:
    corner->setDcalcAnalysisPtcount(2);
    min_ap = makeDcalcAnalysisPt(corner, MinMax::min(), MinMax::max());
    max_ap = makeDcalcAnalysisPt(corner, MinMax::max(), MinMax::min());
    min_ap->setCheckClkSlewIndex(max_ap->index());
    max_ap->setCheckClkSlewIndex(min_ap->index());
    break;
  }
}

DcalcAnalysisPt *
Corners::makeDcalcAnalysisPt(Corner *corner,
			     const MinMax *min_max,
			     const MinMax *check_clk_slew_min_max)
{
  OperatingConditions *op_cond = sdc_->operatingConditions(min_max);
  DcalcAnalysisPt *dcalc_ap = new DcalcAnalysisPt(corner,
						  dcalc_analysis_pts_.size(),
						  op_cond, min_max,
						  check_clk_slew_min_max);
  dcalc_analysis_pts_.push_back(dcalc_ap);
  corner->addDcalcAP(dcalc_ap);
  return dcalc_ap;
}

// The clock insertion delay (source latency) required for setup and
// hold checks is:
//
// hold check
// report_timing -delay_type min
//          path insertion pll_delay
//  src clk  min   early    max
//  tgt clk  max   late     min
//
// setup check
// report_timing -delay_type max
//          path insertion pll_delay
//  src clk  max   late     min
//  tgt clk  min   early    max
//
// For analysis type single or bc_wc only one path is required, but as
// shown above both early and late insertion delays are required.
// To find propagated generated clock insertion delays both early and
// late clock network paths are required. Thus, analysis type single
// makes min and max analysis points.
// Only one of them is enabled to "report paths".
void
Corners::makePathAnalysisPts(Corner *corner)
{
  DcalcAnalysisPt *dcalc_ap_min = corner->findDcalcAnalysisPt(MinMax::min());
  DcalcAnalysisPt *dcalc_ap_max = corner->findDcalcAnalysisPt(MinMax::max());
  switch (sdc_->analysisType()) {
  case AnalysisType::single:
  case AnalysisType::bc_wc:
    makePathAnalysisPts(corner, false, dcalc_ap_min, dcalc_ap_max);
    break;
  case AnalysisType::ocv:
    makePathAnalysisPts(corner, true, dcalc_ap_min, dcalc_ap_max);
    break;
  }
}


void
Corners::makePathAnalysisPts(Corner *corner,
			     bool swap_clk_min_max,
			     DcalcAnalysisPt *dcalc_ap_min,
			     DcalcAnalysisPt *dcalc_ap_max)
{
  PathAnalysisPt *min_ap = new PathAnalysisPt(corner,
					      path_analysis_pts_.size(),
					      MinMax::min(), dcalc_ap_min);
  path_analysis_pts_.push_back(min_ap);
  corner->addPathAP(min_ap);

  PathAnalysisPt *max_ap = new PathAnalysisPt(corner,
					      path_analysis_pts_.size(),
					      MinMax::max(), dcalc_ap_max);
  path_analysis_pts_.push_back(max_ap);
  corner->addPathAP(max_ap);

  if (swap_clk_min_max) {
    min_ap->setTgtClkAnalysisPt(max_ap);
    max_ap->setTgtClkAnalysisPt(min_ap);
  }
  else {
    min_ap->setTgtClkAnalysisPt(min_ap);
    max_ap->setTgtClkAnalysisPt(max_ap);
  }

  min_ap->setInsertionAnalysisPt(MinMax::min(), min_ap);
  min_ap->setInsertionAnalysisPt(MinMax::max(), max_ap);
  max_ap->setInsertionAnalysisPt(MinMax::min(), min_ap);
  max_ap->setInsertionAnalysisPt(MinMax::max(), max_ap);
}

int
Corners::parasiticAnalysisPtCount() const
{
  return parasitic_analysis_pts_.size();
}

ParasiticAnalysisPtSeq &
Corners::parasiticAnalysisPts()
{
  return parasitic_analysis_pts_;
}

DcalcAPIndex
Corners::dcalcAnalysisPtCount() const
{
  return dcalc_analysis_pts_.size();
}

DcalcAnalysisPtSeq &
Corners::dcalcAnalysisPts()
{
  return dcalc_analysis_pts_;
}

const DcalcAnalysisPtSeq &
Corners::dcalcAnalysisPts() const
{
  return dcalc_analysis_pts_;
}

PathAPIndex
Corners::pathAnalysisPtCount() const
{
  return path_analysis_pts_.size();
}

PathAnalysisPtSeq &
Corners::pathAnalysisPts()
{
  return path_analysis_pts_;
}

const PathAnalysisPtSeq &
Corners::pathAnalysisPts() const
{
  return path_analysis_pts_;
}

PathAnalysisPt *
Corners::findPathAnalysisPt(PathAPIndex path_index) const
{
  return path_analysis_pts_[path_index];
}

////////////////////////////////////////////////////////////////

Corner::Corner(const char *name,
	       int index) :
  name_(stringCopy(name)),
  index_(index),
  path_analysis_pts_(MinMax::index_count)
{
}

Corner::~Corner()
{
  stringDelete(name_);
}

ParasiticAnalysisPt *
Corner::findParasiticAnalysisPt(const MinMax *min_max) const
{
  int ap_count = parasitic_analysis_pts_.size();
  if (ap_count == 0)
    return nullptr;
  else if (ap_count == 1)
    return parasitic_analysis_pts_[0];
  else if (ap_count == 2)
    return parasitic_analysis_pts_[min_max->index()];
  else {
    criticalError(246, "unknown parasitic analysis point count");
    return nullptr;
  }
}

void
Corner::setParasiticAnalysisPtcount(int ap_count)
{
  parasitic_analysis_pts_.resize(ap_count);
}

void 
Corner::setParasiticAP(ParasiticAnalysisPt *ap,
                       int mm_index)
{
  parasitic_analysis_pts_[mm_index] = ap;
}

void
Corner::setDcalcAnalysisPtcount(DcalcAPIndex ap_count)
{
  dcalc_analysis_pts_.resize(ap_count);
}

void
Corner::addDcalcAP(DcalcAnalysisPt *dcalc_ap)
{
  if (dcalc_analysis_pts_.size() == 1)
    dcalc_analysis_pts_[0] = dcalc_ap;
  else
    dcalc_analysis_pts_[dcalc_ap->constraintMinMax()->index()] = dcalc_ap;
}

DcalcAnalysisPt *
Corner::findDcalcAnalysisPt(const MinMax *min_max) const
{
  int ap_count = dcalc_analysis_pts_.size();
  if (ap_count == 0)
    return nullptr;
  else if (ap_count == 1)
    return dcalc_analysis_pts_[0];
  else if (ap_count == 2)
    return dcalc_analysis_pts_[min_max->index()];
  else {
    criticalError(247, "unknown analysis point count");
    return nullptr;
  }
}

PathAnalysisPt *
Corner::findPathAnalysisPt(const MinMax *min_max) const
{
  return path_analysis_pts_[min_max->index()];
}

void
Corner::addPathAP(PathAnalysisPt *path_ap)
{
  path_analysis_pts_[path_ap->pathMinMax()->index()] = path_ap;
}

void
Corner::addLiberty(LibertyLibrary *lib,
		   const MinMax *min_max)
{
  liberty_[min_max->index()].push_back(lib);
}

const LibertySeq &
Corner::libertyLibraries(const MinMax *min_max) const
{
  return liberty_[min_max->index()];
}

int
Corner::libertyIndex(const MinMax *min_max) const
{
  return index_ * MinMax::index_count + min_max->index();
}

} // namespace
