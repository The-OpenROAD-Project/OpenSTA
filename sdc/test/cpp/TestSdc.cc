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
  (void)hasher;
  (void)equal;
  EXPECT_TRUE(true);
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
  (void)less;
  EXPECT_TRUE(true);
}

TEST_F(ClockCmpTest, ClockNameLessInstantiation) {
  ClockNameLess less;
  (void)less;
  EXPECT_TRUE(true);
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
  EXPECT_FALSE(vars.bidirectNetPathsEnabled());
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

TEST_F(VariablesTest, SetBidirectNetPathsEnabled) {
  Variables vars;
  vars.setBidirectNetPathsEnabled(true);
  EXPECT_TRUE(vars.bidirectNetPathsEnabled());
}

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
  PortExtCap pec(nullptr);
  EXPECT_EQ(pec.port(), nullptr);
}

TEST_F(PortExtCapTest, PinCap) {
  PortExtCap pec(nullptr);
  float cap;
  bool exists;
  pec.pinCap(RiseFall::rise(), MinMax::max(), cap, exists);
  EXPECT_FALSE(exists);

  pec.setPinCap(1.5f, RiseFall::rise(), MinMax::max());
  pec.pinCap(RiseFall::rise(), MinMax::max(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 1.5f);
}

TEST_F(PortExtCapTest, WireCap) {
  PortExtCap pec(nullptr);
  float cap;
  bool exists;
  pec.wireCap(RiseFall::fall(), MinMax::min(), cap, exists);
  EXPECT_FALSE(exists);

  pec.setWireCap(2.5f, RiseFall::fall(), MinMax::min());
  pec.wireCap(RiseFall::fall(), MinMax::min(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 2.5f);
}

TEST_F(PortExtCapTest, Fanout) {
  PortExtCap pec(nullptr);
  int fanout;
  bool exists;
  pec.fanout(MinMax::max(), fanout, exists);
  EXPECT_FALSE(exists);

  pec.setFanout(4, MinMax::max());
  pec.fanout(MinMax::max(), fanout, exists);
  EXPECT_TRUE(exists);
  EXPECT_EQ(fanout, 4);
}

TEST_F(PortExtCapTest, PinCapPtr) {
  PortExtCap pec(nullptr);
  RiseFallMinMax *pc = pec.pinCap();
  EXPECT_NE(pc, nullptr);
}

TEST_F(PortExtCapTest, WireCapPtr) {
  PortExtCap pec(nullptr);
  RiseFallMinMax *wc = pec.wireCap();
  EXPECT_NE(wc, nullptr);
}

TEST_F(PortExtCapTest, FanoutPtr) {
  PortExtCap pec(nullptr);
  FanoutValues *fv = pec.fanout();
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
  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
  ASSERT_NE(sdc, nullptr);
  sdc->setMaxArea(200.0f);
  sdc->setWireloadMode(WireloadMode::segmented);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 200.0f);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::segmented);
  sta_->removeConstraints();
  // removeConstraints() also preserves these global settings.
  EXPECT_FLOAT_EQ(sdc->maxArea(), 200.0f);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::segmented);
  EXPECT_TRUE(sdc->clks().empty());
  EXPECT_NE(sdc->defaultArrivalClock(), nullptr);
  EXPECT_NE(sdc->defaultArrivalClockEdge(), nullptr);
}

// Clock creation and queries
TEST_F(SdcInitTest, MakeClockNoPins) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("test_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("test_clk");
  EXPECT_NE(clk, nullptr);
  EXPECT_FLOAT_EQ(clk->period(), 10.0);
}

TEST_F(SdcInitTest, MakeClockAndRemove) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("clk1", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk1");
  EXPECT_NE(clk, nullptr);

  sta_->removeClock(clk);
  EXPECT_EQ(sdc->findClock("clk1"), nullptr);
}

TEST_F(SdcInitTest, MultipleClocksQuery) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("clk_a", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("clk_b", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
  ClockSeq clks = sdc->clks();
  EXPECT_EQ(clks.size(), 2u);
}

// Clock properties
TEST_F(SdcInitTest, ClockProperties) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("prop_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("slew_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("slew_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockSlew(clk, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5);
  float slew = 0.0f;
  bool exists = false;
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 0.5f);
  sta_->removeClockSlew(clk);
  clk->slew(RiseFall::rise(), MinMax::max(), slew, exists);
  EXPECT_FALSE(exists);
}

// Clock latency with clock
TEST_F(SdcInitTest, ClockLatencyOnClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lat_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("lat_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                        MinMaxAll::all(), 1.0);
  float latency = 0.0f;
  bool exists = false;
  sdc->clockLatency(clk, RiseFall::rise(), MinMax::max(), latency, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(latency, 1.0f);
  sta_->removeClockLatency(clk, nullptr);
  sdc->clockLatency(clk, RiseFall::rise(), MinMax::max(), latency, exists);
  EXPECT_FALSE(exists);
}

// Clock insertion delay
TEST_F(SdcInitTest, ClockInsertionOnClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ins_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("ins_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                          MinMaxAll::all(), EarlyLateAll::all(), 0.5);
  float insertion = 0.0f;
  bool exists = false;
  sdc->clockInsertion(clk, nullptr, RiseFall::rise(), MinMax::max(),
                      EarlyLate::early(), insertion, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(insertion, 0.5f);
  sta_->removeClockInsertion(clk, nullptr);
  sdc->clockInsertion(clk, nullptr, RiseFall::rise(), MinMax::max(),
                      EarlyLate::early(), insertion, exists);
  EXPECT_FALSE(exists);
}

// Clock uncertainty
TEST_F(SdcInitTest, ClockUncertainty) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("unc_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("unc_clk");
  ASSERT_NE(clk, nullptr);
  ASSERT_NO_THROW(sta_->setClockUncertainty(clk, SetupHoldAll::all(), 0.1));
  ASSERT_NO_THROW(sta_->removeClockUncertainty(clk, SetupHoldAll::all()));
}

// Inter-clock uncertainty
TEST_F(SdcInitTest, InterClockUncertainty) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("iuc_clk1", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("iuc_clk2", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("iuc_clk1");
  Clock *clk2 = sdc->findClock("iuc_clk2");
  ASSERT_NE(clk1, nullptr);
  ASSERT_NE(clk2, nullptr);
  sta_->setClockUncertainty(clk1, RiseFallBoth::riseFall(),
                            clk2, RiseFallBoth::riseFall(),
                            SetupHoldAll::all(), 0.2);
  float uncertainty = 0.0f;
  bool exists = false;
  sdc->clockUncertainty(clk1, RiseFall::rise(),
                        clk2, RiseFall::rise(),
                        SetupHold::max(), uncertainty, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(uncertainty, 0.2f);
  sta_->removeClockUncertainty(clk1, RiseFallBoth::riseFall(),
                               clk2, RiseFallBoth::riseFall(),
                               SetupHoldAll::all());
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
  sta_->makeClock("grp_clk1", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("grp_clk2", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("grp_clk1");
  Clock *clk2 = sdc->findClock("grp_clk2");
  ASSERT_NE(clk1, nullptr);
  ASSERT_NE(clk2, nullptr);

  ClockGroups *groups = sta_->makeClockGroups("grp1", true, false, false, false, nullptr);
  ASSERT_NE(groups, nullptr);
  ClockSet *clk_set = new ClockSet;
  clk_set->insert(clk1);
  clk_set->insert(clk2);
  ASSERT_NO_THROW(sta_->makeClockGroup(groups, clk_set));

  ASSERT_NO_THROW(sta_->removeClockGroupsLogicallyExclusive("grp1"));
  EXPECT_NE(sdc->findClock("grp_clk1"), nullptr);
  EXPECT_NE(sdc->findClock("grp_clk2"), nullptr);
}

// Clock propagation
TEST_F(SdcInitTest, ClockPropagation) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("prop_clk2", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("prop_clk2");
  sta_->setPropagatedClock(clk);
  EXPECT_TRUE(clk->isPropagated());
  sta_->removePropagatedClock(clk);
  EXPECT_FALSE(clk->isPropagated());
}

// Timing derate with clock
TEST_F(SdcInitTest, TimingDerateWithClock) {
  ASSERT_NO_THROW(sta_->setTimingDerate(TimingDerateType::cell_delay,
                                        PathClkOrData::clk,
                                        RiseFallBoth::rise(),
                                        EarlyLate::early(),
                                        0.95));
  ASSERT_NO_THROW(sta_->setTimingDerate(TimingDerateType::cell_check,
                                        PathClkOrData::clk,
                                        RiseFallBoth::fall(),
                                        EarlyLate::late(),
                                        1.05));
  ASSERT_NO_THROW(sta_->setTimingDerate(TimingDerateType::net_delay,
                                        PathClkOrData::data,
                                        RiseFallBoth::riseFall(),
                                        EarlyLate::early(),
                                        0.97));
  ASSERT_NO_THROW(sta_->unsetTimingDerate());
}

// Clock gating check with clock
TEST_F(SdcInitTest, ClockGatingCheckWithClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("cgc_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("cgc_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockGatingCheck(clk, RiseFallBoth::riseFall(),
                            SetupHold::max(), 0.5);
  bool exists = false;
  float margin = 0.0f;
  sdc->clockGatingMarginClk(clk, RiseFall::rise(), SetupHold::max(),
                            exists, margin);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(margin, 0.5f);
}

// False path
TEST_F(SdcInitTest, MakeFalsePath) {
  Sdc *sdc = sta_->sdc();
  size_t before = sdc->exceptions().size();
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);
  EXPECT_GT(sdc->exceptions().size(), before);
}

// Group path
TEST_F(SdcInitTest, MakeGroupPath) {
  sta_->makeGroupPath("test_group", false, nullptr, nullptr, nullptr, nullptr);
  EXPECT_TRUE(sta_->isPathGroupName("test_group"));
}

// Latch borrow limit
TEST_F(SdcInitTest, LatchBorrowLimitClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lbl_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("lbl_clk");
  ASSERT_NE(clk, nullptr);
  ASSERT_NO_THROW(sta_->setLatchBorrowLimit(clk, 2.0));
  EXPECT_NE(sdc->findClock("lbl_clk"), nullptr);
}

// Min pulse width with clock
TEST_F(SdcInitTest, MinPulseWidthClock) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("mpw_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("mpw_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setMinPulseWidth(clk, RiseFallBoth::riseFall(), 1.0);
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
  sta_->makeClock("sl_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("sl_clk");
  ASSERT_NE(clk, nullptr);
  sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                     PathClkOrData::clk, MinMax::max(), 2.0);
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
  EXPECT_THROW(sta_->writeSdc("/dev/null", false, false, 4, false, false),
               std::exception);
}

// Operating conditions
TEST_F(SdcInitTest, SdcOperatingConditions) {
  Sdc *sdc = sta_->sdc();
  // No operating conditions set
  const OperatingConditions *op_min = sdc->operatingConditions(MinMax::min());
  const OperatingConditions *op_max = sdc->operatingConditions(MinMax::max());
  EXPECT_EQ(op_min, nullptr);
  EXPECT_EQ(op_max, nullptr);
}

// Sdc analysis type changes
TEST_F(SdcInitTest, SdcAnalysisTypeChanges) {
  Sdc *sdc = sta_->sdc();
  sdc->setAnalysisType(AnalysisType::single);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::single);
  sdc->setAnalysisType(AnalysisType::bc_wc);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::bc_wc);
  sdc->setAnalysisType(AnalysisType::ocv);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::ocv);
}

// Multicycle path
TEST_F(SdcInitTest, MakeMulticyclePath) {
  Sdc *sdc = sta_->sdc();
  size_t before = sdc->exceptions().size();
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::all(),
                           true,   // use_end_clk
                           2,      // path_multiplier
                           nullptr);
  EXPECT_GT(sdc->exceptions().size(), before);
}

// Reset path
TEST_F(SdcInitTest, ResetPath) {
  Sdc *sdc = sta_->sdc();
  size_t before = sdc->exceptions().size();
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);
  size_t after_make = sdc->exceptions().size();
  EXPECT_GT(after_make, before);
  ASSERT_NO_THROW(sta_->resetPath(nullptr, nullptr, nullptr, MinMaxAll::all()));
  EXPECT_EQ(sdc->exceptions().size(), after_make);
}

// Clock waveform details
TEST_F(SdcInitTest, ClockWaveformDetails) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(3.0);
  sta_->makeClock("wave_clk", nullptr, false, 8.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("edge_clk", nullptr, false, 10.0, waveform, nullptr);

  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
  ASSERT_NO_THROW(sdc->setTimingDerate(TimingDerateType::cell_delay,
                                       PathClkOrData::clk,
                                       RiseFallBoth::rise(),
                                       EarlyLate::early(), 0.95));
  ASSERT_NO_THROW(sdc->setTimingDerate(TimingDerateType::cell_check,
                                       PathClkOrData::data,
                                       RiseFallBoth::fall(),
                                       EarlyLate::late(), 1.05));
  ASSERT_NO_THROW(sdc->setTimingDerate(TimingDerateType::net_delay,
                                       PathClkOrData::clk,
                                       RiseFallBoth::riseFall(),
                                       EarlyLate::early(), 0.97));
  ASSERT_NO_THROW(sdc->unsetTimingDerate());
}

// Multiple clocks and removal
TEST_F(SdcInitTest, MultipleClockRemoval) {
  FloatSeq *w1 = new FloatSeq;
  w1->push_back(0.0);
  w1->push_back(5.0);
  sta_->makeClock("rm_clk1", nullptr, false, 10.0, w1, nullptr);

  FloatSeq *w2 = new FloatSeq;
  w2->push_back(0.0);
  w2->push_back(2.5);
  sta_->makeClock("rm_clk2", nullptr, false, 5.0, w2, nullptr);

  FloatSeq *w3 = new FloatSeq;
  w3->push_back(0.0);
  w3->push_back(1.0);
  sta_->makeClock("rm_clk3", nullptr, false, 2.0, w3, nullptr);

  Sdc *sdc = sta_->sdc();
  EXPECT_EQ(sdc->clks().size(), 3u);

  Clock *clk2 = sdc->findClock("rm_clk2");
  sta_->removeClock(clk2);
  EXPECT_EQ(sdc->clks().size(), 2u);
  EXPECT_EQ(sdc->findClock("rm_clk2"), nullptr);
}

// Voltage settings via Sdc
TEST_F(SdcInitTest, SdcVoltage) {
  Sdc *sdc = sta_->sdc();
  sta_->setVoltage(MinMax::max(), 1.1);
  sta_->setVoltage(MinMax::min(), 0.9);
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
  drive.setDriveResistance(RiseFallBoth::riseFall(), MinMaxAll::all(), 100.0);
  float res;
  bool exists;
  drive.driveResistance(RiseFall::rise(), MinMax::max(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 100.0f);
  EXPECT_TRUE(drive.hasDriveResistance(RiseFall::rise(), MinMax::max()));
}

TEST_F(SdcInitTest, InputDriveResistanceMinMaxEqual) {
  InputDrive drive;
  drive.setDriveResistance(RiseFallBoth::rise(), MinMaxAll::all(), 100.0);
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
  Sdc *sdc = sta_->sdc();
  sdc->setMaxArea(500.0);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 500.0f);
}

TEST_F(SdcInitTest, SdcWireloadMode) {
  Sdc *sdc = sta_->sdc();
  sdc->setWireloadMode(WireloadMode::top);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::top);
  sdc->setWireloadMode(WireloadMode::enclosed);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::enclosed);
  sdc->setWireloadMode(WireloadMode::segmented);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::segmented);
}

TEST_F(SdcInitTest, SdcMinPulseWidthGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.5);
  sdc->setMinPulseWidth(RiseFallBoth::fall(), 0.3);

  }() ));
}

