// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "Error.hh"
#include "SdcClass.hh"
#include "SdcCmdComment.hh"

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

using ExceptionPathSeq = std::vector<ExceptionPath*>;

class ExceptionPath : public SdcCmdComment
{
public:
  ExceptionPath(ExceptionFrom *from,
                ExceptionThruSeq *thrus,
                ExceptionTo *to,
                const MinMaxAll *min_max,
                bool own_pts,
                int priority,
                std::string_view comment);
  virtual ~ExceptionPath();
  size_t id() const { return id_; }
  void setId(size_t id);
  virtual bool isFalse() const { return false; }
  virtual bool isLoop() const { return false; }
  virtual bool isMultiCycle() const { return false; }
  virtual bool isPathDelay() const { return false; }
  virtual bool isGroupPath() const { return false; }
  virtual bool isFilter() const { return false; }
  virtual ExceptionPathType type() const = 0;
  virtual std::string to_string(const Network *network) const;
  virtual std::string_view typeString() const = 0;
  ExceptionFrom *from() const { return from_; }
  ExceptionThruSeq *thrus() const { return thrus_; }
  ExceptionTo *to() const { return to_; }
  ExceptionPt *firstPt();
  bool intersectsPts(ExceptionPath *exception,
                     const Network *network) const;
  const MinMaxAll *minMax() const { return min_max_; }
  virtual bool matches(const MinMax *min_max,
                       bool exact) const;
  bool matchesFirstPt(const RiseFall *to_rf,
                      const MinMax *min_max);
  ExceptionState *firstState();
  virtual bool resetMatch(ExceptionFrom *from,
                          ExceptionThruSeq *thrus,
                          ExceptionTo *to,
                          const MinMaxAll *min_max,
                          const Network *network);
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
  void deleteInstance(const Instance *inst,
                      const Network *network);

  // Default handlers.
  virtual bool useEndClk() const { return false; }
  virtual int pathMultiplier() const { return 0; }
  virtual float delay() const { return 0.0; }
  virtual std::string_view name() const { return {}; }
  virtual bool isDefault() const { return false; }
  virtual bool ignoreClkLatency() const { return false; }
  virtual bool breakPath() const { return false; }

protected:
  std::string fromThruToString(const Network *network) const;
  void makeStates();

  ExceptionFrom *from_;
  ExceptionThruSeq *thrus_;
  ExceptionTo *to_;
  const MinMaxAll *min_max_;
  bool own_pts_;
  int priority_;
  size_t id_{0};                   // Unique ID assigned by Sdc.
  ExceptionState *states_;
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
            std::string_view comment);
  FalsePath(ExceptionFrom *from,
            ExceptionThruSeq *thrus,
            ExceptionTo *to,
            const MinMaxAll *min_max,
            bool own_pts,
            int priority,
            std::string_view comment);
  ExceptionPath *clone(ExceptionFrom *from,
                       ExceptionThruSeq *thrus,
                       ExceptionTo *to,
                       bool own_pts) override;
  bool isFalse() const override { return true; }
  ExceptionPathType type() const override { return ExceptionPathType::false_path; }
  std::string_view typeString() const override;
  bool mergeable(ExceptionPath *exception) const override;
  bool overrides(ExceptionPath *exception) const override;
  int typePriority() const override;
  bool tighterThan(ExceptionPath *exception) const override;
};

// Loop paths are false paths used to disable paths around
// combinational loops when dynamic loop breaking is enabled.
class LoopPath : public FalsePath
{
public:
  LoopPath(ExceptionThruSeq *thrus,
           bool own_pts);
  bool isLoop() const override { return true; }
  ExceptionPathType type() const override { return ExceptionPathType::loop; }
  std::string_view typeString() const override;
  bool mergeable(ExceptionPath *exception) const override;
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
            bool break_path,
            float delay,
            bool own_pts,
            std::string_view comment);
  ExceptionPath *clone(ExceptionFrom *from,
                       ExceptionThruSeq *thrus,
                       ExceptionTo *to,
                       bool own_pts) override;
  bool isPathDelay() const override { return true; }
  ExceptionPathType type() const override { return ExceptionPathType::path_delay; }
  std::string to_string(const Network *network) const override;
  std::string_view typeString() const override;
  bool mergeable(ExceptionPath *exception) const override;
  bool overrides(ExceptionPath *exception) const override;
  float delay() const override { return delay_; }
  int typePriority() const override;
  bool tighterThan(ExceptionPath *exception) const override;
  bool ignoreClkLatency() const override { return ignore_clk_latency_; }
  bool breakPath() const override { return break_path_; }

