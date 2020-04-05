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
#include "Error.hh"
#include "Set.hh"
#include "SdcCmdComment.hh"
#include "SdcClass.hh"

namespace sta {

class RiseFall;
class RiseFallBoth;
class MinMaxAll;
class Network;
class Pin;
class Clock;

class ExceptionPt;
class ExceptionFrom;
class ExceptionThru;
class ExceptionTo;
class ExceptionState;

typedef Vector<ExceptionPath*> ExceptionPathSeq;

class ExceptionPath : public SdcCmdComment
{
public:
  ExceptionPath(ExceptionFrom *from,
		ExceptionThruSeq *thrus,
		ExceptionTo *to,
		const MinMaxAll *min_max,
		bool own_pts,
		int priority,
		const char *comment);
  virtual ~ExceptionPath();
  virtual bool isFalse() const { return false; }
  virtual bool isLoop() const { return false; }
  virtual bool isMultiCycle() const { return false; }
  virtual bool isPathDelay() const { return false; }
  virtual bool isGroupPath() const { return false; }
  virtual bool isFilter() const { return false; }
  virtual ExceptionPathType type() const = 0;
  virtual const char *asString(const Network *network) const;
  ExceptionFrom *from() const { return from_; }
  ExceptionThruSeq *thrus() const { return thrus_; }
  ExceptionTo *to() const { return to_; }
  ExceptionPt *firstPt();
  bool intersectsPts(ExceptionPath *exception) const;
  const MinMaxAll *minMax() const { return min_max_; }
  virtual bool matches(const MinMax *min_max,
		       bool exact) const;
  bool matchesFirstPt(const RiseFall *to_rf,
		      const MinMax *min_max);
  ExceptionState *firstState();
  virtual bool resetMatch(ExceptionFrom *from,
			  ExceptionThruSeq *thrus,
			  ExceptionTo *to,
			  const MinMaxAll *min_max);
  // The priority remains the same even though pin/clock/net/inst objects
  // are added to the exceptions points during exception merging because
  // only exceptions with the same priority are merged.
  virtual int priority(const MinMax *) const { return priority_; }
  int priority() const { return priority_; }
  void setPriority(int priority);
  virtual int typePriority() const = 0;
  // Exception type priorities are spaced to accomodate
  // fromThruToPriority from 0 thru 127.
  static int falsePathPriority() { return 4000; }
  static int pathDelayPriority() { return 3000; }
  static int multiCyclePathPriority() { return 2000; }
  static int filterPathPriority() { return 1000; }
  static int groupPathPriority() { return 0; }
  // Compare the value (path delay or cycle count) to another exception
  // of the same priority.  Because the exception "values" are floats,
  // they cannot be coded into the priority.
  virtual bool tighterThan(ExceptionPath *exception) const = 0;
  static int fromThruToPriority(ExceptionFrom *from,
  				ExceptionThruSeq *thrus,
				ExceptionTo *to);
  size_t hash() const;
  size_t hash(ExceptionPt *missing_pt) const;
  // Mergeable properties (independent of exception points).
  virtual bool mergeable(ExceptionPath *exception) const;
  bool mergeablePts(ExceptionPath *exception) const;
  bool mergeablePts(ExceptionPath *exception2,
		    ExceptionPt *missing_pt2,
		    ExceptionPt *&missing_pt) const;
  // Overrides properties (independent of exception points).
  virtual bool overrides(ExceptionPath *exception) const = 0;
  virtual ExceptionPath *clone(ExceptionFrom *from,
			       ExceptionThruSeq *thrus,
			       ExceptionTo *to,
			       bool own_pts) = 0;
  // Default handlers.
  virtual bool useEndClk() const { return false; }
  virtual int pathMultiplier() const { return 0; }
  virtual float delay() const { return 0.0; }
  virtual const char *name() const { return nullptr; }
  virtual bool isDefault() const { return false; }
  virtual bool ignoreClkLatency() { return false; }

protected:
  virtual const char *typeString() const = 0;
  const char *fromThruToString(const Network *network) const;
  void makeStates();