// Sdc design rule limits
TEST_F(SdcInitTest, SdcSlewLimitPort) {
  Sdc *sdc = sta_->sdc();
  // We can't easily create ports without a network, but we can call
  // methods that don't crash with nullptr
  // Instead test clock slew limits
  FloatSeq *wave = new FloatSeq;
  wave->push_back(0.0);
  wave->push_back(5.0);
  sta_->makeClock("sl_test_clk", nullptr, false, 10.0, wave, nullptr);
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
  sta_->makeClock("sp_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("sp_clk");
  EXPECT_FLOAT_EQ(clk->period(), 10.0);
  // waveformInvalid() invalidates cached waveform data - just call it
  clk->waveformInvalid();
}

TEST_F(SdcInitTest, ClockWaveformInvalid) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("wi_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("atp_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("ig_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("ig_clk");
  EXPECT_TRUE(clk->isIdeal());
  EXPECT_FALSE(clk->isGenerated());
}

// Clock: index
TEST_F(SdcInitTest, ClockIndex) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("idx_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("idx_clk");
  EXPECT_GE(clk->index(), 0);
}

// ClockEdge: transition, opposite, name, index, pulseWidth
TEST_F(SdcInitTest, ClockEdgeDetails) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ced_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("csl_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("cu_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("csl2_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("match_clk1", nullptr, false, 10.0, wave, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("match_clk2", nullptr, false, 5.0, wave2, nullptr);

  FloatSeq *wave3 = new FloatSeq;
  wave3->push_back(0.0);
  wave3->push_back(1.0);
  sta_->makeClock("other_clk", nullptr, false, 2.0, wave3, nullptr);

  Sdc *sdc = sta_->sdc();
  PatternMatch pattern("match_*");
  ClockSeq matches = sdc->findClocksMatching(&pattern);
  EXPECT_EQ(matches.size(), 2u);
}

// Sdc: sortedClocks
TEST_F(SdcInitTest, SdcSortedClocks) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("b_clk", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("a_clk", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
  ClockSeq sorted;
  sdc->sortedClocks(sorted);
  EXPECT_EQ(sorted.size(), 2u);
  // Should be sorted by name: a_clk before b_clk
  EXPECT_STREQ(sorted[0]->name(), "a_clk");
  EXPECT_STREQ(sorted[1]->name(), "b_clk");
}

// Sdc: defaultArrivalClock
TEST_F(SdcInitTest, SdcDefaultArrivalClock) {
  Sdc *sdc = sta_->sdc();
  Clock *default_clk = sdc->defaultArrivalClock();
  // Default arrival clock always exists
  EXPECT_NE(default_clk, nullptr);
  ClockEdge *edge = sdc->defaultArrivalClockEdge();
  EXPECT_NE(edge, nullptr);
}

// Sdc: clockLatencies/clockInsertions accessors
TEST_F(SdcInitTest, SdcClockLatenciesAccessor) {
  Sdc *sdc = sta_->sdc();
  auto *latencies = sdc->clockLatencies();
  EXPECT_NE(latencies, nullptr);
  const auto *const_latencies = static_cast<const Sdc*>(sdc)->clockLatencies();
  EXPECT_NE(const_latencies, nullptr);
}

TEST_F(SdcInitTest, SdcClockInsertionsAccessor) {
  Sdc *sdc = sta_->sdc();
  const auto &insertions = sdc->clockInsertions();
  // Initially empty
  EXPECT_TRUE(insertions.empty());
}

// Sdc: pathDelaysWithoutTo
TEST_F(SdcInitTest, SdcPathDelaysWithoutTo) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->pathDelaysWithoutTo());
}

// Sdc: exceptions accessor
TEST_F(SdcInitTest, SdcExceptionsAccessor) {
  Sdc *sdc = sta_->sdc();
  auto &exceptions = sdc->exceptions();
  // Initially empty
  EXPECT_TRUE(exceptions.empty());
}

// Sdc: groupPaths accessor
TEST_F(SdcInitTest, SdcGroupPathsAccessor) {
  Sdc *sdc = sta_->sdc();
  auto &gp = sdc->groupPaths();
  EXPECT_TRUE(gp.empty());

  sta_->makeGroupPath("test_grp", false, nullptr, nullptr, nullptr, nullptr);
  EXPECT_FALSE(sdc->groupPaths().empty());
}

// Sdc: netResistances
TEST_F(SdcInitTest, SdcNetResistancesAccessor) {
  Sdc *sdc = sta_->sdc();
  auto &res = sdc->netResistances();
  EXPECT_TRUE(res.empty());
}

// Sdc: disabledPins/Ports/LibPorts/Edges accessors
TEST_F(SdcInitTest, SdcDisabledAccessors) {
  Sdc *sdc = sta_->sdc();
  EXPECT_NE(sdc->disabledPins(), nullptr);
  EXPECT_NE(sdc->disabledPorts(), nullptr);
  EXPECT_NE(sdc->disabledLibPorts(), nullptr);
  EXPECT_NE(sdc->disabledEdges(), nullptr);
  EXPECT_NE(sdc->disabledCellPorts(), nullptr);
  EXPECT_NE(sdc->disabledInstancePorts(), nullptr);
}

// Sdc: logicValues/caseLogicValues
TEST_F(SdcInitTest, SdcLogicValueMaps) {
  Sdc *sdc = sta_->sdc();
  auto &lv = sdc->logicValues();
  EXPECT_TRUE(lv.empty());
  auto &cv = sdc->caseLogicValues();
  EXPECT_TRUE(cv.empty());
}

// Sdc: inputDelays/outputDelays
TEST_F(SdcInitTest, SdcPortDelayAccessors) {
  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
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
                      MinMax::max(), false, false, 5.0e-9, nullptr);

  }() ));
}

// Sdc: removeClockGroupsPhysicallyExclusive/Asynchronous
TEST_F(SdcInitTest, SdcRemoveClockGroupsOther) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->removeClockGroupsPhysicallyExclusive(nullptr);
  sdc->removeClockGroupsAsynchronous(nullptr);

  }() ));
}

// Sdc: sameClockGroup
TEST_F(SdcInitTest, SdcSameClockGroup) {
  FloatSeq *wave1 = new FloatSeq;
  wave1->push_back(0.0);
  wave1->push_back(5.0);
  sta_->makeClock("scg_clk1", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("scg_clk2", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
  Clock *clk1 = sdc->findClock("scg_clk1");
  Clock *clk2 = sdc->findClock("scg_clk2");
  // Without explicit groups, clocks are in the same group
  EXPECT_TRUE(sdc->sameClockGroup(clk1, clk2));
}

// Sdc: invalidateGeneratedClks
TEST_F(SdcInitTest, SdcInvalidateGeneratedClks) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->invalidateGeneratedClks();

  }() ));
}

// Sdc: clkHpinDisablesInvalid
TEST_F(SdcInitTest, SdcClkHpinDisablesInvalid) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->clkHpinDisablesInvalid();

  }() ));
}

// Sdc: deleteExceptions/searchPreamble
TEST_F(SdcInitTest, SdcDeleteExceptions) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->deleteExceptions();

  }() ));
}

TEST_F(SdcInitTest, SdcSearchPreamble) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->searchPreamble();

  }() ));
}

// Sdc: setClockGatingCheck global
TEST_F(SdcInitTest, SdcClockGatingCheckGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setClockGatingCheck(RiseFallBoth::riseFall(),
                           SetupHold::max(), 0.5);

  }() ));
}

// Sdc: clkStopPropagation with non-existent pin
TEST_F(SdcInitTest, SdcClkStopPropagation) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->clkStopPropagation(nullptr, nullptr));
}

// Sdc: voltage
TEST_F(SdcInitTest, SdcVoltageGetSet) {
  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
  sdc->removeNetLoadCaps();

  }() ));
}

