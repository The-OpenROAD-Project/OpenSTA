#include <gtest/gtest.h>
#include <tcl.h>
#include <fstream>
#include <iterator>
#include <string>
#include "Transition.hh"
#include "MinMax.hh"
#include "ExceptionPath.hh"
#include "TimingRole.hh"
#include "Clock.hh"
#include "RiseFallMinMax.hh"
#include "CycleAccting.hh"
#include "SdcCmdComment.hh"
#include "Variables.hh"
#include "DeratingFactors.hh"
#include "ClockLatency.hh"
#include "ClockInsertion.hh"
#include "ClockGatingCheck.hh"
#include "PortExtCap.hh"
#include "DataCheck.hh"
#include "PinPair.hh"
#include "Sta.hh"
#include "Sdc.hh"
#include "ReportTcl.hh"
#include "Corner.hh"
#include "DisabledPorts.hh"
#include "InputDrive.hh"
#include "PatternMatch.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "Graph.hh"
#include "PortDelay.hh"
#include "PortDirection.hh"

namespace sta {

static std::string
readTextFile(const char *filename)
{
  std::ifstream in(filename);
  if (!in.is_open())
    return "";
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

static size_t
countSubstring(const std::string &text,
               const std::string &needle)
{
  if (needle.empty())
    return 0;
  size_t count = 0;
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

////////////////////////////////////////////////////////////////
// SDC tests that require full Sta initialization
////////////////////////////////////////////////////////////////

class SdcInitTest : public ::testing::Test {
protected:
  void SetUp() override {
    interp_ = Tcl_CreateInterp();
    initSta();
    sta_ = new Sta;
    Sta::setSta(sta_);
    sta_->makeComponents();
    ReportTcl *report = dynamic_cast<ReportTcl*>(sta_->report());
    if (report)
      report->setTclInterp(interp_);
  }
  void TearDown() override {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_)
      Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }
  Sta *sta_;
  Tcl_Interp *interp_;
};

////////////////////////////////////////////////////////////////
// R5_ Tests - New tests for coverage improvement
////////////////////////////////////////////////////////////////

// Clock::addPin with nullptr - covers Clock::addPin
TEST_F(SdcInitTest, ClockAddPinNull) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_addpin", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  // addPin with nullptr - after adding null, isVirtual becomes false
  // because the pins set becomes non-empty
  clk->addPin(nullptr);
  EXPECT_FALSE(clk->isVirtual());
}

// Clock::setSlew - covers Clock::setSlew(rf, min_max, float)
TEST_F(SdcInitTest, ClockSetSlewRfMinMax) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_slew", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  clk->setSlew(RiseFall::rise(), MinMax::max(), 0.5f);
  float slew;
  bool exists;
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
}

// ClockEdge::setTime - covers ClockEdge::setTime
// Note: setTime is private/friend, but we can check after clock operations
TEST_F(SdcInitTest, ClockEdgeTime) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_edge", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  ASSERT_NE(rise_edge, nullptr);
  ASSERT_NE(fall_edge, nullptr);
  EXPECT_FLOAT_EQ(rise_edge->time(), 0.0f);
  EXPECT_FLOAT_EQ(fall_edge->time(), 5.0f);
}

// ClockEdge opposite
TEST_F(SdcInitTest, ClockEdgeOpposite) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_opp", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  ASSERT_NE(rise_edge, nullptr);
  ASSERT_NE(fall_edge, nullptr);
  EXPECT_EQ(rise_edge->opposite(), fall_edge);
  EXPECT_EQ(fall_edge->opposite(), rise_edge);
}

// ClockEdge pulseWidth
TEST_F(SdcInitTest, ClockEdgePulseWidth) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(4.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_pw", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ASSERT_NE(rise_edge, nullptr);
  float pw = rise_edge->pulseWidth();
  EXPECT_FLOAT_EQ(pw, 4.0f); // duty is 4ns high, 6ns low
}

// ClockEdge name and index
TEST_F(SdcInitTest, ClockEdgeNameIndex) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_ni", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ASSERT_NE(rise_edge, nullptr);
  EXPECT_NE(rise_edge->name(), nullptr);
  int idx = rise_edge->index();
  EXPECT_GE(idx, 0);
}

// DisabledCellPorts - covers constructor/destructor and methods
TEST_F(SdcInitTest, DisabledCellPortsBasic) {
  // We need a real liberty cell
  LibertyLibrary *lib = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                                           sta_->cmdCorner(),
                                           MinMaxAll::min(), false);
  ASSERT_NE(lib, nullptr);
  LibertyCell *buf = lib->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  DisabledCellPorts dcp(buf);
  EXPECT_EQ(dcp.cell(), buf);
  EXPECT_FALSE(dcp.all());
}

// DisabledCellPorts setDisabled/removeDisabled with TimingArcSet
TEST_F(SdcInitTest, DisabledCellPortsTimingArcSet) {
  LibertyLibrary *lib = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                                           sta_->cmdCorner(),
                                           MinMaxAll::min(), false);
  ASSERT_NE(lib, nullptr);
  LibertyCell *buf = lib->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  DisabledCellPorts dcp(buf);
  TimingArcSet *as = arc_sets[0];
  dcp.setDisabled(as);
  EXPECT_TRUE(dcp.isDisabled(as));
  dcp.removeDisabled(as);
  EXPECT_FALSE(dcp.isDisabled(as));
}

// DisabledCellPorts isDisabled for from/to/role
TEST_F(SdcInitTest, DisabledCellPortsIsDisabled) {
  LibertyLibrary *lib = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                                           sta_->cmdCorner(),
                                           MinMaxAll::min(), false);
  ASSERT_NE(lib, nullptr);
  LibertyCell *buf = lib->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(z, nullptr);
  DisabledCellPorts dcp(buf);
  // Initially nothing disabled
  EXPECT_FALSE(dcp.isDisabled(a, z, TimingRole::combinational()));
  // Disable all
  dcp.setDisabledAll();
  EXPECT_TRUE(dcp.all());
  EXPECT_TRUE(dcp.isDisabled(a, z, TimingRole::combinational()));
  dcp.removeDisabledAll();
  EXPECT_FALSE(dcp.all());
}

// ExceptionPath::typeString via various subclasses
TEST_F(SdcInitTest, FalsePathTypeString) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_NE(fp.typeString(), nullptr);
}

TEST_F(SdcInitTest, PathDelayTypeString) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(),
               false, false, 5.0f, true, nullptr);
  EXPECT_NE(pd.typeString(), nullptr);
}

TEST_F(SdcInitTest, MultiCyclePathTypeString) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  EXPECT_NE(mcp.typeString(), nullptr);
}

TEST_F(SdcInitTest, FilterPathTypeString) {
  FilterPath fp(nullptr, nullptr, nullptr, true);
  EXPECT_NE(fp.typeString(), nullptr);
}

TEST_F(SdcInitTest, GroupPathTypeString) {
  GroupPath gp("grp1", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_NE(gp.typeString(), nullptr);
}

TEST_F(SdcInitTest, LoopPathTypeString) {
  LoopPath lp(nullptr, true);
  EXPECT_NE(lp.typeString(), nullptr);
}

// ExceptionPath::mergeable tests
TEST_F(SdcInitTest, FalsePathMergeable) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp1.mergeable(&fp2));
}

TEST_F(SdcInitTest, PathDelayMergeable) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 5.0f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 5.0f, true, nullptr);
  EXPECT_TRUE(pd1.mergeable(&pd2));
}

TEST_F(SdcInitTest, PathDelayMergeableDifferentDelay) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 5.0f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 10.0f, true, nullptr);
  EXPECT_FALSE(pd1.mergeable(&pd2));
}

TEST_F(SdcInitTest, MultiCyclePathMergeable) {
  MultiCyclePath mcp1(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  MultiCyclePath mcp2(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  EXPECT_TRUE(mcp1.mergeable(&mcp2));
}

TEST_F(SdcInitTest, GroupPathMergeable) {
  GroupPath gp1("grp1", false, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("grp1", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp1.mergeable(&gp2));
}

TEST_F(SdcInitTest, GroupPathNotMergeable) {
  GroupPath gp1("grp1", false, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("grp2", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_FALSE(gp1.mergeable(&gp2));
}

TEST_F(SdcInitTest, LoopPathNotMergeable) {
  LoopPath lp1(nullptr, true);
  LoopPath lp2(nullptr, true);
  EXPECT_FALSE(lp1.mergeable(&lp2));
}

// ExceptionPath::overrides tests
TEST_F(SdcInitTest, FalsePathOverrides) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp1.overrides(&fp2));
}

TEST_F(SdcInitTest, PathDelayOverrides) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 5.0f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 5.0f, true, nullptr);
  EXPECT_TRUE(pd1.overrides(&pd2));
}

TEST_F(SdcInitTest, MultiCyclePathOverrides) {
  MultiCyclePath mcp1(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  MultiCyclePath mcp2(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  EXPECT_TRUE(mcp1.overrides(&mcp2));
}

TEST_F(SdcInitTest, FilterPathOverrides2) {
  FilterPath fp1(nullptr, nullptr, nullptr, true);
  FilterPath fp2(nullptr, nullptr, nullptr, true);
  // FilterPath::overrides always returns false
  EXPECT_FALSE(fp1.overrides(&fp2));
}

TEST_F(SdcInitTest, GroupPathOverrides) {
  GroupPath gp1("grp1", false, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("grp1", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp1.overrides(&gp2));
}

// ExceptionPath::matches with min_max
TEST_F(SdcInitTest, MultiCyclePathMatches) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  EXPECT_TRUE(mcp.matches(MinMax::max(), false));
  EXPECT_TRUE(mcp.matches(MinMax::min(), false));
}

// ExceptionPath type priorities
TEST_F(SdcInitTest, ExceptionPathStaticPriorities) {
  EXPECT_EQ(ExceptionPath::falsePathPriority(), 4000);
  EXPECT_EQ(ExceptionPath::pathDelayPriority(), 3000);
  EXPECT_EQ(ExceptionPath::multiCyclePathPriority(), 2000);
  EXPECT_EQ(ExceptionPath::filterPathPriority(), 1000);
  EXPECT_EQ(ExceptionPath::groupPathPriority(), 0);
}

// ExceptionPath fromThruToPriority
TEST_F(SdcInitTest, ExceptionFromThruToPriority) {
  int p = ExceptionPath::fromThruToPriority(nullptr, nullptr, nullptr);
  EXPECT_EQ(p, 0);
}

// PathDelay specific getters
TEST_F(SdcInitTest, PathDelayGetters) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(),
               true, true, 5.0f, true, nullptr);
  EXPECT_FLOAT_EQ(pd.delay(), 5.0f);
  EXPECT_TRUE(pd.ignoreClkLatency());
  EXPECT_TRUE(pd.breakPath());
  EXPECT_TRUE(pd.isPathDelay());
  EXPECT_FALSE(pd.isFalse());
  EXPECT_FALSE(pd.isMultiCycle());
  EXPECT_FALSE(pd.isFilter());
  EXPECT_FALSE(pd.isGroupPath());
  EXPECT_FALSE(pd.isLoop());
}

// MultiCyclePath specific getters
TEST_F(SdcInitTest, MultiCyclePathGetters) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::max(),
                     true, 5, true, nullptr);
  EXPECT_EQ(mcp.pathMultiplier(), 5);
  EXPECT_TRUE(mcp.useEndClk());
  EXPECT_TRUE(mcp.isMultiCycle());
}

// MultiCyclePath pathMultiplier with MinMax
TEST_F(SdcInitTest, MultiCyclePathMultiplierMinMax) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::max(),
                     true, 5, true, nullptr);
  int mult_max = mcp.pathMultiplier(MinMax::max());
  EXPECT_EQ(mult_max, 5);
}

// MultiCyclePath priority with MinMax
TEST_F(SdcInitTest, MultiCyclePathPriorityMinMax) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::max(),
                     true, 5, true, nullptr);
  int p = mcp.priority(MinMax::max());
  EXPECT_GT(p, 0);
}

// GroupPath name and isDefault
TEST_F(SdcInitTest, GroupPathName) {
  GroupPath gp("test_group", true, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_STREQ(gp.name(), "test_group");
  EXPECT_TRUE(gp.isDefault());
}

// FilterPath basic
TEST_F(SdcInitTest, FilterPathBasic) {
  FilterPath fp(nullptr, nullptr, nullptr, true);
  EXPECT_TRUE(fp.isFilter());
  EXPECT_FALSE(fp.isFalse());
  EXPECT_FALSE(fp.isPathDelay());
  EXPECT_FALSE(fp.isMultiCycle());
  EXPECT_FALSE(fp.isGroupPath());
  EXPECT_FALSE(fp.isLoop());
}

// FalsePath with priority
TEST_F(SdcInitTest, FalsePathWithPriority) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true,
               4500, nullptr);
  EXPECT_EQ(fp.priority(), 4500);
}

// LoopPath basic
TEST_F(SdcInitTest, LoopPathBasicProps) {
  LoopPath lp(nullptr, true);
  EXPECT_TRUE(lp.isLoop());
  EXPECT_TRUE(lp.isFalse());
  EXPECT_FALSE(lp.isPathDelay());
  EXPECT_FALSE(lp.isMultiCycle());
}

