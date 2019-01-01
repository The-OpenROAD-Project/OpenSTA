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

#ifndef STA_TRANSITION_H
#define STA_TRANSITION_H

#include "DisallowCopyAssign.hh"
#include "Iterator.hh"
#include "Map.hh"
#include "StringUtil.hh"

namespace sta {

class Transition;
class TransRiseFall;
class TransRiseFallBoth;

typedef Map<const char*, Transition*, CharPtrLess> TransitionMap;

// Rise/fall transition.
class TransRiseFall
{
public:
  static void init();
  static void destroy();
  // Singleton accessors.
  static TransRiseFall *rise() { return rise_; }
  static TransRiseFall *fall() { return fall_; }
  static int riseIndex() { return rise_->sdf_triple_index_; }
  static int fallIndex() { return fall_->sdf_triple_index_; }
  const char *asString() const { return short_name_; }
  const char *name() const { return name_; }
  const char *shortName() const { return short_name_; }
  void setShortName(const char *short_name);
  int index() const { return sdf_triple_index_; }
  TransRiseFallBoth *asRiseFallBoth();
  const TransRiseFallBoth *asRiseFallBoth() const;
  Transition *asTransition() const;
  // Find transition corresponding to tr_str.
  static TransRiseFall *find(const char *tr_str);
  // Find transition from index.
  static TransRiseFall *find(int index);
  TransRiseFall *opposite() const;

  static const int index_count = 2;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 1;

protected:
  TransRiseFall(const char *name,
		const char *short_name,
		int sdf_triple_index);
  ~TransRiseFall();

  const char *name_;
  const char *short_name_;
  const int sdf_triple_index_;

  static TransRiseFall *rise_;
  static TransRiseFall *fall_;

private:
  DISALLOW_COPY_AND_ASSIGN(TransRiseFall);
};

// Rise/fall/risefall transition.
class TransRiseFallBoth
{
public:
  static void init();
  static void destroy();
  // Singleton accessors.
  static TransRiseFallBoth *rise() { return rise_; }
  static TransRiseFallBoth *fall() { return fall_; }
  static TransRiseFallBoth *riseFall() { return rise_fall_; }
  const char *asString() const { return short_name_; }
  const char *name() const { return name_; }
  const char *shortName() const { return short_name_; }
  void setShortName(const char *short_name);
  int index() const { return sdf_triple_index_; }
  bool matches(const TransRiseFall *tr) const;
  bool matches(const Transition *tr) const;
  TransRiseFall *asRiseFall() const { return as_rise_fall_; }
  // Find transition corresponding to string.
  static TransRiseFallBoth *find(const char *tr_str);

  static const int index_count = 3;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 2;

protected:
  TransRiseFallBoth(const char *name,
		    const char *short_name,
		    int sdf_triple_index,
		    TransRiseFall *as_rise_fall);
  ~TransRiseFallBoth();

  const char *name_;
  const char *short_name_;
  const int sdf_triple_index_;
  TransRiseFall *as_rise_fall_;

  static TransRiseFallBoth *rise_;
  static TransRiseFallBoth *fall_;
  static TransRiseFallBoth *rise_fall_;

private:
  DISALLOW_COPY_AND_ASSIGN(TransRiseFallBoth);
};

// General SDF transition.
class Transition
{
public:
  static void init();
  static void destroy();
  // Singleton accessors.
  static Transition *rise() { return rise_; }
  static Transition *fall() { return fall_; }
  static Transition *tr0Z() { return tr_0Z_; }
  static Transition *trZ1() { return tr_Z1_; }
  static Transition *tr1Z() { return tr_1Z_; }
  static Transition *trZ0() { return tr_Z0_; }
  static Transition *tr0X() { return tr_0X_; }
  static Transition *trX1() { return tr_X1_; }
  static Transition *tr1X() { return tr_1X_; }
  static Transition *trX0() { return tr_X0_; }
  static Transition *trXZ() { return tr_XZ_; }
  static Transition *trZX() { return tr_ZX_; }
  void setName(const char *name);
  // Matches rise and fall.
  static Transition *riseFall() { return rise_fall_; }
  const char *asString() const { return name_; }
  // As initial/final value pair.
  const char *asInitFinalString() const { return init_final_; }
  int sdfTripleIndex() const { return sdf_triple_index_; }
  int index() const { return sdf_triple_index_; }
  TransRiseFall *asRiseFall() const { return as_rise_fall_; }
  const TransRiseFallBoth *asRiseFallBoth() const;
  bool matches(const Transition *tr) const;
  // Find transition corresponding to string.
  static Transition *find(const char *tr_str);
  static int maxIndex() { return max_index_; }

private:
  Transition(const char *name,
	     const char *init_final,
	     TransRiseFall *as_rise_fall,
	     int sdf_triple_index);




  ~Transition();

  const char *name_;
  const char *init_final_;
  TransRiseFall *as_rise_fall_;
  const int sdf_triple_index_;

  static Transition *rise_;
  static Transition *fall_;
  static Transition *tr_0Z_;
  static Transition *tr_Z1_;
  static Transition *tr_1Z_;
  static Transition *tr_Z0_;
  static Transition *tr_0X_;
  static Transition *tr_X1_;
  static Transition *tr_1X_;
  static Transition *tr_X0_;
  static Transition *tr_XZ_;
  static Transition *tr_ZX_;
  static Transition *rise_fall_;
  static const int index_count = 13;
  static const int index_max = (index_count - 1);
  static const int index_bit_count = 4;

  static TransitionMap *transition_map_;
  static int max_index_;

private:
  DISALLOW_COPY_AND_ASSIGN(Transition);
};

class TransRiseFallIterator : public Iterator<TransRiseFall*>
{
public:
  TransRiseFallIterator() : index_(0), index_max_(TransRiseFall::index_max) {}
  explicit TransRiseFallIterator(const TransRiseFallBoth *tr);
  void init();
  virtual bool hasNext();
  virtual TransRiseFall *next();

private:
  DISALLOW_COPY_AND_ASSIGN(TransRiseFallIterator);

  int index_;
  int index_max_;
};

} // namespace
#endif