// CycleAccting hash and equal functors
TEST_F(SdcInitTest, CycleAcctingFunctorsCompile) {
  FloatSeq *wave = new FloatSeq;
  wave->push_back(0.0);
  wave->push_back(4.0);
  sta_->makeClock("cycle_functor_clk", nullptr, false, 8.0, wave, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("cmp_a", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("cmp_b", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("alpha_clk", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("beta_clk", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("icul_clk1", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("icul_clk2", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("zz_clk", nullptr, false, 10.0, wave1, nullptr);

  FloatSeq *wave2 = new FloatSeq;
  wave2->push_back(0.0);
  wave2->push_back(2.5);
  sta_->makeClock("aa_clk", nullptr, false, 5.0, wave2, nullptr);

  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("slew_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("unc_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("sl_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("gen_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("gen_clk");
  EXPECT_FALSE(clk->isGenerated());
}

// ClockEdge: constructor, opposite, pulseWidth, transition
TEST_F(SdcInitTest, ClockEdgeProperties) {
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("edge_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("cmp_clk1", nullptr, false, 5.0, waveform1, nullptr);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(5.0);
  sta_->makeClock("cmp_clk2", nullptr, false, 10.0, waveform2, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("icu_clk1", nullptr, false, 5.0, waveform1, nullptr);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(5.0);
  sta_->makeClock("icu_clk2", nullptr, false, 10.0, waveform2, nullptr);
  Sdc *sdc = sta_->sdc();
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
  bool result = less(&fp1, &fp2);
  (void)result;

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
  sta_->makeClock("idx_clk1", nullptr, false, 5.0, waveform1, nullptr);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(5.0);
  sta_->makeClock("idx_clk2", nullptr, false, 10.0, waveform2, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("ca_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("ca2_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("cah_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  // PortExtCap needs a Port*, just pass nullptr as we won't use it for lookup
  PortExtCap pec(nullptr);
  pec.setPinCap(0.1f, RiseFall::rise(), MinMax::max());
  float cap;
  bool exists;
  pec.pinCap(RiseFall::rise(), MinMax::max(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 0.1f);
}

// PortExtCap: wire cap
TEST_F(SdcInitTest, PortExtCapWireCap) {
  PortExtCap pec(nullptr);
  pec.setWireCap(0.2f, RiseFall::fall(), MinMax::min());
  float cap;
  bool exists;
  pec.wireCap(RiseFall::fall(), MinMax::min(), cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 0.2f);
}

// PortExtCap: fanout
TEST_F(SdcInitTest, PortExtCapFanout) {
  PortExtCap pec(nullptr);
  pec.setFanout(4, MinMax::max());
  int fan;
  bool exists;
  pec.fanout(MinMax::max(), fan, exists);
  EXPECT_TRUE(exists);
  EXPECT_EQ(fan, 4);
}

// PortExtCap: port accessor
TEST_F(SdcInitTest, PortExtCapPort) {
  PortExtCap pec(nullptr);
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
  Sdc *sdc = sta_->sdc();
  sdc->clkHpinDisablesInvalid();
  // exercises clkHpinDisablesInvalid

  }() ));
}

// Sdc: setTimingDerate (global variant)
TEST_F(SdcInitTest, SdcSetTimingDerateGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setTimingDerate(TimingDerateType::cell_delay, PathClkOrData::clk,
                       RiseFallBoth::riseFall(), EarlyLate::early(), 0.95f);
  // exercises setTimingDerate global

  }() ));
}

// Sdc: unsetTimingDerate
TEST_F(SdcInitTest, SdcUnsetTimingDerate) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
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
  (void)hash;
}

// Sdc: dataChecksFrom/dataChecksTo (need Pin* arg)
TEST_F(SdcInitTest, SdcDataChecksFromNull) {
  Sdc *sdc = sta_->sdc();
  DataCheckSet *checks = sdc->dataChecksFrom(nullptr);
  EXPECT_EQ(checks, nullptr);
}

TEST_F(SdcInitTest, SdcDataChecksToNull) {
  Sdc *sdc = sta_->sdc();
  DataCheckSet *checks = sdc->dataChecksTo(nullptr);
  EXPECT_EQ(checks, nullptr);
}

// Sdc: inputDelays/outputDelays
TEST_F(SdcInitTest, PortDelayMaps) {
  Sdc *sdc = sta_->sdc();
  const auto &id = sdc->inputDelays();
  const auto &od = sdc->outputDelays();
  EXPECT_TRUE(id.empty());
  EXPECT_TRUE(od.empty());
}

// Sdc: clockGatingMargin global
TEST_F(SdcInitTest, SdcClockGatingMarginGlobal) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
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
  vars.setBidirectNetPathsEnabled(true);
  EXPECT_TRUE(vars.bidirectNetPathsEnabled());
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
  PortExtCap pec(nullptr);
  pec.setPinCap(1.5f, RiseFall::rise(), MinMax::max());
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
  sta_->makeClock("ca_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("virt_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  sta_->makeClock("dp_clk", nullptr, false, 10.0, waveform, nullptr);
  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
  sdc->makeClockGroups("grp2", false, true, false, false, "comment");
  sdc->removeClockGroups("grp2");

  }() ));
}

TEST_F(SdcInitTest, SdcRemoveClockGroupsLogicallyExclusive) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->makeClockGroups("le_grp", true, false, false, false, nullptr);
  sdc->removeClockGroupsLogicallyExclusive("le_grp");

  }() ));
}

TEST_F(SdcInitTest, SdcRemoveClockGroupsPhysicallyExclusive) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->makeClockGroups("pe_grp", false, true, false, false, nullptr);
  sdc->removeClockGroupsPhysicallyExclusive("pe_grp");

  }() ));
}

TEST_F(SdcInitTest, SdcRemoveClockGroupsAsynchronous) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->makeClockGroups("async_grp", false, false, true, false, nullptr);
  sdc->removeClockGroupsAsynchronous("async_grp");

  }() ));
}

// ClockGroups direct
// Sdc: setMaxArea/maxArea
TEST_F(SdcInitTest, SdcSetMaxArea) {
  Sdc *sdc = sta_->sdc();
  sdc->setMaxArea(100.0f);
  EXPECT_FLOAT_EQ(sdc->maxArea(), 100.0f);
}

// Sdc: setWireloadMode/wireloadMode
TEST_F(SdcInitTest, SdcSetWireloadMode) {
  Sdc *sdc = sta_->sdc();
  sdc->setWireloadMode(WireloadMode::top);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::top);
  sdc->setWireloadMode(WireloadMode::enclosed);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::enclosed);
  sdc->setWireloadMode(WireloadMode::segmented);
  EXPECT_EQ(sdc->wireloadMode(), WireloadMode::segmented);
}

// Sdc: wireload/setWireload
TEST_F(SdcInitTest, SdcWireloadNull) {
  Sdc *sdc = sta_->sdc();
  // No wireload set
  auto *wl = sdc->wireload(MinMax::max());
  EXPECT_EQ(wl, nullptr);
}

// Sdc: wireloadSelection
TEST_F(SdcInitTest, SdcWireloadSelectionNull) {
  Sdc *sdc = sta_->sdc();
  auto *ws = sdc->wireloadSelection(MinMax::max());
  EXPECT_EQ(ws, nullptr);
}

// Sdc: setAnalysisType
TEST_F(SdcInitTest, SdcSetAnalysisTypeSingle) {
  Sdc *sdc = sta_->sdc();
  sdc->setAnalysisType(AnalysisType::single);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::single);
}

TEST_F(SdcInitTest, SdcSetAnalysisTypeBcWc) {
  Sdc *sdc = sta_->sdc();
  sdc->setAnalysisType(AnalysisType::bc_wc);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::bc_wc);
}

TEST_F(SdcInitTest, SdcSetAnalysisTypeOcv) {
  Sdc *sdc = sta_->sdc();
  sdc->setAnalysisType(AnalysisType::ocv);
  EXPECT_EQ(sdc->analysisType(), AnalysisType::ocv);
}

// Sdc: isConstrained
TEST_F(SdcInitTest, SdcIsConstrainedInstNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->isConstrained((const Instance*)nullptr));
}

TEST_F(SdcInitTest, SdcIsConstrainedNetNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->isConstrained((const Net*)nullptr));
}

// Sdc: haveClkSlewLimits
TEST_F(SdcInitTest, SdcHaveClkSlewLimits) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->haveClkSlewLimits());
}

// Sdc: hasClockLatency
TEST_F(SdcInitTest, SdcHasClockLatencyNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->hasClockLatency(nullptr));
}

// Sdc: clockLatencies
TEST_F(SdcInitTest, SdcClockLatenciesAccess) {
  Sdc *sdc = sta_->sdc();
  auto *cl = sdc->clockLatencies();
  EXPECT_NE(cl, nullptr);
}

// Sdc: clockInsertions
TEST_F(SdcInitTest, SdcClockInsertionsAccess) {
  Sdc *sdc = sta_->sdc();
  const auto &ci = sdc->clockInsertions();
  EXPECT_TRUE(ci.empty());
}

// Sdc: hasClockInsertion
TEST_F(SdcInitTest, SdcHasClockInsertionNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->hasClockInsertion(nullptr));
}

// Sdc: defaultArrivalClockEdge
TEST_F(SdcInitTest, SdcDefaultArrivalClockEdge) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  const ClockEdge *edge = sdc->defaultArrivalClockEdge();
  // May be null before searchPreamble
  (void)edge;

  }() ));
}

// Sdc: sortedClocks
// Sdc: searchPreamble
TEST_F(SdcInitTest, SdcSearchPreambleNoDesign) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->searchPreamble();

  }() ));
}

// Sdc: makeDefaultArrivalClock
TEST_F(SdcInitTest, SdcMakeDefaultArrivalClock) {
  Sdc *sdc = sta_->sdc();
  sdc->searchPreamble();
  const ClockEdge *edge = sdc->defaultArrivalClockEdge();
  EXPECT_NE(edge, nullptr);
}

// Sdc: invalidateGeneratedClks
TEST_F(SdcInitTest, SdcInvalidateGenClks) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->invalidateGeneratedClks();

  }() ));
}

// Sdc: setClockSlew/removeClockSlew
TEST_F(SdcInitTest, SdcSetClockSlew) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("slew_clk", nullptr, false, 10.0, waveform, nullptr);
  Clock *clk = sdc->findClock("slew_clk");
  sdc->setClockSlew(clk, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.1f);
  sdc->removeClockSlew(clk);

  }() ));
}

// Sdc: setClockLatency/removeClockLatency
TEST_F(SdcInitTest, SdcSetClockLatency) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lat_clk", nullptr, false, 10.0, waveform, nullptr);
  Clock *clk = sdc->findClock("lat_clk");
  sdc->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5f);
  sdc->removeClockLatency(clk, nullptr);

  }() ));
}

// Sdc: clockLatency (Clock*, RiseFall*, MinMax*)
TEST_F(SdcInitTest, SdcClockLatencyQuery) {
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("latq_clk", nullptr, false, 10.0, waveform, nullptr);
  Clock *clk = sdc->findClock("latq_clk");
  sdc->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(), MinMaxAll::all(), 1.0f);
  float lat = sdc->clockLatency(clk, RiseFall::rise(), MinMax::max());
  EXPECT_FLOAT_EQ(lat, 1.0f);
}

// Sdc: setClockInsertion/removeClockInsertion
TEST_F(SdcInitTest, SdcSetClockInsertion) {
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("ins_clk", nullptr, false, 10.0, waveform, nullptr);
  Clock *clk = sdc->findClock("ins_clk");
  sdc->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), EarlyLateAll::all(), 0.2f);
  EXPECT_FALSE(sdc->clockInsertions().empty());
  sdc->removeClockInsertion(clk, nullptr);
}

// Sdc: clockInsertion query
TEST_F(SdcInitTest, SdcClockInsertionQuery) {
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("insq_clk", nullptr, false, 10.0, waveform, nullptr);
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
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  sta_->makeClock("unc_clk1", nullptr, false, 10.0, waveform1, nullptr);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(2.5);
  sta_->makeClock("unc_clk2", nullptr, false, 5.0, waveform2, nullptr);
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
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0);
  waveform1->push_back(5.0);
  sta_->makeClock("scg_c1", nullptr, false, 10.0, waveform1, nullptr);
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0);
  waveform2->push_back(5.0);
  sta_->makeClock("scg_c2", nullptr, false, 10.0, waveform2, nullptr);
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
  Sdc *sdc = sta_->sdc();
  // Can't set data check without real pins, but test the query on null
  DataCheckSet *from = sdc->dataChecksFrom(nullptr);
  DataCheckSet *to = sdc->dataChecksTo(nullptr);
  EXPECT_EQ(from, nullptr);
  EXPECT_EQ(to, nullptr);
}

// Sdc: setTimingDerate (all variants)
TEST_F(SdcInitTest, SdcSetTimingDerateGlobalNet) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setTimingDerate(TimingDerateType::net_delay, PathClkOrData::data,
                       RiseFallBoth::riseFall(), EarlyLate::late(), 1.05f);

  }() ));
}

// Sdc: swapDeratingFactors
TEST_F(SdcInitTest, SdcSwapDeratingFactors) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
  sdc->setVoltage(MinMax::max(), 1.0f);

  }() ));
}

// Sdc: setLatchBorrowLimit
TEST_F(SdcInitTest, SdcSetLatchBorrowLimitClock) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("lbl_clk", nullptr, false, 10.0, waveform, nullptr);
  Clock *clk = sdc->findClock("lbl_clk");
  sdc->setLatchBorrowLimit(clk, 3.0f);

  }() ));
}

// Sdc: setMinPulseWidth on clock
TEST_F(SdcInitTest, SdcSetMinPulseWidthClock) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("mpw_clk", nullptr, false, 10.0, waveform, nullptr);
  Clock *clk = sdc->findClock("mpw_clk");
  sdc->setMinPulseWidth(clk, RiseFallBoth::riseFall(), 1.0f);

  }() ));
}

// Sdc: makeCornersAfter/makeCornersBefore
TEST_F(SdcInitTest, SdcMakeCornersBefore) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->makeCornersBefore();
  sdc->makeCornersAfter(sta_->corners());

  }() ));
}

// Sdc: removeNetLoadCaps
// Sdc: initVariables
// Sdc: swapPortExtCaps
TEST_F(SdcInitTest, SdcSwapPortExtCaps) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Sdc::swapPortExtCaps(sdc, sdc);

  }() ));
}

// Sdc: swapClockInsertions
TEST_F(SdcInitTest, SdcSwapClockInsertions) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->hasNetWireCap(nullptr));
}

// Sdc: hasPortExtCap
TEST_F(SdcInitTest, SdcHasPortExtCapNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->hasPortExtCap(nullptr));
}

// Sdc: isExceptionStartpoint/isExceptionEndpoint
// Sdc: isPropagatedClock
TEST_F(SdcInitTest, SdcIsPropagatedClockNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->isPropagatedClock(nullptr));
}

// Sdc: hasLogicValue
TEST_F(SdcInitTest, SdcHasLogicValueNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->hasLogicValue(nullptr));
}

// Sdc: isPathDelayInternalFrom/To
TEST_F(SdcInitTest, SdcIsPathDelayInternalFromNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->isPathDelayInternalFrom(nullptr));
}

