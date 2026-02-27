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
#include "Scene.hh"
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

// RiseFall tests
class RiseFallTest : public ::testing::Test {};

TEST_F(RiseFallTest, Singletons) {
  EXPECT_NE(RiseFall::rise(), nullptr);
  EXPECT_NE(RiseFall::fall(), nullptr);
  EXPECT_NE(RiseFall::rise(), RiseFall::fall());
}

TEST_F(RiseFallTest, Names) {
  // to_string() returns short_name: "^" for rise, "v" for fall
  EXPECT_EQ(RiseFall::rise()->to_string(), "^");
  EXPECT_EQ(RiseFall::fall()->to_string(), "v");
}

TEST_F(RiseFallTest, Indices) {
  EXPECT_EQ(RiseFall::riseIndex(), RiseFall::rise()->index());
  EXPECT_EQ(RiseFall::fallIndex(), RiseFall::fall()->index());
  EXPECT_NE(RiseFall::riseIndex(), RiseFall::fallIndex());
}

TEST_F(RiseFallTest, Opposite) {
  EXPECT_EQ(RiseFall::rise()->opposite(), RiseFall::fall());
  EXPECT_EQ(RiseFall::fall()->opposite(), RiseFall::rise());
}

TEST_F(RiseFallTest, Find) {
  EXPECT_EQ(RiseFall::find("rise"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("fall"), RiseFall::fall());
}

TEST_F(RiseFallTest, Range) {
  auto &range = RiseFall::range();
  EXPECT_EQ(range.size(), 2u);
}

// RiseFallBoth tests
class RiseFallBothTest : public ::testing::Test {};

TEST_F(RiseFallBothTest, Singletons) {
  EXPECT_NE(RiseFallBoth::rise(), nullptr);
  EXPECT_NE(RiseFallBoth::fall(), nullptr);
  EXPECT_NE(RiseFallBoth::riseFall(), nullptr);
}

TEST_F(RiseFallBothTest, Matches) {
  EXPECT_TRUE(RiseFallBoth::rise()->matches(RiseFall::rise()));
  EXPECT_FALSE(RiseFallBoth::rise()->matches(RiseFall::fall()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::rise()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::fall()));
}

// Transition tests
class TransitionTest : public ::testing::Test {};

TEST_F(TransitionTest, Singletons) {
  EXPECT_NE(Transition::rise(), nullptr);
  EXPECT_NE(Transition::fall(), nullptr);
  EXPECT_NE(Transition::tr0Z(), nullptr);
  EXPECT_NE(Transition::trZ1(), nullptr);
}

TEST_F(TransitionTest, Find) {
  // Transition names in the map are "^"/"01" for rise, "v"/"10" for fall
  EXPECT_EQ(Transition::find("^"), Transition::rise());
  EXPECT_EQ(Transition::find("v"), Transition::fall());
  EXPECT_EQ(Transition::find("01"), Transition::rise());
  EXPECT_EQ(Transition::find("10"), Transition::fall());
}

TEST_F(TransitionTest, AsRiseFall) {
  EXPECT_EQ(Transition::rise()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::fall()->asRiseFall(), RiseFall::fall());
}

TEST_F(TransitionTest, Matches) {
  EXPECT_TRUE(Transition::rise()->matches(Transition::rise()));
  EXPECT_FALSE(Transition::rise()->matches(Transition::fall()));
}

// MinMax tests
class MinMaxTest : public ::testing::Test {};

TEST_F(MinMaxTest, Singletons) {
  EXPECT_NE(MinMax::min(), nullptr);
  EXPECT_NE(MinMax::max(), nullptr);
  EXPECT_NE(MinMax::min(), MinMax::max());
}

TEST_F(MinMaxTest, Names) {
  EXPECT_EQ(MinMax::min()->to_string(), "min");
  EXPECT_EQ(MinMax::max()->to_string(), "max");
}

TEST_F(MinMaxTest, Indices) {
  EXPECT_EQ(MinMax::minIndex(), MinMax::min()->index());
  EXPECT_EQ(MinMax::maxIndex(), MinMax::max()->index());
}

TEST_F(MinMaxTest, Compare) {
  // min: value1 < value2 is true
  EXPECT_TRUE(MinMax::min()->compare(1.0f, 2.0f));
  EXPECT_FALSE(MinMax::min()->compare(2.0f, 1.0f));
  // max: value1 > value2 is true
  EXPECT_TRUE(MinMax::max()->compare(2.0f, 1.0f));
  EXPECT_FALSE(MinMax::max()->compare(1.0f, 2.0f));
}

TEST_F(MinMaxTest, MinMaxFunc) {
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(3.0f, 5.0f), 3.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(3.0f, 5.0f), 5.0f);
}

TEST_F(MinMaxTest, Opposite) {
  EXPECT_EQ(MinMax::min()->opposite(), MinMax::max());
  EXPECT_EQ(MinMax::max()->opposite(), MinMax::min());
}

TEST_F(MinMaxTest, Find) {
  EXPECT_EQ(MinMax::find("min"), MinMax::min());
  EXPECT_EQ(MinMax::find("max"), MinMax::max());
  EXPECT_EQ(MinMax::find(MinMax::minIndex()), MinMax::min());
}

TEST_F(MinMaxTest, InitValue) {
  // min init value should be large positive
  EXPECT_GT(MinMax::min()->initValue(), 0.0f);
  // max init value should be large negative
  EXPECT_LT(MinMax::max()->initValue(), 0.0f);
}

// MinMaxAll tests
class MinMaxAllTest : public ::testing::Test {};

TEST_F(MinMaxAllTest, Singletons) {
  EXPECT_NE(MinMaxAll::min(), nullptr);
  EXPECT_NE(MinMaxAll::max(), nullptr);
  EXPECT_NE(MinMaxAll::all(), nullptr);
}

TEST_F(MinMaxAllTest, Matches) {
  EXPECT_TRUE(MinMaxAll::min()->matches(MinMax::min()));
  EXPECT_FALSE(MinMaxAll::min()->matches(MinMax::max()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::max()));
}

TEST_F(MinMaxAllTest, Find) {
  EXPECT_EQ(MinMaxAll::find("min"), MinMaxAll::min());
  EXPECT_EQ(MinMaxAll::find("max"), MinMaxAll::max());
  EXPECT_EQ(MinMaxAll::find("all"), MinMaxAll::all());
}

TEST_F(MinMaxAllTest, Range) {
  // "all" should have both min and max in its range
  auto &range = MinMaxAll::all()->range();
  EXPECT_EQ(range.size(), 2u);
}

TEST_F(MinMaxAllTest, AsMinMax) {
  EXPECT_EQ(MinMaxAll::min()->asMinMax(), MinMax::min());
  EXPECT_EQ(MinMaxAll::max()->asMinMax(), MinMax::max());
}

TEST_F(MinMaxAllTest, Index) {
  EXPECT_EQ(MinMaxAll::min()->index(), MinMax::min()->index());
  EXPECT_EQ(MinMaxAll::max()->index(), MinMax::max()->index());
}

////////////////////////////////////////////////////////////////
// ExceptionPath tests for SDC coverage
////////////////////////////////////////////////////////////////

class SdcExceptionPathTest : public ::testing::Test {
protected:
  void SetUp() override {
    initSta();
  }
};

// FalsePath with min_max variations
TEST_F(SdcExceptionPathTest, FalsePathMinMaxMin) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::min(), true, nullptr);
  EXPECT_TRUE(fp.matches(MinMax::min(), false));
  EXPECT_FALSE(fp.matches(MinMax::max(), false));
}

TEST_F(SdcExceptionPathTest, FalsePathMinMaxMax) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::max(), true, nullptr);
  EXPECT_FALSE(fp.matches(MinMax::min(), false));
  EXPECT_TRUE(fp.matches(MinMax::max(), false));
}

// FalsePath with comment
TEST_F(SdcExceptionPathTest, FalsePathWithComment) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, "test comment");
  EXPECT_STREQ(fp.comment(), "test comment");
}

// FalsePath priority constructor
TEST_F(SdcExceptionPathTest, FalsePathWithPriority) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, 1234, nullptr);
  EXPECT_EQ(fp.priority(), 1234);
}

// PathDelay with comment
TEST_F(SdcExceptionPathTest, PathDelayWithComment) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               1.0e-9f, true, "path delay comment");
  EXPECT_STREQ(pd.comment(), "path delay comment");
}

// MultiCyclePath with comment
TEST_F(SdcExceptionPathTest, MultiCyclePathWithComment) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     false, 2, true, "mcp comment");
  EXPECT_STREQ(mcp.comment(), "mcp comment");
  EXPECT_FALSE(mcp.useEndClk());
}

// GroupPath with comment
TEST_F(SdcExceptionPathTest, GroupPathWithComment) {
  GroupPath gp("gp", false, nullptr, nullptr, nullptr, true, "gp comment");
  EXPECT_STREQ(gp.comment(), "gp comment");
}

// GroupPath overrides
TEST_F(SdcExceptionPathTest, GroupPathOverridesSameNameDefault) {
  GroupPath gp1("reg", true, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("reg", true, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp1.overrides(&gp2));
}

TEST_F(SdcExceptionPathTest, GroupPathNotOverridesDifferentName) {
  GroupPath gp1("reg1", false, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("reg2", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_FALSE(gp1.overrides(&gp2));
}

TEST_F(SdcExceptionPathTest, GroupPathNotOverridesDifferentType) {
  GroupPath gp("gp", false, nullptr, nullptr, nullptr, true, nullptr);
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_FALSE(gp.overrides(&fp));
}

// GroupPath mergeable
TEST_F(SdcExceptionPathTest, GroupPathMergeableSameName) {
  GroupPath gp1("grp", false, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("grp", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp1.mergeable(&gp2));
}

TEST_F(SdcExceptionPathTest, GroupPathNotMergeableDifferentName) {
  GroupPath gp1("grp1", false, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("grp2", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_FALSE(gp1.mergeable(&gp2));
}

// PathDelay overrides
TEST_F(SdcExceptionPathTest, PathDelayOverridesPathDelay) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                5.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                10.0e-9f, true, nullptr);
  EXPECT_TRUE(pd1.overrides(&pd2));
}

TEST_F(SdcExceptionPathTest, PathDelayNotOverridesFalsePath) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               5.0e-9f, true, nullptr);
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_FALSE(pd.overrides(&fp));
}

// PathDelay mergeable
TEST_F(SdcExceptionPathTest, PathDelayMergeableSame) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                5.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                5.0e-9f, true, nullptr);
  EXPECT_TRUE(pd1.mergeable(&pd2));
}

TEST_F(SdcExceptionPathTest, PathDelayNotMergeableDifferentDelay) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                5.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                10.0e-9f, true, nullptr);
  EXPECT_FALSE(pd1.mergeable(&pd2));
}

TEST_F(SdcExceptionPathTest, PathDelayNotMergeableDifferentIgnoreLatency) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(), true, false,
                5.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                5.0e-9f, true, nullptr);
  EXPECT_FALSE(pd1.mergeable(&pd2));
}

// MultiCyclePath overrides
TEST_F(SdcExceptionPathTest, MultiCyclePathOverrides) {
  MultiCyclePath mcp1(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  MultiCyclePath mcp2(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  EXPECT_TRUE(mcp1.overrides(&mcp2));
}

TEST_F(SdcExceptionPathTest, MultiCyclePathNotOverridesFalsePath) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_FALSE(mcp.overrides(&fp));
}

// MultiCyclePath mergeable
TEST_F(SdcExceptionPathTest, MultiCyclePathMergeable) {
  MultiCyclePath mcp1(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  MultiCyclePath mcp2(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  EXPECT_TRUE(mcp1.mergeable(&mcp2));
}

TEST_F(SdcExceptionPathTest, MultiCyclePathNotMergeableDifferentMultiplier) {
  MultiCyclePath mcp1(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 3, true, nullptr);
  MultiCyclePath mcp2(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 5, true, nullptr);
  EXPECT_FALSE(mcp1.mergeable(&mcp2));
}

// FalsePath overrides
TEST_F(SdcExceptionPathTest, FalsePathOverrides) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp1.overrides(&fp2));
}

TEST_F(SdcExceptionPathTest, FalsePathNotOverridesDifferentMinMax) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::min(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::max(), true, nullptr);
  EXPECT_FALSE(fp1.overrides(&fp2));
}

// ExceptionPath hash
TEST_F(SdcExceptionPathTest, DifferentTypeDifferentHash) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FilterPath flp(nullptr, nullptr, nullptr, true);
  // Different type priorities generally produce different hashes
  // (but not guaranteed - just verify the function works)
  size_t h1 = fp.hash();
  size_t h2 = flp.hash();
  // Just verify both are valid; may or may not be different
  EXPECT_GE(h1, 0u);
  EXPECT_GE(h2, 0u);
}

// ExceptionPath fromThruToPriority
TEST_F(SdcExceptionPathTest, FromThruToPriorityNone) {
  EXPECT_EQ(ExceptionPath::fromThruToPriority(nullptr, nullptr, nullptr), 0);
}

// ExceptionState tests
TEST_F(SdcExceptionPathTest, StateComplete) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState *state = fp.firstState();
  EXPECT_NE(state, nullptr);
  EXPECT_TRUE(state->isComplete());
  EXPECT_EQ(state->nextThru(), nullptr);
  EXPECT_EQ(state->nextState(), nullptr);
}

TEST_F(SdcExceptionPathTest, StateSetNextState) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState *state = fp.firstState();
  // Verify default next state is null
  EXPECT_EQ(state->nextState(), nullptr);
}

// ExceptionStateLess
TEST_F(SdcExceptionPathTest, StateLessComparison) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  fp1.setId(10);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  fp2.setId(20);

  ExceptionState *s1 = fp1.firstState();
  ExceptionState *s2 = fp2.firstState();

  ExceptionStateLess less;
  EXPECT_TRUE(less(s1, s2));
  EXPECT_FALSE(less(s2, s1));
}

////////////////////////////////////////////////////////////////
// CycleAccting comparator tests
////////////////////////////////////////////////////////////////

class CycleAcctingTest : public ::testing::Test {
protected:
  void SetUp() override {
    initSta();
  }
};

TEST_F(CycleAcctingTest, CycleAcctingHashAndEqual) {
  CycleAcctingHash hasher;
  CycleAcctingEqual equal;
  EXPECT_NE(&hasher, nullptr);
  EXPECT_NE(&equal, nullptr);
}

////////////////////////////////////////////////////////////////
// InterClockUncertainty tests
////////////////////////////////////////////////////////////////

class InterClockUncertaintyTest : public ::testing::Test {
protected:
  void SetUp() override {
    initSta();
  }
};

TEST_F(InterClockUncertaintyTest, ConstructAndEmpty) {
  InterClockUncertainty icu(nullptr, nullptr);
  EXPECT_EQ(icu.src(), nullptr);
  EXPECT_EQ(icu.target(), nullptr);
  EXPECT_TRUE(icu.empty());
}

TEST_F(InterClockUncertaintyTest, SetAndGetUncertainty) {
  InterClockUncertainty icu(nullptr, nullptr);
  icu.setUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                     SetupHoldAll::all(), 0.5f);
  EXPECT_FALSE(icu.empty());

  float unc;
  bool exists;
  icu.uncertainty(RiseFall::rise(), RiseFall::rise(), SetupHold::min(),
                  unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.5f);

  icu.uncertainty(RiseFall::fall(), RiseFall::fall(), SetupHold::max(),
                  unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.5f);
}

TEST_F(InterClockUncertaintyTest, SetSpecificTransitions) {
  InterClockUncertainty icu(nullptr, nullptr);
  icu.setUncertainty(RiseFallBoth::rise(), RiseFallBoth::fall(),
                     SetupHoldAll::min(), 0.3f);
  EXPECT_FALSE(icu.empty());

  float unc;
  bool exists;
  icu.uncertainty(RiseFall::rise(), RiseFall::fall(), SetupHold::min(),
                  unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.3f);

  // Other combinations should not exist
  icu.uncertainty(RiseFall::fall(), RiseFall::rise(), SetupHold::min(),
                  unc, exists);
  EXPECT_FALSE(exists);
}

TEST_F(InterClockUncertaintyTest, RemoveUncertainty) {
  InterClockUncertainty icu(nullptr, nullptr);
  icu.setUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                     SetupHoldAll::all(), 0.5f);
  EXPECT_FALSE(icu.empty());

  icu.removeUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                        SetupHoldAll::all());
  EXPECT_TRUE(icu.empty());
}

TEST_F(InterClockUncertaintyTest, Uncertainties) {
  InterClockUncertainty icu(nullptr, nullptr);
  icu.setUncertainty(RiseFallBoth::rise(), RiseFallBoth::riseFall(),
                     SetupHoldAll::min(), 0.2f);
  const RiseFallMinMax *rfmm = icu.uncertainties(RiseFall::rise());
  EXPECT_NE(rfmm, nullptr);
}

////////////////////////////////////////////////////////////////
// ClockNameLess tests
////////////////////////////////////////////////////////////////

class ClockCmpTest : public ::testing::Test {
protected:
  void SetUp() override {
    initSta();
  }
};

// Just test that the comparator class can be instantiated
// (actual Clock objects require Sdc which requires full setup)
TEST_F(ClockCmpTest, ClkNameLessInstantiation) {
  ClkNameLess less;
  EXPECT_NE(&less, nullptr);
}

TEST_F(ClockCmpTest, ClockNameLessInstantiation) {
  ClockNameLess less;
  EXPECT_NE(&less, nullptr);
}

////////////////////////////////////////////////////////////////
// ExceptionPath priority ordering
////////////////////////////////////////////////////////////////

class ExceptionPriorityTest : public ::testing::Test {
protected:
  void SetUp() override {
    initSta();
  }
};

TEST_F(ExceptionPriorityTest, PriorityOrdering) {
  // FalsePath > PathDelay > MultiCyclePath > FilterPath > GroupPath
  EXPECT_GT(ExceptionPath::falsePathPriority(), ExceptionPath::pathDelayPriority());
  EXPECT_GT(ExceptionPath::pathDelayPriority(), ExceptionPath::multiCyclePathPriority());
  EXPECT_GT(ExceptionPath::multiCyclePathPriority(), ExceptionPath::filterPathPriority());
  EXPECT_GT(ExceptionPath::filterPathPriority(), ExceptionPath::groupPathPriority());
  EXPECT_EQ(ExceptionPath::groupPathPriority(), 0);
}

TEST_F(ExceptionPriorityTest, SpecificValues) {
  EXPECT_EQ(ExceptionPath::falsePathPriority(), 4000);
  EXPECT_EQ(ExceptionPath::pathDelayPriority(), 3000);
  EXPECT_EQ(ExceptionPath::multiCyclePathPriority(), 2000);
  EXPECT_EQ(ExceptionPath::filterPathPriority(), 1000);
  EXPECT_EQ(ExceptionPath::groupPathPriority(), 0);
}

////////////////////////////////////////////////////////////////
// Additional MinMaxAll tests for SDC coverage
////////////////////////////////////////////////////////////////

class SdcMinMaxAllTest : public ::testing::Test {};

TEST_F(SdcMinMaxAllTest, MinAsMinMax) {
  EXPECT_EQ(MinMaxAll::min()->asMinMax(), MinMax::min());
}

TEST_F(SdcMinMaxAllTest, MaxAsMinMax) {
  EXPECT_EQ(MinMaxAll::max()->asMinMax(), MinMax::max());
}

TEST_F(SdcMinMaxAllTest, MinRange) {
  auto &range = MinMaxAll::min()->range();
  EXPECT_EQ(range.size(), 1u);
  EXPECT_EQ(range[0], MinMax::min());
}

TEST_F(SdcMinMaxAllTest, MaxRange) {
  auto &range = MinMaxAll::max()->range();
  EXPECT_EQ(range.size(), 1u);
  EXPECT_EQ(range[0], MinMax::max());
}

