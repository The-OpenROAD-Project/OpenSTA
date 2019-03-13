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

#ifndef STA_CORNER_H
#define STA_CORNER_H

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

typedef Map<const char *, Corner*, CharPtrLess> CornerMap;
typedef Vector<ParasiticAnalysisPt*> ParasiticAnalysisPtSeq;
typedef Vector<DcalcAnalysisPt*> DcalcAnalysisPtSeq;
typedef Vector<PathAnalysisPt*> PathAnalysisPtSeq;
typedef Vector<LibertyLibrary*> LibertySeq;

class Corners : public StaState
{
public:
  explicit Corners(StaState *sta);;
  virtual ~Corners();
  void clear();
  int count() const;
  bool multiCorner() const;
  Corner *findCorner(const char *corner);
  Corner *defaultCorner();
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
  CornerMap &corners() { return corners_; }

private:
  CornerMap corners_;
  Corner *default_corner_;
  ParasiticAnalysisPtSeq parasitic_analysis_pts_;
  DcalcAnalysisPtSeq dcalc_analysis_pts_;
  PathAnalysisPtSeq path_analysis_pts_;

  friend class CornerIterator;
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

class CornerIterator : public Iterator<Corner*>
{
public:
  explicit CornerIterator(const StaState *sta);
  virtual ~CornerIterator() {}
  virtual bool hasNext();
  virtual Corner *next();

protected:
  CornerMap::ConstIterator iter_;

private:
  DISALLOW_COPY_AND_ASSIGN(CornerIterator);
};

class ParasiticAnalysisPtIterator : public Iterator<ParasiticAnalysisPt*>
{
public:
  explicit ParasiticAnalysisPtIterator(const StaState *sta);
  virtual ~ParasiticAnalysisPtIterator() {}
  virtual bool hasNext();
  virtual ParasiticAnalysisPt *next();

protected:
  ParasiticAnalysisPtSeq::ConstIterator ap_iter_;

private:
  DISALLOW_COPY_AND_ASSIGN(ParasiticAnalysisPtIterator);
};

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
#endif