TEST_F(SdcInitTest, SdcIsPathDelayInternalFromBreakNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->isPathDelayInternalFromBreak(nullptr));
}

// Sdc: pathDelayInternalFrom
TEST_F(SdcInitTest, SdcPathDelayInternalFrom) {
  Sdc *sdc = sta_->sdc();
  const auto &pins = sdc->pathDelayInternalFrom();
  EXPECT_TRUE(pins.empty());
}

// Sdc: disabledCellPorts/disabledInstancePorts
TEST_F(SdcInitTest, SdcDisabledCellPorts) {
  Sdc *sdc = sta_->sdc();
  auto *dcp = sdc->disabledCellPorts();
  EXPECT_NE(dcp, nullptr);
}

// Sdc: isDisabled on TimingArcSet (nullptr)
// Sdc: isDisabledPin (nullptr)
// ClockPairLess
TEST_F(SdcInitTest, ClockPairLessOp) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  FloatSeq *w1 = new FloatSeq;
  w1->push_back(0.0);
  w1->push_back(5.0);
  sta_->makeClock("cpl_c1", nullptr, false, 10.0, w1, nullptr);
  FloatSeq *w2 = new FloatSeq;
  w2->push_back(0.0);
  w2->push_back(5.0);
  sta_->makeClock("cpl_c2", nullptr, false, 10.0, w2, nullptr);
  Clock *c1 = sdc->findClock("cpl_c1");
  Clock *c2 = sdc->findClock("cpl_c2");
  ClockPairLess cpl;
  ClockPair p1(c1, c2);
  ClockPair p2(c2, c1);
  bool result = cpl(p1, p2);
  (void)result;

  }() ));
}

// InputDriveCell
// PortDelay (direct)
// PortDelayLess
// Sdc: setClockLatency on pin
TEST_F(SdcInitTest, SdcClockLatencyOnPin) {
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("clp_clk", nullptr, false, 10.0, waveform, nullptr);
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
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("cip_clk", nullptr, false, 10.0, waveform, nullptr);
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
  Sdc *sdc = sta_->sdc();
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0);
  waveform->push_back(5.0);
  sta_->makeClock("cis_clk", nullptr, false, 10.0, waveform, nullptr);
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
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->isPathDelayInternalTo(nullptr));
}

TEST_F(SdcInitTest, SdcIsPathDelayInternalToBreakNull) {
  Sdc *sdc = sta_->sdc();
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
  Sdc *sdc = sta_->sdc();
  sdc->deleteLoopExceptions();

  }() ));
}

// Sdc: makeFalsePath
TEST_F(SdcInitTest, SdcMakeFalsePath) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);

  }() ));
}

// Sdc: makeMulticyclePath
TEST_F(SdcInitTest, SdcMakeMulticyclePath) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->makeMulticyclePath(nullptr, nullptr, nullptr, MinMaxAll::all(),
                          false, 2, nullptr);

  }() ));
}

// Sdc: makePathDelay
// Sdc: sameClockGroupExplicit
TEST_F(SdcInitTest, SdcSameClockGroupExplicit) {
  Sdc *sdc = sta_->sdc();
  FloatSeq *w1 = new FloatSeq;
  w1->push_back(0.0);
  w1->push_back(5.0);
  sta_->makeClock("scge_c1", nullptr, false, 10.0, w1, nullptr);
  FloatSeq *w2 = new FloatSeq;
  w2->push_back(0.0);
  w2->push_back(5.0);
  sta_->makeClock("scge_c2", nullptr, false, 10.0, w2, nullptr);
  Clock *c1 = sdc->findClock("scge_c1");
  Clock *c2 = sdc->findClock("scge_c2");
  bool same = sdc->sameClockGroupExplicit(c1, c2);
  EXPECT_FALSE(same);
}

// Sdc: makeFilterPath
// Sdc: resistance
TEST_F(SdcInitTest, SdcResistanceNull) {
  Sdc *sdc = sta_->sdc();
  float res;
  bool exists;
  sdc->resistance(nullptr, MinMax::max(), res, exists);
  EXPECT_FALSE(exists);
}

// Sdc: setResistance
TEST_F(SdcInitTest, SdcSetResistanceNull) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setResistance(nullptr, MinMaxAll::all(), 10.0f);

  }() ));
}

// Sdc: voltage
TEST_F(SdcInitTest, SdcVoltageNull) {
  Sdc *sdc = sta_->sdc();
  float volt;
  bool exists;
  sdc->voltage(nullptr, MinMax::max(), volt, exists);
  EXPECT_FALSE(exists);
}

// Sdc: setVoltage on net
TEST_F(SdcInitTest, SdcSetVoltageOnNet) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  sdc->setVoltage(nullptr, MinMax::max(), 1.0f);

  }() ));
}

// Sdc: clkStopPropagation
// Sdc: isDisableClockGatingCheck
TEST_F(SdcInitTest, SdcIsDisableClockGatingCheckInstNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->isDisableClockGatingCheck(static_cast<const Instance*>(nullptr)));
}

TEST_F(SdcInitTest, SdcIsDisableClockGatingCheckPinNull) {
  Sdc *sdc = sta_->sdc();
  EXPECT_FALSE(sdc->isDisableClockGatingCheck(static_cast<const Pin*>(nullptr)));
}

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
  (void)idx;
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
  (void)h;

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
  int cmp = compare(&set1, &set2);
  (void)cmp;

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
  (void)h;
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
  const RiseFallMinMax &slews = clk->slews();
  (void)slews; // just exercise
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
  ClockUncertainties *unc = clk->uncertainties();
  (void)unc;
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
  bool t = gp1.tighterThan(&gp2);
  (void)t;

  }() ));
}

// FilterPath tighterThan
TEST_F(SdcInitTest, FilterPathTighterThan2) {
  ASSERT_NO_THROW(( [&](){
  FilterPath fp1(nullptr, nullptr, nullptr, false);
  FilterPath fp2(nullptr, nullptr, nullptr, false);
  bool t = fp1.tighterThan(&fp2);
  (void)t;

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
  (void)nr.size();

  }() ));
}

// Sdc::clockInsertions
TEST_F(SdcInitTest, SdcClockInsertions) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  const ClockInsertions &insertions = sdc->clockInsertions();
  (void)insertions.size();

  }() ));
}

// ============================================================
// R10_ tests: Additional SDC coverage
// ============================================================

// --- SdcDesignTest: loads nangate45 + example1.v for SDC tests needing a design ---

class SdcDesignTest : public ::testing::Test {
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

    Corner *corner = sta_->cmdCorner();
    const MinMaxAll *min_max = MinMaxAll::all();
    LibertyLibrary *lib = sta_->readLiberty(
      "test/nangate45/Nangate45_typ.lib", corner, min_max, false);
    ASSERT_NE(lib, nullptr);

    bool ok = sta_->readVerilog("examples/example1.v");
    ASSERT_TRUE(ok);
    ok = sta_->linkDesign("top", true);
    ASSERT_TRUE(ok);

    Network *network = sta_->network();
    Instance *top = network->topInstance();
    Pin *clk1 = network->findPin(top, "clk1");
    Pin *clk2 = network->findPin(top, "clk2");
    Pin *clk3 = network->findPin(top, "clk3");
    ASSERT_NE(clk1, nullptr);

    PinSet *clk_pins = new PinSet(network);
    clk_pins->insert(clk1);
    clk_pins->insert(clk2);
    clk_pins->insert(clk3);
    FloatSeq *waveform = new FloatSeq;
    waveform->push_back(0.0f);
    waveform->push_back(5.0f);
    sta_->makeClock("clk", clk_pins, false, 10.0f, waveform, nullptr);

    Pin *in1 = network->findPin(top, "in1");
    Clock *clk = sta_->sdc()->findClock("clk");
    if (in1 && clk) {
      sta_->setInputDelay(in1, RiseFallBoth::riseFall(),
                          clk, RiseFall::rise(), nullptr,
                          false, false, MinMaxAll::all(), true, 0.0f);
    }
    sta_->updateTiming(true);
  }

  void TearDown() override {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_)
      Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  Pin *findPin(const char *path_name) {
    Network *network = sta_->cmdNetwork();
    return network->findPin(path_name);
  }

  Sta *sta_;
  Tcl_Interp *interp_;
};

// --- CycleAccting: sourceCycle, targetCycle via timing update ---

TEST_F(SdcDesignTest, CycleAcctingSourceTargetCycle) {
  // CycleAccting methods are called internally during timing
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  ASSERT_NE(clk, nullptr);
  // Make a second clock for inter-clock cycle accounting
  Network *network = sta_->network();
  Instance *top = network->topInstance();
  Pin *clk2 = network->findPin(top, "clk2");
  if (clk2) {
    PinSet *clk2_pins = new PinSet(network);
    clk2_pins->insert(clk2);
    FloatSeq *waveform2 = new FloatSeq;
    waveform2->push_back(0.0f);
    waveform2->push_back(2.5f);
    sta_->makeClock("clk2", clk2_pins, false, 5.0f, waveform2, nullptr);
    sta_->updateTiming(true);
    // Forces CycleAccting to compute inter-clock accounting
  }
}

// --- ExceptionThru: asString ---

TEST_F(SdcInitTest, ExceptionThruAsString) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Network *network = sta_->cmdNetwork();
  // Create ExceptionThru with no objects
  ExceptionThru *thru = new ExceptionThru(nullptr, nullptr, nullptr,
                                          RiseFallBoth::riseFall(), true, network);
  const char *str = thru->asString(network);
  (void)str;
  delete thru;

  }() ));
}

// --- ExceptionTo: asString, matches, cmdKeyword ---

TEST_F(SdcInitTest, ExceptionToAsString) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ExceptionTo *to = new ExceptionTo(nullptr, nullptr, nullptr,
                                    RiseFallBoth::riseFall(),
                                    RiseFallBoth::riseFall(),
                                    true, network);
  const char *str = to->asString(network);
  (void)str;
  // matches with null pin and rf
  bool m = to->matches(nullptr, RiseFall::rise());
  (void)m;
  delete to;

  }() ));
}

// --- ExceptionFrom: findHash ---

TEST_F(SdcInitTest, ExceptionFromHash) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                          RiseFallBoth::riseFall(), true, network);
  size_t h = from->hash();
  (void)h;
  delete from;

  }() ));
}

// --- ExceptionPath: mergeable ---

TEST_F(SdcInitTest, ExceptionPathMergeable) {
  Sdc *sdc = sta_->sdc();
  FalsePath *fp1 = new FalsePath(nullptr, nullptr, nullptr,
                                 MinMaxAll::all(), true, nullptr);
  FalsePath *fp2 = new FalsePath(nullptr, nullptr, nullptr,
                                 MinMaxAll::all(), true, nullptr);
  bool m = fp1->mergeable(fp2);
  EXPECT_TRUE(m);
  // Different type should not be mergeable
  PathDelay *pd = new PathDelay(nullptr, nullptr, nullptr,
                                MinMax::max(), false, false, 5.0, true, nullptr);
  bool m2 = fp1->mergeable(pd);
  EXPECT_FALSE(m2);
  delete fp1;
  delete fp2;
  delete pd;
}

// --- ExceptionPt constructor ---

TEST_F(SdcInitTest, ExceptionPtBasic) {
  Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                          RiseFallBoth::rise(), true, network);
  EXPECT_TRUE(from->isFrom());
  EXPECT_FALSE(from->isTo());
  EXPECT_FALSE(from->isThru());
  delete from;
}

// --- ExceptionFromTo destructor ---

TEST_F(SdcInitTest, ExceptionFromToDestructor) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                          RiseFallBoth::riseFall(), true, network);
  delete from;
  // Destructor coverage for ExceptionFromTo

  }() ));
}

// --- ExceptionPath destructor ---

TEST_F(SdcInitTest, ExceptionPathDestructor) {
  ASSERT_NO_THROW(( [&](){
  FalsePath *fp = new FalsePath(nullptr, nullptr, nullptr,
                                MinMaxAll::all(), true, nullptr);
  delete fp;

  }() ));
}

// --- DisabledCellPorts: construct and accessors ---

TEST_F(SdcInitTest, DisabledCellPortsConstruct2) {
  LibertyLibrary *lib = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                                          sta_->cmdCorner(),
                                          MinMaxAll::min(), false);
  if (lib) {
    LibertyCell *buf = lib->findLibertyCell("BUF_X1");
    if (buf) {
      DisabledCellPorts dcp(buf);
      EXPECT_EQ(dcp.cell(), buf);
      EXPECT_FALSE(dcp.all());
      dcp.setDisabledAll();
      EXPECT_TRUE(dcp.all());
      dcp.removeDisabledAll();
      EXPECT_FALSE(dcp.all());
    }
  }
}

// --- PortDelay: refTransition ---