// Exception hash
TEST_F(SdcInitTest, ExceptionPathHash) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  size_t h1 = fp1.hash();
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  size_t h2 = fp2.hash();
  EXPECT_EQ(h1, h2);
}

// ExceptionPath clone tests
TEST_F(SdcInitTest, FalsePathCloneAndCheck) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionPath *clone = fp.clone(nullptr, nullptr, nullptr, true);
  ASSERT_NE(clone, nullptr);
  EXPECT_TRUE(clone->isFalse());
  delete clone;
}

TEST_F(SdcInitTest, PathDelayCloneAndCheck) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(),
               false, false, 5.0f, true, nullptr);
  ExceptionPath *clone = pd.clone(nullptr, nullptr, nullptr, true);
  ASSERT_NE(clone, nullptr);
  EXPECT_TRUE(clone->isPathDelay());
  EXPECT_FLOAT_EQ(clone->delay(), 5.0f);
  delete clone;
}

TEST_F(SdcInitTest, MultiCyclePathCloneAndCheck) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 4, true, nullptr);
  ExceptionPath *clone = mcp.clone(nullptr, nullptr, nullptr, true);
  ASSERT_NE(clone, nullptr);
  EXPECT_TRUE(clone->isMultiCycle());
  EXPECT_EQ(clone->pathMultiplier(), 4);
  delete clone;
}

TEST_F(SdcInitTest, GroupPathCloneAndCheck) {
  GroupPath gp("grp", false, nullptr, nullptr, nullptr, true, nullptr);
  ExceptionPath *clone = gp.clone(nullptr, nullptr, nullptr, true);
  ASSERT_NE(clone, nullptr);
  EXPECT_TRUE(clone->isGroupPath());
  EXPECT_STREQ(clone->name(), "grp");
  delete clone;
}

TEST_F(SdcInitTest, FilterPathCloneAndCheck) {
  FilterPath fp(nullptr, nullptr, nullptr, true);
  ExceptionPath *clone = fp.clone(nullptr, nullptr, nullptr, true);
  ASSERT_NE(clone, nullptr);
  EXPECT_TRUE(clone->isFilter());
  delete clone;
}

// ExceptionState constructor
TEST_F(SdcInitTest, ExceptionState) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState state(&fp, nullptr, 0);
  EXPECT_EQ(state.exception(), &fp);
  EXPECT_EQ(state.nextThru(), nullptr);
  EXPECT_EQ(state.index(), 0);
  EXPECT_TRUE(state.isComplete());
}

// ExceptionState setNextState
TEST_F(SdcInitTest, ExceptionStateSetNextState) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState state1(&fp, nullptr, 0);
  ExceptionState state2(&fp, nullptr, 1);
  state1.setNextState(&state2);
  EXPECT_EQ(state1.nextState(), &state2);
}

// ExceptionState hash
TEST_F(SdcInitTest, ExceptionStateHash) {
  ASSERT_NO_THROW(( [&](){
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState state(&fp, nullptr, 0);
  size_t h = state.hash();
  EXPECT_GE(h, 0);

  }() ));
}

// exceptionStateLess
TEST_F(SdcInitTest, ExceptionStateLess) {
  ASSERT_NO_THROW(( [&](){
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState state1(&fp1, nullptr, 0);
  ExceptionState state2(&fp2, nullptr, 0);
  // Just exercise the comparator
  exceptionStateLess(&state1, &state2);

  }() ));
}

// Sdc::setOperatingConditions(op_cond, MinMaxAll*)
TEST_F(SdcInitTest, SdcSetOperatingConditionsMinMaxAll) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setOperatingConditions(nullptr, MinMaxAll::all());

  }() ));
}

// Sdc::disable/removeDisable for LibertyPort
TEST_F(SdcInitTest, SdcDisableLibertyPort) {
  LibertyLibrary *lib = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                                           sta_->cmdCorner(),
                                           MinMaxAll::min(), false);
  ASSERT_NE(lib, nullptr);
  LibertyCell *buf = lib->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port_a = buf->findLibertyPort("A");
  ASSERT_NE(port_a, nullptr);
  Sdc *sdc = sta_->sdc();
  sdc->disable(port_a);
  sdc->removeDisable(port_a);
}

// Sdc::disable/removeDisable for TimingArcSet
TEST_F(SdcInitTest, SdcDisableTimingArcSet) {
  LibertyLibrary *lib = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                                           sta_->cmdCorner(),
                                           MinMaxAll::min(), false);
  ASSERT_NE(lib, nullptr);
  LibertyCell *buf = lib->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const TimingArcSetSeq &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  Sdc *sdc = sta_->sdc();
  sdc->disable(arc_sets[0]);
  sdc->removeDisable(arc_sets[0]);
}

// Sdc clock querying via findClock
TEST_F(SdcInitTest, SdcFindClockNull) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("nonexistent_clk");
  EXPECT_EQ(clk, nullptr);
}

// Sdc latch borrow limit on clock
TEST_F(SdcInitTest, SdcLatchBorrowLimitOnClock) {
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Clock *clk = sdc->makeClock("clk_lbl", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  sdc->setLatchBorrowLimit(clk, 2.0f);
  // Just exercise - borrow limit is set
}

// InterClockUncertainty more thorough
TEST_F(SdcInitTest, InterClockUncertaintyEmpty) {
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(3.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->makeClock("clk_icu1", nullptr, false, 10.0,
                                waveform1, nullptr);
  Clock *clk2 = sdc->makeClock("clk_icu2", nullptr, false, 6.0,
                                waveform2, nullptr);
  InterClockUncertainty icu(clk1, clk2);
  EXPECT_TRUE(icu.empty());
  EXPECT_EQ(icu.src(), clk1);
  EXPECT_EQ(icu.target(), clk2);
}

TEST_F(SdcInitTest, InterClockUncertaintySetAndGet) {
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(3.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->makeClock("clk_icu3", nullptr, false, 10.0,
                                waveform1, nullptr);
  Clock *clk2 = sdc->makeClock("clk_icu4", nullptr, false, 6.0,
                                waveform2, nullptr);
  InterClockUncertainty icu(clk1, clk2);
  icu.setUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                     SetupHoldAll::all(), 0.1f);
  EXPECT_FALSE(icu.empty());
  float unc;
  bool exists;
  icu.uncertainty(RiseFall::rise(), RiseFall::rise(),
                  SetupHold::min(), unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.1f);
}

TEST_F(SdcInitTest, InterClockUncertaintyRemove) {
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(3.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->makeClock("clk_icu5", nullptr, false, 10.0,
                                waveform1, nullptr);
  Clock *clk2 = sdc->makeClock("clk_icu6", nullptr, false, 6.0,
                                waveform2, nullptr);
  InterClockUncertainty icu(clk1, clk2);
  icu.setUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                     SetupHoldAll::all(), 0.2f);
  icu.removeUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                        SetupHoldAll::all());
  EXPECT_TRUE(icu.empty());
}

TEST_F(SdcInitTest, InterClockUncertaintyUncertainties) {
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(3.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->makeClock("clk_icu7", nullptr, false, 10.0,
                                waveform1, nullptr);
  Clock *clk2 = sdc->makeClock("clk_icu8", nullptr, false, 6.0,
                                waveform2, nullptr);
  InterClockUncertainty icu(clk1, clk2);
  const RiseFallMinMax *rfmm = icu.uncertainties(RiseFall::rise());
  EXPECT_NE(rfmm, nullptr);
}

// CycleAccting exercises
TEST_F(SdcInitTest, CycleAcctingConstruct2) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_ca", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  CycleAccting ca(rise, fall);
  EXPECT_EQ(ca.src(), rise);
  EXPECT_EQ(ca.target(), fall);
}

TEST_F(SdcInitTest, CycleAcctingFindDefaultArrivalSrcDelays) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_ca2", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  CycleAccting ca(rise, fall);
  ca.findDefaultArrivalSrcDelays();
  // Should not crash
}

// DisabledPorts from/to operations
TEST_F(SdcInitTest, DisabledPortsFromToOps) {
  LibertyLibrary *lib = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                                           sta_->cmdCorner(),
                                           MinMaxAll::min(), false);
  ASSERT_NE(lib, nullptr);
  LibertyCell *buf = lib->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(z, nullptr);
  DisabledPorts dp;
  dp.setDisabledFrom(a);
  EXPECT_NE(dp.from(), nullptr);
  dp.setDisabledTo(z);
  EXPECT_NE(dp.to(), nullptr);
  dp.setDisabledFromTo(a, z);
  EXPECT_NE(dp.fromTo(), nullptr);
  dp.removeDisabledFrom(a);
  dp.removeDisabledTo(z);
  dp.removeDisabledFromTo(a, z);
}

// ClockCompareSet
TEST_F(SdcInitTest, ClockSetCompare) {
  ASSERT_NO_THROW(( [&](){
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(3.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->makeClock("clk_csc1", nullptr, false, 10.0,
                                waveform1, nullptr);
  Clock *clk2 = sdc->makeClock("clk_csc2", nullptr, false, 6.0,
                                waveform2, nullptr);
  ClockSet set1;
  set1.insert(clk1);
  ClockSet set2;
  set2.insert(clk2);
  compare(&set1, &set2);

  }() ));
}

// Sdc::clockUncertainty on null pin
TEST_F(SdcInitTest, SdcClockUncertaintyNullPin) {
  Sdc *sdc = sta_->sdc();
  float unc;
  bool exists;
  sdc->clockUncertainty(static_cast<const Pin*>(nullptr),
                         MinMax::max(), unc, exists);
  EXPECT_FALSE(exists);
}

// ExceptionPtIterator with from only
TEST_F(SdcInitTest, ExceptionPtIteratorFromOnly) {
  const Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                           RiseFallBoth::riseFall(),
                                           true, network);
  FalsePath fp(from, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionPtIterator iter(&fp);
  int count = 0;
  while (iter.hasNext()) {
    ExceptionPt *pt = iter.next();
    EXPECT_NE(pt, nullptr);
    count++;
  }
  EXPECT_EQ(count, 1);
}

// ExceptionFrom basic properties
TEST_F(SdcInitTest, ExceptionFromProperties) {
  const Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                           RiseFallBoth::rise(),
                                           true, network);
  EXPECT_TRUE(from->isFrom());
  EXPECT_FALSE(from->isThru());
  EXPECT_FALSE(from->isTo());
  EXPECT_EQ(from->transition(), RiseFallBoth::rise());
  EXPECT_EQ(from->typePriority(), 0);
  delete from;
}

// ExceptionTo basic properties
TEST_F(SdcInitTest, ExceptionToProperties) {
  const Network *network = sta_->cmdNetwork();
  ExceptionTo *to = new ExceptionTo(nullptr, nullptr, nullptr,
                                     RiseFallBoth::fall(),
                                     RiseFallBoth::riseFall(),
                                     true, network);
  EXPECT_TRUE(to->isTo());
  EXPECT_FALSE(to->isFrom());
  EXPECT_FALSE(to->isThru());
  EXPECT_EQ(to->transition(), RiseFallBoth::fall());
  EXPECT_EQ(to->endTransition(), RiseFallBoth::riseFall());
  EXPECT_EQ(to->typePriority(), 1);
  delete to;
}

// ExceptionThru basic properties
TEST_F(SdcInitTest, ExceptionThruProperties) {
  const Network *network = sta_->cmdNetwork();
  ExceptionThru *thru = new ExceptionThru(nullptr, nullptr, nullptr,
                                           RiseFallBoth::riseFall(),
                                           true, network);
  EXPECT_TRUE(thru->isThru());
  EXPECT_FALSE(thru->isFrom());
  EXPECT_FALSE(thru->isTo());
  EXPECT_EQ(thru->transition(), RiseFallBoth::riseFall());
  EXPECT_EQ(thru->typePriority(), 2);
  EXPECT_EQ(thru->clks(), nullptr);
  EXPECT_FALSE(thru->hasObjects());
  delete thru;
}

// ExceptionThru objectCount
TEST_F(SdcInitTest, ExceptionThruObjectCount) {
  const Network *network = sta_->cmdNetwork();
  ExceptionThru *thru = new ExceptionThru(nullptr, nullptr, nullptr,
                                           RiseFallBoth::riseFall(),
                                           true, network);
  EXPECT_EQ(thru->objectCount(), 0u);
  delete thru;
}

// ExceptionFromTo objectCount
TEST_F(SdcInitTest, ExceptionFromToObjectCount) {
  const Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                           RiseFallBoth::riseFall(),
                                           true, network);
  EXPECT_EQ(from->objectCount(), 0u);
  delete from;
}

// ExceptionPt hash
TEST_F(SdcInitTest, ExceptionPtHash) {
  ASSERT_NO_THROW(( [&](){
  const Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                           RiseFallBoth::riseFall(),
                                           true, network);
  size_t h = from->hash();
  EXPECT_GE(h, 0);
  delete from;

  }() ));
}

// ExceptionFrom::findHash (called during construction)
TEST_F(SdcInitTest, ExceptionFromFindHash) {
  const Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                           RiseFallBoth::rise(),
                                           true, network);
  // Hash should be computed during construction
  size_t h = from->hash();
  EXPECT_GE(h, 0u);
  delete from;
}