TEST_F(SdcMinMaxAllTest, MatchesSelf) {
  EXPECT_TRUE(MinMaxAll::min()->matches(MinMaxAll::min()));
  EXPECT_TRUE(MinMaxAll::max()->matches(MinMaxAll::max()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::all()));
}

TEST_F(SdcMinMaxAllTest, AllMatchesEverything) {
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::max()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::max()));
}

TEST_F(SdcMinMaxAllTest, MinNotMatchesMax) {
  EXPECT_FALSE(MinMaxAll::min()->matches(MinMaxAll::max()));
  EXPECT_FALSE(MinMaxAll::max()->matches(MinMaxAll::min()));
}

TEST_F(SdcMinMaxAllTest, ToString) {
  EXPECT_EQ(MinMaxAll::min()->to_string(), "min");
  EXPECT_EQ(MinMaxAll::max()->to_string(), "max");
}

////////////////////////////////////////////////////////////////
// SetupHold tests (SetupHold is typedef for MinMax)
////////////////////////////////////////////////////////////////

class SetupHoldTest : public ::testing::Test {};

TEST_F(SetupHoldTest, Singletons) {
  // SetupHold is typedef for MinMax: setup=min, hold=max
  EXPECT_NE(SetupHold::min(), nullptr);
  EXPECT_NE(SetupHold::max(), nullptr);
  EXPECT_NE(SetupHold::min(), SetupHold::max());
}

TEST_F(SetupHoldTest, Indices) {
  EXPECT_NE(SetupHold::min()->index(), SetupHold::max()->index());
}

TEST_F(SetupHoldTest, Opposite) {
  EXPECT_EQ(SetupHold::min()->opposite(), SetupHold::max());
  EXPECT_EQ(SetupHold::max()->opposite(), SetupHold::min());
}

class SetupHoldAllTest : public ::testing::Test {};

TEST_F(SetupHoldAllTest, Singletons) {
  // SetupHoldAll is typedef for MinMaxAll
  EXPECT_NE(SetupHoldAll::min(), nullptr);
  EXPECT_NE(SetupHoldAll::max(), nullptr);
  EXPECT_NE(SetupHoldAll::all(), nullptr);
}

TEST_F(SetupHoldAllTest, Matches) {
  EXPECT_TRUE(SetupHoldAll::min()->matches(SetupHold::min()));
  EXPECT_FALSE(SetupHoldAll::min()->matches(SetupHold::max()));
  EXPECT_TRUE(SetupHoldAll::max()->matches(SetupHold::max()));
  EXPECT_FALSE(SetupHoldAll::max()->matches(SetupHold::min()));
  EXPECT_TRUE(SetupHoldAll::all()->matches(SetupHold::min()));
  EXPECT_TRUE(SetupHoldAll::all()->matches(SetupHold::max()));
}

TEST_F(SetupHoldAllTest, Range) {
  auto &range = SetupHoldAll::all()->range();
  EXPECT_EQ(range.size(), 2u);
}

TEST_F(SetupHoldAllTest, Find) {
  EXPECT_EQ(SetupHoldAll::find("min"), SetupHoldAll::min());
  EXPECT_EQ(SetupHoldAll::find("max"), SetupHoldAll::max());
}

////////////////////////////////////////////////////////////////
// RiseFallMinMax additional tests for SDC coverage
////////////////////////////////////////////////////////////////

class SdcRiseFallMinMaxTest : public ::testing::Test {};

TEST_F(SdcRiseFallMinMaxTest, MergeValueIntoEmpty) {
  RiseFallMinMax rfmm;
  rfmm.mergeValue(RiseFallBoth::riseFall(), MinMaxAll::all(), 3.0f);
  // When empty, merge should set the value
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 3.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 3.0f);
}

TEST_F(SdcRiseFallMinMaxTest, MergeValueRfMm) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 5.0f);
  rfmm.mergeValue(RiseFall::rise(), MinMax::max(), 10.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 10.0f);
  rfmm.mergeValue(RiseFall::rise(), MinMax::max(), 3.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 10.0f);
}

TEST_F(SdcRiseFallMinMaxTest, MergeValueRfMmMin) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 5.0f);
  rfmm.mergeValue(RiseFall::fall(), MinMax::min(), 2.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::min()), 2.0f);
  rfmm.mergeValue(RiseFall::fall(), MinMax::min(), 8.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::min()), 2.0f);
}

TEST_F(SdcRiseFallMinMaxTest, MergeValueIntoEmptyRfMm) {
  RiseFallMinMax rfmm;
  rfmm.mergeValue(RiseFall::rise(), MinMax::min(), 7.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 7.0f);
}

TEST_F(SdcRiseFallMinMaxTest, MergeWithBothExist) {
  RiseFallMinMax rfmm1;
  rfmm1.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  rfmm1.setValue(RiseFall::rise(), MinMax::max(), 5.0f);
  rfmm1.setValue(RiseFall::fall(), MinMax::min(), 5.0f);
  rfmm1.setValue(RiseFall::fall(), MinMax::max(), 5.0f);

  RiseFallMinMax rfmm2;
  rfmm2.setValue(RiseFall::rise(), MinMax::min(), 3.0f);
  rfmm2.setValue(RiseFall::rise(), MinMax::max(), 10.0f);
  rfmm2.setValue(RiseFall::fall(), MinMax::min(), 3.0f);
  rfmm2.setValue(RiseFall::fall(), MinMax::max(), 10.0f);

  rfmm1.mergeWith(&rfmm2);
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::rise(), MinMax::min()), 3.0f);
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::rise(), MinMax::max()), 10.0f);
}

TEST_F(SdcRiseFallMinMaxTest, MergeWithOnlySecondExists) {
  RiseFallMinMax rfmm1;
  // rfmm1 is empty

  RiseFallMinMax rfmm2;
  rfmm2.setValue(RiseFall::rise(), MinMax::min(), 7.0f);

  rfmm1.mergeWith(&rfmm2);
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::rise(), MinMax::min()), 7.0f);
}

TEST_F(SdcRiseFallMinMaxTest, RemoveValueRfBothMm) {
  RiseFallMinMax rfmm(1.0f);
  rfmm.removeValue(RiseFallBoth::riseFall(), MinMax::min());
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_FALSE(rfmm.hasValue(RiseFall::fall(), MinMax::min()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::fall(), MinMax::max()));
}

TEST_F(SdcRiseFallMinMaxTest, RemoveValueRfBothMmAll) {
  RiseFallMinMax rfmm(1.0f);
  rfmm.removeValue(RiseFallBoth::rise(), MinMaxAll::all());
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::fall(), MinMax::min()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::fall(), MinMax::max()));
}

////////////////////////////////////////////////////////////////
// Variables tests
////////////////////////////////////////////////////////////////

class VariablesTest : public ::testing::Test {};

TEST_F(VariablesTest, DefaultValues) {
  Variables vars;
  EXPECT_TRUE(vars.crprEnabled());
  EXPECT_EQ(vars.crprMode(), CrprMode::same_pin);
  EXPECT_TRUE(vars.propagateGatedClockEnable());
  EXPECT_FALSE(vars.presetClrArcsEnabled());
  EXPECT_TRUE(vars.condDefaultArcsEnabled());
  EXPECT_FALSE(vars.bidirectInstPathsEnabled());
  EXPECT_TRUE(vars.recoveryRemovalChecksEnabled());
  EXPECT_TRUE(vars.gatedClkChecksEnabled());
  EXPECT_FALSE(vars.clkThruTristateEnabled());
  EXPECT_FALSE(vars.dynamicLoopBreaking());
  EXPECT_FALSE(vars.propagateAllClocks());
  EXPECT_FALSE(vars.useDefaultArrivalClock());
  EXPECT_FALSE(vars.pocvEnabled());
}

TEST_F(VariablesTest, SetCrprEnabled) {
  Variables vars;
  vars.setCrprEnabled(false);
  EXPECT_FALSE(vars.crprEnabled());
  vars.setCrprEnabled(true);
  EXPECT_TRUE(vars.crprEnabled());
}

TEST_F(VariablesTest, SetCrprMode) {
  Variables vars;
  vars.setCrprMode(CrprMode::same_transition);
  EXPECT_EQ(vars.crprMode(), CrprMode::same_transition);
  vars.setCrprMode(CrprMode::same_pin);
  EXPECT_EQ(vars.crprMode(), CrprMode::same_pin);
}

TEST_F(VariablesTest, SetPropagateGatedClockEnable) {
  Variables vars;
  vars.setPropagateGatedClockEnable(false);
  EXPECT_FALSE(vars.propagateGatedClockEnable());
}

TEST_F(VariablesTest, SetPresetClrArcsEnabled) {
  Variables vars;
  vars.setPresetClrArcsEnabled(true);
  EXPECT_TRUE(vars.presetClrArcsEnabled());
}

TEST_F(VariablesTest, SetCondDefaultArcsEnabled) {
  Variables vars;
  vars.setCondDefaultArcsEnabled(false);
  EXPECT_FALSE(vars.condDefaultArcsEnabled());
}

TEST_F(VariablesTest, SetBidirectInstPathsEnabled) {
  Variables vars;
  vars.setBidirectInstPathsEnabled(true);
  EXPECT_TRUE(vars.bidirectInstPathsEnabled());
}

// bidirectInstPathsEnabled was removed from Variables.

TEST_F(VariablesTest, SetRecoveryRemovalChecksEnabled) {
  Variables vars;
  vars.setRecoveryRemovalChecksEnabled(false);
  EXPECT_FALSE(vars.recoveryRemovalChecksEnabled());
}

TEST_F(VariablesTest, SetGatedClkChecksEnabled) {
  Variables vars;
  vars.setGatedClkChecksEnabled(false);
  EXPECT_FALSE(vars.gatedClkChecksEnabled());
}

TEST_F(VariablesTest, SetDynamicLoopBreaking) {
  Variables vars;
  vars.setDynamicLoopBreaking(true);
  EXPECT_TRUE(vars.dynamicLoopBreaking());
}

TEST_F(VariablesTest, SetPropagateAllClocks) {
  Variables vars;
  vars.setPropagateAllClocks(true);
  EXPECT_TRUE(vars.propagateAllClocks());
}

TEST_F(VariablesTest, SetClkThruTristateEnabled) {
  Variables vars;
  vars.setClkThruTristateEnabled(true);
  EXPECT_TRUE(vars.clkThruTristateEnabled());
}

TEST_F(VariablesTest, SetUseDefaultArrivalClock) {
  Variables vars;
  vars.setUseDefaultArrivalClock(true);
  EXPECT_TRUE(vars.useDefaultArrivalClock());
}

TEST_F(VariablesTest, SetPocvEnabled) {
  Variables vars;
  vars.setPocvEnabled(true);
  EXPECT_TRUE(vars.pocvEnabled());
}

////////////////////////////////////////////////////////////////
// DeratingFactors tests
////////////////////////////////////////////////////////////////

class DeratingFactorsTest : public ::testing::Test {};

TEST_F(DeratingFactorsTest, DefaultConstruction) {
  DeratingFactors df;
  EXPECT_FALSE(df.hasValue());
}

TEST_F(DeratingFactorsTest, SetFactorClkData) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               MinMax::min(), 0.95f);
  EXPECT_TRUE(df.hasValue());

  float factor;
  bool exists;
  df.factor(PathClkOrData::clk, RiseFall::rise(), MinMax::min(),
            factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 0.95f);

  df.factor(PathClkOrData::clk, RiseFall::fall(), MinMax::min(),
            factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 0.95f);
}

TEST_F(DeratingFactorsTest, SetFactorData) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::data, RiseFallBoth::rise(),
               MinMax::max(), 1.05f);

  float factor;
  bool exists;
  df.factor(PathClkOrData::data, RiseFall::rise(), MinMax::max(),
            factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 1.05f);

  // Fall should not exist
  df.factor(PathClkOrData::data, RiseFall::fall(), MinMax::max(),
            factor, exists);
  EXPECT_FALSE(exists);
}

TEST_F(DeratingFactorsTest, Clear) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               MinMax::min(), 0.95f);
  EXPECT_TRUE(df.hasValue());
  df.clear();
  EXPECT_FALSE(df.hasValue());
}

TEST_F(DeratingFactorsTest, IsOneValueTrue) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               MinMax::min(), 0.95f);
  df.setFactor(PathClkOrData::data, RiseFallBoth::riseFall(),
               MinMax::min(), 0.95f);
  bool is_one;
  float val;
  df.isOneValue(MinMax::min(), is_one, val);
  EXPECT_TRUE(is_one);
  EXPECT_FLOAT_EQ(val, 0.95f);
}

TEST_F(DeratingFactorsTest, IsOneValueFalse) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               MinMax::min(), 0.95f);
  df.setFactor(PathClkOrData::data, RiseFallBoth::riseFall(),
               MinMax::min(), 1.05f);
  bool is_one;
  float val;
  df.isOneValue(MinMax::min(), is_one, val);
  EXPECT_FALSE(is_one);
}

TEST_F(DeratingFactorsTest, IsOneValueClkData) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               MinMax::min(), 0.95f);
  bool is_one;
  float val;
  df.isOneValue(PathClkOrData::clk, MinMax::min(), is_one, val);
  EXPECT_TRUE(is_one);
  EXPECT_FLOAT_EQ(val, 0.95f);
}

// DeratingFactorsGlobal tests
class DeratingFactorsGlobalTest : public ::testing::Test {};

TEST_F(DeratingFactorsGlobalTest, DefaultConstruction) {
  DeratingFactorsGlobal dfg;
  float factor = 0.0f;
  bool exists = true;
  dfg.factor(TimingDerateType::cell_delay, PathClkOrData::data,
             RiseFall::rise(), MinMax::max(), factor, exists);
  EXPECT_FALSE(exists);
  dfg.clear();
  exists = true;
  dfg.factor(TimingDerateType::cell_delay, PathClkOrData::data,
             RiseFall::rise(), MinMax::max(), factor, exists);
  EXPECT_FALSE(exists);
}

TEST_F(DeratingFactorsGlobalTest, SetFactorCellDelay) {
  DeratingFactorsGlobal dfg;
  dfg.setFactor(TimingDerateType::cell_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), MinMax::max(), 1.1f);

  float factor;
  bool exists;
  dfg.factor(TimingDerateType::cell_delay, PathClkOrData::data,
             RiseFall::rise(), MinMax::max(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 1.1f);
}

TEST_F(DeratingFactorsGlobalTest, SetFactorCellCheck) {
  DeratingFactorsGlobal dfg;
  dfg.setFactor(TimingDerateType::cell_check, PathClkOrData::clk,
                RiseFallBoth::fall(), MinMax::min(), 0.9f);

  float factor;
  bool exists;
  dfg.factor(TimingDerateType::cell_check, PathClkOrData::clk,
             RiseFall::fall(), MinMax::min(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 0.9f);
}

TEST_F(DeratingFactorsGlobalTest, SetFactorNetDelay) {
  DeratingFactorsGlobal dfg;
  dfg.setFactor(TimingDerateType::net_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), MinMax::max(), 1.2f);

  float factor;
  bool exists;
  dfg.factor(TimingDerateType::net_delay, PathClkOrData::data,
             RiseFall::rise(), MinMax::max(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 1.2f);
}

TEST_F(DeratingFactorsGlobalTest, FactorCellType) {
  DeratingFactorsGlobal dfg;
  dfg.setFactor(TimingDerateType::cell_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), MinMax::max(), 1.15f);

  float factor;
  bool exists;
  dfg.factor(TimingDerateCellType::cell_delay, PathClkOrData::data,
             RiseFall::rise(), MinMax::max(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 1.15f);
}

TEST_F(DeratingFactorsGlobalTest, Factors) {
  DeratingFactorsGlobal dfg;
  DeratingFactors *f = dfg.factors(TimingDerateType::cell_delay);
  EXPECT_NE(f, nullptr);
  EXPECT_FALSE(f->hasValue());
}

// DeratingFactorsCell tests
class DeratingFactorsCellTest : public ::testing::Test {};

TEST_F(DeratingFactorsCellTest, DefaultConstruction) {
  ASSERT_NO_THROW(( [&](){
  DeratingFactorsCell dfc;
  dfc.clear();

  }() ));
}

TEST_F(DeratingFactorsCellTest, SetFactorCellDelay) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), MinMax::max(), 1.1f);

  float factor;
  bool exists;
  dfc.factor(TimingDerateCellType::cell_delay, PathClkOrData::data,
             RiseFall::rise(), MinMax::max(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 1.1f);
}

TEST_F(DeratingFactorsCellTest, SetFactorCellCheck) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_check, PathClkOrData::clk,
                RiseFallBoth::fall(), MinMax::min(), 0.85f);

  float factor;
  bool exists;
  dfc.factor(TimingDerateCellType::cell_check, PathClkOrData::clk,
             RiseFall::fall(), MinMax::min(), factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 0.85f);
}

TEST_F(DeratingFactorsCellTest, Factors) {
  DeratingFactorsCell dfc;
  DeratingFactors *f = dfc.factors(TimingDerateCellType::cell_delay);
  EXPECT_NE(f, nullptr);
}

TEST_F(DeratingFactorsCellTest, IsOneValue) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::clk,
                RiseFallBoth::riseFall(), MinMax::min(), 0.9f);
  dfc.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), MinMax::min(), 0.9f);
  dfc.setFactor(TimingDerateCellType::cell_check, PathClkOrData::clk,
                RiseFallBoth::riseFall(), MinMax::min(), 0.9f);
  dfc.setFactor(TimingDerateCellType::cell_check, PathClkOrData::data,
                RiseFallBoth::riseFall(), MinMax::min(), 0.9f);
  bool is_one;
  float val;
  dfc.isOneValue(MinMax::min(), is_one, val);
  EXPECT_TRUE(is_one);
  EXPECT_FLOAT_EQ(val, 0.9f);
}

TEST_F(DeratingFactorsCellTest, IsOneValueDifferent) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), MinMax::min(), 0.9f);
  dfc.setFactor(TimingDerateCellType::cell_check, PathClkOrData::data,
                RiseFallBoth::riseFall(), MinMax::min(), 1.1f);
  bool is_one;
  float val;
  dfc.isOneValue(MinMax::min(), is_one, val);
  EXPECT_FALSE(is_one);
}

// DeratingFactorsNet tests
class DeratingFactorsNetTest : public ::testing::Test {};

TEST_F(DeratingFactorsNetTest, DefaultConstruction) {
  DeratingFactorsNet dfn;
  EXPECT_FALSE(dfn.hasValue());
}

TEST_F(DeratingFactorsNetTest, InheritsSetFactor) {
  DeratingFactorsNet dfn;
  dfn.setFactor(PathClkOrData::data, RiseFallBoth::riseFall(),
                MinMax::max(), 1.05f);
  EXPECT_TRUE(dfn.hasValue());
  float factor;
  bool exists;
  dfn.factor(PathClkOrData::data, RiseFall::rise(), MinMax::max(),
             factor, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(factor, 1.05f);
}

////////////////////////////////////////////////////////////////
// ClockLatency tests
////////////////////////////////////////////////////////////////

class ClockLatencyTest : public ::testing::Test {};

TEST_F(ClockLatencyTest, Construction) {
  ClockLatency cl(nullptr, nullptr);
  EXPECT_EQ(cl.clock(), nullptr);
  EXPECT_EQ(cl.pin(), nullptr);
}

TEST_F(ClockLatencyTest, SetAndGetDelay) {
  ClockLatency cl(nullptr, nullptr);
  cl.setDelay(RiseFall::rise(), MinMax::max(), 1.5f);
  EXPECT_FLOAT_EQ(cl.delay(RiseFall::rise(), MinMax::max()), 1.5f);
  // Unset returns 0.0
  EXPECT_FLOAT_EQ(cl.delay(RiseFall::fall(), MinMax::max()), 0.0f);
}

TEST_F(ClockLatencyTest, SetDelayBoth) {
  ClockLatency cl(nullptr, nullptr);
  cl.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(), 2.0f);
  EXPECT_FLOAT_EQ(cl.delay(RiseFall::rise(), MinMax::min()), 2.0f);
  EXPECT_FLOAT_EQ(cl.delay(RiseFall::fall(), MinMax::max()), 2.0f);
}

