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
#include "Sdc.hh"
#include "Parasitics.hh"
#include "DcalcAnalysisPt.hh"
#include "PathAnalysisPt.hh"
#include "Corner.hh"

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
  DcalcAnalysisPtSeq::Iterator ap_iter(dcalc_analysis_pts_);
  while (ap_iter.hasNext()) {
    DcalcAnalysisPt *dcalc_ap = ap_iter.next();
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
  StringSet::Iterator name_iter(corner_names);
  while (name_iter.hasNext()) {
    const char *name = name_iter.next();
    Corner *corner = new Corner(name, index);
    corners_.push_back(corner);
    // Use the copied name in the map.
    corner_map_[corner->name()] = corner;
    index++;
  }
  updateCornerParasiticAnalysisPts();
  makeAnalysisPts();
}

void
Corners::makeParasiticAnalysisPtsSingle()
{
  if (parasitic_analysis_pts_.size() != 1) {
    parasitics_->deleteParasitics();
    parasitic_analysis_pts_.deleteContentsClear();
    ParasiticAnalysisPt *ap = new ParasiticAnalysisPt("min_max", 0,
						      MinMax::max());
    parasitic_analysis_pts_.push_back(ap);
    updateCornerParasiticAnalysisPts();
  }
}

void
Corners::makeParasiticAnalysisPtsMinMax()
{
  if (parasitic_analysis_pts_.size() != 2) {
    parasitics_->deleteParasitics();
    parasitic_analysis_pts_.deleteContentsClear();
    parasitic_analysis_pts_.resize(MinMax::index_count);
    for (auto min_max : MinMax::range()) {
      int mm_index = min_max->index();
      ParasiticAnalysisPt *ap = new ParasiticAnalysisPt(min_max->asString(),
							mm_index,
							min_max);
      parasitic_analysis_pts_[mm_index] = ap;
    }
    updateCornerParasiticAnalysisPts();
  }
}

void
Corners::updateCornerParasiticAnalysisPts()
{
  CornerSeq::Iterator corner_iter(corners_);
  while (corner_iter.hasNext()) {
    Corner *corner = corner_iter.next();
    corner->setParasiticAnalysisPtcount(parasitic_analysis_pts_.size());
    ParasiticAnalysisPtSeq::Iterator ap_iter(parasitic_analysis_pts_);
    while (ap_iter.hasNext()) {
      ParasiticAnalysisPt *ap = ap_iter.next();
      corner->addParasiticAP(ap);
    }
  }
}

void
Corners::makeAnalysisPts()
{
  dcalc_analysis_pts_.deleteContentsClear();
  path_analysis_pts_.deleteContentsClear();

  CornerSeq::Iterator corner_iter(corners_);
  while (corner_iter.hasNext()) {
    Corner *corner = corner_iter.next();
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
    internalError("unknown parasitic analysis point count");
    return nullptr;
  }
}

void
Corner::setParasiticAnalysisPtcount(int ap_count)
{
  parasitic_analysis_pts_.resize(ap_count);
}

void 
Corner::addParasiticAP(ParasiticAnalysisPt *ap)
{
  if (parasitic_analysis_pts_.size() == 1)
    parasitic_analysis_pts_[0] = ap;
  else
    parasitic_analysis_pts_[ap->minMax()->index()] = ap;
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
    internalError("unknown analysis point count");
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

LibertySeq *
Corner::libertyLibraries(const MinMax *min_max)
{
  return &liberty_[min_max->index()];
}

int
Corner::libertyIndex(const MinMax *min_max) const
{
  return index_ * MinMax::index_count + min_max->index();
}

////////////////////////////////////////////////////////////////

CornerIterator::CornerIterator(const StaState *sta) :
  iter_(sta->corners()->corners())
{
}

bool
CornerIterator::hasNext()
{
  return iter_.hasNext();
}

Corner *
CornerIterator::next()
{
  return iter_.next();
}

////////////////////////////////////////////////////////////////

DcalcAnalysisPtIterator::DcalcAnalysisPtIterator(const StaState *sta) :
  ap_iter_(sta->corners()->dcalcAnalysisPts())
{
}

bool
DcalcAnalysisPtIterator::hasNext()
{
  return ap_iter_.hasNext();
}

DcalcAnalysisPt *
DcalcAnalysisPtIterator::next()
{
  return ap_iter_.next();
}

////////////////////////////////////////////////////////////////

PathAnalysisPtIterator::PathAnalysisPtIterator(const StaState *sta) :
  ap_iter_(sta->corners()->pathAnalysisPts())
{
}

bool
PathAnalysisPtIterator::hasNext()
{
  return ap_iter_.hasNext();
}

PathAnalysisPt *
PathAnalysisPtIterator::next()
{
  return ap_iter_.next();
}

} // namespace