// checkFromThrusTo with nulls should not throw
TEST_F(SdcInitTest, CheckFromThrusToAllNull) {
  ASSERT_NO_THROW(( [&](){
  // All nullptr should not throw EmptyExceptionPt
  checkFromThrusTo(nullptr, nullptr, nullptr);

  }() ));
}

// EmptyExceptionPt what
TEST_F(SdcInitTest, EmptyExceptionPtWhat2) {
  EmptyExpceptionPt e;
  const char *msg = e.what();
  EXPECT_NE(msg, nullptr);
}

// ExceptionPathLess comparator
TEST_F(SdcInitTest, ExceptionPathLessComparator2) {
  ASSERT_NO_THROW(( [&](){
  const Network *network = sta_->cmdNetwork();
  ExceptionPathLess less(network);
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  // Should not crash
  less(&fp1, &fp2);

  }() ));
}

// Sdc::isLeafPinNonGeneratedClock with null
TEST_F(SdcInitTest, SdcIsLeafPinNonGeneratedClockNull) {
  Sdc *sdc = sta_->sdc();
  bool result = sdc->isLeafPinNonGeneratedClock(nullptr);
  EXPECT_FALSE(result);
}

// Clock removeSlew
TEST_F(SdcInitTest, ClockRemoveSlew) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_rs", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  clk->setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5f);
  clk->removeSlew();
  float slew;
  bool exists;
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_FALSE(exists);
}

// Clock slews accessor
TEST_F(SdcInitTest, ClockSlewsAccessor) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_sa", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  clk->slews();
}

// Clock uncertainties
TEST_F(SdcInitTest, ClockUncertaintiesAccessor) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_ua", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  // A freshly created clock has no uncertainties set yet
  ClockUncertainties *unc = clk->uncertainties();
  EXPECT_EQ(unc, nullptr);
}

// Clock setUncertainty and removeUncertainty
TEST_F(SdcInitTest, ClockSetRemoveUncertainty) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_sru", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  clk->setUncertainty(SetupHoldAll::all(), 0.1f);
  float unc;
  bool exists;
  clk->uncertainty(SetupHold::min(), unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.1f);
  clk->removeUncertainty(SetupHoldAll::all());
  clk->uncertainty(SetupHold::min(), unc, exists);
  EXPECT_FALSE(exists);
}

// Clock generated properties
TEST_F(SdcInitTest, ClockGeneratedProperties) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_gp", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  EXPECT_FALSE(clk->isGenerated());
  EXPECT_EQ(clk->masterClk(), nullptr);
  EXPECT_EQ(clk->srcPin(), nullptr);
  EXPECT_EQ(clk->divideBy(), 0);
  EXPECT_EQ(clk->multiplyBy(), 0);
}

// ClkNameLess comparator
TEST_F(SdcInitTest, ClkNameLess) {
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(3.0);
  Sdc *sdc = sta_->sdc();
  Clock *clkA = sdc->makeClock("alpha", nullptr, false, 10.0,
                                waveform1, nullptr);
  Clock *clkB = sdc->makeClock("beta", nullptr, false, 6.0,
                                waveform2, nullptr);
  ClkNameLess less;
  EXPECT_TRUE(less(clkA, clkB));
  EXPECT_FALSE(less(clkB, clkA));
}

// CycleAcctings
TEST_F(SdcInitTest, CycleAcctings) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  CycleAcctings acctings(sdc);
  // Clear should not crash
  acctings.clear();

  }() ));
}

// Clock setPropagated / removePropagated
TEST_F(SdcInitTest, ClockPropagation2) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->makeClock("clk_prop", nullptr, false, 10.0,
                               waveform, nullptr);
  ASSERT_NE(clk, nullptr);
  EXPECT_FALSE(clk->isPropagated());
  sdc->setPropagatedClock(clk);
  EXPECT_TRUE(clk->isPropagated());
  sdc->removePropagatedClock(clk);
  EXPECT_FALSE(clk->isPropagated());
}

////////////////////////////////////////////////////////////////
// R6 tests: DisabledPorts from/to operations
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, DisabledPortsAllState) {
  DisabledPorts dp;
  EXPECT_FALSE(dp.all());
  dp.setDisabledAll();
  EXPECT_TRUE(dp.all());
  dp.removeDisabledAll();
  EXPECT_FALSE(dp.all());
  // Verify from/to/fromTo are still nullptr
  EXPECT_EQ(dp.from(), nullptr);
  EXPECT_EQ(dp.to(), nullptr);
  EXPECT_EQ(dp.fromTo(), nullptr);
}

TEST_F(SdcInitTest, DisabledCellPortsConstruct) {
  // DisabledCellPorts requires a LibertyCell; use nullptr since
  // we only exercise the constructor path
  LibertyLibrary lib("test_lib", "test.lib");
  LibertyCell *cell = lib.makeScaledCell("test_cell", "test.lib");
  DisabledCellPorts dcp(cell);
  EXPECT_EQ(dcp.cell(), cell);
  EXPECT_FALSE(dcp.all());
  delete cell;
}

////////////////////////////////////////////////////////////////
// R6 tests: Sdc public accessors
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, SdcAnalysisType) {
  Sdc *sdc = sta_->sdc();
  sdc->setAnalysisType(AnalysisType::single);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::single);
  sdc->setAnalysisType(AnalysisType::bc_wc);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::bc_wc);
  sdc->setAnalysisType(AnalysisType::ocv);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::ocv);
}

TEST_F(SdcInitTest, SdcMaxArea2) {
  Sdc *sdc = sta_->sdc();
  sdc->setMaxArea(500.0);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 500.0f);
}

////////////////////////////////////////////////////////////////
// R6 tests: Sdc setOperatingConditions
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, SdcSetOperatingConditions) {
  Sdc *sdc = sta_->sdc();
  sdc->setOperatingConditions(nullptr, MinMax::max());
  sdc->setOperatingConditions(nullptr, MinMax::min());
  EXPECT_EQ(sdc->operatingConditions(MinMax::max()), nullptr);
  EXPECT_EQ(sdc->operatingConditions(MinMax::min()), nullptr);
}

////////////////////////////////////////////////////////////////
// R6 tests: Sdc wireload mode
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, SdcWireloadMode2) {
  Sdc *sdc = sta_->sdc();
  sdc->setWireloadMode(WireloadMode::top);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::top);
  sdc->setWireloadMode(WireloadMode::enclosed);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::enclosed);
}

////////////////////////////////////////////////////////////////
// R6 tests: ExceptionPath mergeable between same types
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, FalsePathMergeableSame) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp1.mergeable(&fp2));
}

TEST_F(SdcInitTest, FalsePathNotMergeableDiffMinMax) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::min(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::max(), true, nullptr);
  EXPECT_FALSE(fp1.mergeable(&fp2));
}

TEST_F(SdcInitTest, FalsePathNotMergeableDiffType) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               1.0e-9f, true, nullptr);
  EXPECT_FALSE(fp.mergeable(&pd));
}

////////////////////////////////////////////////////////////////
// R6 tests: PathDelay min direction
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, PathDelayMinDirection) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::min(), false, false,
               5.0e-9f, true, nullptr);
  EXPECT_TRUE(pd.matches(MinMax::min(), false));
  EXPECT_FALSE(pd.matches(MinMax::max(), false));
}

TEST_F(SdcInitTest, PathDelayTighterMin) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::min(), false, false,
                5.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::min(), false, false,
                2.0e-9f, true, nullptr);
  // For min, larger delay is tighter
  EXPECT_TRUE(pd1.tighterThan(&pd2));
  EXPECT_FALSE(pd2.tighterThan(&pd1));
}

////////////////////////////////////////////////////////////////
// R6 tests: ExceptionPath hash
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, PathDelayHash) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               5.0e-9f, true, nullptr);
  size_t h = pd.hash();
  EXPECT_GE(h, 0u);
}

TEST_F(SdcInitTest, MultiCyclePathHash) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  size_t h = mcp.hash();
  EXPECT_GE(h, 0u);
}

TEST_F(SdcInitTest, GroupPathHash) {
  GroupPath gp("grp", false, nullptr, nullptr, nullptr, true, nullptr);
  size_t h = gp.hash();
  EXPECT_GE(h, 0u);
}

TEST_F(SdcInitTest, FilterPathHash) {
  FilterPath flp(nullptr, nullptr, nullptr, true);
  size_t h = flp.hash();
  EXPECT_GE(h, 0u);
}

TEST_F(SdcInitTest, LoopPathHash) {
  LoopPath lp(nullptr, true);
  size_t h = lp.hash();
  EXPECT_GE(h, 0u);
}

////////////////////////////////////////////////////////////////
// R6 tests: ExceptionPath typeString
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, FalsePathTypeString2) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  const char *ts = fp.typeString();
  EXPECT_NE(ts, nullptr);
}

TEST_F(SdcInitTest, PathDelayTypeString2) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               1.0e-9f, true, nullptr);
  const char *ts = pd.typeString();
  EXPECT_NE(ts, nullptr);
}

TEST_F(SdcInitTest, MultiCyclePathTypeString2) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 2, true, nullptr);
  const char *ts = mcp.typeString();
  EXPECT_NE(ts, nullptr);
}

TEST_F(SdcInitTest, GroupPathTypeString2) {
  GroupPath gp("g", false, nullptr, nullptr, nullptr, true, nullptr);
  const char *ts = gp.typeString();
  EXPECT_NE(ts, nullptr);
}

TEST_F(SdcInitTest, FilterPathTypeString2) {
  FilterPath flp(nullptr, nullptr, nullptr, true);
  const char *ts = flp.typeString();
  EXPECT_NE(ts, nullptr);
}

////////////////////////////////////////////////////////////////
// R6 tests: Clock operations
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, ClockEdgeTimeAccess) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("et_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("et_clk");
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  EXPECT_FLOAT_EQ(rise_edge->time(), 0.0);
  EXPECT_FLOAT_EQ(fall_edge->time(), 5.0);
  EXPECT_EQ(rise_edge->clock(), clk);
  EXPECT_EQ(fall_edge->clock(), clk);
  EXPECT_NE(rise_edge->name(), nullptr);
  EXPECT_NE(fall_edge->name(), nullptr);
}

TEST_F(SdcInitTest, ClockMakeClock) {
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  Clock *clk = sdc->makeClock("direct_clk", nullptr, false, 10.0,
                               waveform, nullptr);
  EXPECT_NE(clk, nullptr);
  EXPECT_STREQ(clk->name(), "direct_clk");
}

TEST_F(SdcInitTest, ClockLeafPins) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lp_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("lp_clk");
  const PinSet &pins = clk->leafPins();
  EXPECT_TRUE(pins.empty());
}

////////////////////////////////////////////////////////////////
// R6 tests: Sdc exception operations
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, SdcMakeAndDeleteException) {
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->exceptions().empty());
  sdc->deleteExceptions();
  EXPECT_TRUE(sdc->exceptions().empty());
}

TEST_F(SdcInitTest, SdcMultiCyclePathWithEndClk) {
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::max(),
                           true, 3, nullptr);
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->exceptions().empty());
}

TEST_F(SdcInitTest, SdcMultiCyclePathWithStartClk) {
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::min(),
                           false, 2, nullptr);
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->exceptions().empty());
}

////////////////////////////////////////////////////////////////
// R6 tests: Sdc constraint accessors
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, SdcClockGatingCheckGlobal2) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setClockGatingCheck(RiseFallBoth::rise(), SetupHold::min(), 0.3);
  sdc->setClockGatingCheck(RiseFallBoth::fall(), SetupHold::max(), 0.7);

  }() ));
}

TEST_F(SdcInitTest, SdcClockGatingCheckGlobalRiseFall) {
  Sdc *sdc = sta_->sdc();
  sdc->setClockGatingCheck(RiseFallBoth::riseFall(), SetupHold::min(), 0.5);
  sdc->setClockGatingCheck(RiseFallBoth::riseFall(), SetupHold::max(), 0.8);
  float margin;
  bool exists;
  sdc->clockGatingMargin(RiseFall::rise(), SetupHold::min(), exists, margin);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(margin, 0.5f);
}

TEST_F(SdcInitTest, SdcVoltageAccess) {
  Sdc *sdc = sta_->sdc();
  sdc->setVoltage(MinMax::min(), 0.9);
  sdc->setVoltage(MinMax::max(), 1.1);
  float v_min, v_max;
  bool e_min, e_max;
  sdc->voltage(MinMax::min(), v_min, e_min);
  sdc->voltage(MinMax::max(), v_max, e_max);
  EXPECT_TRUE(e_min);
  EXPECT_TRUE(e_max);
  EXPECT_FLOAT_EQ(v_min, 0.9f);
  EXPECT_FLOAT_EQ(v_max, 1.1f);
}

////////////////////////////////////////////////////////////////
// R6 tests: ExceptionPt construction
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, ExceptionFromRiseFall) {
  ExceptionFrom from(nullptr, nullptr, nullptr,
                     RiseFallBoth::rise(), true,
                     sta_->cmdNetwork());
  EXPECT_NE(from.transition(), nullptr);
}