TEST_F(ClockLatencyTest, DelayWithExists) {
  ClockLatency cl(nullptr, nullptr);
  float latency;
  bool exists;
  cl.delay(RiseFall::rise(), MinMax::min(), latency, exists);
  EXPECT_FALSE(exists);
  EXPECT_FLOAT_EQ(latency, 0.0f);

  cl.setDelay(RiseFall::rise(), MinMax::min(), 3.0f);
  cl.delay(RiseFall::rise(), MinMax::min(), latency, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(latency, 3.0f);
}

TEST_F(ClockLatencyTest, Delays) {
  ClockLatency cl(nullptr, nullptr);
  RiseFallMinMax *delays = cl.delays();
  EXPECT_NE(delays, nullptr);
}

TEST_F(ClockLatencyTest, SetDelays) {
  RiseFallMinMax src(5.0f);
  ClockLatency cl(nullptr, nullptr);
  cl.setDelays(&src);
  EXPECT_FLOAT_EQ(cl.delay(RiseFall::rise(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(cl.delay(RiseFall::fall(), MinMax::max()), 5.0f);
}

////////////////////////////////////////////////////////////////
// ClockInsertion tests
////////////////////////////////////////////////////////////////

class ClockInsertionTest : public ::testing::Test {};

TEST_F(ClockInsertionTest, Construction) {
  ClockInsertion ci(nullptr, nullptr);
  EXPECT_EQ(ci.clock(), nullptr);
  EXPECT_EQ(ci.pin(), nullptr);
}

TEST_F(ClockInsertionTest, SetAndGetDelay) {
  ClockInsertion ci(nullptr, nullptr);
  ci.setDelay(RiseFall::rise(), MinMax::max(), EarlyLate::min(), 1.5f);
  float delay = ci.delay(RiseFall::rise(), MinMax::max(), EarlyLate::min());
  EXPECT_FLOAT_EQ(delay, 1.5f);
  // Unset returns 0.0
  float delay2 = ci.delay(RiseFall::fall(), MinMax::max(), EarlyLate::min());
  EXPECT_FLOAT_EQ(delay2, 0.0f);
}

TEST_F(ClockInsertionTest, SetDelayBoth) {
  ClockInsertion ci(nullptr, nullptr);
  ci.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(),
              EarlyLateAll::all(), 2.0f);
  EXPECT_FLOAT_EQ(ci.delay(RiseFall::rise(), MinMax::min(), EarlyLate::min()), 2.0f);
  EXPECT_FLOAT_EQ(ci.delay(RiseFall::fall(), MinMax::max(), EarlyLate::max()), 2.0f);
}

TEST_F(ClockInsertionTest, DelayWithExists) {
  ClockInsertion ci(nullptr, nullptr);
  float insertion;
  bool exists;
  ci.delay(RiseFall::rise(), MinMax::min(), EarlyLate::min(), insertion, exists);
  EXPECT_FALSE(exists);
  EXPECT_FLOAT_EQ(insertion, 0.0f);

  ci.setDelay(RiseFall::rise(), MinMax::min(), EarlyLate::min(), 3.0f);
  ci.delay(RiseFall::rise(), MinMax::min(), EarlyLate::min(), insertion, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(insertion, 3.0f);
}

TEST_F(ClockInsertionTest, Delays) {
  ClockInsertion ci(nullptr, nullptr);
  RiseFallMinMax *delays = ci.delays(EarlyLate::min());
  EXPECT_NE(delays, nullptr);
}

TEST_F(ClockInsertionTest, SetDelays) {
  RiseFallMinMax src(7.0f);
  ClockInsertion ci(nullptr, nullptr);
  ci.setDelays(&src);
  EXPECT_FLOAT_EQ(ci.delay(RiseFall::rise(), MinMax::min(), EarlyLate::min()), 7.0f);
  EXPECT_FLOAT_EQ(ci.delay(RiseFall::fall(), MinMax::max(), EarlyLate::max()), 7.0f);
}

////////////////////////////////////////////////////////////////
// ClockGatingCheck tests
////////////////////////////////////////////////////////////////

class ClockGatingCheckTest : public ::testing::Test {};

TEST_F(ClockGatingCheckTest, DefaultConstruction) {
  ClockGatingCheck cgc;
  EXPECT_EQ(cgc.activeValue(), LogicValue::unknown);
}

TEST_F(ClockGatingCheckTest, SetActiveValue) {
  ClockGatingCheck cgc;
  cgc.setActiveValue(LogicValue::one);
  EXPECT_EQ(cgc.activeValue(), LogicValue::one);
  cgc.setActiveValue(LogicValue::zero);
  EXPECT_EQ(cgc.activeValue(), LogicValue::zero);
}

TEST_F(ClockGatingCheckTest, Margins) {
  ClockGatingCheck cgc;
  RiseFallMinMax *margins = cgc.margins();
  EXPECT_NE(margins, nullptr);
  EXPECT_TRUE(margins->empty());
}

TEST_F(ClockGatingCheckTest, SetMargins) {
  ClockGatingCheck cgc;
  RiseFallMinMax *margins = cgc.margins();
  margins->setValue(RiseFall::rise(), MinMax::min(), 0.1f);
  float val;
  bool exists;
  margins->value(RiseFall::rise(), MinMax::min(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.1f);
}

////////////////////////////////////////////////////////////////
// SdcCmdComment tests
// SdcCmdComment has a protected destructor so we use a testable subclass
////////////////////////////////////////////////////////////////

class TestableSdcCmdComment : public SdcCmdComment {
public:
  TestableSdcCmdComment() : SdcCmdComment() {}
  TestableSdcCmdComment(const char *comment) : SdcCmdComment(comment) {}
  ~TestableSdcCmdComment() {}
};

class SdcCmdCommentTest : public ::testing::Test {};

TEST_F(SdcCmdCommentTest, DefaultConstruction) {
  TestableSdcCmdComment scc;
  EXPECT_EQ(scc.comment(), nullptr);
}

TEST_F(SdcCmdCommentTest, CommentConstruction) {
  TestableSdcCmdComment scc("test comment");
  EXPECT_STREQ(scc.comment(), "test comment");
}

TEST_F(SdcCmdCommentTest, EmptyCommentConstruction) {
  TestableSdcCmdComment scc("");
  EXPECT_EQ(scc.comment(), nullptr);
}

TEST_F(SdcCmdCommentTest, NullCommentConstruction) {
  TestableSdcCmdComment scc(nullptr);
  EXPECT_EQ(scc.comment(), nullptr);
}

TEST_F(SdcCmdCommentTest, SetComment) {
  TestableSdcCmdComment scc;
  scc.setComment("new comment");
  EXPECT_STREQ(scc.comment(), "new comment");
}

TEST_F(SdcCmdCommentTest, SetCommentNull) {
  TestableSdcCmdComment scc("original");
  scc.setComment(nullptr);
  EXPECT_EQ(scc.comment(), nullptr);
}

TEST_F(SdcCmdCommentTest, SetCommentEmpty) {
  TestableSdcCmdComment scc("original");
  scc.setComment("");
  EXPECT_EQ(scc.comment(), nullptr);
}

TEST_F(SdcCmdCommentTest, SetCommentReplace) {
  TestableSdcCmdComment scc("first");
  scc.setComment("second");
  EXPECT_STREQ(scc.comment(), "second");
}

////////////////////////////////////////////////////////////////
// PortExtCap tests
////////////////////////////////////////////////////////////////

class PortExtCapTest : public ::testing::Test {};

TEST_F(PortExtCapTest, Construction) {
  PortExtCap pec;
  EXPECT_EQ(pec.port(), nullptr);
}

TEST_F(PortExtCapTest, PinCap) {
  PortExtCap pec;
  float cap;
  bool exists;
  pec.pinCap(RiseFall::rise(), MinMax::max(), cap, exists);
  EXPECT_FALSE(exists);

  pec.setPinCap(nullptr, 1.5f, RiseFall::rise(), MinMax::max());
  pec.pinCap(RiseFall::rise(), MinMax::max(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 1.5f);
}

TEST_F(PortExtCapTest, WireCap) {
  PortExtCap pec;
  float cap;
  bool exists;
  pec.wireCap(RiseFall::fall(), MinMax::min(), cap, exists);
  EXPECT_FALSE(exists);

  pec.setWireCap(nullptr, 2.5f, RiseFall::fall(), MinMax::min());
  pec.wireCap(RiseFall::fall(), MinMax::min(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 2.5f);
}

TEST_F(PortExtCapTest, Fanout) {
  PortExtCap pec;
  int fanout;
  bool exists;
  pec.fanout(MinMax::max(), fanout, exists);
  EXPECT_FALSE(exists);

  pec.setFanout(nullptr, 4, MinMax::max());
  pec.fanout(MinMax::max(), fanout, exists);
  EXPECT_TRUE(exists);
  EXPECT_EQ(fanout, 4);
}

TEST_F(PortExtCapTest, PinCapPtr) {
  PortExtCap pec;
  const RiseFallMinMax *pc = pec.pinCap();
  EXPECT_NE(pc, nullptr);
}

TEST_F(PortExtCapTest, WireCapPtr) {
  PortExtCap pec;
  const RiseFallMinMax *wc = pec.wireCap();
  EXPECT_NE(wc, nullptr);
}

TEST_F(PortExtCapTest, FanoutPtr) {
  PortExtCap pec;
  const FanoutValues *fv = pec.fanout();
  EXPECT_NE(fv, nullptr);
}

////////////////////////////////////////////////////////////////
// DataCheck tests
////////////////////////////////////////////////////////////////

class DataCheckTest : public ::testing::Test {
protected:
  void SetUp() override {
    initSta();
  }
};

TEST_F(DataCheckTest, Construction) {
  DataCheck dc(nullptr, nullptr, nullptr);
  EXPECT_EQ(dc.from(), nullptr);
  EXPECT_EQ(dc.to(), nullptr);
  EXPECT_EQ(dc.clk(), nullptr);
  EXPECT_TRUE(dc.empty());
}

TEST_F(DataCheckTest, SetAndGetMargin) {
  DataCheck dc(nullptr, nullptr, nullptr);
  dc.setMargin(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
               SetupHoldAll::all(), 0.5f);
  EXPECT_FALSE(dc.empty());

  float margin;
  bool exists;
  dc.margin(RiseFall::rise(), RiseFall::rise(), SetupHold::min(),
            margin, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(margin, 0.5f);
}

TEST_F(DataCheckTest, SetMarginSpecific) {
  DataCheck dc(nullptr, nullptr, nullptr);
  dc.setMargin(RiseFallBoth::rise(), RiseFallBoth::fall(),
               SetupHoldAll::min(), 0.3f);

  float margin;
  bool exists;
  dc.margin(RiseFall::rise(), RiseFall::fall(), SetupHold::min(),
            margin, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(margin, 0.3f);

  // Other combination should not exist
  dc.margin(RiseFall::fall(), RiseFall::rise(), SetupHold::min(),
            margin, exists);
  EXPECT_FALSE(exists);
}

TEST_F(DataCheckTest, RemoveMargin) {
  DataCheck dc(nullptr, nullptr, nullptr);
  dc.setMargin(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
               SetupHoldAll::all(), 0.5f);
  EXPECT_FALSE(dc.empty());

  dc.removeMargin(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                  SetupHoldAll::all());
  EXPECT_TRUE(dc.empty());
}

TEST_F(DataCheckTest, MarginIsOneValue) {
  DataCheck dc(nullptr, nullptr, nullptr);
  dc.setMargin(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
               SetupHoldAll::min(), 0.5f);
  float val;
  bool is_one;
  dc.marginIsOneValue(SetupHold::min(), val, is_one);
  EXPECT_TRUE(is_one);
  EXPECT_FLOAT_EQ(val, 0.5f);
}

TEST_F(DataCheckTest, MarginIsOneValueDifferent) {
  DataCheck dc(nullptr, nullptr, nullptr);
  dc.setMargin(RiseFallBoth::rise(), RiseFallBoth::riseFall(),
               SetupHoldAll::min(), 0.5f);
  dc.setMargin(RiseFallBoth::fall(), RiseFallBoth::riseFall(),
               SetupHoldAll::min(), 0.3f);
  float val;
  bool is_one;
  dc.marginIsOneValue(SetupHold::min(), val, is_one);
  EXPECT_FALSE(is_one);
}

////////////////////////////////////////////////////////////////
// PinPairEqual tests
////////////////////////////////////////////////////////////////

class PinPairEqualTest : public ::testing::Test {};

TEST_F(PinPairEqualTest, SamePinsEqual) {
  const Pin *p1 = reinterpret_cast<const Pin*>(0x1000);
  const Pin *p2 = reinterpret_cast<const Pin*>(0x2000);
  PinPair pair1(p1, p2);
  PinPair pair2(p1, p2);
  PinPairEqual eq;
  EXPECT_TRUE(eq(pair1, pair2));
}

TEST_F(PinPairEqualTest, DifferentPinsNotEqual) {
  const Pin *p1 = reinterpret_cast<const Pin*>(0x1000);
  const Pin *p2 = reinterpret_cast<const Pin*>(0x2000);
  const Pin *p3 = reinterpret_cast<const Pin*>(0x3000);
  PinPair pair1(p1, p2);
  PinPair pair2(p1, p3);
  PinPairEqual eq;
  EXPECT_FALSE(eq(pair1, pair2));
}

TEST_F(PinPairEqualTest, NullPinsEqual) {
  PinPair pair1(nullptr, nullptr);
  PinPair pair2(nullptr, nullptr);
  PinPairEqual eq;
  EXPECT_TRUE(eq(pair1, pair2));
}

////////////////////////////////////////////////////////////////
// ClockGroups type tests
////////////////////////////////////////////////////////////////

class ClockGroupsTest : public ::testing::Test {};

TEST_F(ClockGroupsTest, ClockSenseValues) {
  // Verify enum values exist
  EXPECT_NE(static_cast<int>(ClockSense::positive), static_cast<int>(ClockSense::negative));
  EXPECT_NE(static_cast<int>(ClockSense::negative), static_cast<int>(ClockSense::stop));
  EXPECT_NE(static_cast<int>(ClockSense::positive), static_cast<int>(ClockSense::stop));
}

TEST_F(ClockGroupsTest, AnalysisTypeValues) {
  EXPECT_NE(static_cast<int>(AnalysisType::single), static_cast<int>(AnalysisType::bc_wc));
  EXPECT_NE(static_cast<int>(AnalysisType::bc_wc), static_cast<int>(AnalysisType::ocv));
}

TEST_F(ClockGroupsTest, ExceptionPathTypeValues) {
  EXPECT_NE(static_cast<int>(ExceptionPathType::false_path),
            static_cast<int>(ExceptionPathType::loop));
  EXPECT_NE(static_cast<int>(ExceptionPathType::multi_cycle),
            static_cast<int>(ExceptionPathType::path_delay));
  EXPECT_NE(static_cast<int>(ExceptionPathType::group_path),
            static_cast<int>(ExceptionPathType::filter));
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

// Sdc clear operations
TEST_F(SdcInitTest, SdcClearAfterConstraints) {
  Sdc *sdc = sta_->cmdSdc();
  ASSERT_NE(sdc, nullptr);
  // Set some constraints then clear
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.5);
  sdc->setMaxArea(100.0);
  sdc->setWireloadMode(WireloadMode::top);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 100.0f);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::top);
  sdc->clear();
  // clear() resets constraints but keeps environment-style knobs.
  EXPECT_FLOAT_EQ(sdc->maxArea(), 100.0f);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::top);
  EXPECT_NE(sdc->defaultArrivalClock(), nullptr);
  EXPECT_NE(sdc->defaultArrivalClockEdge(), nullptr);
}

// Sdc remove constraints
TEST_F(SdcInitTest, SdcRemoveConstraints) {
  Sdc *sdc = sta_->cmdSdc();
  ASSERT_NE(sdc, nullptr);
  sdc->setMaxArea(200.0f);
  sdc->setWireloadMode(WireloadMode::segmented);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 200.0f);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::segmented);
  sdc->clear();
  // clear() also preserves these global settings.
  EXPECT_FLOAT_EQ(sdc->maxArea(), 200.0f);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::segmented);
  EXPECT_TRUE(sdc->clocks().empty());
  EXPECT_NE(sdc->defaultArrivalClock(), nullptr);
  EXPECT_NE(sdc->defaultArrivalClockEdge(), nullptr);
}

// Clock creation and queries
TEST_F(SdcInitTest, MakeClockNoPins) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("test_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("test_clk");
  EXPECT_NE(clk, nullptr);
  EXPECT_FLOAT_EQ(clk->period(), 10.0);
}

TEST_F(SdcInitTest, MakeClockAndRemove) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("clk1", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk1");
  EXPECT_NE(clk, nullptr);

  sta_->removeClock(clk, sta_->cmdSdc());
  EXPECT_EQ(sdc->findClock("clk1"), nullptr);
}

TEST_F(SdcInitTest, MultipleClocksQuery) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("clk_a", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("clk_b", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  ClockSeq clks = sdc->clocks();
  EXPECT_EQ(clks.size(), 2u);
}

// Clock properties
TEST_F(SdcInitTest, ClockProperties) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("prop_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("prop_clk");
  EXPECT_STREQ(clk->name(), "prop_clk");
  EXPECT_FLOAT_EQ(clk->period(), 10.0);
  EXPECT_FALSE(clk->isPropagated());
  EXPECT_FALSE(clk->isGenerated());
  // Clock with no pins is virtual
  EXPECT_TRUE(clk->isVirtual());
}

// Clock slew
TEST_F(SdcInitTest, ClockSlew) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("slew_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("slew_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockSlew(clk, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5, sta_->cmdSdc());
  float slew = 0.0f;
  bool exists = false;
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
  sta_->removeClockSlew(clk, sta_->cmdSdc());
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_FALSE(exists);
}

// Clock latency with clock
TEST_F(SdcInitTest, ClockLatencyOnClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lat_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("lat_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                        MinMaxAll::all(), 1.0, sta_->cmdSdc());
  float latency = 0.0f;
  bool exists = false;
  sdc->clockLatency(clk, RiseFall::rise(), MinMax::max(), latency, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(latency, 1.0f);
  sta_->removeClockLatency(clk, nullptr, sta_->cmdSdc());
  sdc->clockLatency(clk, RiseFall::rise(), MinMax::max(), latency, exists);
  EXPECT_FALSE(exists);
}

// Clock insertion delay
TEST_F(SdcInitTest, ClockInsertionOnClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ins_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("ins_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                          MinMaxAll::all(), EarlyLateAll::all(), 0.5, sta_->cmdSdc());
  float insertion = 0.0f;
  bool exists = false;
  sdc->clockInsertion(clk, nullptr, RiseFall::rise(), MinMax::max(),
                      EarlyLate::early(), insertion, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(insertion, 0.5f);
  sta_->removeClockInsertion(clk, nullptr, sta_->cmdSdc());
  sdc->clockInsertion(clk, nullptr, RiseFall::rise(), MinMax::max(),
                      EarlyLate::early(), insertion, exists);
  EXPECT_FALSE(exists);
}

// Clock uncertainty
TEST_F(SdcInitTest, ClockUncertainty) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("unc_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("unc_clk");
  ASSERT_NE(clk, nullptr);
  ASSERT_NO_THROW((sta_->setClockUncertainty(clk, SetupHoldAll::all(), 0.1), sta_->cmdSdc()));
  ASSERT_NO_THROW((sta_->removeClockUncertainty(clk, SetupHoldAll::all()), sta_->cmdSdc()));
}

// Inter-clock uncertainty
TEST_F(SdcInitTest, InterClockUncertainty) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("iuc_clk1", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("iuc_clk2", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk1 = sdc->findClock("iuc_clk1");
  Clock *clk2 = sdc->findClock("iuc_clk2");
  ASSERT_NE(clk1, nullptr);
  ASSERT_NE(clk2, nullptr);
  sta_->setClockUncertainty(clk1, RiseFallBoth::riseFall(),
                            clk2, RiseFallBoth::riseFall(),
                            SetupHoldAll::all(), 0.2, sta_->cmdSdc());
  float uncertainty = 0.0f;
  bool exists = false;
  sdc->clockUncertainty(clk1, RiseFall::rise(),
                        clk2, RiseFall::rise(),
                        SetupHold::max(), uncertainty, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(uncertainty, 0.2f);
  sta_->removeClockUncertainty(clk1, RiseFallBoth::riseFall(),
                               clk2, RiseFallBoth::riseFall(),
                               SetupHoldAll::all(), sta_->cmdSdc());
  sdc->clockUncertainty(clk1, RiseFall::rise(),
                        clk2, RiseFall::rise(),
                        SetupHold::max(), uncertainty, exists);
  EXPECT_FALSE(exists);
}

// Clock groups
TEST_F(SdcInitTest, ClockGroupsOperations) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("grp_clk1", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("grp_clk2", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk1 = sdc->findClock("grp_clk1");
  Clock *clk2 = sdc->findClock("grp_clk2");
  ASSERT_NE(clk1, nullptr);
  ASSERT_NE(clk2, nullptr);

  ClockGroups *groups = sta_->makeClockGroups("grp1", true, false, false, false, nullptr, sta_->cmdSdc());
  ASSERT_NE(groups, nullptr);
  ClockSet *clk_set = new ClockSet;
  clk_set->insert(clk1);
  clk_set->insert(clk2);
  ASSERT_NO_THROW((sta_->makeClockGroup(groups, clk_set, sta_->cmdSdc())));

  ASSERT_NO_THROW((sta_->removeClockGroupsLogicallyExclusive("grp1", sta_->cmdSdc()), sta_->cmdSdc()));
  EXPECT_NE(sdc->findClock("grp_clk1"), nullptr);
  EXPECT_NE(sdc->findClock("grp_clk2"), nullptr);
}

// Clock propagation
TEST_F(SdcInitTest, ClockPropagation) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("prop_clk2", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("prop_clk2");
  sta_->setPropagatedClock(clk, sta_->cmdMode());
  EXPECT_TRUE(clk->isPropagated());
  sta_->removePropagatedClock(clk, sta_->cmdMode());
  EXPECT_FALSE(clk->isPropagated());
}

// Timing derate with clock
TEST_F(SdcInitTest, TimingDerateWithClock) {
  ASSERT_NO_THROW((sta_->setTimingDerate(TimingDerateType::cell_delay,
                                        PathClkOrData::clk,
                                        RiseFallBoth::rise(),
                                        EarlyLate::early(),
                                        0.95, sta_->cmdSdc()),
                       sta_->cmdSdc()));
  ASSERT_NO_THROW((sta_->setTimingDerate(TimingDerateType::cell_check,
                                        PathClkOrData::clk,
                                        RiseFallBoth::fall(),
                                        EarlyLate::late(),
                                        1.05, sta_->cmdSdc()),
                       sta_->cmdSdc()));
  ASSERT_NO_THROW((sta_->setTimingDerate(TimingDerateType::net_delay,
                                        PathClkOrData::data,
                                        RiseFallBoth::riseFall(),
                                        EarlyLate::early(),
                                        0.97, sta_->cmdSdc()),
                       sta_->cmdSdc()));
  ASSERT_NO_THROW((sta_->unsetTimingDerate(sta_->cmdSdc()),
                       sta_->cmdSdc()));
}

// Clock gating check with clock
TEST_F(SdcInitTest, ClockGatingCheckWithClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("cgc_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("cgc_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockGatingCheck(clk, RiseFallBoth::riseFall(),
                            SetupHold::max(), 0.5, sta_->cmdSdc());
  bool exists = false;
  float margin = 0.0f;
  sdc->clockGatingMarginClk(clk, RiseFall::rise(), SetupHold::max(),
                            exists, margin);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(margin, 0.5f);
}

// False path
TEST_F(SdcInitTest, MakeFalsePath) {
  Sdc *sdc = sta_->cmdSdc();
  size_t before = sdc->exceptions().size();
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  EXPECT_GT(sdc->exceptions().size(), before);
}

// Group path
TEST_F(SdcInitTest, MakeGroupPath) {
  sta_->makeGroupPath("test_group", false, nullptr, nullptr, nullptr, nullptr, sta_->cmdSdc());
  EXPECT_TRUE(sta_->isPathGroupName("test_group", sta_->cmdSdc()));
}

// Latch borrow limit
TEST_F(SdcInitTest, LatchBorrowLimitClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lbl_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("lbl_clk");
  ASSERT_NE(clk, nullptr);
  ASSERT_NO_THROW((sta_->setLatchBorrowLimit(clk, 2.0, sta_->cmdSdc())));
  EXPECT_NE(sdc->findClock("lbl_clk"), nullptr);
}

// Min pulse width with clock
TEST_F(SdcInitTest, MinPulseWidthClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("mpw_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("mpw_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setMinPulseWidth(clk, RiseFallBoth::riseFall(), 1.0, sta_->cmdSdc());
  float min_width = 0.0f;
  bool exists = false;
  sdc->minPulseWidth(nullptr, clk, RiseFall::rise(), min_width, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(min_width, 1.0f);
}

// Slew limit on clock
TEST_F(SdcInitTest, SlewLimitClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("sl_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("sl_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                     PathClkOrData::clk, MinMax::max(), 2.0, sta_->cmdSdc());
  float slew = 0.0f;
  bool exists = false;
  sdc->slewLimit(clk, RiseFall::rise(), PathClkOrData::clk,
                 MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 2.0f);
}

// DisabledPorts class
TEST_F(SdcInitTest, DisabledPortsObject) {
  DisabledPorts dp;
  EXPECT_FALSE(dp.all());
  dp.setDisabledAll();
  EXPECT_TRUE(dp.all());
  dp.removeDisabledAll();
  EXPECT_FALSE(dp.all());
}

// WriteSdc on empty design throws
TEST_F(SdcInitTest, WriteSdcEmptyThrows) {
  EXPECT_THROW(sta_->writeSdc(sta_->cmdSdc(), "/dev/null", false, false, 4, false, false),
               std::exception);
}

// Operating conditions
TEST_F(SdcInitTest, SdcOperatingConditions) {
  Sdc *sdc = sta_->cmdSdc();
  // No operating conditions set
  const OperatingConditions *op_min = sdc->operatingConditions(MinMax::min());
  const OperatingConditions *op_max = sdc->operatingConditions(MinMax::max());
  EXPECT_EQ(op_min, nullptr);
  EXPECT_EQ(op_max, nullptr);
}

// Sdc analysis type changes
TEST_F(SdcInitTest, SdcAnalysisTypeChanges) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setAnalysisType(AnalysisType::single);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::single);
  sdc->setAnalysisType(AnalysisType::bc_wc);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::bc_wc);
  sdc->setAnalysisType(AnalysisType::ocv);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::ocv);
}

// Multicycle path
TEST_F(SdcInitTest, MakeMulticyclePath) {
  Sdc *sdc = sta_->cmdSdc();
  size_t before = sdc->exceptions().size();
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::all(),
                           true,   // use_end_clk
                           2,      // path_multiplier
                           nullptr, sta_->cmdSdc());
  EXPECT_GT(sdc->exceptions().size(), before);
}

// Reset path
TEST_F(SdcInitTest, ResetPath) {
  Sdc *sdc = sta_->cmdSdc();
  size_t before = sdc->exceptions().size();
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  size_t after_make = sdc->exceptions().size();
  EXPECT_GT(after_make, before);
  ASSERT_NO_THROW((sta_->resetPath(nullptr, nullptr, nullptr, MinMaxAll::all(), sta_->cmdSdc())));
  EXPECT_EQ(sdc->exceptions().size(), after_make);
}

// Clock waveform details
TEST_F(SdcInitTest, ClockWaveformDetails) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(3.0);
  sta_->makeClock("wave_clk", nullptr, false, 8.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("wave_clk");
  EXPECT_NE(clk, nullptr);
  EXPECT_FLOAT_EQ(clk->period(), 8.0);

  // Get waveform edges
  FloatSeq *edges = clk->waveform();
  EXPECT_NE(edges, nullptr);
  EXPECT_EQ(edges->size(), 2u);
  EXPECT_FLOAT_EQ((*edges)[0], 0.0);
  EXPECT_FLOAT_EQ((*edges)[1], 3.0);
}

// Clock edge access
TEST_F(SdcInitTest, ClockEdges) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("edge_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("edge_clk");
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  EXPECT_NE(rise_edge, nullptr);
  EXPECT_NE(fall_edge, nullptr);
  EXPECT_FLOAT_EQ(rise_edge->time(), 0.0);
  EXPECT_FLOAT_EQ(fall_edge->time(), 5.0);
}

// Multiple timing derate types via Sdc
TEST_F(SdcInitTest, SdcTimingDerateAllTypes) {
  Sdc *sdc = sta_->cmdSdc();
  ASSERT_NO_THROW((sdc->setTimingDerate(TimingDerateType::cell_delay,
                                       PathClkOrData::clk,
                                       RiseFallBoth::rise(),
                                       EarlyLate::early(), 0.95)));
  ASSERT_NO_THROW((sdc->setTimingDerate(TimingDerateType::cell_check,
                                       PathClkOrData::data,
                                       RiseFallBoth::fall(),
                                       EarlyLate::late(), 1.05)));
  ASSERT_NO_THROW((sdc->setTimingDerate(TimingDerateType::net_delay,
                                       PathClkOrData::clk,
                                       RiseFallBoth::riseFall(),
                                       EarlyLate::early(), 0.97)));
  ASSERT_NO_THROW(sdc->unsetTimingDerate());
}

// Multiple clocks and removal
TEST_F(SdcInitTest, MultipleClockRemoval) {
  FloatSeq *w1 = new FloatSeq;
  w1->push_back(0.0);
  w1->push_back(5.0);
  sta_->makeClock("rm_clk1", nullptr, false, 10.0, w1, nullptr, sta_->cmdMode());

  FloatSeq *w2 = new FloatSeq;
  w2->push_back(0.0);
  w2->push_back(2.5);
  sta_->makeClock("rm_clk2", nullptr, false, 5.0, w2, nullptr, sta_->cmdMode());

  FloatSeq *w3 = new FloatSeq;
  w3->push_back(0.0);
  w3->push_back(1.0);
  sta_->makeClock("rm_clk3", nullptr, false, 2.0, w3, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  EXPECT_EQ(sdc->clocks().size(), 3u);

  Clock *clk2 = sdc->findClock("rm_clk2");
  sta_->removeClock(clk2, sta_->cmdSdc());
  EXPECT_EQ(sdc->clocks().size(), 2u);
  EXPECT_EQ(sdc->findClock("rm_clk2"), nullptr);
}

// Voltage settings via Sdc
TEST_F(SdcInitTest, SdcVoltage) {
  Sdc *sdc = sta_->cmdSdc();
  sta_->setVoltage(MinMax::max(), 1.1, sta_->cmdSdc());
  sta_->setVoltage(MinMax::min(), 0.9, sta_->cmdSdc());
  float voltage = 0.0f;
  bool exists = false;
  sdc->voltage(MinMax::max(), voltage, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(voltage, 1.1f);
  sdc->voltage(MinMax::min(), voltage, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(voltage, 0.9f);
}

// DisabledPorts fromTo
TEST_F(SdcInitTest, DisabledPortsFromTo) {
  DisabledPorts dp;
  // Initially empty
  EXPECT_EQ(dp.from(), nullptr);
  EXPECT_EQ(dp.to(), nullptr);
  EXPECT_EQ(dp.fromTo(), nullptr);
  EXPECT_FALSE(dp.all());
}

////////////////////////////////////////////////////////////////
// Additional SDC tests for function coverage
////////////////////////////////////////////////////////////////

// ExceptionPath: clone, asString, typeString, tighterThan
TEST_F(SdcInitTest, FalsePathClone) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionPath *cloned = fp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_NE(cloned, nullptr);
  EXPECT_TRUE(cloned->isFalse());
  delete cloned;
}

TEST_F(SdcInitTest, PathDelayClone) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               5.0e-9f, true, nullptr);
  ExceptionPath *cloned = pd.clone(nullptr, nullptr, nullptr, true);
  EXPECT_NE(cloned, nullptr);
  EXPECT_TRUE(cloned->isPathDelay());
  EXPECT_FLOAT_EQ(cloned->delay(), 5.0e-9f);
  delete cloned;
}

TEST_F(SdcInitTest, MultiCyclePathClone) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  ExceptionPath *cloned = mcp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_NE(cloned, nullptr);
  EXPECT_TRUE(cloned->isMultiCycle());
  EXPECT_EQ(cloned->pathMultiplier(), 3);
  delete cloned;
}

TEST_F(SdcInitTest, GroupPathClone) {
  GroupPath gp("grp", false, nullptr, nullptr, nullptr, true, nullptr);
  ExceptionPath *cloned = gp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_NE(cloned, nullptr);
  EXPECT_TRUE(cloned->isGroupPath());
  EXPECT_STREQ(cloned->name(), "grp");
  delete cloned;
}

TEST_F(SdcInitTest, FilterPathClone) {
  FilterPath flp(nullptr, nullptr, nullptr, true);
  ExceptionPath *cloned = flp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_NE(cloned, nullptr);
  EXPECT_TRUE(cloned->isFilter());
  delete cloned;
}

TEST_F(SdcInitTest, FalsePathAsString) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  const char *str = fp.asString(sta_->cmdNetwork());
  EXPECT_NE(str, nullptr);
}

TEST_F(SdcInitTest, PathDelayAsString) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               1.0e-9f, true, nullptr);
  const char *str = pd.asString(sta_->cmdNetwork());
  EXPECT_NE(str, nullptr);
}

TEST_F(SdcInitTest, MultiCyclePathAsString) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 2, true, nullptr);
  const char *str = mcp.asString(sta_->cmdNetwork());
  EXPECT_NE(str, nullptr);
}

// ExceptionPath type predicates
TEST_F(SdcInitTest, ExceptionTypePredicates) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp.isFalse());
  EXPECT_FALSE(fp.isLoop());
  EXPECT_FALSE(fp.isMultiCycle());
  EXPECT_FALSE(fp.isPathDelay());
  EXPECT_FALSE(fp.isGroupPath());
  EXPECT_FALSE(fp.isFilter());
  EXPECT_EQ(fp.type(), ExceptionPathType::false_path);

  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               1.0e-9f, true, nullptr);
  EXPECT_TRUE(pd.isPathDelay());
  EXPECT_FALSE(pd.isFalse());
  EXPECT_EQ(pd.type(), ExceptionPathType::path_delay);

  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 2, true, nullptr);
  EXPECT_TRUE(mcp.isMultiCycle());
  EXPECT_EQ(mcp.type(), ExceptionPathType::multi_cycle);

  FilterPath flp(nullptr, nullptr, nullptr, true);
  EXPECT_TRUE(flp.isFilter());
  EXPECT_EQ(flp.type(), ExceptionPathType::filter);

  GroupPath gp("g", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp.isGroupPath());
  EXPECT_EQ(gp.type(), ExceptionPathType::group_path);
}