  ExceptionFrom *from_;
  ExceptionThruSeq *thrus_;
  ExceptionTo *to_;
  const MinMaxAll *min_max_;
  bool own_pts_;
  int priority_;
  ExceptionState *states_;

private:
  DISALLOW_COPY_AND_ASSIGN(ExceptionPath);
};

// set_false_path
class FalsePath : public ExceptionPath
{
public:
  FalsePath(ExceptionFrom *from,
	    ExceptionThruSeq *thrus,
	    ExceptionTo *to,
	    const MinMaxAll *min_max,
	    bool own_pts,
	    const char *comment);
  FalsePath(ExceptionFrom *from,
	    ExceptionThruSeq *thrus,
	    ExceptionTo *to,
	    const MinMaxAll *min_max,
	    bool own_pts,
	    int priority,
	    const char *comment);
  virtual ExceptionPath *clone(ExceptionFrom *from,
			       ExceptionThruSeq *thrus,
			       ExceptionTo *to,
			       bool own_pts);
  virtual bool isFalse() const { return true; }
  virtual ExceptionPathType type() const { return ExceptionPathType::false_path; }
  virtual const char *typeString() const;
  virtual bool mergeable(ExceptionPath *exception) const;
  virtual bool overrides(ExceptionPath *exception) const;
  virtual int typePriority() const;
  virtual bool tighterThan(ExceptionPath *exception) const;

private:
  DISALLOW_COPY_AND_ASSIGN(FalsePath);
};

// Loop paths are false paths used to disable paths around
// combinational loops when dynamic loop breaking is enabled.
class LoopPath : public FalsePath
{
public:
  LoopPath(ExceptionThruSeq *thrus,
	   bool own_pts);
  virtual bool isLoop() const { return true; }
  virtual ExceptionPathType type() const { return ExceptionPathType::loop; }
  virtual const char *typeString() const;
  virtual bool mergeable(ExceptionPath *exception) const;

private:
  DISALLOW_COPY_AND_ASSIGN(LoopPath);
};

// set_max_delay/set_min_delay
class PathDelay : public ExceptionPath
{
public:
  PathDelay(ExceptionFrom *from,
	    ExceptionThruSeq *thrus,
	    ExceptionTo *to,
	    const MinMax *min_max,
	    bool ignore_clk_latency,
	    float delay,
	    bool own_pts,
	    const char *comment);
  virtual ExceptionPath *clone(ExceptionFrom *from,
			       ExceptionThruSeq *thrus,
			       ExceptionTo *to,
			       bool own_pts);
  virtual bool isPathDelay() const { return true; }
  virtual ExceptionPathType type() const { return ExceptionPathType::path_delay; }
  virtual const char *asString(const Network *network) const;
  virtual const char *typeString() const;
  virtual bool mergeable(ExceptionPath *exception) const;
  virtual bool overrides(ExceptionPath *exception) const;
  virtual float delay() const { return delay_; }
  virtual int typePriority() const;
  virtual bool tighterThan(ExceptionPath *exception) const;
  virtual bool ignoreClkLatency() { return ignore_clk_latency_; }

protected:
  bool ignore_clk_latency_;
  float delay_;

private:
  DISALLOW_COPY_AND_ASSIGN(PathDelay);
};

// set_multicycle_path
class MultiCyclePath : public ExceptionPath
{
public:
  MultiCyclePath(ExceptionFrom *from,
		 ExceptionThruSeq *thrus,
		 ExceptionTo *to,
		 const MinMaxAll *min_max,
		 bool use_end_clk,
		 int path_multiplier,
		 bool own_pts,
		 const char *comment);
  virtual ExceptionPath *clone(ExceptionFrom *from,
			       ExceptionThruSeq *thrus,
			       ExceptionTo *to,
			       bool own_pts);
  virtual bool isMultiCycle() const { return true; }
  virtual ExceptionPathType type() const { return ExceptionPathType::multi_cycle; }
  virtual bool matches(const MinMax *min_max,
		       bool exactly) const;
  virtual const char *asString(const Network *network) const;
  virtual const char *typeString() const;
  virtual bool mergeable(ExceptionPath *exception) const;
  virtual bool overrides(ExceptionPath *exception) const;
  virtual bool useEndClk() const { return use_end_clk_; }
  virtual int pathMultiplier(const MinMax *min_max) const;
  virtual int pathMultiplier() const { return path_multiplier_; }
  virtual int priority(const MinMax *min_max) const;
  virtual int typePriority() const;
  virtual bool tighterThan(ExceptionPath *exception) const;