protected:
  bool ignore_clk_latency_;
  bool break_path_;
  float delay_;
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
                 std::string_view comment);
  ExceptionPath *clone(ExceptionFrom *from,
                       ExceptionThruSeq *thrus,
                       ExceptionTo *to,
                       bool own_pts) override;
  bool isMultiCycle() const override { return true; }
  ExceptionPathType type() const override { return ExceptionPathType::multi_cycle; }
  bool matches(const MinMax *min_max,
               bool exactly) const override;
  std::string to_string(const Network *network) const override;
  std::string_view typeString() const override;
  bool mergeable(ExceptionPath *exception) const override;
  bool overrides(ExceptionPath *exception) const override;
  bool useEndClk() const override { return use_end_clk_; }
  int pathMultiplier(const MinMax *min_max) const;  // overload, not override
  int pathMultiplier() const override { return path_multiplier_; }
  int priority(const MinMax *min_max) const override;
  int typePriority() const override;
  bool tighterThan(ExceptionPath *exception) const override;

  using ExceptionPath::priority;

protected:
  bool use_end_clk_;
  int path_multiplier_;
};

// Filter used restrict path reporting -from/-thru nets/pins.
class FilterPath : public ExceptionPath
{
public:
  FilterPath(ExceptionFrom *from,
             ExceptionThruSeq *thrus,
             ExceptionTo *to,
             bool own_pts);
  ExceptionPath *clone(ExceptionFrom *from,
                       ExceptionThruSeq *thrus,
                       ExceptionTo *to,
                       bool own_pts) override;
  bool isFilter() const override { return true; }
  ExceptionPathType type() const override { return ExceptionPathType::filter; }
  std::string_view typeString() const override;
  bool mergeable(ExceptionPath *exception) const override;
  bool overrides(ExceptionPath *exception) const override;
  bool resetMatch(ExceptionFrom *from,
                  ExceptionThruSeq *thrus,
                  ExceptionTo *to,
                  const MinMaxAll *min_max,
                  const Network *network) override;
  int typePriority() const override;
  bool tighterThan(ExceptionPath *exception) const override;
};

class GroupPath : public ExceptionPath
{
public:
  GroupPath(std::string_view name,
            bool is_default,
            ExceptionFrom *from,
            ExceptionThruSeq *thrus,
            ExceptionTo *to,
            bool own_pts,
            std::string_view comment);
  ExceptionPath *clone(ExceptionFrom *from,
                       ExceptionThruSeq *thrus,
                       ExceptionTo *to,
                       bool own_pts) override;
  bool isGroupPath() const override { return true; }
  ExceptionPathType type() const override { return ExceptionPathType::group_path; }
  std::string_view typeString() const override;
  bool mergeable(ExceptionPath *exception) const override;
  bool overrides(ExceptionPath *exception) const override;
  int typePriority() const override;
  bool tighterThan(ExceptionPath *exception) const override;
  std::string_view name() const override { return name_; }
  bool isDefault() const override { return is_default_; }

protected:
  std::string name_;
  bool is_default_;
};

// Base class for Exception from/thru/to.
class ExceptionPt
{
public:
  ExceptionPt(const RiseFallBoth *rf,
              bool own_pts);
  virtual ~ExceptionPt() = default;
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
  virtual int compare(ExceptionPt *pt,
                      const Network *network) const = 0;
  virtual void mergeInto(ExceptionPt *pt,
                         const Network *network) = 0;
  // All pins and instance/net pins.
  virtual PinSet allPins(const Network *network) = 0;
  virtual int typePriority() const = 0;
  virtual std::string to_string(const Network *network) const = 0;
  virtual size_t objectCount() const = 0;
  virtual void addPin(const Pin *pin,
                      const Network *network) = 0;
  virtual void addClock(Clock *clk) = 0;
  virtual void addInstance(const Instance *inst,
                           const Network *network) = 0;
  virtual void addNet(const Net *net,
                      const Network *network) = 0;
  virtual void addEdge(const EdgePins &edge,
                       const Network *network) = 0;
  virtual void connectPinAfter(PinSet *,
			       Network *network) = 0;
  virtual void deletePinBefore(const Pin *pin,
                               Network *network) = 0;

protected:
  const RiseFallBoth *rf_;
  // True when the pin/net/inst/edge sets are owned by the exception point.
  bool own_pts_;
  // Hash is cached because there may be many objects to speed up
  // exception merging.
  size_t hash_{0};