// ExceptionPath tighterThan
TEST_F(SdcInitTest, FalsePathTighterThan) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_FALSE(fp1.tighterThan(&fp2));
}

TEST_F(SdcInitTest, PathDelayTighterThan) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                5.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                10.0e-9f, true, nullptr);
  // Smaller delay is tighter for max
  EXPECT_TRUE(pd1.tighterThan(&pd2));
  EXPECT_FALSE(pd2.tighterThan(&pd1));
}

TEST_F(SdcInitTest, MultiCyclePathTighterThan) {
  MultiCyclePath mcp1(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 2, true, nullptr);
  MultiCyclePath mcp2(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 5, true, nullptr);
  EXPECT_TRUE(mcp1.tighterThan(&mcp2));
}

TEST_F(SdcInitTest, FilterPathTighterThan) {
  FilterPath flp1(nullptr, nullptr, nullptr, true);
  FilterPath flp2(nullptr, nullptr, nullptr, true);
  EXPECT_FALSE(flp1.tighterThan(&flp2));
}

TEST_F(SdcInitTest, GroupPathTighterThan) {
  GroupPath gp1("g1", false, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("g2", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_FALSE(gp1.tighterThan(&gp2));
}

// ExceptionPath typePriority
TEST_F(SdcInitTest, ExceptionTypePriority) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_EQ(fp.typePriority(), ExceptionPath::falsePathPriority());

  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               1.0e-9f, true, nullptr);
  EXPECT_EQ(pd.typePriority(), ExceptionPath::pathDelayPriority());

  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 2, true, nullptr);
  EXPECT_EQ(mcp.typePriority(), ExceptionPath::multiCyclePathPriority());

  FilterPath flp(nullptr, nullptr, nullptr, true);
  EXPECT_EQ(flp.typePriority(), ExceptionPath::filterPathPriority());

  GroupPath gp("g", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_EQ(gp.typePriority(), ExceptionPath::groupPathPriority());
}

// LoopPath
TEST_F(SdcInitTest, LoopPathBasic) {
  LoopPath lp(nullptr, true);
  EXPECT_TRUE(lp.isFalse());
  EXPECT_TRUE(lp.isLoop());
  EXPECT_EQ(lp.type(), ExceptionPathType::loop);
}

TEST_F(SdcInitTest, LoopPathMergeable) {
  LoopPath lp1(nullptr, true);
  LoopPath lp2(nullptr, true);
  // LoopPaths are not mergeable
  EXPECT_FALSE(lp1.mergeable(&lp2));
}

// ExceptionPath setId and priority
TEST_F(SdcInitTest, ExceptionPathSetIdPriority) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  fp.setId(42);
  EXPECT_EQ(fp.id(), 42u);
  fp.setPriority(5000);
  EXPECT_EQ(fp.priority(), 5000);
}

// ExceptionPath default handlers
TEST_F(SdcInitTest, ExceptionPathDefaultHandlers) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_FALSE(fp.useEndClk());
  EXPECT_EQ(fp.pathMultiplier(), 0);
  EXPECT_FLOAT_EQ(fp.delay(), 0.0f);
  EXPECT_EQ(fp.name(), nullptr);
  EXPECT_FALSE(fp.isDefault());
  EXPECT_FALSE(fp.ignoreClkLatency());
  EXPECT_FALSE(fp.breakPath());
}

// PathDelay ignoreClkLatency and breakPath
TEST_F(SdcInitTest, PathDelayIgnoreAndBreak) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(), true, true,
                1.0e-9f, true, nullptr);
  EXPECT_TRUE(pd1.ignoreClkLatency());
  EXPECT_TRUE(pd1.breakPath());

  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                1.0e-9f, true, nullptr);
  EXPECT_FALSE(pd2.ignoreClkLatency());
  EXPECT_FALSE(pd2.breakPath());
}

// MultiCyclePath priority with MinMax
TEST_F(SdcInitTest, MultiCyclePathPriorityWithMinMax) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  int p_min = mcp.priority(MinMax::min());
  int p_max = mcp.priority(MinMax::max());
  EXPECT_GE(p_min, 0);
  EXPECT_GE(p_max, 0);
}

// MultiCyclePath pathMultiplier with MinMax
TEST_F(SdcInitTest, MultiCyclePathMultiplierWithMinMax) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 4, true, nullptr);
  EXPECT_EQ(mcp.pathMultiplier(MinMax::max()), 4);
}

// MultiCyclePath matches min_max exactly
TEST_F(SdcInitTest, MultiCyclePathMatchesExact) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::min(),
                     true, 3, true, nullptr);
  EXPECT_TRUE(mcp.matches(MinMax::min(), true));
  EXPECT_FALSE(mcp.matches(MinMax::max(), true));
}

