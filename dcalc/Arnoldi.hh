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

#include "ConcreteParasiticsPvt.hh"

namespace sta {

struct delay_work;
class rcmodel;

class GateTableModel;
class Pin;

//
// single-driver arnoldi model
//
class arnoldi1
{
public:
  arnoldi1() { order=0; n=0; d=nullptr; e=nullptr; U=nullptr; ctot=0.0; sqc=0.0; }
  ~arnoldi1();
  double elmore(int term_index);

  //
  // calculate poles/residues for given rdrive
  //
  void calculate_poles_res(delay_work *D,double rdrive);

public:
  int order;
  int n;     // number of terms, including driver
  double *d; // [order]
  double *e; // [order-1]
  double **U; // [order][n]
  double ctot;
  double sqc;
};

// This is the rcmodel, without Rd.
// n is the number of terms
// The vectors U[j] are of size n
class rcmodel : public ConcreteParasitic,
		public arnoldi1
{
public:
  rcmodel();
  virtual ~rcmodel();
  virtual float capacitance() const;

  const Pin **pinV; // [n]
};

struct timing_table
{
  GateTableModel *table;
  const LibertyCell *cell;
  const Pvt *pvt;
  float in_slew;
  float relcap;
};

} // namespace