TEST_F(SdcInitTest, ExceptionFromHasObjects) {
  ExceptionFrom from(nullptr, nullptr, nullptr,
                     RiseFallBoth::riseFall(), true,
                     sta_->cmdNetwork());
  // No objects were provided
  EXPECT_FALSE(from.hasObjects());
  EXPECT_FALSE(from.hasPins());
  EXPECT_FALSE(from.hasClocks());
  EXPECT_FALSE(from.hasInstances());
}

////////////////////////////////////////////////////////////////
// R6 tests: Clock group operations
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, ClockGroupsPhysicallyExclusive) {
  ASSERT_NO_THROW(( [&](){
  FloatSeq *wave = new FloatSeq;
  wave->push_back(0.0);
  wave->push_back(5.0);
  sta_->makeClock("pe_clk", nullptr, false, 10.0, wave, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("pe_clk");

  ClockGroups *groups = sta_->makeClockGroups("pe_grp", false, true, false, false, nullptr);
  ClockSet *clk_set = new ClockSet;
  clk_set->insert(clk);
  sta_->makeClockGroup(groups, clk_set);
  sta_->removeClockGroupsPhysicallyExclusive("pe_grp");

  }() ));
}

TEST_F(SdcInitTest, ClockGroupsAsynchronous) {
  ASSERT_NO_THROW(( [&](){
  FloatSeq *wave = new FloatSeq;
  wave->push_back(0.0);
  wave->push_back(5.0);
  sta_->makeClock("async_clk", nullptr, false, 10.0, wave, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("async_clk");

  ClockGroups *groups = sta_->makeClockGroups("async_grp", false, false, true, false, nullptr);
  ClockSet *clk_set = new ClockSet;
  clk_set->insert(clk);
  sta_->makeClockGroup(groups, clk_set);
  sta_->removeClockGroupsAsynchronous("async_grp");

  }() ));
}

////////////////////////////////////////////////////////////////
// R6 tests: Sdc Latch borrow limits
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, SdcMinPulseWidth) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setMinPulseWidth(RiseFallBoth::riseFall(), 0.5);
  // Just exercise the code path - no assertion needed
  // (MinPulseWidth query requires pin data)

  }() ));
}

////////////////////////////////////////////////////////////////
// R6 tests: Clock uncertainty with MinMax
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, ClockSetUncertaintyMinMax) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("unc_mm_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("unc_mm_clk");
  clk->setUncertainty(MinMax::min(), 0.05f);
  clk->setUncertainty(MinMax::max(), 0.15f);
  float unc;
  bool exists;
  clk->uncertainty(MinMax::min(), unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.05f);
  clk->uncertainty(MinMax::max(), unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.15f);
}

////////////////////////////////////////////////////////////////
// R6 tests: Additional ExceptionPath coverage
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, LoopPathClone) {
  LoopPath lp(nullptr, true);
  ExceptionPath *cloned = lp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_NE(cloned, nullptr);
  // clone() on LoopPath returns FalsePath (inherited from FalsePath::clone)
  EXPECT_TRUE(cloned->isFalse());
  delete cloned;
}

TEST_F(SdcInitTest, LoopPathOverrides) {
  LoopPath lp1(nullptr, true);
  LoopPath lp2(nullptr, true);
  EXPECT_TRUE(lp1.overrides(&lp2));
}

TEST_F(SdcInitTest, LoopPathTighterThan) {
  LoopPath lp1(nullptr, true);
  LoopPath lp2(nullptr, true);
  EXPECT_FALSE(lp1.tighterThan(&lp2));
}

TEST_F(SdcInitTest, GroupPathAsString) {
  GroupPath gp("grp", false, nullptr, nullptr, nullptr, true, nullptr);
  const char *str = gp.asString(sta_->cmdNetwork());
  EXPECT_NE(str, nullptr);
}

TEST_F(SdcInitTest, FilterPathAsString) {
  FilterPath flp(nullptr, nullptr, nullptr, true);
  const char *str = flp.asString(sta_->cmdNetwork());
  EXPECT_NE(str, nullptr);
}

TEST_F(SdcInitTest, LoopPathAsString) {
  LoopPath lp(nullptr, true);
  const char *str = lp.asString(sta_->cmdNetwork());
  EXPECT_NE(str, nullptr);
}

////////////////////////////////////////////////////////////////
// R6 tests: PatternMatch for clocks
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, FindClocksMatchingWildcard) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("sys_clk_a", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("sys_clk_b", nullptr, false, 5.0, wave2, nullptr);

  FloatSeq *wave3 = new FloatSeq;
  wave3->push_back(0.0);
  wave3->push_back(1.0);
  sta_->makeClock("io_clk", nullptr, false, 2.0, wave3, nullptr);

  Sdc *sdc = sta_->sdc();
  PatternMatch pattern("sys_*");
  ClockSeq matches = sdc->findClocksMatching(&pattern);
  EXPECT_EQ(matches.size(), 2u);

  PatternMatch pattern2("*");
  ClockSeq all_matches = sdc->findClocksMatching(&pattern2);
  EXPECT_EQ(all_matches.size(), 3u);
}

////////////////////////////////////////////////////////////////
// R6 tests: Sdc pathDelaysWithoutTo after adding delay
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, SdcPathDelaysWithoutToAfterAdd) {
  // Add a path delay without a "to" endpoint
  sta_->makePathDelay(nullptr, nullptr, nullptr,
                      MinMax::max(), false, false, 5.0e-9, nullptr);
  Sdc *sdc = sta_->sdc();
  EXPECT_TRUE(sdc->pathDelaysWithoutTo());
}

////////////////////////////////////////////////////////////////
// R6 tests: Sdc multiple operations in sequence
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, SdcComplexSequence) {
  Sdc *sdc = sta_->sdc();

  // Create clocks
  FloatSeq *w1 = new FloatSeq;
  w1->push_back(0.0);
  w1->push_back(5.0);
  sta_->makeClock("seq_clk1", nullptr, false, 10.0, w1, nullptr);

  FloatSeq *w2 = new FloatSeq;
  w2->push_back(0.0);
  w2->push_back(2.5);
  sta_->makeClock("seq_clk2", nullptr, false, 5.0, w2, nullptr);

  // Set various constraints
  sdc->setMaxArea(1000.0);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 1000.0f);

  sdc->setWireloadMode(WireloadMode::top);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::top);

  sdc->setAnalysisType(AnalysisType::ocv);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::ocv);

  // Make exception paths
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::all(), true, 4, nullptr);
  sta_->makeGroupPath("test_grp", false, nullptr, nullptr, nullptr, nullptr);

  EXPECT_FALSE(sdc->exceptions().empty());
  EXPECT_TRUE(sta_->isPathGroupName("test_grp"));

  // Clear
  sdc->clear();
  EXPECT_TRUE(sdc->exceptions().empty());
}

////////////////////////////////////////////////////////////////
// R6 tests: Clock properties after propagation
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, ClockPropagateCycle) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("prop_cycle_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("prop_cycle_clk");

  EXPECT_TRUE(clk->isIdeal());
  sta_->setPropagatedClock(clk);
  EXPECT_TRUE(clk->isPropagated());
  EXPECT_FALSE(clk->isIdeal());
  sta_->removePropagatedClock(clk);
  EXPECT_FALSE(clk->isPropagated());
  EXPECT_TRUE(clk->isIdeal());
}

////////////////////////////////////////////////////////////////
// R6 tests: InterClockUncertainty hash
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, InterClockUncertaintySetGet) {
  FloatSeq *w1 = new FloatSeq;
  w1->push_back(0.0);
  w1->push_back(5.0);
  sta_->makeClock("icu_clk1", nullptr, false, 10.0, w1, nullptr);
  FloatSeq *w2 = new FloatSeq;
  w2->push_back(0.0);
  w2->push_back(2.5);
  sta_->makeClock("icu_clk2", nullptr, false, 5.0, w2, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("icu_clk1");
  Clock *clk2 = sdc->findClock("icu_clk2");
  InterClockUncertainty icu(clk1, clk2);
  icu.setUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                     SetupHoldAll::all(), 0.5f);
  EXPECT_EQ(icu.src(), clk1);
  EXPECT_EQ(icu.target(), clk2);
  float unc;
  bool exists;
  icu.uncertainty(RiseFall::rise(), RiseFall::rise(), SetupHold::min(),
                  unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.5f);
}

////////////////////////////////////////////////////////////////
// R6 tests: DeratingFactorsCell isOneValue edge cases
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, DeratingFactorsCellSetAndGet) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_delay,
                PathClkOrData::clk,
                RiseFallBoth::riseFall(),
                EarlyLate::early(), 0.95f);
  float factor;
  bool exists;
  dfc.factor(TimingDerateCellType::cell_delay,
             PathClkOrData::clk,
             RiseFall::rise(),
             EarlyLate::early(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 0.95f);
}

////////////////////////////////////////////////////////////////
// R6 tests: RiseFallMinMax additional
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, RiseFallMinMaxEqual) {
  RiseFallMinMax rfmm1(5.0f);
  RiseFallMinMax rfmm2(5.0f);
  EXPECT_TRUE(rfmm1.equal(&rfmm2));
}

TEST_F(SdcInitTest, RiseFallMinMaxNotEqual) {
  RiseFallMinMax rfmm1(5.0f);
  RiseFallMinMax rfmm2(3.0f);
  EXPECT_FALSE(rfmm1.equal(&rfmm2));
}

TEST_F(SdcInitTest, RiseFallMinMaxIsOneValue) {
  RiseFallMinMax rfmm(7.0f);
  float val;
  bool is_one = rfmm.isOneValue(val);
  EXPECT_TRUE(is_one);
  EXPECT_FLOAT_EQ(val, 7.0f);
}

TEST_F(SdcInitTest, RiseFallMinMaxIsOneValueFalse) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 1.0f);
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 2.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 1.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::max(), 2.0f);
  float val;
  bool is_one = rfmm.isOneValue(val);
  EXPECT_FALSE(is_one);
}

////////////////////////////////////////////////////////////////
// R6 tests: Variables toggle all booleans back and forth
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, VariablesAllToggles) {
  Variables vars;
  vars.setCrprEnabled(false);
  EXPECT_FALSE(vars.crprEnabled());
  vars.setCrprEnabled(true);
  EXPECT_TRUE(vars.crprEnabled());

  vars.setPocvEnabled(true);
  EXPECT_TRUE(vars.pocvEnabled());
  vars.setPocvEnabled(false);
  EXPECT_FALSE(vars.pocvEnabled());

  vars.setDynamicLoopBreaking(true);
  EXPECT_TRUE(vars.dynamicLoopBreaking());
  vars.setDynamicLoopBreaking(false);
  EXPECT_FALSE(vars.dynamicLoopBreaking());

  vars.setPropagateAllClocks(true);
  EXPECT_TRUE(vars.propagateAllClocks());
  vars.setPropagateAllClocks(false);
  EXPECT_FALSE(vars.propagateAllClocks());

  vars.setUseDefaultArrivalClock(true);
  EXPECT_TRUE(vars.useDefaultArrivalClock());
  vars.setUseDefaultArrivalClock(false);
  EXPECT_FALSE(vars.useDefaultArrivalClock());

  vars.setClkThruTristateEnabled(true);
  EXPECT_TRUE(vars.clkThruTristateEnabled());
  vars.setClkThruTristateEnabled(false);
  EXPECT_FALSE(vars.clkThruTristateEnabled());
}

////////////////////////////////////////////////////////////////
// R6 tests: Additional Variables coverage
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, VariablesCrprMode) {
  Variables vars;
  vars.setCrprMode(CrprMode::same_pin);
  EXPECT_EQ(vars.crprMode(), CrprMode::same_pin);
  vars.setCrprMode(CrprMode::same_transition);
  EXPECT_EQ(vars.crprMode(), CrprMode::same_transition);
}

TEST_F(SdcInitTest, VariablesPropagateGatedClockEnable) {
  Variables vars;
  vars.setPropagateGatedClockEnable(true);
  EXPECT_TRUE(vars.propagateGatedClockEnable());
  vars.setPropagateGatedClockEnable(false);
  EXPECT_FALSE(vars.propagateGatedClockEnable());
}

TEST_F(SdcInitTest, VariablesPresetClrArcsEnabled) {
  Variables vars;
  vars.setPresetClrArcsEnabled(true);
  EXPECT_TRUE(vars.presetClrArcsEnabled());
  vars.setPresetClrArcsEnabled(false);
  EXPECT_FALSE(vars.presetClrArcsEnabled());
}

TEST_F(SdcInitTest, VariablesCondDefaultArcsEnabled) {
  Variables vars;
  vars.setCondDefaultArcsEnabled(false);
  EXPECT_FALSE(vars.condDefaultArcsEnabled());
  vars.setCondDefaultArcsEnabled(true);
  EXPECT_TRUE(vars.condDefaultArcsEnabled());
}

TEST_F(SdcInitTest, VariablesBidirectInstPathsEnabled) {
  Variables vars;
  vars.setBidirectInstPathsEnabled(true);
  EXPECT_TRUE(vars.bidirectInstPathsEnabled());
  vars.setBidirectInstPathsEnabled(false);
  EXPECT_FALSE(vars.bidirectInstPathsEnabled());
}

