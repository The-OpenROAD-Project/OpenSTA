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

// (c) 2018 Nefelus, Inc.
//
// Author: W. Scott

#pragma once

#include "Map.hh"
#include "NetworkClass.hh"
#include "ParasiticsClass.hh"

namespace sta {

class ConcreteParasiticNetwork;
class ConcreteParasiticNode;

class rcmodel;
struct ts_edge;
struct ts_point;

typedef Map<ConcreteParasiticNode*, int> ArnolidPtMap;

class ArnoldiReduce : public StaState
{
public:
  ArnoldiReduce(StaState *sta);
  ~ArnoldiReduce();
  Parasitic *reduceToArnoldi(Parasitic *parasitic,
			     const Pin *drvr_pin,
			     float coupling_cap_factor,
			     const RiseFall *rf,
			     const OperatingConditions *op_cond,
			     const Corner *corner,
			     const MinMax *cnst_min_max,
			     const ParasiticAnalysisPt *ap);

protected:
  void loadWork();
  rcmodel *makeRcmodelDrv();
  void allocPoints();
  void allocTerms(int nterms);
  ts_point *findPt(ParasiticNode *node);
  void makeRcmodelDfs(ts_point *pdrv);
  void getRC();
  float pinCapacitance(ParasiticNode *node);
  void setTerms(ts_point *pdrv);
  void makeRcmodelFromTs();
  rcmodel *makeRcmodelFromW();
  
  ConcreteParasiticNetwork *parasitic_network_;
  const Pin *drvr_pin_;
  float coupling_cap_factor_;
  const RiseFall *rf_;
  const OperatingConditions *op_cond_;
  const Corner *corner_;
  const MinMax *cnst_min_max_;
  const ParasiticAnalysisPt *ap_;
  // ParasiticNode -> ts_point index.
  ArnolidPtMap pt_map_;

  // rcWork
  ts_point *ts_pointV;
  int ts_pointN;
  int ts_pointNmax;
  static const int ts_point_count_incr_;
  ts_edge *ts_edgeV;
  int ts_edgeN;
  int ts_edgeNmax;
  static const int ts_edge_count_incr_;
  ts_edge **ts_eV;
  ts_edge **ts_stackV;
  int *ts_ordV;
  ts_point **ts_pordV;
  int ts_ordN;

  int termNmax;
  int termN;
  ts_point *pterm0;
  const Pin **pinV; // fixed order, offset from pterm0
  int *termV; // from drv-ordered to fixed order
  int *outV;  // from drv-ordered to ts_pordV

  int dNmax;
  double *d;
  double *e;
  double *U0;
  double **U;

  double ctot_;
  double sqc_;
  double *_u0, *_u1;
  double *y, *iv;
  double *c, *r;
  int    *par;
  int order;
};

} // namespace