  // Maximum number of objects for to_string() to show.
  static const int to_string_max_objects_;
  static const size_t hash_clk  =  3;
  static const size_t hash_pin  =  5;
  static const size_t hash_net  =  7;
  static const size_t hash_inst = 11;
};

class ExceptionFromTo : public ExceptionPt
{
public:
  ExceptionFromTo(PinSet *pins,
                  ClockSet *clks,
                  InstanceSet *insts,
                  const RiseFallBoth *rf,
                  bool own_pts,
                  const Network *network);
  ~ExceptionFromTo() override;
  PinSet *pins() override { return pins_; }
  bool hasPins() const;
  ClockSet *clks() override { return clks_; }
  bool hasClocks() const;
  InstanceSet *instances() override { return insts_; }
  bool hasInstances() const;
  NetSet *nets() override { return nullptr; }
  EdgePinsSet *edges() override { return nullptr; }
  bool hasObjects() const;
  void deleteObjects(ExceptionFromTo *pt,
                     const Network *network);
  PinSet allPins(const Network *network) override;
  bool equal(ExceptionFromTo *from_to) const;
  int compare(ExceptionPt *pt,
              const Network *network) const override;
  void mergeInto(ExceptionPt *pt,
                 const Network *network) override;
  std::string to_string(const Network *network) const override;
  size_t objectCount() const override;
  void deleteClock(Clock *clk);
  void addPin(const Pin *pin,
              const Network *network) override;
  void addClock(Clock *clk) override;
  void addInstance(const Instance *inst,
                   const Network *network) override;
  void addNet(const Net *,
              const Network *) override {}
  void addEdge(const EdgePins &,
               const Network *) override {}
  void connectPinAfter(PinSet *,
		       Network *) override {}
  void deletePinBefore(const Pin *,
                       Network *) override;
  void deleteInstance(const Instance *inst,
                      const Network *network);

protected:
  virtual void findHash(const Network *network);

  void deletePin(const Pin *pin,
                 const Network *network);
  virtual const char *cmdKeyword() const = 0;

  PinSet *pins_;
  ClockSet *clks_;
  InstanceSet *insts_;
};

class ExceptionFrom : public ExceptionFromTo
{
public:
  ExceptionFrom(PinSet *pins,
                ClockSet *clks,
                InstanceSet *insts,
                const RiseFallBoth *rf,
                bool own_pts,
                const Network *network);
  ExceptionFrom *clone(const Network *network);
  bool isFrom() const override { return true; }
  bool intersectsPts(ExceptionFrom *from,
                     const Network *network) const;
  int typePriority() const override { return 0; }

protected:
  const char *cmdKeyword() const override;
  void findHash(const Network *network) override;
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
              bool own_pts,
              const Network *network);
  ExceptionTo *clone(const Network *network);
  bool isTo() const override { return true; }
  std::string to_string(const Network *network) const override;
  const RiseFallBoth *endTransition() { return end_rf_; }
  bool intersectsPts(ExceptionTo *to,
                     const Network *network) const;
  int typePriority() const override { return 1; }
  bool matches(const Pin *pin,
               const ClockEdge *clk_edge, 
               const RiseFall *end_rf,
               const Network *network) const;
  bool matches(const Pin *pin,
               const RiseFall *end_rf) const;
  bool matches(const Clock *clk) const;
  bool matches(const Pin *pin,
               const RiseFall *end_rf,
               const Network *network) const;
  bool matchesFilter(const Pin *pin,
                     const ClockEdge *clk_edge,
                     const RiseFall *end_rf,
                     const Network *network) const;
  int compare(ExceptionPt *pt,
              const Network *network) const override;