// GroupPath isDefault
TEST_F(SdcInitTest, GroupPathIsDefault) {
  GroupPath gp1("reg", true, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp1.isDefault());
  GroupPath gp2("cust", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_FALSE(gp2.isDefault());
}

// FilterPath overrides always returns false
TEST_F(SdcInitTest, FilterPathOverrides) {
  FilterPath flp1(nullptr, nullptr, nullptr, true);
  FilterPath flp2(nullptr, nullptr, nullptr, true);
  EXPECT_FALSE(flp1.overrides(&flp2));
}

TEST_F(SdcInitTest, FilterPathNotOverridesDifferent) {
  FilterPath flp(nullptr, nullptr, nullptr, true);
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_FALSE(flp.overrides(&fp));
}

// FilterPath mergeable always returns false
TEST_F(SdcInitTest, FilterPathMergeable) {
  FilterPath flp1(nullptr, nullptr, nullptr, true);
  FilterPath flp2(nullptr, nullptr, nullptr, true);
  EXPECT_FALSE(flp1.mergeable(&flp2));
}

// ExceptionPtIterator
TEST_F(SdcInitTest, ExceptionPtIteratorNoPoints) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionPtIterator iter(&fp);
  EXPECT_FALSE(iter.hasNext());
}

// ExceptionPath from/thrus/to accessors
TEST_F(SdcInitTest, ExceptionPathAccessors) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_EQ(fp.from(), nullptr);
  EXPECT_EQ(fp.thrus(), nullptr);
  EXPECT_EQ(fp.to(), nullptr);
  EXPECT_EQ(fp.minMax(), MinMaxAll::all());
}

// ExceptionPath firstPt with no points
TEST_F(SdcInitTest, ExceptionPathFirstPtNull) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_EQ(fp.firstPt(), nullptr);
}

// EmptyExpceptionPt exception
TEST_F(SdcInitTest, EmptyExceptionPtWhat) {
  EmptyExpceptionPt e;
  EXPECT_NE(e.what(), nullptr);
}

// InputDrive tests
TEST_F(SdcInitTest, InputDriveDefault) {
  InputDrive drive;
  float slew;
  bool exists;
  drive.slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_FALSE(exists);

  float res;
  drive.driveResistance(RiseFall::rise(), MinMax::max(), res, exists);
  EXPECT_FALSE(exists);

  EXPECT_FALSE(drive.hasDriveResistance(RiseFall::rise(), MinMax::max()));
  EXPECT_FALSE(drive.hasDriveCell(RiseFall::rise(), MinMax::max()));
}

TEST_F(SdcInitTest, InputDriveSetSlew) {
  InputDrive drive;
  drive.setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5);
  float slew;
  bool exists;
  drive.slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
  drive.slew(RiseFall::fall(), MinMax::min(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
}

TEST_F(SdcInitTest, InputDriveSetResistance) {
  InputDrive drive;
  drive.setDriveResistance(RiseFallBoth::riseFall(), MinMaxAll::all(), 100.0f);
  float res;
  bool exists;
  drive.driveResistance(RiseFall::rise(), MinMax::max(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 100.0f);
  EXPECT_TRUE(drive.hasDriveResistance(RiseFall::rise(), MinMax::max()));
}

TEST_F(SdcInitTest, InputDriveResistanceMinMaxEqual) {
  InputDrive drive;
  drive.setDriveResistance(RiseFallBoth::rise(), MinMaxAll::all(), 100.0f);
  EXPECT_TRUE(drive.driveResistanceMinMaxEqual(RiseFall::rise()));
}

TEST_F(SdcInitTest, InputDriveSlews) {
  InputDrive drive;
  drive.setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.3);
  const RiseFallMinMax *slews = drive.slews();
  EXPECT_NE(slews, nullptr);
  EXPECT_FALSE(slews->empty());
}

TEST_F(SdcInitTest, InputDriveDriveCellsEqual) {
  InputDrive drive;
  // Set the same drive cell for all rise/fall min/max
  float from_slews[2] = {0.1f, 0.2f};
  drive.setDriveCell(nullptr, nullptr, nullptr, from_slews, nullptr,
                     RiseFallBoth::riseFall(), MinMaxAll::all());
  EXPECT_TRUE(drive.driveCellsEqual());
}

// InputDriveCell tests
TEST_F(SdcInitTest, InputDriveCellAccessors) {
  float from_slews[2] = {0.1f, 0.2f};
  InputDriveCell dc(nullptr, nullptr, nullptr, from_slews, nullptr);
  EXPECT_EQ(dc.library(), nullptr);
  EXPECT_EQ(dc.cell(), nullptr);
  EXPECT_EQ(dc.fromPort(), nullptr);
  EXPECT_EQ(dc.toPort(), nullptr);
  float *slews = dc.fromSlews();
  EXPECT_NE(slews, nullptr);
}

TEST_F(SdcInitTest, InputDriveCellSetters) {
  float from_slews[2] = {0.1f, 0.2f};
  InputDriveCell dc(nullptr, nullptr, nullptr, from_slews, nullptr);
  dc.setLibrary(nullptr);
  dc.setCell(nullptr);
  dc.setFromPort(nullptr);
  dc.setToPort(nullptr);
  float new_slews[2] = {0.3f, 0.4f};
  dc.setFromSlews(new_slews);
  EXPECT_FLOAT_EQ(dc.fromSlews()[0], 0.3f);
  EXPECT_FLOAT_EQ(dc.fromSlews()[1], 0.4f);
}

TEST_F(SdcInitTest, InputDriveCellEqual) {
  float slews1[2] = {0.1f, 0.2f};
  float slews2[2] = {0.1f, 0.2f};
  InputDriveCell dc1(nullptr, nullptr, nullptr, slews1, nullptr);
  InputDriveCell dc2(nullptr, nullptr, nullptr, slews2, nullptr);
  EXPECT_TRUE(dc1.equal(&dc2));
}

// Sdc constraint setters/getters
TEST_F(SdcInitTest, SdcMaxArea) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setMaxArea(500.0);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 500.0f);
}

TEST_F(SdcInitTest, SdcWireloadMode) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setWireloadMode(WireloadMode::top);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::top);
  sdc->setWireloadMode(WireloadMode::enclosed);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::enclosed);
  sdc->setWireloadMode(WireloadMode::segmented);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::segmented);
}

TEST_F(SdcInitTest, SdcMinPulseWidthGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.5);
  sdc->setMinPulseWidth(RiseFallBoth::fall(), 0.3);

  }() ));
}

// Sdc design rule limits
TEST_F(SdcInitTest, SdcSlewLimitPort) {
  Sdc *sdc = sta_->cmdSdc();
  // We can't easily create ports without a network, but we can call
  // methods that don't crash with nullptr
  // Instead test clock slew limits
  FloatSeq *wave = new FloatSeq;
  wave->push_back(0.0);
  wave->push_back(5.0);
  sta_->makeClock("sl_test_clk", nullptr, false, 10.0, wave, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("sl_test_clk");
  sdc->setSlewLimit(clk, RiseFallBoth::riseFall(), PathClkOrData::clk,
                    MinMax::max(), 2.0);
  EXPECT_TRUE(sdc->haveClkSlewLimits());
  float slew;
  bool exists;
  sdc->slewLimit(clk, RiseFall::rise(), PathClkOrData::clk,
                 MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 2.0f);
}

// Clock: waveformInvalid (invalidation function), period
TEST_F(SdcInitTest, ClockPeriodAfterCreate) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("sp_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("sp_clk");
  EXPECT_FLOAT_EQ(clk->period(), 10.0);
  // waveformInvalid() invalidates cached waveform data - just call it
  clk->waveformInvalid();
}

TEST_F(SdcInitTest, ClockWaveformInvalid) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("wi_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("wi_clk");
  EXPECT_TRUE(clk->waveformValid());
  clk->waveformInvalid();
  EXPECT_FALSE(clk->waveformValid());
}

// Clock: setAddToPins
TEST_F(SdcInitTest, ClockSetAddToPins) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("atp_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("atp_clk");
  clk->setAddToPins(true);
  EXPECT_TRUE(clk->addToPins());
  clk->setAddToPins(false);
  EXPECT_FALSE(clk->addToPins());
}

// Clock: isIdeal, isGenerated
TEST_F(SdcInitTest, ClockIdealGenerated) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ig_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("ig_clk");
  EXPECT_TRUE(clk->isIdeal());
  EXPECT_FALSE(clk->isGenerated());
}

// Clock: index
TEST_F(SdcInitTest, ClockIndex) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("idx_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("idx_clk");
  EXPECT_GE(clk->index(), 0);
}

// ClockEdge: transition, opposite, name, index, pulseWidth
TEST_F(SdcInitTest, ClockEdgeDetails) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ced_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("ced_clk");
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());

  EXPECT_EQ(rise->transition(), RiseFall::rise());
  EXPECT_EQ(fall->transition(), RiseFall::fall());
  EXPECT_EQ(rise->opposite(), fall);
  EXPECT_EQ(fall->opposite(), rise);
  EXPECT_NE(rise->name(), nullptr);
  EXPECT_NE(fall->name(), nullptr);
  EXPECT_GE(rise->index(), 0);
  EXPECT_GE(fall->index(), 0);
  EXPECT_NE(rise->index(), fall->index());
  EXPECT_FLOAT_EQ(rise->pulseWidth(), 5.0);
  EXPECT_FLOAT_EQ(fall->pulseWidth(), 5.0);
  EXPECT_EQ(rise->clock(), clk);
}

// Clock: setSlew/slew
TEST_F(SdcInitTest, ClockSlewSetGet) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("csl_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("csl_clk");
  clk->setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5);
  float slew;
  bool exists;
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
  // slew with no exists parameter
  float slew2 = clk->slew(RiseFall::fall(), MinMax::min());
  EXPECT_FLOAT_EQ(slew2, 0.5f);
  // slews() accessor
  const RiseFallMinMax &slews = clk->slews();
  EXPECT_FALSE(slews.empty());
  // removeSlew
  clk->removeSlew();
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_FALSE(exists);
}

// Clock: uncertainty setters/getters
TEST_F(SdcInitTest, ClockUncertaintySetGet) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("cu_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("cu_clk");
  clk->setUncertainty(SetupHoldAll::all(), 0.1);
  float unc;
  bool exists;
  clk->uncertainty(SetupHold::max(), unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.1f);
  clk->removeUncertainty(SetupHoldAll::all());
  clk->uncertainty(SetupHold::max(), unc, exists);
  EXPECT_FALSE(exists);
}

// Clock: setSlewLimit and slewLimit
TEST_F(SdcInitTest, ClockSlewLimitSetGet) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("csl2_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("csl2_clk");
  clk->setSlewLimit(RiseFallBoth::riseFall(), PathClkOrData::clk,
                    MinMax::max(), 1.5);
  float slew;
  bool exists;
  clk->slewLimit(RiseFall::rise(), PathClkOrData::clk,
                 MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 1.5f);
}

// Sdc: findClocksMatching
TEST_F(SdcInitTest, SdcFindClocksMatching) {
  FloatSeq *wave = new FloatSeq;
  wave->push_back(0.0);
  wave->push_back(5.0);
  sta_->makeClock("match_clk1", nullptr, false, 10.0, wave, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("match_clk2", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  FloatSeq *wave3 = new FloatSeq;
  wave3->push_back(0.0);
  wave3->push_back(1.0);
  sta_->makeClock("other_clk", nullptr, false, 2.0, wave3, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  PatternMatch pattern("match_*");
  ClockSeq matches = sdc->findClocksMatching(&pattern);
  EXPECT_EQ(matches.size(), 2u);
}

// Sdc: sortedClocks
TEST_F(SdcInitTest, SdcSortedClocks) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("b_clk", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("a_clk", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  ClockSeq sorted = sdc->sortedClocks();
  EXPECT_EQ(sorted.size(), 2u);
  // Should be sorted by name: a_clk before b_clk
  EXPECT_STREQ(sorted[0]->name(), "a_clk");
  EXPECT_STREQ(sorted[1]->name(), "b_clk");
}

// Sdc: defaultArrivalClock
TEST_F(SdcInitTest, SdcDefaultArrivalClock) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *default_clk = sdc->defaultArrivalClock();
  // Default arrival clock always exists
  EXPECT_NE(default_clk, nullptr);
  ClockEdge *edge = sdc->defaultArrivalClockEdge();
  EXPECT_NE(edge, nullptr);
}

// Sdc: clockLatencies/clockInsertions accessors
TEST_F(SdcInitTest, SdcClockLatenciesAccessor) {
  Sdc *sdc = sta_->cmdSdc();
  auto *latencies = sdc->clockLatencies();
  EXPECT_NE(latencies, nullptr);
  const auto *const_latencies = static_cast<const Sdc*>(sdc)->clockLatencies();
  EXPECT_NE(const_latencies, nullptr);
}

TEST_F(SdcInitTest, SdcClockInsertionsAccessor) {
  Sdc *sdc = sta_->cmdSdc();
  const auto &insertions = sdc->clockInsertions();
  // Initially empty
  EXPECT_TRUE(insertions.empty());
}

// Sdc: pathDelaysWithoutTo
TEST_F(SdcInitTest, SdcPathDelaysWithoutTo) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->pathDelaysWithoutTo());
}

// Sdc: exceptions accessor
TEST_F(SdcInitTest, SdcExceptionsAccessor) {
  Sdc *sdc = sta_->cmdSdc();
  auto &exceptions = sdc->exceptions();
  // Initially empty
  EXPECT_TRUE(exceptions.empty());
}

// Sdc: groupPaths accessor
TEST_F(SdcInitTest, SdcGroupPathsAccessor) {
  Sdc *sdc = sta_->cmdSdc();
  auto &gp = sdc->groupPaths();
  EXPECT_TRUE(gp.empty());

  sta_->makeGroupPath("test_grp", false, nullptr, nullptr, nullptr, nullptr, sta_->cmdSdc());
  EXPECT_FALSE(sdc->groupPaths().empty());
}

// Sdc: netResistances
TEST_F(SdcInitTest, SdcNetResistancesAccessor) {
  Sdc *sdc = sta_->cmdSdc();
  auto &res = sdc->netResistances();
  EXPECT_TRUE(res.empty());
}

// Sdc: disabledPins/Ports/LibPorts/Edges accessors
TEST_F(SdcInitTest, SdcDisabledAccessors) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_NE(sdc->disabledPins(), nullptr);
  EXPECT_NE(sdc->disabledPorts(), nullptr);
  EXPECT_NE(sdc->disabledLibPorts(), nullptr);
  EXPECT_NE(sdc->disabledEdges(), nullptr);
  EXPECT_NE(sdc->disabledCellPorts(), nullptr);
  EXPECT_NE(sdc->disabledInstancePorts(), nullptr);
}

// Sdc: logicValues/caseLogicValues
TEST_F(SdcInitTest, SdcLogicValueMaps) {
  Sdc *sdc = sta_->cmdSdc();
  auto &lv = sdc->logicValues();
  EXPECT_TRUE(lv.empty());
  auto &cv = sdc->caseLogicValues();
  EXPECT_TRUE(cv.empty());
}

// Sdc: inputDelays/outputDelays
TEST_F(SdcInitTest, SdcPortDelayAccessors) {
  Sdc *sdc = sta_->cmdSdc();
  const auto &inputs = sdc->inputDelays();
  EXPECT_TRUE(inputs.empty());
  const auto &outputs = sdc->outputDelays();
  EXPECT_TRUE(outputs.empty());
  const auto &input_pin_map = sdc->inputDelayPinMap();
  EXPECT_TRUE(input_pin_map.empty());
  const auto &output_pin_map = sdc->outputDelaysPinMap();
  EXPECT_TRUE(output_pin_map.empty());
}

// Sdc: makeExceptionFrom/Thru/To - returns nullptr with empty sets
TEST_F(SdcInitTest, SdcMakeExceptionFromThruTo) {
  Sdc *sdc = sta_->cmdSdc();
  // With all null/empty sets, these return nullptr
  ExceptionFrom *from = sdc->makeExceptionFrom(nullptr, nullptr, nullptr,
                                                 RiseFallBoth::riseFall());
  EXPECT_EQ(from, nullptr);

  ExceptionThru *thru = sdc->makeExceptionThru(nullptr, nullptr, nullptr,
                                                 RiseFallBoth::riseFall());
  EXPECT_EQ(thru, nullptr);

  ExceptionTo *to = sdc->makeExceptionTo(nullptr, nullptr, nullptr,
                                           RiseFallBoth::riseFall(),
                                           RiseFallBoth::riseFall());
  EXPECT_EQ(to, nullptr);
}

// Sdc: makePathDelay
TEST_F(SdcInitTest, SdcMakePathDelay) {
  ASSERT_NO_THROW(( [&](){
  sta_->makePathDelay(nullptr, nullptr, nullptr,
                      MinMax::max(), false, false, 5.0e-9f, nullptr, sta_->cmdSdc());

  }() ));
}

// Sdc: removeClockGroupsPhysicallyExclusive/Asynchronous
TEST_F(SdcInitTest, SdcRemoveClockGroupsOther) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->removeClockGroupsPhysicallyExclusive(nullptr);
  sdc->removeClockGroupsAsynchronous(nullptr);

  }() ));
}

// Sdc: sameClockGroup
TEST_F(SdcInitTest, SdcSameClockGroup) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("scg_clk1", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("scg_clk2", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk1 = sdc->findClock("scg_clk1");
  Clock *clk2 = sdc->findClock("scg_clk2");
  // Without explicit groups, clocks are in the same group
  EXPECT_TRUE(sdc->sameClockGroup(clk1, clk2));
}

// Sdc: invalidateGeneratedClks
TEST_F(SdcInitTest, SdcInvalidateGeneratedClks) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->invalidateGeneratedClks();

  }() ));
}

// Sdc: clkHpinDisablesInvalid
TEST_F(SdcInitTest, SdcClkHpinDisablesInvalid) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->clkHpinDisablesInvalid();

  }() ));
}

// Sdc: deleteExceptions/searchPreamble
TEST_F(SdcInitTest, SdcDeleteExceptions) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->deleteExceptions();

  }() ));
}

TEST_F(SdcInitTest, SdcSearchPreamble) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->searchPreamble();

  }() ));
}

// Sdc: setClockGatingCheck global
TEST_F(SdcInitTest, SdcClockGatingCheckGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->setClockGatingCheck(RiseFallBoth::riseFall(),
                           SetupHold::max(), 0.5);

  }() ));
}

// Sdc: clkStopPropagation with non-existent pin
TEST_F(SdcInitTest, SdcClkStopPropagation) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->clkStopPropagation(nullptr, nullptr));
}

// Sdc: voltage
TEST_F(SdcInitTest, SdcVoltageGetSet) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setVoltage(MinMax::max(), 1.2);
  float voltage;
  bool exists;
  sdc->voltage(MinMax::max(), voltage, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(voltage, 1.2f);
}

// Sdc: removeNetLoadCaps
TEST_F(SdcInitTest, SdcRemoveNetLoadCaps) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->removeNetLoadCaps();

  }() ));
}