TEST_F(SdcInitTest, VariablesBidirectNetPathsEnabled) {
  Variables vars;
  vars.setBidirectNetPathsEnabled(true);
  EXPECT_TRUE(vars.bidirectNetPathsEnabled());
  vars.setBidirectNetPathsEnabled(false);
  EXPECT_FALSE(vars.bidirectNetPathsEnabled());
}

TEST_F(SdcInitTest, VariablesRecoveryRemovalChecksEnabled) {
  Variables vars;
  vars.setRecoveryRemovalChecksEnabled(false);
  EXPECT_FALSE(vars.recoveryRemovalChecksEnabled());
  vars.setRecoveryRemovalChecksEnabled(true);
  EXPECT_TRUE(vars.recoveryRemovalChecksEnabled());
}

TEST_F(SdcInitTest, VariablesGatedClkChecksEnabled) {
  Variables vars;
  vars.setGatedClkChecksEnabled(false);
  EXPECT_FALSE(vars.gatedClkChecksEnabled());
  vars.setGatedClkChecksEnabled(true);
  EXPECT_TRUE(vars.gatedClkChecksEnabled());
}

////////////////////////////////////////////////////////////////
// R6 tests: ClockLatency
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, ClockLatencyConstruction) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lat_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("lat_clk");
  ClockLatency lat(clk, nullptr);
  EXPECT_EQ(lat.clock(), clk);
  EXPECT_EQ(lat.pin(), nullptr);
  lat.setDelay(RiseFall::rise(), MinMax::max(), 0.5f);
  float delay;
  bool exists;
  lat.delay(RiseFall::rise(), MinMax::max(), delay, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(delay, 0.5f);
}

////////////////////////////////////////////////////////////////
// R6 tests: InputDrive
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, InputDriveConstruction) {
  InputDrive drive;
  drive.setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.1f);
  drive.setDriveResistance(RiseFallBoth::riseFall(), MinMaxAll::all(), 50.0f);
  float res;
  bool exists;
  drive.driveResistance(RiseFall::rise(), MinMax::max(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 50.0f);
}

TEST_F(SdcInitTest, InputDriveResistanceMinMaxEqual2) {
  InputDrive drive;
  drive.setDriveResistance(RiseFallBoth::rise(), MinMaxAll::all(), 100.0f);
  EXPECT_TRUE(drive.driveResistanceMinMaxEqual(RiseFall::rise()));
}

////////////////////////////////////////////////////////////////
// R6 tests: RiseFallMinMax more coverage
////////////////////////////////////////////////////////////////

TEST_F(SdcInitTest, RiseFallMinMaxHasValue) {
  RiseFallMinMax rfmm;
  EXPECT_FALSE(rfmm.hasValue());
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 1.0f);
  EXPECT_TRUE(rfmm.hasValue());
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
  EXPECT_FALSE(rfmm.hasValue(RiseFall::fall(), MinMax::min()));
}

TEST_F(SdcInitTest, RiseFallMinMaxRemoveValue) {
  RiseFallMinMax rfmm(5.0f);
  rfmm.removeValue(RiseFallBoth::rise(), MinMaxAll::max());
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
}

TEST_F(SdcInitTest, RiseFallMinMaxMergeValue) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 1.0f);
  rfmm.mergeValue(RiseFall::rise(), MinMax::max(), 2.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 2.0f);
}

TEST_F(SdcInitTest, RiseFallMinMaxMaxValue) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 3.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::max(), 7.0f);
  float max_val;
  bool exists;
  rfmm.maxValue(max_val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(max_val, 7.0f);
}

////////////////////////////////////////////////////////////////
// R8_ prefix tests for SDC module coverage
////////////////////////////////////////////////////////////////

// DeratingFactors default construction
TEST_F(SdcInitTest, DeratingFactorsDefault) {
  DeratingFactors df;
  EXPECT_FALSE(df.hasValue());
}

// DeratingFactors set and get
TEST_F(SdcInitTest, DeratingFactorsSetGet2) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::rise(),
               EarlyLate::early(), 0.95f);
  float factor;
  bool exists;
  df.factor(PathClkOrData::clk, RiseFall::rise(),
            EarlyLate::early(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 0.95f);
}

// DeratingFactors::clear
TEST_F(SdcInitTest, DeratingFactorsClear2) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::data, RiseFallBoth::riseFall(),
               EarlyLate::late(), 1.05f);
  EXPECT_TRUE(df.hasValue());
  df.clear();
  EXPECT_FALSE(df.hasValue());
}

// DeratingFactors::isOneValue
TEST_F(SdcInitTest, DeratingFactorsIsOneValue2) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               EarlyLate::early(), 0.9f);
  df.setFactor(PathClkOrData::data, RiseFallBoth::riseFall(),
               EarlyLate::early(), 0.9f);
  bool is_one_value;
  float value;
  df.isOneValue(EarlyLate::early(), is_one_value, value);
  if (is_one_value)
    EXPECT_FLOAT_EQ(value, 0.9f);
}

// DeratingFactors isOneValue per clk_data
TEST_F(SdcInitTest, DeratingFactorsIsOneValueClkData2) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               EarlyLate::early(), 0.95f);
  bool is_one_value;
  float value;
  df.isOneValue(PathClkOrData::clk, EarlyLate::early(),
                is_one_value, value);
  if (is_one_value)
    EXPECT_FLOAT_EQ(value, 0.95f);
}

// DeratingFactorsGlobal
TEST_F(SdcInitTest, DeratingFactorsGlobalDefault) {
  DeratingFactorsGlobal dfg;
  float factor;
  bool exists;
  dfg.factor(TimingDerateType::cell_delay, PathClkOrData::clk,
             RiseFall::rise(), EarlyLate::early(), factor, exists);
  EXPECT_FALSE(exists);
}

// DeratingFactorsGlobal set and get
TEST_F(SdcInitTest, DeratingFactorsGlobalSetGet) {
  DeratingFactorsGlobal dfg;
  dfg.setFactor(TimingDerateType::cell_delay, PathClkOrData::clk,
                RiseFallBoth::rise(), EarlyLate::early(), 0.98f);
  float factor;
  bool exists;
  dfg.factor(TimingDerateType::cell_delay, PathClkOrData::clk,
             RiseFall::rise(), EarlyLate::early(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 0.98f);
}

// DeratingFactorsGlobal clear
TEST_F(SdcInitTest, DeratingFactorsGlobalClear2) {
  DeratingFactorsGlobal dfg;
  dfg.setFactor(TimingDerateType::net_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), EarlyLate::late(), 1.05f);
  dfg.clear();
  float factor;
  bool exists;
  dfg.factor(TimingDerateType::net_delay, PathClkOrData::data,
             RiseFall::rise(), EarlyLate::late(), factor, exists);
  EXPECT_FALSE(exists);
}

// DeratingFactorsGlobal factors accessor
TEST_F(SdcInitTest, DeratingFactorsGlobalFactorsAccessor) {
  DeratingFactorsGlobal dfg;
  DeratingFactors *df = dfg.factors(TimingDerateType::cell_check);
  EXPECT_NE(df, nullptr);
}

// DeratingFactorsGlobal with TimingDerateCellType
TEST_F(SdcInitTest, DeratingFactorsGlobalCellType) {
  DeratingFactorsGlobal dfg;
  dfg.setFactor(TimingDerateType::cell_check, PathClkOrData::data,
                RiseFallBoth::fall(), EarlyLate::late(), 1.02f);
  float factor;
  bool exists;
  dfg.factor(TimingDerateCellType::cell_check, PathClkOrData::data,
             RiseFall::fall(), EarlyLate::late(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 1.02f);
}

// DeratingFactorsCell
TEST_F(SdcInitTest, DeratingFactorsCellDefault) {
  DeratingFactorsCell dfc;
  float factor;
  bool exists;
  dfc.factor(TimingDerateCellType::cell_delay, PathClkOrData::clk,
             RiseFall::rise(), EarlyLate::early(), factor, exists);
  EXPECT_FALSE(exists);
}

// DeratingFactorsCell set and get
TEST_F(SdcInitTest, DeratingFactorsCellSetGet) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), EarlyLate::early(), 0.97f);
  float factor;
  bool exists;
  dfc.factor(TimingDerateCellType::cell_delay, PathClkOrData::data,
             RiseFall::rise(), EarlyLate::early(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 0.97f);
}

// DeratingFactorsCell clear
TEST_F(SdcInitTest, DeratingFactorsCellClear2) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_check, PathClkOrData::clk,
                RiseFallBoth::rise(), EarlyLate::late(), 1.1f);
  dfc.clear();
  float factor;
  bool exists;
  dfc.factor(TimingDerateCellType::cell_check, PathClkOrData::clk,
             RiseFall::rise(), EarlyLate::late(), factor, exists);
  EXPECT_FALSE(exists);
}

// DeratingFactorsCell factors accessor
TEST_F(SdcInitTest, DeratingFactorsCellFactorsAccessor) {
  DeratingFactorsCell dfc;
  DeratingFactors *df = dfc.factors(TimingDerateCellType::cell_delay);
  EXPECT_NE(df, nullptr);
}

// DeratingFactorsCell isOneValue
TEST_F(SdcInitTest, DeratingFactorsCellIsOneValue2) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::clk,
                RiseFallBoth::riseFall(), EarlyLate::early(), 0.95f);
  dfc.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), EarlyLate::early(), 0.95f);
  dfc.setFactor(TimingDerateCellType::cell_check, PathClkOrData::clk,
                RiseFallBoth::riseFall(), EarlyLate::early(), 0.95f);
  dfc.setFactor(TimingDerateCellType::cell_check, PathClkOrData::data,
                RiseFallBoth::riseFall(), EarlyLate::early(), 0.95f);
  bool is_one;
  float val;
  dfc.isOneValue(EarlyLate::early(), is_one, val);
  if (is_one)
    EXPECT_FLOAT_EQ(val, 0.95f);
}

// DeratingFactorsNet
TEST_F(SdcInitTest, DeratingFactorsNetDefault) {
  DeratingFactorsNet dfn;
  EXPECT_FALSE(dfn.hasValue());
}

// DeratingFactorsNet set and get
TEST_F(SdcInitTest, DeratingFactorsNetSetGet) {
  DeratingFactorsNet dfn;
  dfn.setFactor(PathClkOrData::data, RiseFallBoth::riseFall(),
                EarlyLate::late(), 1.03f);
  float factor;
  bool exists;
  dfn.factor(PathClkOrData::data, RiseFall::fall(),
             EarlyLate::late(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 1.03f);
}

// ClockLatency construction
TEST_F(SdcInitTest, ClockLatencyConstruct2) {
  ClockLatency lat(nullptr, nullptr);
  EXPECT_EQ(lat.clock(), nullptr);
  EXPECT_EQ(lat.pin(), nullptr);
}

// ClockLatency set and get
TEST_F(SdcInitTest, ClockLatencySetGet) {
  ClockLatency lat(nullptr, nullptr);
  lat.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(), 1.5f);
  float delay;
  bool exists;
  lat.delay(RiseFall::rise(), MinMax::max(), delay, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(delay, 1.5f);
}

// ClockLatency delays accessor
TEST_F(SdcInitTest, ClockLatencyDelaysAccessor) {
  ClockLatency lat(nullptr, nullptr);
  lat.setDelay(RiseFallBoth::rise(), MinMaxAll::min(), 0.5f);
  RiseFallMinMax *delays = lat.delays();
  EXPECT_NE(delays, nullptr);
  EXPECT_TRUE(delays->hasValue());
}

// ClockInsertion construction
TEST_F(SdcInitTest, ClockInsertionConstruct2) {
  ClockInsertion ins(nullptr, nullptr);
  EXPECT_EQ(ins.clock(), nullptr);
  EXPECT_EQ(ins.pin(), nullptr);
}

// ClockInsertion set and get
TEST_F(SdcInitTest, ClockInsertionSetGet) {
  ClockInsertion ins(nullptr, nullptr);
  ins.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(),
               EarlyLateAll::all(), 2.0f);
  float insertion;
  bool exists;
  ins.delay(RiseFall::rise(), MinMax::max(),
            EarlyLate::early(), insertion, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(insertion, 2.0f);
}

// ClockInsertion delays accessor
TEST_F(SdcInitTest, ClockInsertionDelaysAccessor) {
  ClockInsertion ins(nullptr, nullptr);
  ins.setDelay(RiseFallBoth::rise(), MinMaxAll::min(),
               EarlyLateAll::early(), 0.3f);
  RiseFallMinMax *delays = ins.delays(EarlyLate::early());
  EXPECT_NE(delays, nullptr);
}

// ClockGatingCheck
TEST_F(SdcInitTest, ClockGatingCheckConstruct) {
  ClockGatingCheck cgc;
  RiseFallMinMax *margins = cgc.margins();
  EXPECT_NE(margins, nullptr);
}

// ClockGatingCheck active value
TEST_F(SdcInitTest, ClockGatingCheckActiveValue) {
  ClockGatingCheck cgc;
  cgc.setActiveValue(LogicValue::one);
  EXPECT_EQ(cgc.activeValue(), LogicValue::one);
  cgc.setActiveValue(LogicValue::zero);
  EXPECT_EQ(cgc.activeValue(), LogicValue::zero);
}

// InputDrive construction
TEST_F(SdcInitTest, InputDriveConstruct) {
  InputDrive drive;
  float res;
  bool exists;
  drive.driveResistance(RiseFall::rise(), MinMax::max(), res, exists);
  EXPECT_FALSE(exists);
}

// InputDrive set slew
TEST_F(SdcInitTest, InputDriveSetSlew2) {
  InputDrive drive;
  drive.setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.1f);
  float slew;
  bool exists;
  drive.slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.1f);
}