protected:
  bool matches(const Pin *pin,
               const ClockEdge *clk_edge,
               const RiseFall *end_rf,
               bool inst_matches_reg_clk_pin,
               const Network *network) const;
  const char *cmdKeyword() const override;

  // -rise|-fall endpoint transition.
  const RiseFallBoth *end_rf_;
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
  ~ExceptionThru() override;
  ExceptionThru *clone(const Network *network);
  std::string to_string(const Network *network) const override;
  bool isThru() const override { return true; }
  PinSet *pins() override { return pins_; }
  EdgePinsSet *edges() override { return edges_; }
  NetSet *nets() override { return nets_; }
  InstanceSet *instances() override { return insts_; }
  ClockSet *clks() override { return nullptr; }
  bool hasObjects() const;
  void deleteObjects(ExceptionThru *pt,
                     const Network *network);
  PinSet allPins(const Network *network) override;
  bool matches(const Pin *from_pin,
               const Pin *to_pin,
               const RiseFall *to_rf,
               const Network *network);
  bool equal(ExceptionThru *thru) const;
  int compare(ExceptionPt *pt,
              const Network *network) const override;
  void mergeInto(ExceptionPt *pt,
                 const Network *network) override;
  bool intersectsPts(ExceptionThru *thru,
                     const Network *network) const;
  int typePriority() const override { return 2; }
  size_t objectCount() const override;
  void addPin(const Pin *pin,
              const Network *network) override;
  void addEdge(const EdgePins &edge,
               const Network *network) override;
  void addNet(const Net *net,
              const Network *network) override;
  void addInstance(const Instance *inst,
                   const Network *network) override;
  void addClock(Clock *) override {}
  void connectPinAfter(PinSet *drvrs,
		       Network *network) override;
  void deletePinBefore(const Pin *pin,
                       Network *network) override;
  void deleteInstance(const Instance *inst,
                      const Network *network);

protected:
  void findHash(const Network *network);
  void deletePin(const Pin *pin,
                 const Network *network);
  void deleteEdge(const EdgePins &edge);
  void deleteNet(const Net *net,
                 const Network *network);
  void makeAllEdges(const Network *network);
  void makePinEdges(const Network *network);
  void makeNetEdges(const Network *network);
  void makeInstEdges(const Network *network);
  void makeHpinEdges(const Pin *pin,
                     const Network *network);
  void makePinEdges(const Pin *pin,
                    const Network *network);
  void makeNetEdges(const Net *net,
                    const Network *network);
  void makeInstEdges(Instance *inst,
                     Network *network);
  void deletePinEdges(const Pin *pin,
                      Network *network);
  void deleteNetEdges(Net *net,
                      const Network *network);
  void deleteInstEdges(Instance *inst,
                       Network *network);

  // Leaf/port pins.
  PinSet *pins_;
  // Graph edges that traverse thru hierarchical pins.
  EdgePinsSet *edges_{nullptr};
  NetSet *nets_;
  InstanceSet *insts_;
};

ExceptionThruSeq *
exceptionThrusClone(ExceptionThruSeq *thrus,
                    const Network *network);

// Iterate uniformly across exception from/thru/to's.
class ExceptionPtIterator
{
public:
  ExceptionPtIterator(const ExceptionPath *exception);
  bool hasNext();
  ExceptionPt *next();

private:
  const ExceptionPath *exception_;
  bool from_done_{false};
  ExceptionThruSeq::iterator thru_iter_;
  bool to_done_{false};
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
  void visitExpansions();
  // From/thrus/to have a single exception point (pin/instance/net/clock).
  virtual void visit(ExceptionFrom *from,
                     ExceptionThruSeq *thrus,
                     ExceptionTo *to) = 0;

protected:
  ExceptionPath *exception_;
  const Network *network_;

private:
  void expandFrom();
  void expandThrus(ExceptionFrom *expanded_from);
  void expandThru(ExceptionFrom *expanded_from,
                  size_t next_thru_idx,
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
  const ExceptionPath *exception() const { return exception_; }
  bool matchesNextThru(const Pin *from_pin,
                       const Pin *to_pin,
                       const RiseFall *to_rf,
                       const MinMax *min_max,
                       const Network *network) const;
  bool isComplete() const;
  ExceptionThru *nextThru() const { return next_thru_; }
  ExceptionState *nextState() const { return next_state_; }
  void setNextState(ExceptionState *next_state);
  int index() const { return index_; }
  size_t hash() const;
  
private:
  ExceptionPath *exception_;
  ExceptionThru *next_thru_;
  ExceptionState *next_state_{nullptr};
  int index_;
};

int
exceptionStateCmp(const ExceptionState *state1,
                  const ExceptionState *state2);

// Exception thrown by check.
class EmptyExpceptionPt : public Exception
{
public:
  const char *what() const noexcept override;
};

class ExceptionPathLess
{
public:
  ExceptionPathLess(const Network *network);
  bool operator()(const ExceptionPath *except1,
                  const ExceptionPath *except2) const;

private:
  const Network *network_;
};

// Throws EmptyExpceptionPt it finds an empty exception point.
void
checkFromThrusTo(ExceptionFrom *from,
                 ExceptionThruSeq *thrus,
                 ExceptionTo *to);

} // namespace sta