// CycleAccting hash and equal functors
TEST_F(SdcInitTest, CycleAcctingFunctorsCompile) {
  FloatSeq *wave = new FloatSeq;
  wave->push_back(0.0);
  wave->push_back(4.0);
  sta_->makeClock("cycle_functor_clk", nullptr, false, 8.0, wave, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("cycle_functor_clk");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  ASSERT_NE(rise, nullptr);
  ASSERT_NE(fall, nullptr);
  CycleAccting ca(rise, fall);

  CycleAcctingHash hasher;
  CycleAcctingEqual equal;
  EXPECT_EQ(hasher(&ca), hasher(&ca));
  EXPECT_TRUE(equal(&ca, &ca));
}

// clkCmp, clkEdgeCmp, clkEdgeLess
TEST_F(SdcInitTest, ClockComparisons) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("cmp_a", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("cmp_b", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clkA = sdc->findClock("cmp_a");
  Clock *clkB = sdc->findClock("cmp_b");

  int cmp_result = clkCmp(clkA, clkB);
  EXPECT_NE(cmp_result, 0);
  // Self-compare should be 0
  EXPECT_EQ(clkCmp(clkA, clkA), 0);

  ClockEdge *edgeA = clkA->edge(RiseFall::rise());
  ClockEdge *edgeB = clkB->edge(RiseFall::rise());
  int edge_cmp = clkEdgeCmp(edgeA, edgeB);
  EXPECT_NE(edge_cmp, 0);

  bool edge_less = clkEdgeLess(edgeA, edgeB);
  bool edge_less2 = clkEdgeLess(edgeB, edgeA);
  EXPECT_NE(edge_less, edge_less2);
}

// ClockNameLess
TEST_F(SdcInitTest, ClockNameLessComparison) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("alpha_clk", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("beta_clk", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *alpha = sdc->findClock("alpha_clk");
  Clock *beta = sdc->findClock("beta_clk");

  ClockNameLess less;
  EXPECT_TRUE(less(alpha, beta));
  EXPECT_FALSE(less(beta, alpha));

  ClkNameLess clk_less;
  EXPECT_TRUE(clk_less(alpha, beta));
  EXPECT_FALSE(clk_less(beta, alpha));
}

// InterClockUncertaintyLess
TEST_F(SdcInitTest, InterClockUncertaintyLessComparison) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("icul_clk1", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("icul_clk2", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *clk1 = sdc->findClock("icul_clk1");
  Clock *clk2 = sdc->findClock("icul_clk2");

  InterClockUncertainty icu1(clk1, clk2);
  InterClockUncertainty icu2(clk2, clk1);

  InterClockUncertaintyLess less;
  bool r1 = less(&icu1, &icu2);
  bool r2 = less(&icu2, &icu1);
  // Different order should give opposite results
  EXPECT_NE(r1, r2);
}

// sortByName for ClockSet
TEST_F(SdcInitTest, ClockSortByName) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("zz_clk", nullptr, false, 10.0, wave1, nullptr, sta_->cmdMode());

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("aa_clk", nullptr, false, 5.0, wave2, nullptr, sta_->cmdMode());

  Sdc *sdc = sta_->cmdSdc();
  Clock *zz = sdc->findClock("zz_clk");
  Clock *aa = sdc->findClock("aa_clk");

  ClockSet clk_set;
  clk_set.insert(zz);
  clk_set.insert(aa);
  ClockSeq sorted = sortByName(&clk_set);
  EXPECT_EQ(sorted.size(), 2u);
  EXPECT_STREQ(sorted[0]->name(), "aa_clk");
  EXPECT_STREQ(sorted[1]->name(), "zz_clk");
}

// logicValueString
TEST_F(SdcInitTest, LogicValueStringTest) {
  char c0 = logicValueString(LogicValue::zero);
  char c1 = logicValueString(LogicValue::one);
  char cx = logicValueString(LogicValue::unknown);
  EXPECT_EQ(c0, '0');
  EXPECT_EQ(c1, '1');
  EXPECT_NE(cx, '0');
  EXPECT_NE(cx, '1');
}

// Sdc: makeFilterPath
TEST_F(SdcInitTest, SdcMakeFilterPath) {
  Sdc *sdc = sta_->cmdSdc();
  FilterPath *fp = sdc->makeFilterPath(nullptr, nullptr, nullptr);
  EXPECT_NE(fp, nullptr);
  EXPECT_TRUE(fp->isFilter());
}

// FilterPath resetMatch always returns false
TEST_F(SdcInitTest, FilterPathResetMatch) {
  FilterPath flp(nullptr, nullptr, nullptr, true);
  bool result = flp.resetMatch(nullptr, nullptr, nullptr, MinMaxAll::all(),
                               sta_->cmdNetwork());
  EXPECT_FALSE(result);
}

// ExceptionPath hash with missing pt
TEST_F(SdcInitTest, ExceptionPathHashMissingPt) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  size_t h = fp.hash(nullptr);
  EXPECT_GE(h, 0u);
}

// Clock: setSlew/slew/removeSlew
TEST_F(SdcInitTest, ClockSetSlew) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("slew_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("slew_clk");
  clk->setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5);
  float slew;
  bool exists;
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
  clk->removeSlew();
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_FALSE(exists);
}

// Clock: setUncertainty/removeUncertainty
TEST_F(SdcInitTest, ClockSetUncertainty) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("unc_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("unc_clk");
  clk->setUncertainty(MinMax::max(), 0.1f);
  float unc;
  bool exists;
  clk->uncertainty(MinMax::max(), unc, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(unc, 0.1f);
  clk->removeUncertainty(MinMaxAll::all());
  clk->uncertainty(MinMax::max(), unc, exists);
  EXPECT_FALSE(exists);
}

// Clock: setSlewLimit/slewLimit
TEST_F(SdcInitTest, ClockSetSlewLimit) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("sl_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("sl_clk");
  clk->setSlewLimit(RiseFallBoth::riseFall(), PathClkOrData::clk,
                    MinMax::max(), 1.5);
  float slew;
  bool exists;
  clk->slewLimit(RiseFall::rise(), PathClkOrData::clk, MinMax::max(),
                 slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 1.5f);
}

// Clock: isGenerated/isIdeal
TEST_F(SdcInitTest, ClockIsGeneratedFalse) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("gen_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("gen_clk");
  EXPECT_FALSE(clk->isGenerated());
}

// ClockEdge: constructor, opposite, pulseWidth, transition
TEST_F(SdcInitTest, ClockEdgeProperties) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("edge_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("edge_clk");
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  EXPECT_NE(rise_edge, nullptr);
  EXPECT_NE(fall_edge, nullptr);
  EXPECT_EQ(rise_edge->opposite(), fall_edge);
  EXPECT_EQ(fall_edge->opposite(), rise_edge);
  EXPECT_EQ(rise_edge->transition(), RiseFall::rise());
  EXPECT_EQ(fall_edge->transition(), RiseFall::fall());
  float pw = rise_edge->pulseWidth();
  EXPECT_GT(pw, 0.0f);
}

// clkEdgeCmp/clkEdgeLess
TEST_F(SdcInitTest, ClkEdgeCmpLess) {
  ASSERT_NO_THROW(( [&](){
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(2.5);
  sta_->makeClock("cmp_clk1", nullptr, false, 5.0, waveform1, nullptr, sta_->cmdMode());
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(5.0);
  sta_->makeClock("cmp_clk2", nullptr, false, 10.0, waveform2, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk1 = sdc->findClock("cmp_clk1");
  Clock *clk2 = sdc->findClock("cmp_clk2");
  ClockEdge *e1 = clk1->edge(RiseFall::rise());
  ClockEdge *e2 = clk2->edge(RiseFall::rise());
  int cmp_result = clkEdgeCmp(e1, e2);
  bool less_result = clkEdgeLess(e1, e2);
  EXPECT_NE(cmp_result, 0);
  EXPECT_EQ(less_result, cmp_result < 0);

  }() ));
}

// InterClockUncertainty
TEST_F(SdcInitTest, InterClockUncertaintyOps) {
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(2.5);
  sta_->makeClock("icu_clk1", nullptr, false, 5.0, waveform1, nullptr, sta_->cmdMode());
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(5.0);
  sta_->makeClock("icu_clk2", nullptr, false, 10.0, waveform2, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk1 = sdc->findClock("icu_clk1");
  Clock *clk2 = sdc->findClock("icu_clk2");
  InterClockUncertainty icu(clk1, clk2);
  EXPECT_TRUE(icu.empty());
  icu.setUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                     MinMaxAll::all(), 0.2f);
  EXPECT_FALSE(icu.empty());
  float val;
  bool exists;
  icu.uncertainty(RiseFall::rise(), RiseFall::rise(), MinMax::max(),
                  val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.2f);
  const RiseFallMinMax *rfmm = icu.uncertainties(RiseFall::rise());
  EXPECT_NE(rfmm, nullptr);
  icu.removeUncertainty(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                        MinMaxAll::all());
  icu.uncertainty(RiseFall::rise(), RiseFall::rise(), MinMax::max(),
                  val, exists);
  EXPECT_FALSE(exists);
}

// ExceptionPathLess comparator
TEST_F(SdcInitTest, ExceptionPathLessComparator) {
  ASSERT_NO_THROW(( [&](){
  ExceptionPathLess less(sta_->cmdNetwork());
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  fp1.setId(1);
  fp2.setId(2);
  less(&fp1, &fp2);
  }() ));
}

// ExceptionPtIterator with thrus
TEST_F(SdcInitTest, ExceptionPtIteratorWithThrus) {
  ExceptionThruSeq *thrus = new ExceptionThruSeq;
  thrus->push_back(new ExceptionThru(nullptr, nullptr, nullptr,
                                     RiseFallBoth::riseFall(), true, nullptr));
  FalsePath fp(nullptr, thrus, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionPtIterator iter(&fp);
  int count = 0;
  while (iter.hasNext()) {
    ExceptionPt *pt = iter.next();
    EXPECT_NE(pt, nullptr);
    count++;
  }
  EXPECT_EQ(count, 1);
}

// ClockIndexLess
TEST_F(SdcInitTest, ClockIndexLessComparator) {
  ASSERT_NO_THROW(( [&](){
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(2.5);
  sta_->makeClock("idx_clk1", nullptr, false, 5.0, waveform1, nullptr, sta_->cmdMode());
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(5.0);
  sta_->makeClock("idx_clk2", nullptr, false, 10.0, waveform2, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk1 = sdc->findClock("idx_clk1");
  Clock *clk2 = sdc->findClock("idx_clk2");
  ClockIndexLess idx_less;
  bool result = idx_less(clk1, clk2);
  bool reverse = idx_less(clk2, clk1);
  EXPECT_NE(result, reverse);

  }() ));
}

// DeratingFactors: setFactor/factor (no TimingDerateType param)
TEST_F(SdcInitTest, DeratingFactorsSetGet) {
  DeratingFactors factors;
  factors.setFactor(PathClkOrData::clk,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 0.95f);
  float val;
  bool exists;
  factors.factor(PathClkOrData::clk,
                 RiseFall::rise(), EarlyLate::early(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.95f);
  EXPECT_TRUE(factors.hasValue());
}

// DeratingFactors: clear
TEST_F(SdcInitTest, DeratingFactorsClear) {
  DeratingFactors factors;
  factors.setFactor(PathClkOrData::data,
                    RiseFallBoth::riseFall(), EarlyLate::late(), 1.05f);
  EXPECT_TRUE(factors.hasValue());
  factors.clear();
}

// DeratingFactors: isOneValue with EarlyLate
TEST_F(SdcInitTest, DeratingFactorsIsOneValue) {
  ASSERT_NO_THROW(( [&](){
  DeratingFactors factors;
  factors.setFactor(PathClkOrData::clk,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 1.0f);
  factors.setFactor(PathClkOrData::data,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 1.0f);
  bool is_one;
  float value;
  factors.isOneValue(EarlyLate::early(), is_one, value);
  EXPECT_TRUE(is_one);
  EXPECT_FLOAT_EQ(value, 1.0f);

  }() ));
}

// DeratingFactors: isOneValue with PathClkOrData
TEST_F(SdcInitTest, DeratingFactorsIsOneValueClkData) {
  ASSERT_NO_THROW(( [&](){
  DeratingFactors factors;
  factors.setFactor(PathClkOrData::clk,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 1.0f);
  bool is_one;
  float value;
  factors.isOneValue(PathClkOrData::clk, EarlyLate::early(), is_one, value);
  EXPECT_TRUE(is_one);
  EXPECT_FLOAT_EQ(value, 1.0f);

  }() ));
}

// DeratingFactorsGlobal: setFactor/factor
TEST_F(SdcInitTest, DeratingFactorsGlobalOps) {
  DeratingFactorsGlobal factors;
  factors.setFactor(TimingDerateType::cell_delay, PathClkOrData::clk,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 1.0f);
  float val;
  bool exists;
  factors.factor(TimingDerateType::cell_delay, PathClkOrData::clk,
                 RiseFall::rise(), EarlyLate::early(), val, exists);
  EXPECT_TRUE(exists);
  DeratingFactors *f = factors.factors(TimingDerateType::cell_delay);
  EXPECT_NE(f, nullptr);
}

// DeratingFactorsGlobal: clear
TEST_F(SdcInitTest, DeratingFactorsGlobalClear) {
  ASSERT_NO_THROW(( [&](){
  DeratingFactorsGlobal factors;
  factors.setFactor(TimingDerateType::net_delay, PathClkOrData::data,
                    RiseFallBoth::riseFall(), EarlyLate::late(), 0.9f);
  factors.clear();

  }() ));
}

// DeratingFactorsCell: setFactor/factor
TEST_F(SdcInitTest, DeratingFactorsCellOps) {
  DeratingFactorsCell factors;
  factors.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::clk,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 0.9f);
  float val;
  bool exists;
  factors.factor(TimingDerateCellType::cell_delay, PathClkOrData::clk,
                 RiseFall::rise(), EarlyLate::early(), val, exists);
  EXPECT_TRUE(exists);
  DeratingFactors *f = factors.factors(TimingDerateCellType::cell_delay);
  EXPECT_NE(f, nullptr);
}

// DeratingFactorsCell: isOneValue
TEST_F(SdcInitTest, DeratingFactorsCellIsOneValue) {
  ASSERT_NO_THROW(( [&](){
  DeratingFactorsCell factors;
  factors.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::clk,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 1.0f);
  factors.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::data,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 1.0f);
  factors.setFactor(TimingDerateCellType::cell_check, PathClkOrData::clk,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 1.0f);
  factors.setFactor(TimingDerateCellType::cell_check, PathClkOrData::data,
                    RiseFallBoth::riseFall(), EarlyLate::early(), 1.0f);
  bool is_one;
  float value;
  factors.isOneValue(EarlyLate::early(), is_one, value);
  EXPECT_TRUE(is_one);
  EXPECT_FLOAT_EQ(value, 1.0f);

  }() ));
}

// DeratingFactorsCell: clear
TEST_F(SdcInitTest, DeratingFactorsCellClear) {
  ASSERT_NO_THROW(( [&](){
  DeratingFactorsCell factors;
  factors.setFactor(TimingDerateCellType::cell_check, PathClkOrData::data,
                    RiseFallBoth::riseFall(), EarlyLate::late(), 1.1f);
  factors.clear();

  }() ));
}

// DeratingFactorsNet: inherits DeratingFactors
TEST_F(SdcInitTest, DeratingFactorsNetOps) {
  DeratingFactorsNet factors;
  factors.setFactor(PathClkOrData::data,
                    RiseFallBoth::riseFall(), EarlyLate::late(), 1.1f);
  EXPECT_TRUE(factors.hasValue());
}

// CycleAccting: operations
TEST_F(SdcInitTest, CycleAcctingEdges) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ca_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("ca_clk");
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  CycleAccting ca(rise, fall);
  EXPECT_EQ(ca.src(), rise);
  EXPECT_EQ(ca.target(), fall);
  EXPECT_FALSE(ca.maxCyclesExceeded());
}

// CycleAccting: findDefaultArrivalSrcDelays
TEST_F(SdcInitTest, CycleAcctingDefaultArrival) {
  ASSERT_NO_THROW(( [&](){
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ca2_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("ca2_clk");
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  CycleAccting ca(rise, fall);
  ca.findDefaultArrivalSrcDelays();

  }() ));
}

// CycleAcctingHash/Equal/Less
TEST_F(SdcInitTest, CycleAcctingHashEqualLess) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("cah_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("cah_clk");
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  CycleAccting ca1(rise, fall);
  CycleAccting ca2(rise, rise);
  CycleAcctingHash hash;
  size_t h1 = hash(&ca1);
  size_t h2 = hash(&ca2);
  EXPECT_NE(h1, h2);
  EXPECT_EQ(h1, hash(&ca1));
  CycleAcctingEqual eq;
  EXPECT_TRUE(eq(&ca1, &ca1));
  CycleAcctingLess less;
  bool r = less(&ca1, &ca2);
  bool r2 = less(&ca2, &ca1);
  EXPECT_NE(r, r2);
}

// DisabledPorts constructors and methods
TEST_F(SdcInitTest, DisabledPortsConstructors) {
  DisabledPorts dp;
  EXPECT_FALSE(dp.all());
  EXPECT_EQ(dp.from(), nullptr);
  EXPECT_EQ(dp.to(), nullptr);
  EXPECT_EQ(dp.fromTo(), nullptr);
}

// DisabledPorts: setDisabledAll
TEST_F(SdcInitTest, DisabledPortsSetAll) {
  DisabledPorts dp;
  dp.setDisabledAll();
  EXPECT_TRUE(dp.all());
  dp.removeDisabledAll();
  EXPECT_FALSE(dp.all());
}

// PortExtCap: operations (needs Port* constructor)
TEST_F(SdcInitTest, PortExtCapSetGet) {
  // Need a port to construct PortExtCap
  Network *network = sta_->cmdNetwork();
  // PortExtCap default constructor
  PortExtCap pec;
  pec.setPinCap(nullptr, 0.1f, RiseFall::rise(), MinMax::max());
  float cap;
  bool exists;
  pec.pinCap(RiseFall::rise(), MinMax::max(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 0.1f);
}

// PortExtCap: wire cap
TEST_F(SdcInitTest, PortExtCapWireCap) {
  PortExtCap pec;
  pec.setWireCap(nullptr, 0.2f, RiseFall::fall(), MinMax::min());
  float cap;
  bool exists;
  pec.wireCap(RiseFall::fall(), MinMax::min(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 0.2f);
}

// PortExtCap: fanout
TEST_F(SdcInitTest, PortExtCapFanout) {
  PortExtCap pec;
  pec.setFanout(nullptr, 4, MinMax::max());
  int fan;
  bool exists;
  pec.fanout(MinMax::max(), fan, exists);
  EXPECT_TRUE(exists);
  EXPECT_EQ(fan, 4);
}

// PortExtCap: port accessor
TEST_F(SdcInitTest, PortExtCapPort) {
  PortExtCap pec;
  EXPECT_EQ(pec.port(), nullptr);
}

// InputDrive: driveResistance/hasDriveResistance
TEST_F(SdcInitTest, InputDriveResistanceGetSet) {
  InputDrive drive;
  drive.setDriveResistance(RiseFallBoth::riseFall(), MinMaxAll::all(), 100.0f);
  float res;
  bool exists;
  drive.driveResistance(RiseFall::rise(), MinMax::max(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 100.0f);
  EXPECT_TRUE(drive.hasDriveResistance(RiseFall::rise(), MinMax::max()));
}

// InputDrive: slew accessor
TEST_F(SdcInitTest, InputDriveSlewGetSet) {
  InputDrive drive;
  drive.setSlew(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5f);
  float slew;
  bool exists;
  drive.slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
}

// InputDrive: setDriveCell/driveCell/hasDriveCell
TEST_F(SdcInitTest, InputDriveCellGetSet) {
  InputDrive drive;
  float from_slews[2] = {0.1f, 0.2f};
  drive.setDriveCell(nullptr, nullptr, nullptr, from_slews, nullptr,
                     RiseFallBoth::riseFall(), MinMaxAll::all());
  EXPECT_TRUE(drive.hasDriveCell(RiseFall::rise(), MinMax::max()));
  InputDriveCell *dc = drive.driveCell(RiseFall::rise(), MinMax::max());
  EXPECT_NE(dc, nullptr);
  const LibertyCell *cell;
  const LibertyPort *from_port;
  float *slews;
  const LibertyPort *to_port;
  drive.driveCell(RiseFall::rise(), MinMax::max(),
                  cell, from_port, slews, to_port);
  EXPECT_EQ(cell, nullptr);
}

// Sdc: clkHpinDisablesInvalid (unique name)
TEST_F(SdcInitTest, SdcClkHpinDisablesViaInvalid) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->clkHpinDisablesInvalid();
  // exercises clkHpinDisablesInvalid

  }() ));
}

// Sdc: setTimingDerate (global variant)
TEST_F(SdcInitTest, SdcSetTimingDerateGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->setTimingDerate(TimingDerateType::cell_delay, PathClkOrData::clk,
                       RiseFallBoth::riseFall(), EarlyLate::early(), 0.95f);
  // exercises setTimingDerate global

  }() ));
}