  using ExceptionPath::priority;

protected:
  bool use_end_clk_;
  int path_multiplier_;

private:
  DISALLOW_COPY_AND_ASSIGN(MultiCyclePath);
};

// Filter used restrict path reporting -from/-thru nets/pins.
class FilterPath : public ExceptionPath
{
public:
  FilterPath(ExceptionFrom *from,
	     ExceptionThruSeq *thrus,
	     ExceptionTo *to,
	     bool own_pts);
  virtual ExceptionPath *clone(ExceptionFrom *from,
			       ExceptionThruSeq *thrus,
			       ExceptionTo *to,
			       bool own_pts);
  virtual bool isFilter() const { return true; }
  virtual ExceptionPathType type() const { return ExceptionPathType::filter; }
  virtual const char *typeString() const;
  virtual bool mergeable(ExceptionPath *exception) const;
  virtual bool overrides(ExceptionPath *exception) const;
  virtual bool resetMatch(ExceptionFrom *from,
			  ExceptionThruSeq *thrus,
			  ExceptionTo *to,
			  const MinMaxAll *min_max);
  virtual int typePriority() const;
  virtual bool tighterThan(ExceptionPath *exception) const;

private:
  DISALLOW_COPY_AND_ASSIGN(FilterPath);
};

class GroupPath : public ExceptionPath
{
public:
  GroupPath(const char *name,
	    bool is_default,
	    ExceptionFrom *from,
	    ExceptionThruSeq *thrus,
	    ExceptionTo *to,
	    bool own_pts,
	    const char *comment);
  virtual ~GroupPath();
  virtual ExceptionPath *clone(ExceptionFrom *from,
			       ExceptionThruSeq *thrus,
			       ExceptionTo *to,
			       bool own_pts);
  virtual bool isGroupPath() const { return true; }
  virtual ExceptionPathType type() const { return ExceptionPathType::group_path; }
  virtual const char *typeString() const;
  virtual bool mergeable(ExceptionPath *exception) const;
  virtual bool overrides(ExceptionPath *exception) const;
  virtual int typePriority() const;
  virtual bool tighterThan(ExceptionPath *exception) const;
  virtual const char *name()  const { return name_; }
  virtual bool isDefault() const { return is_default_; }

protected:
  const char *name_;
  bool is_default_;

private:
  DISALLOW_COPY_AND_ASSIGN(GroupPath);
};

// Base class for Exception from/thru/to.
class ExceptionPt
{
public:
  ExceptionPt(const RiseFallBoth *rf,
	      bool own_pts);
  virtual ~ExceptionPt() {};
  virtual bool isFrom() const { return false; }
  virtual bool isThru() const { return false; }
  virtual bool isTo() const { return false; }
  const RiseFallBoth *transition() const { return rf_; }
  virtual PinSet *pins() = 0;
  virtual ClockSet *clks() = 0;
  virtual InstanceSet *instances() = 0;
  virtual NetSet *nets() = 0;
  virtual EdgePinsSet *edges() = 0;
  size_t hash() const;
  virtual int nameCmp(ExceptionPt *pt, const Network *network) const = 0;
  virtual void mergeInto(ExceptionPt *pt) = 0;
  // All pins and instance/net pins.
  virtual void allPins(const Network *network,
		       PinSet *pins) = 0;
  virtual int typePriority() const = 0;
  virtual const char *asString(const Network *network) const = 0;
  virtual size_t objectCount() const = 0;
  virtual void addPin(Pin *pin) = 0;
  virtual void addClock(Clock *clk) = 0;
  virtual void addInstance(Instance *inst) = 0;
  virtual void addNet(Net *net) = 0;
  virtual void addEdge(EdgePins *edge) = 0;
  virtual void connectPinAfter(PinSet *,
			       Network *network) = 0;
  virtual void disconnectPinBefore(Pin *pin,
				   Network *network) = 0;

protected:
  const RiseFallBoth *rf_;
  // True when the pin/net/inst/edge sets are owned by the exception point.
  bool own_pts_;
  // Hash is cached because there may be many objects to speed up
  // exception merging.
  size_t hash_;

  // Maximum number of objects for asString() to show.
  static const int as_string_max_objects_;
  static const size_t hash_clk  =  3;
  static const size_t hash_pin  =  5;
  static const size_t hash_net  =  7;
  static const size_t hash_inst = 11;

private:
  DISALLOW_COPY_AND_ASSIGN(ExceptionPt);
};

class ExceptionFromTo : public ExceptionPt
{
public:
  ExceptionFromTo(PinSet *pins, ClockSet *clks,
		  InstanceSet *insts,
		  const RiseFallBoth *rf,
		  bool own_pts);
  ~ExceptionFromTo();
  virtual PinSet *pins() { return pins_; }
  bool hasPins() const;
  ClockSet *clks() { return clks_; }
  bool hasClocks() const;
  InstanceSet *instances() { return insts_; }
  bool hasInstances() const;
  virtual NetSet *nets() { return nullptr; }
  virtual EdgePinsSet *edges() { return nullptr; }
  bool hasObjects() const;
  void deleteObjects(ExceptionFromTo *pt);
  virtual void allPins(const Network *network,
		       PinSet *pins);
  bool equal(ExceptionFromTo *from_to) const;
  virtual int nameCmp(ExceptionPt *pt,
		      const Network *network) const;
  virtual void mergeInto(ExceptionPt *pt);
  virtual const char *asString(const Network *network) const;
  virtual size_t objectCount() const;
  void deleteClock(Clock *clk);
  virtual void addPin(Pin *pin);
  virtual void addClock(Clock *clk);
  virtual void addInstance(Instance *inst);
  virtual void addNet(Net *) {}
  virtual void addEdge(EdgePins *) {}
  virtual void connectPinAfter(PinSet *,
			       Network *) {}
  virtual void disconnectPinBefore(Pin *,
				   Network *) {}

protected:
  virtual void findHash();