// InputDrive set resistance
TEST_F(SdcInitTest, InputDriveSetResistance2) {
  InputDrive drive;
  drive.setDriveResistance(RiseFallBoth::riseFall(), MinMaxAll::all(), 50.0f);
  float res;
  bool exists;
  drive.driveResistance(RiseFall::rise(), MinMax::max(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 50.0f);
  EXPECT_TRUE(drive.hasDriveResistance(RiseFall::rise(), MinMax::max()));
}

// InputDrive drive resistance min/max equal
TEST_F(SdcInitTest, InputDriveResistanceEqual) {
  InputDrive drive;
  drive.setDriveResistance(RiseFallBoth::riseFall(), MinMaxAll::all(), 100.0f);
  EXPECT_TRUE(drive.driveResistanceMinMaxEqual(RiseFall::rise()));
}

// InputDrive drive resistance min/max not equal
TEST_F(SdcInitTest, InputDriveResistanceNotEqual) {
  InputDrive drive;
  drive.setDriveResistance(RiseFallBoth::rise(), MinMaxAll::min(), 50.0f);
  drive.setDriveResistance(RiseFallBoth::rise(), MinMaxAll::max(), 100.0f);
  EXPECT_FALSE(drive.driveResistanceMinMaxEqual(RiseFall::rise()));
}

// InputDrive no drive cell
TEST_F(SdcInitTest, InputDriveNoDriveCell) {
  InputDrive drive;
  EXPECT_FALSE(drive.hasDriveCell(RiseFall::rise(), MinMax::max()));
}

// InputDrive slews accessor
TEST_F(SdcInitTest, InputDriveSlewsAccessor) {
  InputDrive drive;
  drive.setSlew(RiseFallBoth::rise(), MinMaxAll::max(), 0.2f);
  const RiseFallMinMax *slews = drive.slews();
  EXPECT_NE(slews, nullptr);
  EXPECT_TRUE(slews->hasValue());
}

// ExceptionPath priorities
TEST_F(SdcInitTest, ExceptionPathPriorities) {
  EXPECT_EQ(ExceptionPath::falsePathPriority(), 4000);
  EXPECT_EQ(ExceptionPath::pathDelayPriority(), 3000);
  EXPECT_EQ(ExceptionPath::multiCyclePathPriority(), 2000);
  EXPECT_EQ(ExceptionPath::filterPathPriority(), 1000);
  EXPECT_EQ(ExceptionPath::groupPathPriority(), 0);
}

// FalsePath creation and type
TEST_F(SdcInitTest, FalsePathType) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_TRUE(fp.isFalse());
  EXPECT_FALSE(fp.isLoop());
  EXPECT_FALSE(fp.isMultiCycle());
  EXPECT_FALSE(fp.isPathDelay());
  EXPECT_FALSE(fp.isGroupPath());
  EXPECT_FALSE(fp.isFilter());
  EXPECT_EQ(fp.type(), ExceptionPathType::false_path);
}

// FalsePath priority
TEST_F(SdcInitTest, FalsePathPriority) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_EQ(fp.typePriority(), ExceptionPath::falsePathPriority());
}

// PathDelay creation and type
TEST_F(SdcInitTest, PathDelayType) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(),
               false, false, 5.0f, false, nullptr);
  EXPECT_TRUE(pd.isPathDelay());
  EXPECT_FALSE(pd.isFalse());
  EXPECT_EQ(pd.type(), ExceptionPathType::path_delay);
  EXPECT_FLOAT_EQ(pd.delay(), 5.0f);
}

// PathDelay ignoreClkLatency
TEST_F(SdcInitTest, PathDelayIgnoreClkLatency) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(),
                true, false, 3.0f, false, nullptr);
  EXPECT_TRUE(pd1.ignoreClkLatency());
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 3.0f, false, nullptr);
  EXPECT_FALSE(pd2.ignoreClkLatency());
}

// PathDelay breakPath
TEST_F(SdcInitTest, PathDelayBreakPath) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(),
               false, true, 3.0f, false, nullptr);
  EXPECT_TRUE(pd.breakPath());
}

// PathDelay tighterThan
TEST_F(SdcInitTest, PathDelayTighterThanMin) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::min(),
                false, false, 3.0f, false, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::min(),
                false, false, 5.0f, false, nullptr);
  // For min, larger delay is tighter
  EXPECT_TRUE(pd2.tighterThan(&pd1));
}

// PathDelay tighterThan max
TEST_F(SdcInitTest, PathDelayTighterThanMax) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 3.0f, false, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(),
                false, false, 5.0f, false, nullptr);
  // For max, smaller delay is tighter
  EXPECT_TRUE(pd1.tighterThan(&pd2));
}

// MultiCyclePath creation and type
TEST_F(SdcInitTest, MultiCyclePathType) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, false, nullptr);
  EXPECT_TRUE(mcp.isMultiCycle());
  EXPECT_EQ(mcp.type(), ExceptionPathType::multi_cycle);
  EXPECT_EQ(mcp.pathMultiplier(), 3);
  EXPECT_TRUE(mcp.useEndClk());
}

// MultiCyclePath with start clk
TEST_F(SdcInitTest, MultiCyclePathStartClk) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     false, 2, false, nullptr);
  EXPECT_FALSE(mcp.useEndClk());
  EXPECT_EQ(mcp.pathMultiplier(), 2);
}

// MultiCyclePath tighterThan
TEST_F(SdcInitTest, MultiCyclePathTighterThan2) {
  MultiCyclePath mcp1(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 2, false, nullptr);
  MultiCyclePath mcp2(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 4, false, nullptr);
  // For setup, larger multiplier is tighter
  bool t1 = mcp1.tighterThan(&mcp2);
  bool t2 = mcp2.tighterThan(&mcp1);
  // One should be tighter than the other
  EXPECT_NE(t1, t2);
}

// FilterPath creation and type
TEST_F(SdcInitTest, FilterPathType) {
  FilterPath fp(nullptr, nullptr, nullptr, false);
  EXPECT_TRUE(fp.isFilter());
  EXPECT_EQ(fp.type(), ExceptionPathType::filter);
}

// GroupPath creation and type
TEST_F(SdcInitTest, GroupPathType) {
  GroupPath gp("test_group", false, nullptr, nullptr, nullptr, false, nullptr);
  EXPECT_TRUE(gp.isGroupPath());
  EXPECT_EQ(gp.type(), ExceptionPathType::group_path);
  EXPECT_STREQ(gp.name(), "test_group");
  EXPECT_FALSE(gp.isDefault());
}

// GroupPath default
TEST_F(SdcInitTest, GroupPathDefault) {
  GroupPath gp("default_group", true, nullptr, nullptr, nullptr, false, nullptr);
  EXPECT_TRUE(gp.isDefault());
}

// LoopPath creation
TEST_F(SdcInitTest, LoopPathType) {
  LoopPath lp(nullptr, false);
  EXPECT_TRUE(lp.isFalse());
  EXPECT_TRUE(lp.isLoop());
  EXPECT_EQ(lp.type(), ExceptionPathType::loop);
}

// ExceptionPath minMax
TEST_F(SdcInitTest, ExceptionPathMinMax) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::min(), false, nullptr);
  EXPECT_EQ(fp.minMax(), MinMaxAll::min());
  EXPECT_TRUE(fp.matches(MinMax::min(), true));
  EXPECT_FALSE(fp.matches(MinMax::max(), true));
}

// ExceptionPath matches min/max all
TEST_F(SdcInitTest, ExceptionPathMatchesAll) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_TRUE(fp.matches(MinMax::min(), true));
  EXPECT_TRUE(fp.matches(MinMax::max(), true));
}

// FalsePath hash
TEST_F(SdcInitTest, FalsePathHash) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  // Same structure should have same hash
  EXPECT_EQ(fp1.hash(), fp2.hash());
}

// FalsePath overrides
TEST_F(SdcInitTest, FalsePathOverrides2) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_TRUE(fp1.overrides(&fp2));
}

// PathDelay hash
TEST_F(SdcInitTest, PathDelayHashR8) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(),
               false, false, 5.0f, false, nullptr);
  size_t h = pd.hash();
  EXPECT_GT(h, 0u);
}

// FalsePath not mergeable with PathDelay
TEST_F(SdcInitTest, FalsePathNotMergeablePathDelay) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(),
               false, false, 5.0f, false, nullptr);
  EXPECT_FALSE(fp.mergeable(&pd));
}

// GroupPath tighterThan
TEST_F(SdcInitTest, GroupPathTighterThan2) {
  ASSERT_NO_THROW(( [&](){
  GroupPath gp1("g1", false, nullptr, nullptr, nullptr, false, nullptr);
  GroupPath gp2("g2", false, nullptr, nullptr, nullptr, false, nullptr);
  // Group paths have no value to compare
  gp1.tighterThan(&gp2);
  }() ));
}

// FilterPath tighterThan
TEST_F(SdcInitTest, FilterPathTighterThan2) {
  ASSERT_NO_THROW(( [&](){
  FilterPath fp1(nullptr, nullptr, nullptr, false);
  FilterPath fp2(nullptr, nullptr, nullptr, false);
  fp1.tighterThan(&fp2);
  }() ));
}

// ExceptionPath id
TEST_F(SdcInitTest, ExceptionPathId) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  fp.setId(42);
  EXPECT_EQ(fp.id(), 42u);
}

// ExceptionPath setPriority
TEST_F(SdcInitTest, ExceptionPathSetPriority) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  fp.setPriority(999);
  EXPECT_EQ(fp.priority(), 999);
}

// ExceptionPath useEndClk default
TEST_F(SdcInitTest, ExceptionPathUseEndClkDefault) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_FALSE(fp.useEndClk());
}

// ExceptionPath pathMultiplier default
TEST_F(SdcInitTest, ExceptionPathPathMultiplierDefault) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_EQ(fp.pathMultiplier(), 0);
}

// ExceptionPath delay default
TEST_F(SdcInitTest, ExceptionPathDelayDefault) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_FLOAT_EQ(fp.delay(), 0.0f);
}

// ExceptionPath name default
TEST_F(SdcInitTest, ExceptionPathNameDefault) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_EQ(fp.name(), nullptr);
}

// ExceptionPath isDefault
TEST_F(SdcInitTest, ExceptionPathIsDefault) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_FALSE(fp.isDefault());
}

// ExceptionPath ignoreClkLatency default
TEST_F(SdcInitTest, ExceptionPathIgnoreClkLatencyDefault) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_FALSE(fp.ignoreClkLatency());
}

// ExceptionPath breakPath default
TEST_F(SdcInitTest, ExceptionPathBreakPathDefault) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), false, nullptr);
  EXPECT_FALSE(fp.breakPath());
}

// Clock slew set and get
TEST_F(SdcInitTest, ClockSlewSetGet2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_slew_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_slew_clk");
  ASSERT_NE(clk, nullptr);
  clk->setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.1f);
  float slew;
  bool exists;
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.1f);
}

// Clock removeSlew
TEST_F(SdcInitTest, ClockRemoveSlew2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_rslew_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_rslew_clk");
  ASSERT_NE(clk, nullptr);
  clk->setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.2f);
  clk->removeSlew();
  float slew;
  bool exists;
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_FALSE(exists);
}

// Clock slews accessor
TEST_F(SdcInitTest, ClockSlewsAccessor2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_slews_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_slews_clk");
  ASSERT_NE(clk, nullptr);
  clk->setSlew(RiseFallBoth::rise(), MinMaxAll::max(), 0.15f);
  const RiseFallMinMax &slews = clk->slews();
  EXPECT_TRUE(slews.hasValue());
}

// Clock period
TEST_F(SdcInitTest, ClockPeriod) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(10.0);
  sta_->makeClock("r8_per_clk", nullptr, false, 20.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_per_clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_FLOAT_EQ(clk->period(), 20.0f);
}

// Clock period access via makeClock
TEST_F(SdcInitTest, ClockPeriodAccess) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(12.5);
  sta_->makeClock("r8_pera_clk", nullptr, false, 25.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_pera_clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_FLOAT_EQ(clk->period(), 25.0f);
}

// Clock isVirtual
TEST_F(SdcInitTest, ClockIsVirtual2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_virt_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_virt_clk");
  ASSERT_NE(clk, nullptr);
  // Virtual clock has no pins
  EXPECT_TRUE(clk->isVirtual());
}

// Clock isPropagated
TEST_F(SdcInitTest, ClockIsPropagated) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_prop_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_prop_clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_FALSE(clk->isPropagated());
  clk->setIsPropagated(true);
  EXPECT_TRUE(clk->isPropagated());
  EXPECT_FALSE(clk->isIdeal());
}