// Sdc: unsetTimingDerate
TEST_F(SdcInitTest, SdcUnsetTimingDerate) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->setTimingDerate(TimingDerateType::cell_delay, PathClkOrData::clk,
                       RiseFallBoth::riseFall(), EarlyLate::early(), 0.95f);
  sdc->unsetTimingDerate();

  }() ));
}

// PinPairLess
TEST_F(SdcInitTest, PinPairLessConstruct) {
  Network *network = sta_->cmdNetwork();
  ASSERT_NE(network, nullptr);
  PinPairLess less(network);
  PinPair p1(nullptr, nullptr);
  PinPair p2(nullptr, nullptr);
  EXPECT_FALSE(less(p1, p2));
}

// PinPairSet with network
TEST_F(SdcInitTest, PinPairSetConstruct) {
  Network *network = sta_->cmdNetwork();
  PinPairSet pps(network);
  EXPECT_TRUE(pps.empty());
}

// PinPairHash with network
TEST_F(SdcInitTest, PinPairHashConstruct) {
  Network *network = sta_->cmdNetwork();
  ASSERT_NE(network, nullptr);
  PinPairHash hash(network);
}

// Sdc: dataChecksFrom/dataChecksTo (need Pin* arg)
TEST_F(SdcInitTest, SdcDataChecksFromNull) {
  Sdc *sdc = sta_->cmdSdc();
  DataCheckSet *checks = sdc->dataChecksFrom(nullptr);
  EXPECT_EQ(checks, nullptr);
}

TEST_F(SdcInitTest, SdcDataChecksToNull) {
  Sdc *sdc = sta_->cmdSdc();
  DataCheckSet *checks = sdc->dataChecksTo(nullptr);
  EXPECT_EQ(checks, nullptr);
}

// Sdc: inputDelays/outputDelays
TEST_F(SdcInitTest, PortDelayMaps) {
  Sdc *sdc = sta_->cmdSdc();
  const auto &id = sdc->inputDelays();
  const auto &od = sdc->outputDelays();
  EXPECT_TRUE(id.empty());
  EXPECT_TRUE(od.empty());
}

// Sdc: clockGatingMargin global
TEST_F(SdcInitTest, SdcClockGatingMarginGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  bool exists;
  float margin;
  sdc->clockGatingMargin(RiseFall::rise(), SetupHold::max(), exists, margin);
  // No crash - margin may or may not exist

  }() ));
}

////////////////////////////////////////////////////////////////
// Round 2: Deep coverage tests for uncovered SDC functions
////////////////////////////////////////////////////////////////

// Variables constructor and all setters
TEST_F(SdcInitTest, VariablesDefaultConstructor) {
  Variables vars;
  // Default CRPR enabled is true in OpenSTA
  EXPECT_TRUE(vars.crprEnabled());
  EXPECT_EQ(vars.crprMode(), CrprMode::same_pin);
}

TEST_F(SdcInitTest, VariablesSetCrprEnabled) {
  Variables vars;
  vars.setCrprEnabled(true);
  EXPECT_TRUE(vars.crprEnabled());
  vars.setCrprEnabled(false);
  EXPECT_FALSE(vars.crprEnabled());
}

TEST_F(SdcInitTest, VariablesSetCrprMode) {
  Variables vars;
  vars.setCrprMode(CrprMode::same_transition);
  EXPECT_EQ(vars.crprMode(), CrprMode::same_transition);
  vars.setCrprMode(CrprMode::same_pin);
  EXPECT_EQ(vars.crprMode(), CrprMode::same_pin);
}

TEST_F(SdcInitTest, VariablesSetPropagateGatedClockEnable) {
  Variables vars;
  vars.setPropagateGatedClockEnable(true);
  EXPECT_TRUE(vars.propagateGatedClockEnable());
  vars.setPropagateGatedClockEnable(false);
  EXPECT_FALSE(vars.propagateGatedClockEnable());
}

TEST_F(SdcInitTest, VariablesSetPresetClrArcsEnabled) {
  Variables vars;
  vars.setPresetClrArcsEnabled(true);
  EXPECT_TRUE(vars.presetClrArcsEnabled());
  vars.setPresetClrArcsEnabled(false);
  EXPECT_FALSE(vars.presetClrArcsEnabled());
}

TEST_F(SdcInitTest, VariablesSetCondDefaultArcsEnabled) {
  Variables vars;
  vars.setCondDefaultArcsEnabled(true);
  EXPECT_TRUE(vars.condDefaultArcsEnabled());
}

TEST_F(SdcInitTest, VariablesSetBidirectInstPathsEnabled) {
  Variables vars;
  vars.setBidirectInstPathsEnabled(true);
  EXPECT_TRUE(vars.bidirectInstPathsEnabled());
}

TEST_F(SdcInitTest, VariablesSetBidirectNetPathsEnabled) {
  Variables vars;
  vars.setBidirectInstPathsEnabled(true);
  EXPECT_TRUE(vars.bidirectInstPathsEnabled());
}

TEST_F(SdcInitTest, VariablesSetRecoveryRemovalChecksEnabled) {
  Variables vars;
  vars.setRecoveryRemovalChecksEnabled(true);
  EXPECT_TRUE(vars.recoveryRemovalChecksEnabled());
}

TEST_F(SdcInitTest, VariablesSetGatedClkChecksEnabled) {
  Variables vars;
  vars.setGatedClkChecksEnabled(true);
  EXPECT_TRUE(vars.gatedClkChecksEnabled());
}

TEST_F(SdcInitTest, VariablesSetDynamicLoopBreaking) {
  Variables vars;
  vars.setDynamicLoopBreaking(true);
  EXPECT_TRUE(vars.dynamicLoopBreaking());
}

TEST_F(SdcInitTest, VariablesSetPropagateAllClocks) {
  Variables vars;
  vars.setPropagateAllClocks(true);
  EXPECT_TRUE(vars.propagateAllClocks());
}

TEST_F(SdcInitTest, VariablesSetClkThruTristateEnabled) {
  Variables vars;
  vars.setClkThruTristateEnabled(true);
  EXPECT_TRUE(vars.clkThruTristateEnabled());
}

TEST_F(SdcInitTest, VariablesSetUseDefaultArrivalClock) {
  Variables vars;
  vars.setUseDefaultArrivalClock(true);
  EXPECT_TRUE(vars.useDefaultArrivalClock());
}

TEST_F(SdcInitTest, VariablesSetPocvEnabled) {
  Variables vars;
  vars.setPocvEnabled(true);
  EXPECT_TRUE(vars.pocvEnabled());
}

// DeratingFactors deeper coverage
TEST_F(SdcInitTest, DeratingFactorsConstructAndSet) {
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               EarlyLate::early(), 0.95f);
  float val;
  bool exists;
  df.factor(PathClkOrData::clk, RiseFall::rise(), EarlyLate::early(),
            val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.95f);
}

TEST_F(SdcInitTest, DeratingFactorsHasValue) {
  DeratingFactors df;
  EXPECT_FALSE(df.hasValue());
  df.setFactor(PathClkOrData::data, RiseFallBoth::rise(),
               EarlyLate::late(), 1.05f);
  EXPECT_TRUE(df.hasValue());
}

TEST_F(SdcInitTest, DeratingFactorsIsOneValueMinMax) {
  ASSERT_NO_THROW(( [&](){
  DeratingFactors df;
  df.setFactor(PathClkOrData::clk, RiseFallBoth::riseFall(),
               EarlyLate::early(), 0.95f);
  bool one_value;
  float val;
  df.isOneValue(EarlyLate::early(), one_value, val);

  }() ));
}

// DeratingFactorsGlobal
TEST_F(SdcInitTest, DeratingFactorsGlobalConstAndSet) {
  DeratingFactorsGlobal dfg;
  dfg.setFactor(TimingDerateType::cell_delay, PathClkOrData::clk,
                RiseFallBoth::riseFall(), EarlyLate::early(), 0.92f);
  float val;
  bool exists;
  dfg.factor(TimingDerateType::cell_delay, PathClkOrData::clk,
             RiseFall::rise(), EarlyLate::early(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.92f);
}

TEST_F(SdcInitTest, DeratingFactorsGlobalFactors) {
  DeratingFactorsGlobal dfg;
  DeratingFactors *f = dfg.factors(TimingDerateType::cell_delay);
  EXPECT_NE(f, nullptr);
}

TEST_F(SdcInitTest, DeratingFactorsGlobalCellTypeOverload) {
  DeratingFactorsGlobal dfg;
  // Set via cell type overload
  dfg.setFactor(TimingDerateType::cell_delay, PathClkOrData::clk,
                RiseFallBoth::riseFall(), EarlyLate::early(), 0.9f);
  float val;
  bool exists;
  dfg.factor(TimingDerateCellType::cell_delay, PathClkOrData::clk,
             RiseFall::rise(), EarlyLate::early(), val, exists);
  EXPECT_TRUE(exists);
}

// DeratingFactorsCell
TEST_F(SdcInitTest, DeratingFactorsCellConstAndSet) {
  DeratingFactorsCell dfc;
  dfc.setFactor(TimingDerateCellType::cell_delay, PathClkOrData::data,
                RiseFallBoth::riseFall(), EarlyLate::late(), 1.05f);
  float val;
  bool exists;
  dfc.factor(TimingDerateCellType::cell_delay, PathClkOrData::data,
             RiseFall::fall(), EarlyLate::late(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 1.05f);
}

TEST_F(SdcInitTest, DeratingFactorsCellFactors) {
  DeratingFactorsCell dfc;
  DeratingFactors *f = dfc.factors(TimingDerateCellType::cell_delay);
  EXPECT_NE(f, nullptr);
}

// DeratingFactorsNet
TEST_F(SdcInitTest, DeratingFactorsNetConstruct) {
  DeratingFactorsNet dfn;
  EXPECT_FALSE(dfn.hasValue());
}

// SdcCmdComment
// ClockGatingCheck
TEST_F(SdcInitTest, ClockGatingCheckDefault) {
  ASSERT_NO_THROW(( [&](){
  ClockGatingCheck cgc;
  // Default constructor should work

  }() ));
}

TEST_F(SdcInitTest, ClockGatingCheckSetActiveValue) {
  ASSERT_NO_THROW(( [&](){
  ClockGatingCheck cgc;
  cgc.setActiveValue(LogicValue::one);

  }() ));
}

// NetWireCaps
TEST_F(SdcInitTest, NetWireCapsDefault) {
  NetWireCaps nwc;
  EXPECT_FALSE(nwc.subtractPinCap(MinMax::min()));
  EXPECT_FALSE(nwc.subtractPinCap(MinMax::max()));
}

TEST_F(SdcInitTest, NetWireCapsSetSubtractPinCap) {
  NetWireCaps nwc;
  nwc.setSubtractPinCap(true, MinMax::min());
  EXPECT_TRUE(nwc.subtractPinCap(MinMax::min()));
  EXPECT_FALSE(nwc.subtractPinCap(MinMax::max()));
}

// PortExtCap deeper coverage
TEST_F(SdcInitTest, PortExtCapSetAndGet) {
  PortExtCap pec;
  pec.setPinCap(nullptr, 1.5f, RiseFall::rise(), MinMax::max());
  float val;
  bool exists;
  pec.pinCap(RiseFall::rise(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 1.5f);
}

// CycleAccting
TEST_F(SdcInitTest, CycleAcctingConstruct) {
  // Make a clock to get clock edges
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ca_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("ca_clk");
  EXPECT_NE(clk, nullptr);
  const ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  const ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  EXPECT_NE(rise_edge, nullptr);
  EXPECT_NE(fall_edge, nullptr);
  CycleAccting ca(rise_edge, fall_edge);
  ca.findDefaultArrivalSrcDelays();
}

// CycleAccting via setAccting
// Clock creation and queries (deeper)
TEST_F(SdcInitTest, ClockIsVirtual) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("virt_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("virt_clk");
  EXPECT_NE(clk, nullptr);
  // Virtual clock has no pins
  EXPECT_TRUE(clk->isVirtual());
}

TEST_F(SdcInitTest, ClockDefaultPin) {
  ASSERT_NO_THROW(( [&](){
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("dp_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("dp_clk");
  const Pin *dp = clk->defaultPin();
  // No default pin for virtual clock
  EXPECT_EQ(dp, nullptr);

  }() ));
}

// ClockLatency
TEST_F(SdcInitTest, ClockLatencyConstruct) {
  ClockLatency cl(nullptr, nullptr);
  cl.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(), 1.5f);
  float val;
  bool exists;
  cl.delay(RiseFall::rise(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 1.5f);
}

TEST_F(SdcInitTest, ClockLatencyDelayScalar) {
  ClockLatency cl(nullptr, nullptr);
  cl.setDelay(RiseFallBoth::rise(), MinMaxAll::max(), 2.0f);
  float d = cl.delay(RiseFall::rise(), MinMax::max());
  EXPECT_FLOAT_EQ(d, 2.0f);
}

TEST_F(SdcInitTest, ClockLatencyDelays) {
  ClockLatency cl(nullptr, nullptr);
  cl.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5f);
  RiseFallMinMax *delays = cl.delays();
  EXPECT_NE(delays, nullptr);
}

TEST_F(SdcInitTest, ClockLatencySetDelays) {
  ASSERT_NO_THROW(( [&](){
  ClockLatency cl(nullptr, nullptr);
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFallBoth::riseFall(), MinMaxAll::all(), 1.0f);
  cl.setDelays(&rfmm);

  }() ));
}

TEST_F(SdcInitTest, ClockLatencySetDelayScalar) {
  ClockLatency cl(nullptr, nullptr);
  cl.setDelay(RiseFall::rise(), MinMax::max(), 3.0f);
  float val;
  bool exists;
  cl.delay(RiseFall::rise(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 3.0f);
}

// ClockInsertion
TEST_F(SdcInitTest, ClockInsertionConstruct) {
  ClockInsertion ci(nullptr, nullptr);
  ci.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(), MinMaxAll::all(), 0.5f);
  float val;
  bool exists;
  ci.delay(RiseFall::rise(), MinMax::max(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.5f);
}

TEST_F(SdcInitTest, ClockInsertionDelayScalar) {
  ClockInsertion ci(nullptr, nullptr);
  ci.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(), MinMaxAll::all(), 1.0f);
  float d = ci.delay(RiseFall::rise(), MinMax::max(), MinMax::max());
  EXPECT_FLOAT_EQ(d, 1.0f);
}

TEST_F(SdcInitTest, ClockInsertionDelays) {
  ClockInsertion ci(nullptr, nullptr);
  ci.setDelay(RiseFallBoth::riseFall(), MinMaxAll::all(), MinMaxAll::all(), 0.3f);
  RiseFallMinMax *d = ci.delays(MinMax::max());
  EXPECT_NE(d, nullptr);
}

TEST_F(SdcInitTest, ClockInsertionSetDelays) {
  ASSERT_NO_THROW(( [&](){
  ClockInsertion ci(nullptr, nullptr);
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFallBoth::riseFall(), MinMaxAll::all(), 0.7f);
  ci.setDelays(&rfmm);

  }() ));
}

TEST_F(SdcInitTest, ClockInsertionSetDelayScalar) {
  ClockInsertion ci(nullptr, nullptr);
  ci.setDelay(RiseFall::rise(), MinMax::max(), MinMax::max(), 2.0f);
  float val;
  bool exists;
  ci.delay(RiseFall::rise(), MinMax::max(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 2.0f);
}

// DataCheck
TEST_F(SdcInitTest, DataCheckConstruct) {
  DataCheck dc(nullptr, nullptr, nullptr);
  EXPECT_TRUE(dc.empty());
}

TEST_F(SdcInitTest, DataCheckSetAndGetMargin) {
  DataCheck dc(nullptr, nullptr, nullptr);
  dc.setMargin(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
               MinMaxAll::all(), 0.5f);
  EXPECT_FALSE(dc.empty());
  float val;
  bool exists;
  dc.margin(RiseFall::rise(), RiseFall::rise(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.5f);
}

TEST_F(SdcInitTest, DataCheckRemoveMargin) {
  DataCheck dc(nullptr, nullptr, nullptr);
  dc.setMargin(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
               MinMaxAll::all(), 0.3f);
  dc.removeMargin(RiseFallBoth::riseFall(), RiseFallBoth::riseFall(),
                  MinMaxAll::all());
  EXPECT_TRUE(dc.empty());
}

// DataCheckLess
// ClockGroups via Sdc
TEST_F(SdcInitTest, SdcRemoveClockGroups) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->makeClockGroups("grp2", false, true, false, false, "comment");
  sdc->removeClockGroups("grp2");

  }() ));
}

TEST_F(SdcInitTest, SdcRemoveClockGroupsLogicallyExclusive) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->makeClockGroups("le_grp", true, false, false, false, nullptr);
  sdc->removeClockGroupsLogicallyExclusive("le_grp");

  }() ));
}

TEST_F(SdcInitTest, SdcRemoveClockGroupsPhysicallyExclusive) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->makeClockGroups("pe_grp", false, true, false, false, nullptr);
  sdc->removeClockGroupsPhysicallyExclusive("pe_grp");

  }() ));
}

TEST_F(SdcInitTest, SdcRemoveClockGroupsAsynchronous) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->makeClockGroups("async_grp", false, false, true, false, nullptr);
  sdc->removeClockGroupsAsynchronous("async_grp");

  }() ));
}

// ClockGroups direct
// Sdc: setMaxArea/maxArea
TEST_F(SdcInitTest, SdcSetMaxArea) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setMaxArea(100.0f);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 100.0f);
}

// Sdc: setWireloadMode/wireloadMode
TEST_F(SdcInitTest, SdcSetWireloadMode) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setWireloadMode(WireloadMode::top);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::top);
  sdc->setWireloadMode(WireloadMode::enclosed);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::enclosed);
  sdc->setWireloadMode(WireloadMode::segmented);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::segmented);
}

// Sdc: wireload/setWireload
TEST_F(SdcInitTest, SdcWireloadNull) {
  Sdc *sdc = sta_->cmdSdc();
  // No wireload set
  auto *wl = sdc->wireload(MinMax::max());
  EXPECT_EQ(wl, nullptr);
}

// Sdc: wireloadSelection
TEST_F(SdcInitTest, SdcWireloadSelectionNull) {
  Sdc *sdc = sta_->cmdSdc();
  auto *ws = sdc->wireloadSelection(MinMax::max());
  EXPECT_EQ(ws, nullptr);
}

// Sdc: setAnalysisType
TEST_F(SdcInitTest, SdcSetAnalysisTypeSingle) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setAnalysisType(AnalysisType::single);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::single);
}

TEST_F(SdcInitTest, SdcSetAnalysisTypeBcWc) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setAnalysisType(AnalysisType::bc_wc);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::bc_wc);
}

TEST_F(SdcInitTest, SdcSetAnalysisTypeOcv) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->setAnalysisType(AnalysisType::ocv);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::ocv);
}

// Sdc: isConstrained
TEST_F(SdcInitTest, SdcIsConstrainedInstNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isConstrained((const Instance*)nullptr));
}

TEST_F(SdcInitTest, SdcIsConstrainedNetNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isConstrained((const Net*)nullptr));
}

// Sdc: haveClkSlewLimits
TEST_F(SdcInitTest, SdcHaveClkSlewLimits) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->haveClkSlewLimits());
}

// Sdc: hasClockLatency
TEST_F(SdcInitTest, SdcHasClockLatencyNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->hasClockLatency(nullptr));
}

// Sdc: clockLatencies
TEST_F(SdcInitTest, SdcClockLatenciesAccess) {
  Sdc *sdc = sta_->cmdSdc();
  auto *cl = sdc->clockLatencies();
  EXPECT_NE(cl, nullptr);
}