  void deletePin(Pin *pin);
  void deleteInstance(Instance *inst);
  virtual const char *cmdKeyword() const = 0;

  PinSet *pins_;
  ClockSet *clks_;
  InstanceSet *insts_;

private:
  DISALLOW_COPY_AND_ASSIGN(ExceptionFromTo);
};

class ExceptionFrom : public ExceptionFromTo
{
public:
  ExceptionFrom(PinSet *pins,
		ClockSet *clks,
		InstanceSet *insts,
		const RiseFallBoth *rf,
		bool own_pts);
  ExceptionFrom *clone();
  virtual bool isFrom() const { return true; }
  bool intersectsPts(ExceptionFrom *from) const;
  virtual int typePriority() const { return 0; }

protected:
  virtual const char *cmdKeyword() const;
  virtual void findHash();

private:
  DISALLOW_COPY_AND_ASSIGN(ExceptionFrom);
};

class ExceptionTo : public ExceptionFromTo
{
public:
  ExceptionTo(PinSet *pins,
	      ClockSet *clks,
	      InstanceSet *insts,
	      // -to|-rise_to|-fall_to
	      const RiseFallBoth *rf,
	      // -rise|-fall endpoint transition.
	      const RiseFallBoth *end_rf,
	      bool own_pts);
  ExceptionTo *clone();
  virtual bool isTo() const { return true; }
  const char *asString(const Network *network) const;
  const RiseFallBoth *endTransition() { return end_rf_; }
  bool intersectsPts(ExceptionTo *to) const;
  virtual int typePriority() const { return 1; }
  bool matches(const Pin *pin,
 	       const ClockEdge *clk_edge, 
 	       const RiseFall *end_rf,
 	       const Network *network) const;
  bool matches(const Pin *pin,
	       const RiseFall *end_rf) const;
  bool matches(const Clock *clk) const;
  bool matchesFilter(const Pin *pin,
		     const ClockEdge *clk_edge,
		     const RiseFall *end_rf,
		     const Network *network) const;
  virtual int nameCmp(ExceptionPt *pt, const Network *network) const;

protected:
  bool matches(const Pin *pin,
	       const ClockEdge *clk_edge,
	       const RiseFall *end_rf,
	       bool inst_matches_reg_clk_pin,
	       const Network *network) const;
  virtual const char *cmdKeyword() const;

  // -rise|-fall endpoint transition.
  const RiseFallBoth *end_rf_;

private:
  DISALLOW_COPY_AND_ASSIGN(ExceptionTo);
};

class ExceptionThru : public ExceptionPt
{
public:
  ExceptionThru(PinSet *pins,
		NetSet *nets,
		InstanceSet *insts,
		const RiseFallBoth *rf,
		bool own_pts,
		const Network *network);
  ~ExceptionThru();
  ExceptionThru *clone(const Network *network);
  virtual const char *asString(const Network *network) const;
  virtual bool isThru() const { return true; }
  PinSet *pins() { return pins_; }
  EdgePinsSet *edges() { return edges_; }
  NetSet *nets() { return nets_; }
  InstanceSet *instances() { return insts_; }
  virtual ClockSet *clks() { return nullptr; }
  bool hasObjects() const;
  void deleteObjects(ExceptionThru *pt);
  virtual void allPins(const Network *network,
		       PinSet *pins);
  bool matches(const Pin *from_pin,
	       const Pin *to_pin,
	       const RiseFall *to_rf,
	       const Network *network);
  bool equal(ExceptionThru *thru) const;
  virtual int nameCmp(ExceptionPt *pt,
		      const Network *network) const;
  virtual void mergeInto(ExceptionPt *pt);
  bool intersectsPts(ExceptionThru *thru) const;
  virtual int typePriority() const { return 2; }
  virtual size_t objectCount() const;
  virtual void connectPinAfter(PinSet *drvrs,
			       Network *network);
  virtual void disconnectPinBefore(Pin *pin,
				   Network *network);

protected:
  void findHash();
  virtual void addPin(Pin *pin);
  virtual void addEdge(EdgePins *edge);
  virtual void addNet(Net *net);
  virtual void addInstance(Instance *inst);
  virtual void addClock(Clock *) {}
  void deletePin(Pin *pin);
  void deleteEdge(EdgePins *edge);
  void deleteNet(Net *net);
  void deleteInstance(Instance *inst);
  void makeAllEdges(const Network *network);
  void makePinEdges(const Network *network);
  void makeNetEdges(const Network *network);
  void makeInstEdges(const Network *network);
  void makeHpinEdges(const Pin *pin,
		     const Network *network);
  void makePinEdges(Pin *pin,
		    const Network *network);
  void makeNetEdges(Net *net,
		    const Network *network);
  void makeInstEdges(Instance *inst,
		     Network *network);
  void deletePinEdges(Pin *pin,
		      Network *network);
  void deleteNetEdges(Net *net,
		      const Network *network);
  void deleteInstEdges(Instance *inst,
		       Network *network);