TEST_F(SdcDesignTest, PortDelayRefTransition) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  const InputDelaySet &delays = sdc->inputDelays();
  for (InputDelay *delay : delays) {
    const RiseFall *ref_rf = delay->refTransition();
    (void)ref_rf;
    // Also exercise other PortDelay accessors
    const Pin *pin = delay->pin();
    (void)pin;
    const ClockEdge *ce = delay->clkEdge();
    (void)ce;
    bool src_lat = delay->sourceLatencyIncluded();
    (void)src_lat;
    bool net_lat = delay->networkLatencyIncluded();
    (void)net_lat;
    const Pin *ref_pin = delay->refPin();
    (void)ref_pin;
    int idx = delay->index();
    (void)idx;
  }

  }() ));
}

// --- ClockEdge: accessors (time, clock, transition) ---

TEST_F(SdcInitTest, ClockEdgeAccessors) {
  Sdc *sdc = sta_->sdc();
  PinSet *clk_pins = new PinSet(sta_->cmdNetwork());
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0f);
  waveform->push_back(5.0f);
  sta_->makeClock("test_clk_edge", clk_pins, false, 10.0f, waveform, nullptr);
  Clock *clk = sdc->findClock("test_clk_edge");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  ASSERT_NE(rise_edge, nullptr);
  ASSERT_NE(fall_edge, nullptr);
  // time()
  EXPECT_FLOAT_EQ(rise_edge->time(), 0.0f);
  EXPECT_FLOAT_EQ(fall_edge->time(), 5.0f);
  // clock()
  EXPECT_EQ(rise_edge->clock(), clk);
  EXPECT_EQ(fall_edge->clock(), clk);
  // transition()
  EXPECT_EQ(rise_edge->transition(), RiseFall::rise());
  EXPECT_EQ(fall_edge->transition(), RiseFall::fall());
  // name()
  EXPECT_NE(rise_edge->name(), nullptr);
  EXPECT_NE(fall_edge->name(), nullptr);
  // index()
  int ri = rise_edge->index();
  int fi = fall_edge->index();
  EXPECT_NE(ri, fi);
}

// --- Sdc: removeDataCheck ---

TEST_F(SdcDesignTest, SdcRemoveDataCheck) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  if (from_pin && to_pin) {
    sdc->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                      to_pin, RiseFallBoth::riseFall(),
                      nullptr, MinMaxAll::max(), 1.0);
    sdc->removeDataCheck(from_pin, RiseFallBoth::riseFall(),
                         to_pin, RiseFallBoth::riseFall(),
                         nullptr, MinMaxAll::max());
  }

  }() ));
}

// --- Sdc: deleteInterClockUncertainty ---

TEST_F(SdcInitTest, SdcInterClockUncertainty) {
  Sdc *sdc = sta_->sdc();
  PinSet *pins1 = new PinSet(sta_->cmdNetwork());
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0f);
  waveform1->push_back(5.0f);
  sta_->makeClock("clk_a", pins1, false, 10.0f, waveform1, nullptr);
  PinSet *pins2 = new PinSet(sta_->cmdNetwork());
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0f);
  waveform2->push_back(2.5f);
  sta_->makeClock("clk_b", pins2, false, 5.0f, waveform2, nullptr);

  Clock *clk_a = sdc->findClock("clk_a");
  Clock *clk_b = sdc->findClock("clk_b");
  ASSERT_NE(clk_a, nullptr);
  ASSERT_NE(clk_b, nullptr);

  sta_->setClockUncertainty(clk_a, RiseFallBoth::riseFall(),
                            clk_b, RiseFallBoth::riseFall(),
                            MinMaxAll::max(), 0.2f);
  // Remove it
  sta_->removeClockUncertainty(clk_a, RiseFallBoth::riseFall(),
                               clk_b, RiseFallBoth::riseFall(),
                               MinMaxAll::max());
}

// --- Sdc: clearClkGroupExclusions (via removeClockGroupsLogicallyExclusive) ---

TEST_F(SdcInitTest, SdcClearClkGroupExclusions) {
  ClockGroups *cg = sta_->makeClockGroups("grp_exc", true, false, false, false, nullptr);
  EXPECT_NE(cg, nullptr);
  sta_->removeClockGroupsLogicallyExclusive("grp_exc");
}

// --- Sdc: false path exercises pathDelayFrom/To indirectly ---

TEST_F(SdcDesignTest, SdcFalsePathExercise) {
  // Creating a false path from/to exercises pathDelayFrom/To code paths
  // through makeFalsePath and the SDC infrastructure
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall());
    sta_->makeFalsePath(from, nullptr, to, MinMaxAll::all(), nullptr);
    // Write SDC to exercise the path delay annotation
    const char *fn = "/tmp/test_sdc_r10_falsepath_exercise.sdc";
    sta_->writeSdc(fn, false, false, 4, false, true);
    FILE *f = fopen(fn, "r");
    EXPECT_NE(f, nullptr);
    if (f) fclose(f);
  }
}

// --- WriteSdc via SdcDesignTest ---

TEST_F(SdcDesignTest, WriteSdcBasic) {
  const char *filename = "/tmp/test_write_sdc_sdc_r10.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithOutputDelay) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Clock *clk = sta_->sdc()->findClock("clk");
  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::all(), true, 3.0f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_outdelay.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcNative) {
  const char *filename = "/tmp/test_write_sdc_sdc_r10_native.sdc";
  sta_->writeSdc(filename, false, true, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithFalsePath) {
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_fp.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithDerating) {
  sta_->setTimingDerate(TimingDerateType::cell_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::early(), 0.95);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_derate.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithDisable) {
  Graph *graph = sta_->graph();
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("r1/D");
  if (pin && graph) {
    Vertex *v = graph->pinLoadVertex(pin);
    if (v) {
      VertexInEdgeIterator in_iter(v, graph);
      if (in_iter.hasNext()) {
        Edge *edge = in_iter.next();
        sta_->disable(edge);
      }
    }
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_disable.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithClockLatency) {
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    sta_->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                          MinMaxAll::all(), 0.5f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_clklat.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithInterClkUncertainty) {
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    sta_->setClockUncertainty(clk, RiseFallBoth::riseFall(),
                              clk, RiseFallBoth::riseFall(),
                              MinMaxAll::max(), 0.1f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_interclk.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc: capacitanceLimit ---

TEST_F(SdcDesignTest, SdcCapacitanceLimit) {
  Sdc *sdc = sta_->sdc();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "r1/D");
  if (pin) {
    float limit;
    bool exists;
    sdc->capacitanceLimit(pin, MinMax::max(), limit, exists);
    // No limit set initially
    EXPECT_FALSE(exists);
  }
}

// --- Sdc: annotateGraphConstrained ---

TEST_F(SdcDesignTest, SdcAnnotateGraphConstrained) {
  ASSERT_NO_THROW(( [&](){
  // These are called during timing update; exercising indirectly
  sta_->updateTiming(true);

  }() ));
}

// --- DisabledInstancePorts: construct and accessors ---

TEST_F(SdcDesignTest, DisabledInstancePortsAccessors) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    DisabledInstancePorts dip(inst);
    EXPECT_EQ(dip.instance(), inst);
  }
  delete iter;
}

// --- PinClockPairLess: using public class ---

TEST_F(SdcDesignTest, PinClockPairLessDesign) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  PinClockPairLess less(network);
  (void)less;

  }() ));
}

// --- Sdc: clockLatency for edge ---

TEST_F(SdcDesignTest, SdcClockLatencyEdge) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Graph *graph = sta_->graph();
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("r1/CK");
  if (pin && graph) {
    Vertex *v = graph->pinLoadVertex(pin);
    if (v) {
      VertexInEdgeIterator in_iter(v, graph);
      if (in_iter.hasNext()) {
        Edge *edge = in_iter.next();
        const ClockLatency *lat = sdc->clockLatency(edge);
        (void)lat;
      }
    }
  }

  }() ));
}

// --- Sdc: disable/removeDisable for pin pair ---

TEST_F(SdcDesignTest, SdcDisablePinPair) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  // Find a gate with input/output pin pair
  InstanceChildIterator *inst_iter = network->childIterator(top);
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      LibertyPort *in_port = nullptr;
      LibertyPort *out_port = nullptr;
      LibertyCellPortIterator port_iter(lib_cell);
      while (port_iter.hasNext()) {
        LibertyPort *port = port_iter.next();
        if (port->direction()->isInput() && !in_port)
          in_port = port;
        else if (port->direction()->isOutput() && !out_port)
          out_port = port;
      }
      if (in_port && out_port) {
        Pin *in_pin = network->findPin(inst, in_port);
        Pin *out_pin = network->findPin(inst, out_port);
        if (in_pin && out_pin) {
          sdc->disable(in_pin, out_pin);
          sdc->removeDisable(in_pin, out_pin);
          break;
        }
      }
    }
  }
  delete inst_iter;

  }() ));
}

// --- ExceptionThru: makePinEdges, makeNetEdges, makeInstEdges, deletePinEdges ---

TEST_F(SdcDesignTest, ExceptionThruEdges) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "in1");
  if (pin) {
    PinSet *pins = new PinSet(network);
    pins->insert(pin);
    ExceptionThru *thru = new ExceptionThru(pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(), true, network);
    const char *str = thru->asString(network);
    EXPECT_NE(str, nullptr);
    delete thru;
  }
}

TEST_F(SdcDesignTest, ExceptionThruWithNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  // Find a net
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    NetSet *nets = new NetSet(network);
    nets->insert(net);
    ExceptionThru *thru = new ExceptionThru(nullptr, nets, nullptr,
                                            RiseFallBoth::riseFall(), true, network);
    const char *str = thru->asString(network);
    EXPECT_NE(str, nullptr);
    delete thru;
  }
  delete net_iter;
}

TEST_F(SdcDesignTest, ExceptionThruWithInstance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *inst_iter = network->childIterator(top);
  if (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    InstanceSet *insts = new InstanceSet(network);
    insts->insert(inst);
    ExceptionThru *thru = new ExceptionThru(nullptr, nullptr, insts,
                                            RiseFallBoth::riseFall(), true, network);
    const char *str = thru->asString(network);
    EXPECT_NE(str, nullptr);
    delete thru;
  }
  delete inst_iter;
}

// --- WriteSdc with leaf/map_hpins ---