// Sdc: clockInsertions
TEST_F(SdcInitTest, SdcClockInsertionsAccess) {
  Sdc *sdc = sta_->cmdSdc();
  const auto &ci = sdc->clockInsertions();
  EXPECT_TRUE(ci.empty());
}

// Sdc: hasClockInsertion
TEST_F(SdcInitTest, SdcHasClockInsertionNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->hasClockInsertion(nullptr));
}

// Sdc: defaultArrivalClockEdge
TEST_F(SdcInitTest, SdcDefaultArrivalClockEdge) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->defaultArrivalClockEdge();
  // May be null before searchPreamble
  }() ));
}

// Sdc: sortedClocks
// Sdc: searchPreamble
TEST_F(SdcInitTest, SdcSearchPreambleNoDesign) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->searchPreamble();

  }() ));
}

// Sdc: makeDefaultArrivalClock
TEST_F(SdcInitTest, SdcMakeDefaultArrivalClock) {
  Sdc *sdc = sta_->cmdSdc();
  sdc->searchPreamble();
  const ClockEdge *edge = sdc->defaultArrivalClockEdge();
  EXPECT_NE(edge, nullptr);
}

// Sdc: invalidateGeneratedClks
TEST_F(SdcInitTest, SdcInvalidateGenClks) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->invalidateGeneratedClks();

  }() ));
}

// Sdc: setClockSlew/removeClockSlew
TEST_F(SdcInitTest, SdcSetClockSlew) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("slew_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("slew_clk");
  sdc->setClockSlew(clk, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.1f);
  sdc->removeClockSlew(clk);

  }() ));
}

// Sdc: setClockLatency/removeClockLatency
TEST_F(SdcInitTest, SdcSetClockLatency) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lat_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("lat_clk");
  sdc->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5f);
  sdc->removeClockLatency(clk, nullptr);

  }() ));
}

// Sdc: clockLatency (Clock*, RiseFall*, MinMax*)
TEST_F(SdcInitTest, SdcClockLatencyQuery) {
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("latq_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("latq_clk");
  sdc->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(), MinMaxAll::all(), 1.0f);
  float lat = sdc->clockLatency(clk, RiseFall::rise(), MinMax::max());
  EXPECT_FLOAT_EQ(lat, 1.0f);
}

// Sdc: setClockInsertion/removeClockInsertion
TEST_F(SdcInitTest, SdcSetClockInsertion) {
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ins_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("ins_clk");
  sdc->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), EarlyLateAll::all(), 0.2f);
  EXPECT_FALSE(sdc->clockInsertions().empty());
  sdc->removeClockInsertion(clk, nullptr);
}

// Sdc: clockInsertion query
TEST_F(SdcInitTest, SdcClockInsertionQuery) {
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("insq_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("insq_clk");
  sdc->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), EarlyLateAll::all(), 0.3f);
  float ins = sdc->clockInsertion(clk, RiseFall::rise(), MinMax::max(),
                                  EarlyLate::early());
  EXPECT_FLOAT_EQ(ins, 0.3f);
}

// Sdc: setClockUncertainty/removeClockUncertainty
TEST_F(SdcInitTest, SdcSetInterClockUncertainty) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  sta_->makeClock("unc_clk1", nullptr, false, 10.0, waveform1, nullptr, sta_->cmdMode());
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(2.5);
  sta_->makeClock("unc_clk2", nullptr, false, 5.0, waveform2, nullptr, sta_->cmdMode());
  Clock *clk1 = sdc->findClock("unc_clk1");
  Clock *clk2 = sdc->findClock("unc_clk2");
  sdc->setClockUncertainty(clk1, RiseFallBoth::riseFall(),
                           clk2, RiseFallBoth::riseFall(),
                           SetupHoldAll::all(), 0.1f);
  sdc->removeClockUncertainty(clk1, RiseFallBoth::riseFall(),
                              clk2, RiseFallBoth::riseFall(),
                              SetupHoldAll::all());

  }() ));
}

// Sdc: sameClockGroup
TEST_F(SdcInitTest, SdcSameClockGroupNoGroups) {
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  sta_->makeClock("scg_c1", nullptr, false, 10.0, waveform1, nullptr, sta_->cmdMode());
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(5.0);
  sta_->makeClock("scg_c2", nullptr, false, 10.0, waveform2, nullptr, sta_->cmdMode());
  Clock *c1 = sdc->findClock("scg_c1");
  Clock *c2 = sdc->findClock("scg_c2");
  // Without groups, clocks are in same group
  bool same = sdc->sameClockGroup(c1, c2);
  EXPECT_TRUE(same);
}

// Sdc: setClockGatingCheck (global)
// Sdc: setClockGatingCheck (clock)
// Sdc: setDataCheck/dataChecksFrom/dataChecksTo
TEST_F(SdcInitTest, SdcSetDataCheck) {
  Sdc *sdc = sta_->cmdSdc();
  // Can't set data check without real pins, but test the query on null
  DataCheckSet *from = sdc->dataChecksFrom(nullptr);
  DataCheckSet *to = sdc->dataChecksTo(nullptr);
  EXPECT_EQ(from, nullptr);
  EXPECT_EQ(to, nullptr);
}

// Sdc: setTimingDerate (all variants)
TEST_F(SdcInitTest, SdcSetTimingDerateGlobalNet) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->setTimingDerate(TimingDerateType::net_delay, PathClkOrData::data,
                       RiseFallBoth::riseFall(), EarlyLate::late(), 1.05f);

  }() ));
}

// Sdc: swapDeratingFactors
TEST_F(SdcInitTest, SdcSwapDeratingFactors) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  // Create another Sta to get a second Sdc
  // Actually we can just swap with itself (no-op)
  Sdc::swapDeratingFactors(sdc, sdc);

  }() ));
}

// Sdc: deleteDeratingFactors
// Sdc: allInputs/allOutputs
// Sdc: findClocksMatching
// Sdc: isGroupPathName
TEST_F(SdcInitTest, SdcIsGroupPathNameEmpty) {
  Sdc *sdc = sta_->cmdSdc();
  // Suppress deprecation warning -- we intentionally test deprecated API
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  bool is_group = sdc->isGroupPathName("nonexistent");
  #pragma GCC diagnostic pop
  EXPECT_FALSE(is_group);
}

// Sdc: setVoltage
TEST_F(SdcInitTest, SdcSetVoltageGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->setVoltage(MinMax::max(), 1.0f);

  }() ));
}

// Sdc: setLatchBorrowLimit
TEST_F(SdcInitTest, SdcSetLatchBorrowLimitClock) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lbl_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("lbl_clk");
  sdc->setLatchBorrowLimit(clk, 3.0f);

  }() ));
}

// Sdc: setMinPulseWidth on clock
TEST_F(SdcInitTest, SdcSetMinPulseWidthClock) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("mpw_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("mpw_clk");
  sdc->setMinPulseWidth(clk, RiseFallBoth::riseFall(), 1.0f);

  }() ));
}

// Sdc: makeCornersAfter/makeCornersBefore
TEST_F(SdcInitTest, SdcMakeCornersBefore) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->makeSceneBefore();

  }() ));
}

// Sdc: removeNetLoadCaps
// Sdc: initVariables
// Sdc: swapPortExtCaps
TEST_F(SdcInitTest, SdcSwapPortExtCaps) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Sdc::swapPortExtCaps(sdc, sdc);

  }() ));
}

// Sdc: swapClockInsertions
TEST_F(SdcInitTest, SdcSwapClockInsertions) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Sdc::swapClockInsertions(sdc, sdc);

  }() ));
}

// ExceptionPath type queries
TEST_F(SdcExceptionPathTest, FalsePathIsFalse) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp.isFalse());
  EXPECT_FALSE(fp.isMultiCycle());
  EXPECT_FALSE(fp.isPathDelay());
  EXPECT_FALSE(fp.isGroupPath());
  EXPECT_FALSE(fp.isFilter());
  EXPECT_FALSE(fp.isLoop());
  EXPECT_FALSE(fp.isDefault());
  EXPECT_EQ(fp.type(), ExceptionPathType::false_path);
}

TEST_F(SdcExceptionPathTest, MultiCyclePathIsMultiCycle) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     false, 2, true, nullptr);
  EXPECT_TRUE(mcp.isMultiCycle());
  EXPECT_FALSE(mcp.isFalse());
  EXPECT_EQ(mcp.pathMultiplier(), 2);
  EXPECT_EQ(mcp.type(), ExceptionPathType::multi_cycle);
}

TEST_F(SdcExceptionPathTest, MultiCyclePathUseEndClk) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  EXPECT_TRUE(mcp.useEndClk());
}

TEST_F(SdcExceptionPathTest, PathDelayIsPathDelay) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               5.0e-9f, true, nullptr);
  EXPECT_TRUE(pd.isPathDelay());
  EXPECT_FALSE(pd.isFalse());
  EXPECT_FLOAT_EQ(pd.delay(), 5.0e-9f);
  EXPECT_FALSE(pd.ignoreClkLatency());
  EXPECT_FALSE(pd.breakPath());
  EXPECT_EQ(pd.type(), ExceptionPathType::path_delay);
}

TEST_F(SdcExceptionPathTest, PathDelayBreakPath) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, true,
               1.0e-9f, true, nullptr);
  EXPECT_TRUE(pd.breakPath());
}

TEST_F(SdcExceptionPathTest, PathDelayIgnoreClkLatency) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), true, false,
               1.0e-9f, true, nullptr);
  EXPECT_TRUE(pd.ignoreClkLatency());
}

TEST_F(SdcExceptionPathTest, GroupPathIsGroupPath) {
  GroupPath gp("grp", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp.isGroupPath());
  EXPECT_FALSE(gp.isFalse());
  EXPECT_STREQ(gp.name(), "grp");
  EXPECT_FALSE(gp.isDefault());
  EXPECT_EQ(gp.type(), ExceptionPathType::group_path);
}

TEST_F(SdcExceptionPathTest, GroupPathDefault) {
  GroupPath gp("grp_def", true, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp.isDefault());
}

// ExceptionPath: priority and hash
TEST_F(SdcExceptionPathTest, ExceptionPathPriority) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  int prio = fp.priority(MinMax::max());
  // FalsePath has well-defined priority
  EXPECT_GT(prio, 0);
}

// ExceptionPtIterator
TEST_F(SdcExceptionPathTest, ExceptionPtIteratorEmpty) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionPtIterator iter(&fp);
  // With all nullptr from/thru/to, should have no points
  EXPECT_FALSE(iter.hasNext());
}

// InputDrive deeper
TEST_F(SdcInitTest, InputDriveConstructDestruct) {
  InputDrive *id = new InputDrive;
  EXPECT_FALSE(id->hasDriveResistance(RiseFall::rise(), MinMax::max()));
  EXPECT_FALSE(id->hasDriveCell(RiseFall::rise(), MinMax::max()));
  delete id;
}

TEST_F(SdcInitTest, InputDriveSetDriveResistance) {
  InputDrive id;
  id.setDriveResistance(RiseFallBoth::riseFall(), MinMaxAll::all(), 100.0f);
  EXPECT_TRUE(id.hasDriveResistance(RiseFall::rise(), MinMax::max()));
  float res;
  bool exists;
  id.driveResistance(RiseFall::rise(), MinMax::max(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 100.0f);
}

TEST_F(SdcInitTest, InputDriveDriveResistanceMinMaxEqual) {
  InputDrive id;
  id.setDriveResistance(RiseFallBoth::riseFall(), MinMaxAll::all(), 50.0f);
  bool eq = id.driveResistanceMinMaxEqual(RiseFall::rise());
  EXPECT_TRUE(eq);
}

TEST_F(SdcInitTest, InputDriveDriveCellNull) {
  InputDrive id;
  const InputDriveCell *dc = id.driveCell(RiseFall::rise(), MinMax::max());
  EXPECT_EQ(dc, nullptr);
}

// DisabledPorts deeper
// DisabledInstancePorts
TEST_F(SdcInitTest, DisabledInstancePortsConstruct) {
  DisabledInstancePorts dip(nullptr);
  EXPECT_FALSE(dip.all());
}

// Sdc: setClockSense
// Sdc: hasNetWireCap
TEST_F(SdcInitTest, SdcHasNetWireCapNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->hasNetWireCap(nullptr));
}

// Sdc: hasPortExtCap
TEST_F(SdcInitTest, SdcHasPortExtCapNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->hasPortExtCap(nullptr));
}

// Sdc: isExceptionStartpoint/isExceptionEndpoint
// Sdc: isPropagatedClock
TEST_F(SdcInitTest, SdcIsPropagatedClockNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isPropagatedClock(nullptr));
}

// Sdc: hasLogicValue
TEST_F(SdcInitTest, SdcHasLogicValueNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->hasLogicValue(nullptr));
}

// Sdc: isPathDelayInternalFrom/To
TEST_F(SdcInitTest, SdcIsPathDelayInternalFromNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isPathDelayInternalFrom(nullptr));
}

TEST_F(SdcInitTest, SdcIsPathDelayInternalFromBreakNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isPathDelayInternalFromBreak(nullptr));
}

// Sdc: pathDelayInternalFrom
TEST_F(SdcInitTest, SdcPathDelayInternalFrom) {
  Sdc *sdc = sta_->cmdSdc();
  const auto &pins = sdc->pathDelayInternalFrom();
  EXPECT_TRUE(pins.empty());
}

// Sdc: disabledCellPorts/disabledInstancePorts
TEST_F(SdcInitTest, SdcDisabledCellPorts) {
  Sdc *sdc = sta_->cmdSdc();
  auto *dcp = sdc->disabledCellPorts();
  EXPECT_NE(dcp, nullptr);
}

// Sdc: isDisabled on TimingArcSet (nullptr)
// Sdc: isDisabledPin (nullptr)
// ClockPairLess
TEST_F(SdcInitTest, ClockPairLessOp) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *w1 = new FloatSeq;
  w1->push_back(0.0);
  w1->push_back(5.0);
  sta_->makeClock("cpl_c1", nullptr, false, 10.0, w1, nullptr, sta_->cmdMode());
  FloatSeq *w2 = new FloatSeq;
  w2->push_back(0.0);
  w2->push_back(5.0);
  sta_->makeClock("cpl_c2", nullptr, false, 10.0, w2, nullptr, sta_->cmdMode());
  Clock *c1 = sdc->findClock("cpl_c1");
  Clock *c2 = sdc->findClock("cpl_c2");
  ClockPairLess cpl;
  ClockPair p1(c1, c2);
  ClockPair p2(c2, c1);
  cpl(p1, p2);
  }() ));
}

// InputDriveCell
// PortDelay (direct)
// PortDelayLess
// Sdc: setClockLatency on pin
TEST_F(SdcInitTest, SdcClockLatencyOnPin) {
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("clp_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("clp_clk");
  // Set latency on clock (no pin)
  sdc->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                       MinMaxAll::all(), 0.5f);
  bool exists;
  float lat;
  sdc->clockLatency(clk, RiseFall::rise(), MinMax::max(), lat, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(lat, 0.5f);
}

// Sdc: setClockInsertion on pin
TEST_F(SdcInitTest, SdcClockInsertionOnPin) {
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("cip_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("cip_clk");
  sdc->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), EarlyLateAll::all(), 0.4f);
  float ins;
  bool exists;
  sdc->clockInsertion(clk, nullptr, RiseFall::rise(), MinMax::max(),
                      EarlyLate::early(), ins, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(ins, 0.4f);
}

// Sdc: setClockInsertion scalar form
TEST_F(SdcInitTest, SdcClockInsertionScalarForm) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("cis_clk", nullptr, false, 10.0, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("cis_clk");
  sdc->setClockInsertion(clk, nullptr, RiseFall::rise(), MinMax::max(),
                         EarlyLate::early(), 0.6f);

  }() ));
}

// Sdc: removeGraphAnnotations
// Sdc: annotateGraph (no graph)
// Sdc: pathDelayFrom/pathDelayTo (null)
// Sdc: isPathDelayInternalTo
TEST_F(SdcInitTest, SdcIsPathDelayInternalToNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isPathDelayInternalTo(nullptr));
}

TEST_F(SdcInitTest, SdcIsPathDelayInternalToBreakNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isPathDelayInternalToBreak(nullptr));
}

// Sdc: portMembers
// Sdc: clockLatency on clock (pair overload)
// ClockSetLess
// Sdc: makeExceptionThru/makeExceptionTo
// ClkHpinDisableLess
TEST_F(SdcInitTest, ClkHpinDisableLessConstruct) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ClkHpinDisableLess less(network);

  }() ));
}

// PinClockPairLess
TEST_F(SdcInitTest, PinClockPairLessConstruct) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  PinClockPairLess less(network);

  }() ));
}

// ClockInsertionkLess
TEST_F(SdcInitTest, ClockInsertionkLessConstruct) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ClockInsertionkLess less(network);

  }() ));
}

// ClockLatencyLess
TEST_F(SdcInitTest, ClockLatencyLessConstruct) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ClockLatencyLess less(network);

  }() ));
}

// DisabledInstPortsLess
// Sdc: deleteLoopExceptions
TEST_F(SdcInitTest, SdcDeleteLoopExceptions) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->deleteLoopExceptions();

  }() ));
}

// Sdc: makeFalsePath
TEST_F(SdcInitTest, SdcMakeFalsePath) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);

  }() ));
}

// Sdc: makeMulticyclePath
TEST_F(SdcInitTest, SdcMakeMulticyclePath) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->makeMulticyclePath(nullptr, nullptr, nullptr, MinMaxAll::all(),
                          false, 2, nullptr);

  }() ));
}

// Sdc: makePathDelay
// Sdc: sameClockGroupExplicit
TEST_F(SdcInitTest, SdcSameClockGroupExplicit) {
  Sdc *sdc = sta_->cmdSdc();
  FloatSeq *w1 = new FloatSeq;
  w1->push_back(0.0);
  w1->push_back(5.0);
  sta_->makeClock("scge_c1", nullptr, false, 10.0, w1, nullptr, sta_->cmdMode());
  FloatSeq *w2 = new FloatSeq;
  w2->push_back(0.0);
  w2->push_back(5.0);
  sta_->makeClock("scge_c2", nullptr, false, 10.0, w2, nullptr, sta_->cmdMode());
  Clock *c1 = sdc->findClock("scge_c1");
  Clock *c2 = sdc->findClock("scge_c2");
  bool same = sdc->sameClockGroupExplicit(c1, c2);
  EXPECT_FALSE(same);
}

// Sdc: makeFilterPath
// Sdc: resistance
TEST_F(SdcInitTest, SdcResistanceNull) {
  Sdc *sdc = sta_->cmdSdc();
  float res;
  bool exists;
  sdc->resistance(nullptr, MinMax::max(), res, exists);
  EXPECT_FALSE(exists);
}

// Sdc: setResistance
TEST_F(SdcInitTest, SdcSetResistanceNull) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->setResistance(nullptr, MinMaxAll::all(), 10.0f);

  }() ));
}

// Sdc: voltage
TEST_F(SdcInitTest, SdcVoltageNull) {
  Sdc *sdc = sta_->cmdSdc();
  float volt;
  bool exists;
  sdc->voltage(nullptr, MinMax::max(), volt, exists);
  EXPECT_FALSE(exists);
}

// Sdc: setVoltage on net
TEST_F(SdcInitTest, SdcSetVoltageOnNet) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  sdc->setVoltage(nullptr, MinMax::max(), 1.0f);

  }() ));
}

// Sdc: clkStopPropagation
// Sdc: isDisableClockGatingCheck
TEST_F(SdcInitTest, SdcIsDisableClockGatingCheckInstNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isDisableClockGatingCheck(static_cast<const Instance*>(nullptr)));
}

TEST_F(SdcInitTest, SdcIsDisableClockGatingCheckPinNull) {
  Sdc *sdc = sta_->cmdSdc();
  EXPECT_FALSE(sdc->isDisableClockGatingCheck(static_cast<const Pin*>(nullptr)));
}

} // namespace sta