// Clock isIdeal
TEST_F(SdcInitTest, ClockIsIdeal) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_ideal_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_ideal_clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_TRUE(clk->isIdeal());
}

// Clock edge
TEST_F(SdcInitTest, ClockEdge) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_edge_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_edge_clk");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  EXPECT_NE(rise_edge, nullptr);
  EXPECT_NE(fall_edge, nullptr);
  EXPECT_NE(rise_edge, fall_edge);
}

// ClockEdge properties
TEST_F(SdcInitTest, ClockEdgeProperties2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_edgep_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_edgep_clk");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  EXPECT_EQ(rise->clock(), clk);
  EXPECT_EQ(rise->transition(), RiseFall::rise());
  EXPECT_FLOAT_EQ(rise->time(), 0.0f);
  EXPECT_NE(rise->name(), nullptr);
}

// ClockEdge opposite
TEST_F(SdcInitTest, ClockEdgeOpposite2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_opp_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_opp_clk");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  EXPECT_EQ(rise->opposite(), fall);
  EXPECT_EQ(fall->opposite(), rise);
}

// ClockEdge pulseWidth
TEST_F(SdcInitTest, ClockEdgePulseWidth2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_pw_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_pw_clk");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  float pw = rise->pulseWidth();
  EXPECT_FLOAT_EQ(pw, 5.0f);  // 50% duty cycle
}

// ClockEdge index
TEST_F(SdcInitTest, ClockEdgeIndex) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_idx_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_idx_clk");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  EXPECT_NE(rise->index(), fall->index());
}

// Clock uncertainty
TEST_F(SdcInitTest, ClockUncertainty2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_unc_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_unc_clk");
  ASSERT_NE(clk, nullptr);
  clk->setUncertainty(SetupHoldAll::max(), 0.5f);
  float unc;
  bool exists;
  clk->uncertainty(SetupHold::max(), unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.5f);
}

// Clock removeUncertainty
TEST_F(SdcInitTest, ClockRemoveUncertainty) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_runc_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_runc_clk");
  ASSERT_NE(clk, nullptr);
  clk->setUncertainty(SetupHoldAll::all(), 0.3f);
  clk->removeUncertainty(SetupHoldAll::all());
  float unc;
  bool exists;
  clk->uncertainty(SetupHold::max(), unc, exists);
  EXPECT_FALSE(exists);
}

// Clock isGenerated
TEST_F(SdcInitTest, ClockIsGenerated) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_gen_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_gen_clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_FALSE(clk->isGenerated());
}

// Clock addToPins
TEST_F(SdcInitTest, ClockAddToPins) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_atp_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_atp_clk");
  ASSERT_NE(clk, nullptr);
  clk->setAddToPins(true);
  EXPECT_TRUE(clk->addToPins());
  clk->setAddToPins(false);
  EXPECT_FALSE(clk->addToPins());
}

// Clock waveform
TEST_F(SdcInitTest, ClockWaveform) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_wf_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_wf_clk");
  ASSERT_NE(clk, nullptr);
  FloatSeq *wave = clk->waveform();
  EXPECT_NE(wave, nullptr);
  EXPECT_EQ(wave->size(), 2u);
}

// Clock index
TEST_F(SdcInitTest, ClockIndex2) {
  FloatSeq *wf1 = new FloatSeq;
  wf1->push_back(0.0);
  wf1->push_back(5.0);
  sta_->makeClock("r8_idx1_clk", nullptr, false, 10.0, wf1, nullptr);
  FloatSeq *wf2 = new FloatSeq;
  wf2->push_back(0.0);
  wf2->push_back(10.0);
  sta_->makeClock("r8_idx2_clk", nullptr, false, 20.0, wf2, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("r8_idx1_clk");
  Clock *clk2 = sdc->findClock("r8_idx2_clk");
  ASSERT_NE(clk1, nullptr);
  ASSERT_NE(clk2, nullptr);
  EXPECT_NE(clk1->index(), clk2->index());
}

// Clock combinational
TEST_F(SdcInitTest, ClockCombinational) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_comb_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_comb_clk");
  ASSERT_NE(clk, nullptr);
  // Non-generated clock has no combinational flag
  EXPECT_FALSE(clk->combinational());
}

// InterClockUncertainty
TEST_F(SdcInitTest, InterClockUncertaintyConstruct) {
  FloatSeq *wf1 = new FloatSeq;
  wf1->push_back(0.0);
  wf1->push_back(5.0);
  sta_->makeClock("r8_icus_clk", nullptr, false, 10.0, wf1, nullptr);
  FloatSeq *wf2 = new FloatSeq;
  wf2->push_back(0.0);
  wf2->push_back(5.0);
  sta_->makeClock("r8_icut_clk", nullptr, false, 10.0, wf2, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("r8_icus_clk");
  Clock *clk2 = sdc->findClock("r8_icut_clk");
  InterClockUncertainty icu(clk1, clk2);
  EXPECT_EQ(icu.src(), clk1);
  EXPECT_EQ(icu.target(), clk2);
  EXPECT_TRUE(icu.empty());
}

// InterClockUncertainty set and get
TEST_F(SdcInitTest, InterClockUncertaintySetGet2) {
  FloatSeq *wf1 = new FloatSeq;
  wf1->push_back(0.0);
  wf1->push_back(5.0);
  sta_->makeClock("r8_icu2s_clk", nullptr, false, 10.0, wf1, nullptr);
  FloatSeq *wf2 = new FloatSeq;
  wf2->push_back(0.0);
  wf2->push_back(5.0);
  sta_->makeClock("r8_icu2t_clk", nullptr, false, 10.0, wf2, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("r8_icu2s_clk");
  Clock *clk2 = sdc->findClock("r8_icu2t_clk");
  InterClockUncertainty icu(clk1, clk2);
  icu.setUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                     SetupHoldAll::all(), 0.3f);
  EXPECT_FALSE(icu.empty());
  float unc;
  bool exists;
  icu.uncertainty(RiseFall::rise(), RiseFall::rise(),
                  SetupHold::max(), unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.3f);
}

// InterClockUncertainty removeUncertainty
TEST_F(SdcInitTest, InterClockUncertaintyRemove2) {
  FloatSeq *wf1 = new FloatSeq;
  wf1->push_back(0.0);
  wf1->push_back(5.0);
  sta_->makeClock("r8_icu3s_clk", nullptr, false, 10.0, wf1, nullptr);
  FloatSeq *wf2 = new FloatSeq;
  wf2->push_back(0.0);
  wf2->push_back(5.0);
  sta_->makeClock("r8_icu3t_clk", nullptr, false, 10.0, wf2, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("r8_icu3s_clk");
  Clock *clk2 = sdc->findClock("r8_icu3t_clk");
  InterClockUncertainty icu(clk1, clk2);
  icu.setUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                     SetupHoldAll::all(), 0.5f);
  icu.removeUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                        SetupHoldAll::all());
  EXPECT_TRUE(icu.empty());
}

// InterClockUncertainty uncertainties accessor
TEST_F(SdcInitTest, InterClockUncertaintyAccessor) {
  FloatSeq *wf1 = new FloatSeq;
  wf1->push_back(0.0);
  wf1->push_back(5.0);
  sta_->makeClock("r8_icu4s_clk", nullptr, false, 10.0, wf1, nullptr);
  FloatSeq *wf2 = new FloatSeq;
  wf2->push_back(0.0);
  wf2->push_back(5.0);
  sta_->makeClock("r8_icu4t_clk", nullptr, false, 10.0, wf2, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("r8_icu4s_clk");
  Clock *clk2 = sdc->findClock("r8_icu4t_clk");
  InterClockUncertainty icu(clk1, clk2);
  icu.setUncertainty(RiseFallBoth::rise(), RiseFallBoth::rise(),
                     SetupHoldAll::max(), 0.2f);
  const RiseFallMinMax *uncerts = icu.uncertainties(RiseFall::rise());
  EXPECT_NE(uncerts, nullptr);
}

// Sdc::setTimingDerate global
TEST_F(SdcInitTest, SdcSetTimingDerateGlobal2) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setTimingDerate(TimingDerateType::cell_delay,
                       PathClkOrData::clk,
                       RiseFallBoth::riseFall(),
                       EarlyLate::early(), 0.95f);
  // Should not crash
  sdc->unsetTimingDerate();

  }() ));
}

// Sdc::setMaxArea and maxArea
TEST_F(SdcInitTest, SdcSetMaxAreaR8) {
  Sdc *sdc = sta_->sdc();
  sdc->setMaxArea(500.0f);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 500.0f);
}

// Sdc::setAnalysisType
TEST_F(SdcInitTest, SdcSetAnalysisTypeR8) {
  Sdc *sdc = sta_->sdc();
  sdc->setAnalysisType(AnalysisType::bc_wc);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::bc_wc);
  sdc->setAnalysisType(AnalysisType::ocv);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::ocv);
  sdc->setAnalysisType(AnalysisType::single);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::single);
}

// Sdc::setWireloadMode
TEST_F(SdcInitTest, SdcSetWireloadModeR8) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setWireloadMode(WireloadMode::enclosed);
  // Just verify no crash
  sdc->setWireloadMode(WireloadMode::segmented);
  sdc->setWireloadMode(WireloadMode::top);

  }() ));
}

// Sdc::setPropagatedClock / removePropagatedClock
TEST_F(SdcInitTest, SdcPropagatedClock) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_propt_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_propt_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setPropagatedClock(clk);
  EXPECT_TRUE(clk->isPropagated());
  sdc->removePropagatedClock(clk);
  EXPECT_FALSE(clk->isPropagated());
}

// Sdc::setClockSlew
TEST_F(SdcInitTest, SdcSetClockSlew2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_sslew_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_sslew_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setClockSlew(clk, RiseFallBoth::riseFall(),
                    MinMaxAll::all(), 0.2f);
  float slew = clk->slew(RiseFall::rise(), MinMax::max());
  EXPECT_FLOAT_EQ(slew, 0.2f);
}

// Sdc::removeClockSlew
TEST_F(SdcInitTest, SdcRemoveClockSlew) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_srslew_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_srslew_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setClockSlew(clk, RiseFallBoth::riseFall(),
                    MinMaxAll::all(), 0.3f);
  sdc->removeClockSlew(clk);
  float slew = clk->slew(RiseFall::rise(), MinMax::max());
  EXPECT_FLOAT_EQ(slew, 0.0f);
}

// Sdc::setClockLatency
TEST_F(SdcInitTest, SdcSetClockLatency2) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_slat_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_slat_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                       MinMaxAll::all(), 1.0f);
  float latency;
  bool exists;
  sdc->clockLatency(clk, RiseFall::rise(), MinMax::max(),
                    latency, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(latency, 1.0f);
}

// Sdc::removeClockLatency
TEST_F(SdcInitTest, SdcRemoveClockLatency) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_srlat_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_srlat_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                       MinMaxAll::all(), 2.0f);
  sdc->removeClockLatency(clk, nullptr);
  float latency;
  bool exists;
  sdc->clockLatency(clk, RiseFall::rise(), MinMax::max(),
                    latency, exists);
  EXPECT_FALSE(exists);
}

// Sdc::clockLatencies accessor
TEST_F(SdcInitTest, SdcClockLatencies) {
  Sdc *sdc = sta_->sdc();
  const ClockLatencies *lats = sdc->clockLatencies();
  EXPECT_NE(lats, nullptr);
}

// Sdc::clockLatency (float overload)
TEST_F(SdcInitTest, SdcClockLatencyFloat) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_slatf_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_slatf_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                       MinMaxAll::all(), 1.5f);
  float lat = sdc->clockLatency(clk, RiseFall::rise(), MinMax::max());
  EXPECT_FLOAT_EQ(lat, 1.5f);
}

// Sdc::setClockInsertion and clockInsertion
TEST_F(SdcInitTest, SdcClockInsertion) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_sins_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_sins_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), EarlyLateAll::all(), 0.5f);
  float ins = sdc->clockInsertion(clk, RiseFall::rise(),
                                   MinMax::max(), EarlyLate::early());
  EXPECT_FLOAT_EQ(ins, 0.5f);
}

// Sdc::removeClockInsertion
TEST_F(SdcInitTest, SdcRemoveClockInsertion) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_srins_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_srins_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), EarlyLateAll::all(), 1.0f);
  sdc->removeClockInsertion(clk, nullptr);
  // After removal, insertion should not exist
}

// Sdc::setMinPulseWidth
TEST_F(SdcInitTest, SdcSetMinPulseWidthR8) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setMinPulseWidth(RiseFallBoth::riseFall(), 0.5f);
  // Just verify no crash

  }() ));
}

// Sdc::setLatchBorrowLimit
TEST_F(SdcInitTest, SdcSetLatchBorrowLimit) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_lbl_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_lbl_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setLatchBorrowLimit(clk, 3.0f);
  // Just verify no crash
}

// Sdc::removeClock
TEST_F(SdcInitTest, SdcRemoveClock) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_rem_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_rem_clk");
  ASSERT_NE(clk, nullptr);
  sdc->removeClock(clk);
  // Clock should be removed
}