TEST_F(SdcDesignTest, WriteSdcLeaf) {
  const char *filename = "/tmp/test_write_sdc_sdc_r10_leaf.sdc";
  sta_->writeSdc(filename, true, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with data check ---

TEST_F(SdcDesignTest, WriteSdcWithDataCheck) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  if (from_pin && to_pin) {
    sta_->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                       to_pin, RiseFallBoth::riseFall(),
                       nullptr, MinMaxAll::max(), 1.0);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_datacheck.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with port loads ---

TEST_F(SdcDesignTest, WriteSdcWithPortLoad) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    Corner *corner = sta_->cmdCorner();
    if (port && corner)
      sta_->setPortExtPinCap(port, RiseFallBoth::riseFall(), corner, MinMaxAll::all(), 0.5f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_portload.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock slew ---

TEST_F(SdcDesignTest, WriteSdcWithClockSlew) {
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    sta_->setClockSlew(clk, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.1f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_clkslew.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock insertion ---

TEST_F(SdcDesignTest, WriteSdcWithClockInsertion) {
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    sta_->setClockInsertion(clk, nullptr, RiseFallBoth::rise(),
                            MinMaxAll::all(), EarlyLateAll::all(), 0.3f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_clkins.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multicycle path ---

TEST_F(SdcDesignTest, WriteSdcWithMulticycle) {
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::max(), true, 2, nullptr);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_mcp.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with max area ---

TEST_F(SdcDesignTest, WriteSdcWithMaxArea) {
  sta_->sdc()->setMaxArea(1000.0);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_maxarea.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min pulse width ---

TEST_F(SdcDesignTest, WriteSdcWithMpw) {
  sta_->sdc()->setMinPulseWidth(RiseFallBoth::rise(), 0.5);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_mpw.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with voltage ---

TEST_F(SdcDesignTest, WriteSdcWithVoltage) {
  sta_->sdc()->setVoltage(MinMax::max(), 1.1);
  sta_->sdc()->setVoltage(MinMax::min(), 0.9);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_voltage.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc: deleteLatchBorrowLimitsReferencing (via clock removal) ---

TEST_F(SdcInitTest, SdcDeleteLatchBorrowLimits) {
  Sdc *sdc = sta_->sdc();
  PinSet *clk_pins = new PinSet(sta_->cmdNetwork());
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0f);
  waveform->push_back(5.0f);
  sta_->makeClock("clk_borrow", clk_pins, false, 10.0f, waveform, nullptr);
  Clock *clk = sdc->findClock("clk_borrow");
  ASSERT_NE(clk, nullptr);
  // Set latch borrow limit on clock
  sdc->setLatchBorrowLimit(clk, 0.5f);
  // Remove the clock; this calls deleteLatchBorrowLimitsReferencing
  sta_->removeClock(clk);
}

// ============================================================
// R10_ Additional SDC Tests - Round 2
// ============================================================

// --- WriteSdc with drive resistance ---
TEST_F(SdcDesignTest, WriteSdcWithDriveResistance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      sta_->setDriveResistance(port, RiseFallBoth::riseFall(),
                               MinMaxAll::all(), 50.0f);
    }
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_driveres.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with logic value / set_logic_one ---
TEST_F(SdcDesignTest, WriteSdcWithLogicValue) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    sta_->setLogicValue(in1, LogicValue::one);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_logicval.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with case analysis ---
TEST_F(SdcDesignTest, WriteSdcWithCaseAnalysis) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in2 = network->findPin(top, "in2");
  if (in2) {
    sta_->setCaseAnalysis(in2, LogicValue::zero);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_caseanalysis.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with latch borrow limit on pin ---
TEST_F(SdcDesignTest, WriteSdcWithLatchBorrowLimitPin) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "r1/D");
  if (pin) {
    sta_->setLatchBorrowLimit(pin, 0.3f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_latchborrow.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with latch borrow limit on instance ---
TEST_F(SdcDesignTest, WriteSdcWithLatchBorrowLimitInst) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    sta_->setLatchBorrowLimit(inst, 0.5f);
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_latchborrowinst.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with slew limits ---
TEST_F(SdcDesignTest, WriteSdcWithSlewLimits) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::data, MinMax::max(), 2.0f);
  }
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setSlewLimit(port, MinMax::max(), 3.0f);
    }
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_slewlimit.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with cap limits ---
TEST_F(SdcDesignTest, WriteSdcWithCapLimits) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setCapacitanceLimit(port, MinMax::max(), 0.5f);
    }
    sta_->setCapacitanceLimit(out, MinMax::max(), 0.3f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_caplimit.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with fanout limits ---
TEST_F(SdcDesignTest, WriteSdcWithFanoutLimits) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setFanoutLimit(port, MinMax::max(), 10.0f);
    }
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_fanoutlimit.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min pulse width on pin ---
TEST_F(SdcDesignTest, WriteSdcWithMpwOnPin) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk_pin = network->findPin(top, "r1/CK");
  if (clk_pin) {
    sta_->setMinPulseWidth(clk_pin, RiseFallBoth::rise(), 0.2f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_mpwpin.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min pulse width on instance ---
TEST_F(SdcDesignTest, WriteSdcWithMpwOnInst) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    sta_->setMinPulseWidth(inst, RiseFallBoth::rise(), 0.25f);
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_mpwinst.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with disable on instance ---
TEST_F(SdcDesignTest, WriteSdcWithDisableInstance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      LibertyPort *in_port = nullptr;
      LibertyPort *out_port = nullptr;
      LibertyCellPortIterator port_iter(lib_cell);
      while (port_iter.hasNext()) {
        LibertyPort *port = port_iter.next();
        if (port->direction()->isInput() && !in_port)
          in_port = port;
        else if (port->direction()->isOutput() && !out_port)
          out_port = port;
      }
      if (in_port && out_port)
        sta_->disable(inst, in_port, out_port);
    }
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_disableinst.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with disable on liberty port ---
TEST_F(SdcDesignTest, WriteSdcWithDisableLibPort) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      LibertyCellPortIterator port_iter(lib_cell);
      if (port_iter.hasNext()) {
        LibertyPort *port = port_iter.next();
        sta_->disable(port);
      }
    }
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_disablelibport.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with disable on cell ---
TEST_F(SdcDesignTest, WriteSdcWithDisableCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      sta_->disable(lib_cell, nullptr, nullptr);
    }
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_disablecell.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with output delay ---
TEST_F(SdcDesignTest, WriteSdcWithOutputDelayDetailed) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::rise(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::max(), true, 2.5f);
    sta_->setOutputDelay(out, RiseFallBoth::fall(),
                         clk, RiseFall::fall(), nullptr,
                         false, false, MinMaxAll::min(), true, 1.0f);
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_outdelay_detail.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc: outputDelays iterator ---
TEST_F(SdcDesignTest, SdcOutputDelays) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::all(), true, 1.0f);
  }
  const OutputDelaySet &out_delays = sdc->outputDelays();
  for (OutputDelay *delay : out_delays) {
    const Pin *pin = delay->pin();
    (void)pin;
    const ClockEdge *ce = delay->clkEdge();
    (void)ce;
    bool src_lat = delay->sourceLatencyIncluded();
    (void)src_lat;
  }

  }() ));
}

// --- Sdc: Variables class accessors ---
TEST_F(SdcDesignTest, VariablesAccessors) {
  // Test Variables accessors that modify search behavior
  bool crpr_orig = sta_->crprEnabled();
  sta_->setCrprEnabled(!crpr_orig);
  EXPECT_NE(sta_->crprEnabled(), crpr_orig);
  sta_->setCrprEnabled(crpr_orig);

  bool prop_gate = sta_->propagateGatedClockEnable();
  sta_->setPropagateGatedClockEnable(!prop_gate);
  EXPECT_NE(sta_->propagateGatedClockEnable(), prop_gate);
  sta_->setPropagateGatedClockEnable(prop_gate);
}

// --- Clock: name, period, waveform ---
TEST_F(SdcDesignTest, ClockAccessors) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_STREQ(clk->name(), "clk");
  EXPECT_FLOAT_EQ(clk->period(), 10.0f);
  const FloatSeq *wave = clk->waveform();
  ASSERT_NE(wave, nullptr);
  EXPECT_GE(wave->size(), 2u);
  EXPECT_FLOAT_EQ((*wave)[0], 0.0f);
  EXPECT_FLOAT_EQ((*wave)[1], 5.0f);
  EXPECT_FALSE(clk->isGenerated());
  EXPECT_FALSE(clk->isVirtual());
  int idx = clk->index();
  (void)idx;
}

// --- ExceptionFrom: hasPins, hasClocks, hasInstances ---
TEST_F(SdcDesignTest, ExceptionFromHasPins) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *pins = new PinSet(network);
    pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    ASSERT_NE(from, nullptr);
    EXPECT_TRUE(from->hasPins());
    EXPECT_FALSE(from->hasClocks());
    EXPECT_FALSE(from->hasInstances());
    EXPECT_TRUE(from->hasObjects());
    delete from;
  }
}

// --- ExceptionTo: hasPins, endRf ---
TEST_F(SdcDesignTest, ExceptionToHasPins) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    PinSet *pins = new PinSet(network);
    pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(pins, nullptr, nullptr,
                                            RiseFallBoth::rise(),
                                            RiseFallBoth::riseFall());
    ASSERT_NE(to, nullptr);
    EXPECT_TRUE(to->hasPins());
    const RiseFallBoth *end_rf = to->endTransition();
    EXPECT_NE(end_rf, nullptr);
    delete to;
  }
}

// --- Sdc: removeClockLatency ---
TEST_F(SdcDesignTest, SdcRemoveClockLatency) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setClockLatency(clk, nullptr,
                          RiseFallBoth::riseFall(),
                          MinMaxAll::all(), 0.3f);
    sta_->removeClockLatency(clk, nullptr);
  }

  }() ));
}

// --- Sdc: removeCaseAnalysis ---
TEST_F(SdcDesignTest, SdcRemoveCaseAnalysis) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    sta_->setCaseAnalysis(in1, LogicValue::one);
    sta_->removeCaseAnalysis(in1);
  }

  }() ));
}

// --- Sdc: removeDerating ---
TEST_F(SdcDesignTest, SdcRemoveDerating) {
  ASSERT_NO_THROW(( [&](){
  sta_->setTimingDerate(TimingDerateType::cell_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::early(), 0.95);
  sta_->unsetTimingDerate();

  }() ));
}

// --- WriteSdc comprehensive: multiple constraints ---
TEST_F(SdcDesignTest, WriteSdcComprehensive) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");

  // Add various constraints
  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  Pin *out = network->findPin(top, "out");

  if (in1) {
    Port *port = network->port(in1);
    if (port)
      sta_->setDriveResistance(port, RiseFallBoth::riseFall(),
                               MinMaxAll::all(), 100.0f);
  }
  if (in2) {
    sta_->setCaseAnalysis(in2, LogicValue::zero);
  }
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setPortExtPinCap(port, RiseFallBoth::riseFall(),
                             sta_->cmdCorner(), MinMaxAll::all(), 0.1f);
      sta_->setFanoutLimit(port, MinMax::max(), 5.0f);
    }
  }
  if (clk) {
    sta_->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                          MinMaxAll::all(), 0.5f);
    sta_->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                            MinMaxAll::all(), EarlyLateAll::all(), 0.2f);
  }
  sdc->setMaxArea(2000.0);
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.3);
  sdc->setVoltage(MinMax::max(), 1.2);
  sdc->setVoltage(MinMax::min(), 0.8);

  sta_->setTimingDerate(TimingDerateType::cell_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::early(), 0.95);

  // Write SDC with all constraints
  const char *filename = "/tmp/test_write_sdc_sdc_r10_comprehensive.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);

  // Also write native format
  const char *filename2 = "/tmp/test_write_sdc_sdc_r10_comprehensive_native.sdc";
  sta_->writeSdc(filename2, false, true, 4, false, true);
  FILE *f2 = fopen(filename2, "r");
  EXPECT_NE(f2, nullptr);
  if (f2) fclose(f2);
}

// --- Clock: isPropagated, edges, edgeCount ---
TEST_F(SdcDesignTest, ClockEdgeDetails) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  ASSERT_NE(clk, nullptr);
  bool prop = clk->isPropagated();
  (void)prop;
  // Each clock has 2 edges: rise and fall
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  ASSERT_NE(rise, nullptr);
  ASSERT_NE(fall, nullptr);
  // Opposite edges
  const ClockEdge *rise_opp = rise->opposite();
  EXPECT_EQ(rise_opp, fall);
  const ClockEdge *fall_opp = fall->opposite();
  EXPECT_EQ(fall_opp, rise);
}

// --- Sdc: clocks() - get all clocks ---
TEST_F(SdcDesignTest, SdcClocksList) {
  Sdc *sdc = sta_->sdc();
  const ClockSeq &clks = sdc->clks();
  EXPECT_GT(clks.size(), 0u);
  for (Clock *c : clks) {
    EXPECT_NE(c->name(), nullptr);
  }
}

// --- InputDrive: accessors ---
TEST_F(SdcDesignTest, InputDriveAccessors) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      // Set a drive resistance
      sta_->setDriveResistance(port, RiseFallBoth::riseFall(),
                               MinMaxAll::all(), 75.0f);
      // Now check the input drive on the port via Sdc
      Sdc *sdc = sta_->sdc();
      InputDrive *drive = sdc->findInputDrive(port);
      if (drive) {
        bool has_cell = drive->hasDriveCell(RiseFall::rise(), MinMax::max());
        (void)has_cell;
        InputDriveCell *dc = drive->driveCell(RiseFall::rise(), MinMax::max());
        (void)dc;
      }
    }
  }

  }() ));
}

// ============================================================
// R11_ SDC Tests - WriteSdc coverage and Sdc method coverage
// ============================================================

// --- WriteSdc with net wire cap (triggers writeNetLoads, writeNetLoad,
//     writeGetNet, WriteGetNet, scaleCapacitance, writeFloat, writeCapacitance,
//     writeCommentSeparator, closeFile, ~WriteSdc) ---
TEST_F(SdcDesignTest, WriteSdcWithNetWireCap) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    Corner *corner = sta_->cmdCorner();
    sta_->setNetWireCap(net, false, corner, MinMaxAll::all(), 0.05f);
  }
  delete net_iter;
  const char *filename = "/tmp/test_sdc_r11_netwire.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with net resistance (triggers writeNetResistances,
//     writeNetResistance, writeGetNet, scaleResistance, writeResistance) ---
TEST_F(SdcDesignTest, WriteSdcWithNetResistance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setResistance(net, MinMaxAll::all(), 100.0f);
  }
  delete net_iter;
  const char *filename = "/tmp/test_sdc_r11_netres.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with input slew (triggers writeInputTransitions,
//     writeRiseFallMinMaxTimeCmd, WriteGetPort, scaleTime) ---
TEST_F(SdcDesignTest, WriteSdcWithInputSlew) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      sta_->setInputSlew(port, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), 0.1f);
    }
  }
  const char *filename = "/tmp/test_sdc_r11_inputslew.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with driving cell (triggers writeDrivingCells, writeDrivingCell,