  // Leaf/port pins.
  PinSet *pins_;
  // Graph edges that traverse thru hierarchical pins.
  EdgePinsSet *edges_;
  NetSet *nets_;
  InstanceSet *insts_;

private:
  DISALLOW_COPY_AND_ASSIGN(ExceptionThru);
};

ExceptionThruSeq *
exceptionThrusClone(ExceptionThruSeq *thrus,
		    const Network *network);

// Iterate uniformly across exception from/thru/to's.
class ExceptionPtIterator
{
public:
  explicit ExceptionPtIterator(const ExceptionPath *exception);
  bool hasNext();
  ExceptionPt *next();

private:
  DISALLOW_COPY_AND_ASSIGN(ExceptionPtIterator);

  const ExceptionPath *exception_;
  bool from_done_;
  ExceptionThruSeq::Iterator thru_iter_;
  bool to_done_;
};

// Visitor for exception point sets expanded into single object paths.
// For example:
//  -from {A B} -to {C D}
// expands into
//  -from A -to C
//  -from A -to D
//  -from B -to C
//  -from B -to D
class ExpandedExceptionVisitor
{
public:
  ExpandedExceptionVisitor(ExceptionPath *exception,
			   const Network *network);
  virtual ~ExpandedExceptionVisitor() {}
  void visitExpansions();
  // From/thrus/to have a single exception point (pin/instance/net/clock).
  virtual void visit(ExceptionFrom *from,
		     ExceptionThruSeq *thrus,
		     ExceptionTo *to) = 0;

protected:
  ExceptionPath *exception_;
  const Network *network_;

private:
  DISALLOW_COPY_AND_ASSIGN(ExpandedExceptionVisitor);
  void expandFrom();
  void expandThrus(ExceptionFrom *expanded_from);
  void expandThru(ExceptionFrom *expanded_from,
		  ExceptionThruSeq::Iterator &thru_iter,
		  ExceptionThruSeq *expanded_thrus);
  void expandTo(ExceptionFrom *expanded_from,
		ExceptionThruSeq *expanded_thrus);
};

// States used by tags to know what exception points have been seen
// so far in a path.
class ExceptionState
{
public:
  ExceptionState(ExceptionPath *exception,
		 ExceptionThru *next_thru,
		 int index);
  ExceptionPath *exception() { return exception_; }
  bool matchesNextThru(const Pin *from_pin,
		       const Pin *to_pin,
		       const RiseFall *to_rf,
		       const MinMax *min_max,
		       const Network *network) const;
  bool isComplete() const;
  ExceptionThru *nextThru() const { return next_thru_; }
  ExceptionState *nextState() const { return next_state_; }
  void setNextState(ExceptionState *next_state);
  size_t hash() const;
  
private:
  DISALLOW_COPY_AND_ASSIGN(ExceptionState);

  ExceptionPath *exception_;
  ExceptionThru *next_thru_;
  ExceptionState *next_state_;
  int index_;
};

// Exception thrown by check.
class EmptyExpceptionPt : public Exception
{
public:
  virtual const char *what() const noexcept;
};

// Throws EmptyExpceptionPt it finds an empty exception point.
void
checkFromThrusTo(ExceptionFrom *from,
		 ExceptionThruSeq *thrus,
		 ExceptionTo *to);

void
sortExceptions(ExceptionPathSet *set,
	       ExceptionPathSeq &exceptions,
	       Network *network);

bool
intersects(PinSet *set1,
	   PinSet *set2);
bool
intersects(ClockSet *set1,
	   ClockSet *set2);

} // namespace