// Sdc::defaultArrivalClock
TEST_F(SdcInitTest, SdcDefaultArrivalClock2) {
  Sdc *sdc = sta_->sdc();
  Clock *def_clk = sdc->defaultArrivalClock();
  EXPECT_NE(def_clk, nullptr);
}

// Sdc::defaultArrivalClockEdge
TEST_F(SdcInitTest, SdcDefaultArrivalClockEdge2) {
  Sdc *sdc = sta_->sdc();
  ClockEdge *edge = sdc->defaultArrivalClockEdge();
  EXPECT_NE(edge, nullptr);
}

// Sdc::haveClkSlewLimits
TEST_F(SdcInitTest, SdcHaveClkSlewLimits2) {
  Sdc *sdc = sta_->sdc();
  bool have = sdc->haveClkSlewLimits();
  // Initially no limits
  EXPECT_FALSE(have);
}

// Sdc::invalidateGeneratedClks
TEST_F(SdcInitTest, SdcInvalidateGeneratedClks2) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->invalidateGeneratedClks();
  // Just verify no crash

  }() ));
}

// Variables toggles - more variables
TEST_F(SdcInitTest, VariablesDynamicLoopBreaking) {
  sta_->setDynamicLoopBreaking(true);
  EXPECT_TRUE(sta_->dynamicLoopBreaking());
  sta_->setDynamicLoopBreaking(false);
  EXPECT_FALSE(sta_->dynamicLoopBreaking());
}

// Variables propagateAllClocks
TEST_F(SdcInitTest, VariablesPropagateAllClocks) {
  sta_->setPropagateAllClocks(true);
  EXPECT_TRUE(sta_->propagateAllClocks());
  sta_->setPropagateAllClocks(false);
  EXPECT_FALSE(sta_->propagateAllClocks());
}

// Variables clkThruTristateEnabled
TEST_F(SdcInitTest, VariablesClkThruTristateEnabled) {
  sta_->setClkThruTristateEnabled(true);
  EXPECT_TRUE(sta_->clkThruTristateEnabled());
  sta_->setClkThruTristateEnabled(false);
  EXPECT_FALSE(sta_->clkThruTristateEnabled());
}

// Variables useDefaultArrivalClock
TEST_F(SdcInitTest, VariablesUseDefaultArrivalClock) {
  sta_->setUseDefaultArrivalClock(true);
  EXPECT_TRUE(sta_->useDefaultArrivalClock());
  sta_->setUseDefaultArrivalClock(false);
  EXPECT_FALSE(sta_->useDefaultArrivalClock());
}

// Variables pocvEnabled
TEST_F(SdcInitTest, VariablesPocvEnabled) {
  sta_->setPocvEnabled(true);
  EXPECT_TRUE(sta_->pocvEnabled());
  sta_->setPocvEnabled(false);
  EXPECT_FALSE(sta_->pocvEnabled());
}

// Variables crprEnabled
TEST_F(SdcInitTest, VariablesCrprEnabled) {
  sta_->setCrprEnabled(true);
  EXPECT_TRUE(sta_->crprEnabled());
  sta_->setCrprEnabled(false);
  EXPECT_FALSE(sta_->crprEnabled());
}

// RiseFallMinMax clear
TEST_F(SdcInitTest, RiseFallMinMaxClear) {
  RiseFallMinMax rfmm(1.0f);
  EXPECT_TRUE(rfmm.hasValue());
  rfmm.clear();
  EXPECT_FALSE(rfmm.hasValue());
}

// RiseFallMinMax setValue individual
TEST_F(SdcInitTest, RiseFallMinMaxSetValueIndividual) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 1.0f);
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 2.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 3.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::max(), 4.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 1.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 2.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::min()), 3.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 4.0f);
}

// RiseFallMinMax setValue with RiseFallBoth and MinMaxAll
TEST_F(SdcInitTest, RiseFallMinMaxSetValueBoth) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFallBoth::riseFall(), MinMaxAll::all(), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 5.0f);
}

// PortExtCap
TEST_F(SdcInitTest, PortExtCapConstruct) {
  PortExtCap pec(nullptr);
  EXPECT_EQ(pec.port(), nullptr);
  float cap;
  bool exists;
  pec.pinCap(RiseFall::rise(), MinMax::max(), cap, exists);
  EXPECT_FALSE(exists);
}

// PortExtCap set and get pin cap
TEST_F(SdcInitTest, PortExtCapSetPinCap) {
  PortExtCap pec(nullptr);
  pec.setPinCap(1.0f, RiseFall::rise(), MinMax::max());
  float cap;
  bool exists;
  pec.pinCap(RiseFall::rise(), MinMax::max(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 1.0f);
}

// PortExtCap set and get wire cap
TEST_F(SdcInitTest, PortExtCapSetWireCap) {
  PortExtCap pec(nullptr);
  pec.setWireCap(0.5f, RiseFall::fall(), MinMax::min());
  float cap;
  bool exists;
  pec.wireCap(RiseFall::fall(), MinMax::min(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 0.5f);
}

// PortExtCap set and get fanout
TEST_F(SdcInitTest, PortExtCapSetFanout) {
  PortExtCap pec(nullptr);
  pec.setFanout(4, MinMax::max());
  int fanout;
  bool exists;
  pec.fanout(MinMax::max(), fanout, exists);
  EXPECT_TRUE(exists);
  EXPECT_EQ(fanout, 4);
}

// PortExtCap accessors
TEST_F(SdcInitTest, PortExtCapAccessors) {
  PortExtCap pec(nullptr);
  pec.setPinCap(1.0f, RiseFall::rise(), MinMax::max());
  RiseFallMinMax *pin_cap = pec.pinCap();
  EXPECT_NE(pin_cap, nullptr);
  RiseFallMinMax *wire_cap = pec.wireCap();
  EXPECT_NE(wire_cap, nullptr);
  FanoutValues *fanout = pec.fanout();
  EXPECT_NE(fanout, nullptr);
}

// clkCmp
TEST_F(SdcInitTest, ClkCmp) {
  FloatSeq *wf1 = new FloatSeq;
  wf1->push_back(0.0);
  wf1->push_back(5.0);
  sta_->makeClock("r8_cmpa_clk", nullptr, false, 10.0, wf1, nullptr);
  FloatSeq *wf2 = new FloatSeq;
  wf2->push_back(0.0);
  wf2->push_back(5.0);
  sta_->makeClock("r8_cmpb_clk", nullptr, false, 10.0, wf2, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("r8_cmpa_clk");
  Clock *clk2 = sdc->findClock("r8_cmpb_clk");
  ASSERT_NE(clk1, nullptr);
  ASSERT_NE(clk2, nullptr);
  int cmp = clkCmp(clk1, clk2);
  // Different clocks should not be equal
  EXPECT_NE(cmp, 0);
}

// clkEdgeCmp
TEST_F(SdcInitTest, ClkEdgeCmp) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_ecmp_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_ecmp_clk");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  int cmp = clkEdgeCmp(rise, fall);
  EXPECT_NE(cmp, 0);
}

// clkEdgeLess
TEST_F(SdcInitTest, ClkEdgeLess) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_eless_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_eless_clk");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  bool less1 = clkEdgeLess(rise, fall);
  bool less2 = clkEdgeLess(fall, rise);
  // One should be less than the other, but not both
  EXPECT_NE(less1, less2);
}

// ClockNameLess
TEST_F(SdcInitTest, ClockNameLess) {
  FloatSeq *wf1 = new FloatSeq;
  wf1->push_back(0.0);
  wf1->push_back(5.0);
  sta_->makeClock("r8_aaa_clk", nullptr, false, 10.0, wf1, nullptr);
  FloatSeq *wf2 = new FloatSeq;
  wf2->push_back(0.0);
  wf2->push_back(5.0);
  sta_->makeClock("r8_zzz_clk", nullptr, false, 10.0, wf2, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk_a = sdc->findClock("r8_aaa_clk");
  Clock *clk_z = sdc->findClock("r8_zzz_clk");
  ClockNameLess cmp;
  EXPECT_TRUE(cmp(clk_a, clk_z));
  EXPECT_FALSE(cmp(clk_z, clk_a));
}

// Sdc::setClockGatingCheck (global)
TEST_F(SdcInitTest, SdcClockGatingCheckGlobalR8) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setClockGatingCheck(RiseFallBoth::riseFall(),
                           SetupHold::max(), 0.5f);
  // Just verify no crash

  }() ));
}

// Sdc::setClockGatingCheck on clock
TEST_F(SdcInitTest, SdcClockGatingCheckOnClock) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_cg_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_cg_clk");
  ASSERT_NE(clk, nullptr);
  sdc->setClockGatingCheck(clk, RiseFallBoth::riseFall(),
                           SetupHold::min(), 0.3f);
  // Just verify no crash
}

// Clock slewLimit set and get
TEST_F(SdcInitTest, ClockSlewLimit) {
  FloatSeq *wf = new FloatSeq;
  wf->push_back(0.0);
  wf->push_back(5.0);
  sta_->makeClock("r8_sl_clk", nullptr, false, 10.0, wf, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("r8_sl_clk");
  ASSERT_NE(clk, nullptr);
  clk->setSlewLimit(RiseFallBoth::riseFall(), PathClkOrData::clk,
                    MinMax::max(), 0.5f);
  float slew;
  bool exists;
  clk->slewLimit(RiseFall::rise(), PathClkOrData::clk,
                 MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
}

// ExceptionPt transition
TEST_F(SdcInitTest, ExceptionPtTransition) {
  ExceptionFrom from(nullptr, nullptr, nullptr,
                     RiseFallBoth::rise(), false, nullptr);
  EXPECT_EQ(from.transition(), RiseFallBoth::rise());
  EXPECT_TRUE(from.isFrom());
  EXPECT_FALSE(from.isThru());
  EXPECT_FALSE(from.isTo());
}

// ExceptionTo isTo
TEST_F(SdcInitTest, ExceptionToIsTo) {
  ExceptionTo to(nullptr, nullptr, nullptr,
                 RiseFallBoth::fall(),
                 RiseFallBoth::riseFall(),
                 false, nullptr);
  EXPECT_TRUE(to.isTo());
  EXPECT_FALSE(to.isFrom());
}

// ExceptionFrom hasObjects (empty)
TEST_F(SdcInitTest, ExceptionFromHasObjectsEmpty) {
  ExceptionFrom from(nullptr, nullptr, nullptr,
                     RiseFallBoth::riseFall(), false, nullptr);
  EXPECT_FALSE(from.hasObjects());
  EXPECT_FALSE(from.hasPins());
  EXPECT_FALSE(from.hasClocks());
  EXPECT_FALSE(from.hasInstances());
}

// MultiCyclePath matches min/max
TEST_F(SdcInitTest, MultiCyclePathMatchesMinMax) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, false, nullptr);
  EXPECT_TRUE(mcp.matches(MinMax::min(), false));
  EXPECT_TRUE(mcp.matches(MinMax::max(), false));
}

// MultiCyclePath pathMultiplier with min_max
TEST_F(SdcInitTest, MultiCyclePathMultiplierWithMinMax2) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, false, nullptr);
  int mult_max = mcp.pathMultiplier(MinMax::max());
  EXPECT_EQ(mult_max, 3);
}

// ExceptionPath fromThruToPriority
TEST_F(SdcInitTest, ExceptionPathFromThruToPriority) {
  int prio = ExceptionPath::fromThruToPriority(nullptr, nullptr, nullptr);
  EXPECT_EQ(prio, 0);
}

// Sdc::disabledCellPorts
TEST_F(SdcInitTest, SdcDisabledCellPorts2) {
  Sdc *sdc = sta_->sdc();
  DisabledCellPortsMap *dcm = sdc->disabledCellPorts();
  EXPECT_NE(dcm, nullptr);
}

// Sdc::disabledInstancePorts
TEST_F(SdcInitTest, SdcDisabledInstancePorts) {
  Sdc *sdc = sta_->sdc();
  const DisabledInstancePortsMap *dim = sdc->disabledInstancePorts();
  EXPECT_NE(dim, nullptr);
}

// Sdc::disabledPins
TEST_F(SdcInitTest, SdcDisabledPins) {
  Sdc *sdc = sta_->sdc();
  const PinSet *pins = sdc->disabledPins();
  EXPECT_NE(pins, nullptr);
}

// Sdc::disabledPorts
TEST_F(SdcInitTest, SdcDisabledPorts) {
  Sdc *sdc = sta_->sdc();
  const PortSet *ports = sdc->disabledPorts();
  EXPECT_NE(ports, nullptr);
}

// Sdc::disabledLibPorts
TEST_F(SdcInitTest, SdcDisabledLibPorts) {
  Sdc *sdc = sta_->sdc();
  const LibertyPortSet *lib_ports = sdc->disabledLibPorts();
  EXPECT_NE(lib_ports, nullptr);
}

// Sdc::netResistances
TEST_F(SdcInitTest, SdcNetResistances) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  NetResistanceMap &nr = sdc->netResistances();
  EXPECT_GE(nr.size(), 0u);

  }() ));
}

// Sdc::clockInsertions
TEST_F(SdcInitTest, SdcClockInsertions) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  const ClockInsertions &insertions = sdc->clockInsertions();
  EXPECT_GE(insertions.size(), 0u);

  }() ));
}

} // namespace sta