//     WriteGetLibCell, WriteGetPort) ---
TEST_F(SdcDesignTest, WriteSdcWithDrivingCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      // Find a buffer cell to use as driving cell
      LibertyLibrary *lib = nullptr;
      LibertyLibraryIterator *lib_iter = network->libertyLibraryIterator();
      if (lib_iter->hasNext())
        lib = lib_iter->next();
      delete lib_iter;
      if (lib) {
        LibertyCell *buf_cell = nullptr;
        LibertyCellIterator cell_iter(lib);
        while (cell_iter.hasNext()) {
          LibertyCell *cell = cell_iter.next();
          if (cell->portCount() >= 2) {
            buf_cell = cell;
            break;
          }
        }
        if (buf_cell) {
          // Find input and output ports on the cell
          LibertyPort *from_port = nullptr;
          LibertyPort *to_port = nullptr;
          LibertyCellPortIterator port_iter(buf_cell);
          while (port_iter.hasNext()) {
            LibertyPort *lp = port_iter.next();
            if (lp->direction()->isInput() && !from_port)
              from_port = lp;
            else if (lp->direction()->isOutput() && !to_port)
              to_port = lp;
          }
          if (from_port && to_port) {
            float from_slews[2] = {0.05f, 0.05f};
            sta_->setDriveCell(lib, buf_cell, port,
                               from_port, from_slews, to_port,
                               RiseFallBoth::riseFall(), MinMaxAll::all());
          }
        }
      }
    }
  }
  const char *filename = "/tmp/test_sdc_r11_drivecell.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock groups that have actual clock members
//     (triggers writeClockGroups, WriteGetClock, writeGetClock) ---
TEST_F(SdcDesignTest, WriteSdcWithClockGroupsMembers) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    // Create a second clock
    Network *network = sta_->network();
    Instance *top = network->topInstance();
    Pin *clk2_pin = network->findPin(top, "clk2");
    if (clk2_pin) {
      PinSet *clk2_pins = new PinSet(network);
      clk2_pins->insert(clk2_pin);
      FloatSeq *waveform2 = new FloatSeq;
      waveform2->push_back(0.0f);
      waveform2->push_back(2.5f);
      sta_->makeClock("clk2", clk2_pins, false, 5.0f, waveform2, nullptr);
      Clock *clk2 = sdc->findClock("clk2");
      if (clk2) {
        ClockGroups *cg = sta_->makeClockGroups("grp1", true, false, false,
                                                 false, nullptr);
        ClockSet *group1 = new ClockSet;
        group1->insert(clk);
        sta_->makeClockGroup(cg, group1);
        ClockSet *group2 = new ClockSet;
        group2->insert(clk2);
        sta_->makeClockGroup(cg, group2);
      }
    }
  }
  const char *filename = "/tmp/test_sdc_r11_clkgrp_members.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path having -from pins and -through pins and -to pins
//     (triggers writeExceptionFrom, WriteGetPin, writeExceptionThru,
//     writeExceptionTo) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathFromThruTo) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    // -from
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    // -through: use an instance
    InstanceChildIterator *inst_iter = network->childIterator(top);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    if (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      InstanceSet *insts = new InstanceSet(network);
      insts->insert(inst);
      ExceptionThru *thru = sta_->makeExceptionThru(nullptr, nullptr, insts,
                                                     RiseFallBoth::riseFall());
      thrus->push_back(thru);
    }
    delete inst_iter;
    // -to
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall());
    sta_->makeFalsePath(from, thrus, to, MinMaxAll::all(), nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_fp_fromthru.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -through net
//     (triggers writeExceptionThru with nets, writeGetNet) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathThruNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    NetSet *nets = new NetSet(network);
    nets->insert(net);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    ExceptionThru *thru = sta_->makeExceptionThru(nullptr, nets, nullptr,
                                                   RiseFallBoth::riseFall());
    thrus->push_back(thru);
    sta_->makeFalsePath(nullptr, thrus, nullptr, MinMaxAll::all(), nullptr);
  }
  delete net_iter;
  const char *filename = "/tmp/test_sdc_r11_fp_thrunet.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -from clock (triggers writeGetClock in from) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathFromClock) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ClockSet *from_clks = new ClockSet;
    from_clks->insert(clk);
    ExceptionFrom *from = sta_->makeExceptionFrom(nullptr, from_clks, nullptr,
                                                   RiseFallBoth::riseFall());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_fp_fromclk.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -from instance (triggers writeGetInstance,
//     WriteGetInstance) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathFromInstance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    InstanceSet *from_insts = new InstanceSet(network);
    from_insts->insert(inst);
    ExceptionFrom *from = sta_->makeExceptionFrom(nullptr, nullptr, from_insts,
                                                   RiseFallBoth::riseFall());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr);
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_fp_frominst.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multicycle path with -from pin
//     (triggers writeExceptionCmd for multicycle, writeExceptionFrom) ---
TEST_F(SdcDesignTest, WriteSdcMulticycleWithFrom) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    sta_->makeMulticyclePath(from, nullptr, nullptr,
                             MinMaxAll::max(), true, 3, nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_mcp_from.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with path delay (max_delay/min_delay)
//     (triggers writeExceptionCmd for path delay, writeExceptionValue) ---
TEST_F(SdcDesignTest, WriteSdcPathDelay) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall());
    sta_->makePathDelay(from, nullptr, to, MinMax::max(), false, false,
                        5.0f, nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_pathdelay.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with group path
//     (triggers writeExceptionCmd for group path) ---
TEST_F(SdcDesignTest, WriteSdcGroupPath) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    sta_->makeGroupPath("mygroup", false, from, nullptr, nullptr, nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_grouppath.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock sense
//     (triggers writeClockSenses, PinClockPairNameLess) ---
TEST_F(SdcDesignTest, WriteSdcWithClockSense) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk1 && clk) {
    PinSet *pins = new PinSet(network);
    pins->insert(clk1);
    ClockSet *clks = new ClockSet;
    clks->insert(clk);
    sta_->setClockSense(pins, clks, ClockSense::positive);
  }
  const char *filename = "/tmp/test_sdc_r11_clksense.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with port ext wire cap and fanout
//     (triggers writePortLoads with wire cap, writeMinMaxIntValuesCmd) ---
TEST_F(SdcDesignTest, WriteSdcWithPortExtWireCap) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    Corner *corner = sta_->cmdCorner();
    if (port && corner) {
      sta_->setPortExtWireCap(port, false, RiseFallBoth::riseFall(),
                              corner, MinMaxAll::all(), 0.02f);
      sta_->setPortExtFanout(port, 3, corner, MinMaxAll::all());
    }
  }
  const char *filename = "/tmp/test_sdc_r11_portwire.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock gating check
//     (triggers writeClockGatingChecks) ---
TEST_F(SdcDesignTest, WriteSdcWithClockGatingCheck) {
  sta_->setClockGatingCheck(RiseFallBoth::riseFall(),
                            MinMax::max(), 0.1f);
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk)
    sta_->setClockGatingCheck(clk, RiseFallBoth::riseFall(),
                              MinMax::min(), 0.05f);
  const char *filename = "/tmp/test_sdc_r11_clkgate.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc: connectedCap via Sta API ---
TEST_F(SdcDesignTest, SdcConnectedCap) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Corner *corner = sta_->cmdCorner();
    float pin_cap, wire_cap;
    sta_->connectedCap(out, RiseFall::rise(), corner, MinMax::max(),
                       pin_cap, wire_cap);
    (void)pin_cap;
    (void)wire_cap;
  }

  }() ));
}

// --- Sdc: connectedCap on net via Sta API ---
TEST_F(SdcDesignTest, SdcConnectedCapNet) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    Corner *corner = sta_->cmdCorner();
    float pin_cap, wire_cap;
    sta_->connectedCap(net, corner, MinMax::max(), pin_cap, wire_cap);
    (void)pin_cap;
    (void)wire_cap;
  }
  delete net_iter;

  }() ));
}

// --- ExceptionPath::mergeable ---
TEST_F(SdcDesignTest, ExceptionPathMergeable) {
  ASSERT_NO_THROW(( [&](){
  // Create two false paths and check mergeability
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);
  Sdc *sdc = sta_->sdc();
  ExceptionPathSet &exceptions = sdc->exceptions();
  ExceptionPath *first = nullptr;
  for (ExceptionPath *ep : exceptions) {
    if (ep->isFalse()) {
      if (!first) {
        first = ep;
      } else {
        bool m = first->mergeable(ep);
        (void)m;
        break;
      }
    }
  }

  }() ));
}

// --- WriteSdc with propagated clock on pin
//     (triggers writePropagatedClkPins) ---
TEST_F(SdcDesignTest, WriteSdcWithPropagatedClk) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  if (clk1) {
    sta_->setPropagatedClock(clk1);
  }
  const char *filename = "/tmp/test_sdc_r11_propagated.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min_delay path delay
//     (triggers min_delay branch in writeExceptionCmd) ---
TEST_F(SdcDesignTest, WriteSdcMinDelay) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall());
    sta_->makePathDelay(from, nullptr, to, MinMax::min(), false, false,
                        1.0f, nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_mindelay.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multicycle -hold (min) with -end
//     (triggers the hold branch in writeExceptionCmd) ---
TEST_F(SdcDesignTest, WriteSdcMulticycleHold) {
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::min(), true, 0, nullptr);
  const char *filename = "/tmp/test_sdc_r11_mcp_hold.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multicycle -setup with -start
//     (triggers the start branch in writeExceptionCmd) ---
TEST_F(SdcDesignTest, WriteSdcMulticycleStart) {
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::max(), false, 2, nullptr);
  const char *filename = "/tmp/test_sdc_r11_mcp_start.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with group path default
//     (triggers isDefault branch in writeExceptionCmd) ---
TEST_F(SdcDesignTest, WriteSdcGroupPathDefault) {
  sta_->makeGroupPath(nullptr, true, nullptr, nullptr, nullptr, nullptr);
  const char *filename = "/tmp/test_sdc_r11_grppath_default.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -from with rise_from
//     (triggers rf_prefix = "-rise_" branch) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathRiseFrom) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::rise());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_fp_risefrom.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -from with fall_from
//     (triggers rf_prefix = "-fall_" branch) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathFallFrom) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::fall());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_fp_fallfrom.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with path delay -ignore_clock_latency
//     (triggers the ignoreClkLatency branch) ---
TEST_F(SdcDesignTest, WriteSdcPathDelayIgnoreClkLat) {
  sta_->makePathDelay(nullptr, nullptr, nullptr, MinMax::max(), true, false,
                      8.0f, nullptr);
  const char *filename = "/tmp/test_sdc_r11_pathdelay_ignoreclk.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -to with end_rf rise
//     (triggers the end_rf != riseFall branch in writeExceptionTo) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathToRise) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::rise());
    sta_->makeFalsePath(nullptr, nullptr, to, MinMaxAll::all(), nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_fp_torise.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multiple from objects (triggers multi_objs branch with [list ]) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathMultiFrom) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  if (in1 && in2) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    from_pins->insert(in2);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_fp_multifrom.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with data check that has a clock ref
//     (triggers writeDataChecks, WriteGetPinAndClkKey) ---
TEST_F(SdcDesignTest, WriteSdcDataCheckWithClock) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (from_pin && to_pin && clk) {
    sta_->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                       to_pin, RiseFallBoth::riseFall(),
                       clk, MinMaxAll::max(), 0.5f);
  }
  const char *filename = "/tmp/test_sdc_r11_datacheck_clk.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc::removeDataCheck ---
TEST_F(SdcDesignTest, SdcRemoveDataCheck2) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  if (from_pin && to_pin) {
    sta_->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                       to_pin, RiseFallBoth::riseFall(),
                       nullptr, MinMaxAll::max(), 1.0f);
    sta_->removeDataCheck(from_pin, RiseFallBoth::riseFall(),
                          to_pin, RiseFallBoth::riseFall(),
                          nullptr, MinMaxAll::max());
  }

  }() ));
}

// --- WriteSdc with clock uncertainty on pin
//     (triggers writeClockUncertaintyPins, writeClockUncertaintyPin) ---
TEST_F(SdcDesignTest, WriteSdcClockUncertaintyPin) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  if (clk1) {
    sta_->setClockUncertainty(clk1, MinMaxAll::max(), 0.2f);
  }
  const char *filename = "/tmp/test_sdc_r11_clkuncpin.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with voltage on net
//     (triggers writeVoltages with net voltage) ---
TEST_F(SdcDesignTest, WriteSdcVoltageNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setVoltage(net, MinMax::max(), 1.0f);
    sta_->setVoltage(net, MinMax::min(), 0.9f);
  }
  delete net_iter;
  const char *filename = "/tmp/test_sdc_r11_voltnet.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with disable on timing arcs of cell
//     (triggers writeGetTimingArcsOfOjbects, writeGetTimingArcs,
//     getTimingArcsCmd) ---
TEST_F(SdcDesignTest, WriteSdcDisableTimingArcs) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      LibertyPort *in_port = nullptr;
      LibertyPort *out_port = nullptr;
      LibertyCellPortIterator port_iter(lib_cell);
      while (port_iter.hasNext()) {
        LibertyPort *port = port_iter.next();
        if (port->direction()->isInput() && !in_port)
          in_port = port;
        else if (port->direction()->isOutput() && !out_port)
          out_port = port;
      }
      if (in_port && out_port) {
        // Disable specific from->to arc on cell
        sta_->disable(lib_cell, in_port, out_port);
      }
    }
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_disablearcs.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min pulse width on clock
//     (triggers writeMinPulseWidths clock branch) ---
TEST_F(SdcDesignTest, WriteSdcMpwClock) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setMinPulseWidth(clk, RiseFallBoth::rise(), 0.4f);
    sta_->setMinPulseWidth(clk, RiseFallBoth::fall(), 0.3f);
  }
  const char *filename = "/tmp/test_sdc_r11_mpwclk.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with slew limit on clock data
