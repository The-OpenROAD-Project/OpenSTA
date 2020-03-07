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
#include "MinMax.hh"
#include "Vector.hh"
#include "StringSet.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"
#include "StaState.hh"

namespace sta {

class ParasiticAnalysisPt;
class DcalcAnalysisPt;
class PathAnalysisPt;
class Corner;
class Corners;
class LibertyLibrary;

typedef Vector<Corner*> CornerSeq;
typedef Map<const char *, Corner*, CharPtrLess> CornerMap;
typedef Vector<ParasiticAnalysisPt*> ParasiticAnalysisPtSeq;
typedef Vector<DcalcAnalysisPt*> DcalcAnalysisPtSeq;
typedef Vector<PathAnalysisPt*> PathAnalysisPtSeq;
typedef Vector<LibertyLibrary*> LibertySeq;

class Corners : public StaState
{
public:
  explicit Corners(StaState *sta);
  virtual ~Corners();
  void clear();
  int count() const;
  bool multiCorner() const;
  Corner *findCorner(const char *corner);
  Corner *findCorner(int corner_index);
  void makeCorners(StringSet *corner_names);
  void analysisTypeChanged();
  void operatingConditionsChanged();

  void makeParasiticAnalysisPtsSingle();
  void makeParasiticAnalysisPtsMinMax();
  int parasiticAnalysisPtCount() const;
  ParasiticAnalysisPtSeq &parasiticAnalysisPts();

  DcalcAPIndex dcalcAnalysisPtCount() const;
  DcalcAnalysisPtSeq &dcalcAnalysisPts();
  const DcalcAnalysisPtSeq &dcalcAnalysisPts() const;

  PathAPIndex pathAnalysisPtCount() const;
  PathAnalysisPt *findPathAnalysisPt(PathAPIndex path_index) const;
  PathAnalysisPtSeq &pathAnalysisPts();
  const PathAnalysisPtSeq &pathAnalysisPts() const;
  CornerSeq &corners() { return corners_; }
  // Iterators for range iteration.
  // for (auto corner : *sta->corners()) {}
  CornerSeq::iterator begin() { return corners_.begin(); }
  CornerSeq::iterator end() { return corners_.end(); }

protected:
  void makeAnalysisPts();
  void updateCornerParasiticAnalysisPts();
  void makeDcalcAnalysisPts(Corner *corner);
  DcalcAnalysisPt *makeDcalcAnalysisPt(Corner *corner,
				       const MinMax *min_max,
				       const MinMax *check_clk_slew_min_max);
  void makePathAnalysisPts(Corner *corner);
  void makePathAnalysisPts(Corner *corner,
			   bool swap_clk_min_max,
			   DcalcAnalysisPt *dcalc_ap_min,
			   DcalcAnalysisPt *dcalc_ap_max);

private:
  CornerMap corner_map_;
  CornerSeq corners_;
  ParasiticAnalysisPtSeq parasitic_analysis_pts_;
  DcalcAnalysisPtSeq dcalc_analysis_pts_;
  PathAnalysisPtSeq path_analysis_pts_;

  DISALLOW_COPY_AND_ASSIGN(Corners);
};

class Corner
{
public:
  Corner(const char *name,
	 int index);
  ~Corner();
  const char *name() const { return name_; }
  int index() const { return index_; }
  ParasiticAnalysisPt *findParasiticAnalysisPt(const MinMax *min_max) const;
  int parasiticAnalysisPtcount();
  DcalcAnalysisPt *findDcalcAnalysisPt(const MinMax *min_max) const;
  PathAnalysisPt *findPathAnalysisPt(const MinMax *min_max) const;
  void addLiberty(LibertyLibrary *lib,
		  const MinMax *min_max);
  LibertySeq *libertyLibraries(const MinMax *min_max);
  int libertyIndex(const MinMax *min_max) const;

protected:
  void setParasiticAnalysisPtcount(int ap_count);
  void addParasiticAP(ParasiticAnalysisPt *path_ap);
  void setDcalcAnalysisPtcount(DcalcAPIndex ap_count);
  void addDcalcAP(DcalcAnalysisPt *dcalc_ap);
  void addPathAP(PathAnalysisPt *path_ap);

private:
  const char *name_;
  int index_;
  ParasiticAnalysisPtSeq parasitic_analysis_pts_;
  DcalcAnalysisPtSeq dcalc_analysis_pts_;
  PathAnalysisPtSeq path_analysis_pts_;
  LibertySeq liberty_[MinMax::index_count];

  friend class Corners;
  DISALLOW_COPY_AND_ASSIGN(Corner);
};

// Obsolete. Use range iterator.
// for (auto corner : *sta->corners()) {}
class CornerIterator : public Iterator<Corner*>
{
public:
  explicit CornerIterator(const StaState *sta);
  virtual ~CornerIterator() {}
  virtual bool hasNext();
  virtual Corner *next();

protected:
  CornerSeq::ConstIterator iter_;

private:
  DISALLOW_COPY_AND_ASSIGN(CornerIterator);
};

// Obsolete. Use range iterator.
// for (auto dcalc_ap : sta->corners()->dcalcAnalysisPts()) {}
class DcalcAnalysisPtIterator : public Iterator<DcalcAnalysisPt*>
{
public:
  explicit DcalcAnalysisPtIterator(const StaState *sta);
  virtual ~DcalcAnalysisPtIterator() {}
  virtual bool hasNext();
  virtual DcalcAnalysisPt *next();

protected:
  DcalcAnalysisPtSeq::ConstIterator ap_iter_;

private:
  DISALLOW_COPY_AND_ASSIGN(DcalcAnalysisPtIterator);
};

// Obsolete. Use range iterator.
// for (auto path_ap : sta->corners()->pathAnalysisPts()) {}
class PathAnalysisPtIterator : public Iterator<PathAnalysisPt*>
{
public:
  explicit PathAnalysisPtIterator(const StaState *sta);
  virtual ~PathAnalysisPtIterator() {}
  virtual bool hasNext();
  virtual PathAnalysisPt *next();

protected:
  PathAnalysisPtSeq::ConstIterator ap_iter_;

private:
  DISALLOW_COPY_AND_ASSIGN(PathAnalysisPtIterator);
};

} // namespace