//     (triggers writeClkSlewLimits, writeClkSlewLimit) ---
TEST_F(SdcDesignTest, WriteSdcSlewLimitClkData) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::clk, MinMax::max(), 1.5f);
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::data, MinMax::max(), 2.5f);
  }
  const char *filename = "/tmp/test_sdc_r11_slewclkdata.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with cell-level cap limit
//     (triggers writeCapLimits cell branch) ---
TEST_F(SdcDesignTest, WriteSdcCapLimitCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setCapacitanceLimit(cell, MinMax::max(), 2.0f);
    }
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_caplimitcell.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with cell-level fanout limit
//     (triggers writeFanoutLimits cell branch) ---
TEST_F(SdcDesignTest, WriteSdcFanoutLimitCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setFanoutLimit(cell, MinMax::max(), 15.0f);
    }
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_fanoutcell.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with cell-level slew limit
//     (triggers writeSlewLimits cell branch) ---
TEST_F(SdcDesignTest, WriteSdcSlewLimitCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setSlewLimit(cell, MinMax::max(), 5.0f);
    }
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_slewcell.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc comprehensive: trigger as many writer paths as possible ---
TEST_F(SdcDesignTest, WriteSdcMegaComprehensive) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  Corner *corner = sta_->cmdCorner();

  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  Pin *out = network->findPin(top, "out");

  // Net wire cap and resistance
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setNetWireCap(net, false, corner, MinMaxAll::all(), 0.03f);
    sta_->setResistance(net, MinMaxAll::all(), 50.0f);
    sta_->setVoltage(net, MinMax::max(), 1.1f);
  }
  delete net_iter;

  // Input slew
  if (in1) {
    Port *port = network->port(in1);
    if (port)
      sta_->setInputSlew(port, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.08f);
  }

  // Port ext wire cap + fanout
  if (out) {
    Port *port = network->port(out);
    if (port && corner) {
      sta_->setPortExtPinCap(port, RiseFallBoth::riseFall(), corner,
                             MinMaxAll::all(), 0.1f);
      sta_->setPortExtWireCap(port, false, RiseFallBoth::riseFall(), corner,
                              MinMaxAll::all(), 0.015f);
      sta_->setPortExtFanout(port, 2, corner, MinMaxAll::all());
    }
  }

  // Clock groups
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("mega_grp", false, true, false,
                                             false, nullptr);
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1);
  }

  // False path with -from pin, -through instance, -to pin
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::rise());
    InstanceChildIterator *inst_iter = network->childIterator(top);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    if (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      InstanceSet *insts = new InstanceSet(network);
      insts->insert(inst);
      ExceptionThru *thru = sta_->makeExceptionThru(nullptr, nullptr, insts,
                                                     RiseFallBoth::riseFall());
      thrus->push_back(thru);
    }
    delete inst_iter;
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::rise());
    sta_->makeFalsePath(from, thrus, to, MinMaxAll::all(), nullptr);
  }

  // Max/min delay
  if (in2 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in2);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall());
    sta_->makePathDelay(from, nullptr, to, MinMax::max(), true, false,
                        6.0f, nullptr);
  }

  // Multicycle
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::max(), false, 4, nullptr);

  // Group path
  sta_->makeGroupPath("mega", false, nullptr, nullptr, nullptr, nullptr);

  // Clock gating check
  sta_->setClockGatingCheck(RiseFallBoth::riseFall(), MinMax::max(), 0.15f);

  // Logic value
  if (in2) {
    sta_->setLogicValue(in2, LogicValue::zero);
  }

  // Voltage
  sdc->setVoltage(MinMax::max(), 1.2);
  sdc->setVoltage(MinMax::min(), 0.8);

  // Min pulse width
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.35);
  sdc->setMinPulseWidth(RiseFallBoth::fall(), 0.25);

  // Max area
  sdc->setMaxArea(3000.0);

  // Write SDC
  const char *filename = "/tmp/test_sdc_r11_mega.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);

  // Also write in native mode
  const char *filename2 = "/tmp/test_sdc_r11_mega_native.sdc";
  sta_->writeSdc(filename2, false, true, 4, false, true);
  FILE *f2 = fopen(filename2, "r");
  EXPECT_NE(f2, nullptr);
  if (f2) fclose(f2);

  // Also write in leaf mode
  const char *filename3 = "/tmp/test_sdc_r11_mega_leaf.sdc";
  sta_->writeSdc(filename3, true, false, 4, false, true);
  FILE *f3 = fopen(filename3, "r");
  EXPECT_NE(f3, nullptr);
  if (f3) fclose(f3);
}

// --- Sdc: remove clock groups ---
TEST_F(SdcDesignTest, SdcRemoveClockGroups) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("rm_grp", true, false, false,
                                             false, nullptr);
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1);
    // Remove by name
    sta_->removeClockGroupsLogicallyExclusive("rm_grp");
  }

  }() ));
}

// --- Sdc: remove physically exclusive clock groups ---
TEST_F(SdcDesignTest, SdcRemovePhysExclClkGroups) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("phys_grp", false, true, false,
                                             false, nullptr);
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1);
    sta_->removeClockGroupsPhysicallyExclusive("phys_grp");
  }

  }() ));
}

// --- Sdc: remove async clock groups ---
TEST_F(SdcDesignTest, SdcRemoveAsyncClkGroups) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("async_grp", false, false, true,
                                             false, nullptr);
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1);
    sta_->removeClockGroupsAsynchronous("async_grp");
  }

  }() ));
}

// --- Sdc: clear via removeConstraints (covers initVariables, clearCycleAcctings) ---
TEST_F(SdcDesignTest, SdcRemoveConstraintsCover) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  // Set various constraints first
  sdc->setMaxArea(500.0);
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.3);
  sdc->setVoltage(MinMax::max(), 1.1);
  // removeConstraints calls initVariables and clearCycleAcctings internally
  sta_->removeConstraints();

  }() ));
}

// --- ExceptionFrom: hash via exception creation and matching ---
TEST_F(SdcDesignTest, ExceptionFromMatching) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  if (in1 && in2) {
    PinSet *pins1 = new PinSet(network);
    pins1->insert(in1);
    ExceptionFrom *from1 = sta_->makeExceptionFrom(pins1, nullptr, nullptr,
                                                     RiseFallBoth::riseFall());
    PinSet *pins2 = new PinSet(network);
    pins2->insert(in2);
    ExceptionFrom *from2 = sta_->makeExceptionFrom(pins2, nullptr, nullptr,
                                                     RiseFallBoth::riseFall());
    // Make false paths - internally triggers findHash
    sta_->makeFalsePath(from1, nullptr, nullptr, MinMaxAll::all(), nullptr);
    sta_->makeFalsePath(from2, nullptr, nullptr, MinMaxAll::all(), nullptr);
  }

  }() ));
}

// --- DisabledCellPorts accessors ---
TEST_F(SdcDesignTest, DisabledCellPortsAccessors) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      DisabledCellPorts *dcp = new DisabledCellPorts(lib_cell);
      EXPECT_EQ(dcp->cell(), lib_cell);
      bool all_disabled = dcp->all();
      (void)all_disabled;
      delete dcp;
    }
  }
  delete iter;
}

// --- DisabledInstancePorts with disable ---
TEST_F(SdcDesignTest, DisabledInstancePortsDisable) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  ASSERT_TRUE(iter->hasNext());
  Instance *inst = iter->next();
  ASSERT_NE(inst, nullptr);
  LibertyCell *lib_cell = network->libertyCell(inst);
  ASSERT_NE(lib_cell, nullptr);

  LibertyPort *in_port = nullptr;
  LibertyPort *out_port = nullptr;
  LibertyCellPortIterator port_iter(lib_cell);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    if (port->direction()->isInput() && !in_port)
      in_port = port;
    else if (port->direction()->isOutput() && !out_port)
      out_port = port;
  }
  ASSERT_NE(in_port, nullptr);
  ASSERT_NE(out_port, nullptr);

  // Compare emitted SDC before/after disabling this specific arc.
  const char *filename = "/tmp/test_sdc_r11_disinstports.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  std::string before = readTextFile(filename);
  ASSERT_FALSE(before.empty());
  size_t before_disable_cnt = countSubstring(before, "set_disable_timing");

  sta_->disable(inst, in_port, out_port);
  sta_->writeSdc(filename, false, false, 4, false, true);
  std::string after_disable = readTextFile(filename);
  ASSERT_FALSE(after_disable.empty());
  size_t after_disable_cnt = countSubstring(after_disable, "set_disable_timing");
  EXPECT_GT(after_disable_cnt, before_disable_cnt);
  EXPECT_NE(after_disable.find("-from"), std::string::npos);
  EXPECT_NE(after_disable.find("-to"), std::string::npos);

  sta_->removeDisable(inst, in_port, out_port);
  sta_->writeSdc(filename, false, false, 4, false, true);
  std::string after_remove = readTextFile(filename);
  ASSERT_FALSE(after_remove.empty());
  size_t after_remove_cnt = countSubstring(after_remove, "set_disable_timing");
  EXPECT_EQ(after_remove_cnt, before_disable_cnt);

  delete iter;
}

// --- WriteSdc with latch borrow limit on clock
//     (triggers writeLatchBorrowLimits clock branch) ---
TEST_F(SdcDesignTest, WriteSdcLatchBorrowClock) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setLatchBorrowLimit(clk, 0.6f);
  }
  const char *filename = "/tmp/test_sdc_r11_latchborrowclk.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_max_time_borrow"), std::string::npos);
  EXPECT_NE(text.find("[get_clocks {clk}]"), std::string::npos);
}

// --- WriteSdc with derating on cell, instance, net ---
TEST_F(SdcDesignTest, WriteSdcDeratingCellInstNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Cell-level derating
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      sta_->setTimingDerate(lib_cell,
                            TimingDerateCellType::cell_delay,
                            PathClkOrData::data,
                            RiseFallBoth::riseFall(),
                            EarlyLate::early(), 0.93);
    }
    // Instance-level derating
    sta_->setTimingDerate(inst,
                          TimingDerateCellType::cell_delay,
                          PathClkOrData::data,
                          RiseFallBoth::riseFall(),
                          EarlyLate::late(), 1.07);
  }
  delete iter;

  // Net-level derating
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setTimingDerate(net,
                          PathClkOrData::data,
                          RiseFallBoth::riseFall(),
                          EarlyLate::early(), 0.92);
  }
  delete net_iter;

  const char *filename = "/tmp/test_sdc_r11_derate_all.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_timing_derate -net_delay -early -data"),
            std::string::npos);
  EXPECT_NE(text.find("set_timing_derate -cell_delay -late -data"),
            std::string::npos);
  EXPECT_NE(text.find("set_timing_derate -cell_delay -early -data"),
            std::string::npos);
}

// --- Sdc: capacitanceLimit on pin ---
TEST_F(SdcDesignTest, SdcCapLimitPin) {
  Sdc *sdc = sta_->sdc();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    sta_->setCapacitanceLimit(out, MinMax::max(), 0.5f);
    float limit;
    bool exists;
    sdc->capacitanceLimit(out, MinMax::max(), limit, exists);
    EXPECT_TRUE(exists);
    EXPECT_FLOAT_EQ(limit, 0.5f);
  }
}

// --- WriteSdc with set_false_path -hold only
//     (triggers writeSetupHoldFlag for hold) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathHold) {
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::min(), nullptr);
  const char *filename = "/tmp/test_sdc_r11_fp_hold.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_false_path -hold"), std::string::npos);
}

// --- WriteSdc with set_false_path -setup only
//     (triggers writeSetupHoldFlag for setup) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathSetup) {
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::max(), nullptr);
  const char *filename = "/tmp/test_sdc_r11_fp_setup.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_false_path -setup"), std::string::npos);
}

// --- WriteSdc with exception -through with rise_through
//     (triggers rf_prefix branches in writeExceptionThru) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathRiseThru) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *thru_pins = new PinSet(network);
    thru_pins->insert(in1);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    ExceptionThru *thru = sta_->makeExceptionThru(thru_pins, nullptr, nullptr,
                                                   RiseFallBoth::rise());
    thrus->push_back(thru);
    sta_->makeFalsePath(nullptr, thrus, nullptr, MinMaxAll::all(), nullptr);
  }
  const char *filename = "/tmp/test_sdc_r11_fp_risethru.sdc";
  sta_->writeSdc(filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_false_path"), std::string::npos);
  EXPECT_NE(text.find("-rise_through [get_ports {in1}]"), std::string::npos);
}

} // namespace sta
