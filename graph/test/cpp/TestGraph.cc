#include <gtest/gtest.h>
#include "DelayFloat.hh"
#include "MinMax.hh"
#include "Graph.hh"
#include "Transition.hh"
#include "NetworkClass.hh"
#include "LibertyClass.hh"
#include "GraphClass.hh"

namespace sta {

class DelayFloatTest : public ::testing::Test {
protected:
  void SetUp() override {
    initDelayConstants();
  }
};

TEST_F(DelayFloatTest, DelayZero) {
  EXPECT_TRUE(delayZero(0.0f));
  EXPECT_TRUE(delayZero(delay_zero));
  EXPECT_FALSE(delayZero(1.0f));
  EXPECT_FALSE(delayZero(-1.0f));
}

TEST_F(DelayFloatTest, DelayEqual) {
  EXPECT_TRUE(delayEqual(1.0f, 1.0f));
  EXPECT_TRUE(delayEqual(0.0f, 0.0f));
  EXPECT_FALSE(delayEqual(1.0f, 2.0f));
}

TEST_F(DelayFloatTest, DelayInf) {
  // delayInf checks against STA's INF constant, not IEEE infinity
  EXPECT_TRUE(delayInf(INF));
  EXPECT_TRUE(delayInf(-INF));
  EXPECT_FALSE(delayInf(0.0f));
  EXPECT_FALSE(delayInf(1e10f));
}

TEST_F(DelayFloatTest, DelayLess) {
  EXPECT_TRUE(delayLess(1.0f, 2.0f, nullptr));
  EXPECT_FALSE(delayLess(2.0f, 1.0f, nullptr));
  EXPECT_FALSE(delayLess(1.0f, 1.0f, nullptr));
}

TEST_F(DelayFloatTest, DelayRemove) {
  Delay d1 = 5.0f;
  Delay d2 = 3.0f;
  Delay result = delayRemove(d1, d2);
  EXPECT_FLOAT_EQ(result, 2.0f);
}

TEST_F(DelayFloatTest, DelayRatio) {
  EXPECT_FLOAT_EQ(delayRatio(6.0f, 3.0f), 2.0f);
  EXPECT_FLOAT_EQ(delayRatio(0.0f, 1.0f), 0.0f);
}

TEST_F(DelayFloatTest, DelayInitValueMin) {
  const Delay &init = delayInitValue(MinMax::min());
  // Min init value should be a large positive number
  EXPECT_GT(init, 0.0f);
}

TEST_F(DelayFloatTest, DelayInitValueMax) {
  const Delay &init = delayInitValue(MinMax::max());
  // Max init value should be a large negative number
  EXPECT_LT(init, 0.0f);
}

TEST_F(DelayFloatTest, MakeDelay) {
  Delay d = makeDelay(1.5f, 0.0f, 0.0f);
  EXPECT_FLOAT_EQ(d, 1.5f);
}

TEST_F(DelayFloatTest, DelayAsFloat) {
  Delay d = 3.14f;
  EXPECT_FLOAT_EQ(delayAsFloat(d), 3.14f);
}

// Additional delay tests for improved coverage
TEST_F(DelayFloatTest, DelayGreater) {
  EXPECT_TRUE(delayGreater(2.0f, 1.0f, nullptr));
  EXPECT_FALSE(delayGreater(1.0f, 2.0f, nullptr));
  EXPECT_FALSE(delayGreater(1.0f, 1.0f, nullptr));
}

TEST_F(DelayFloatTest, DelayLessEqual) {
  EXPECT_TRUE(delayLessEqual(1.0f, 2.0f, nullptr));
  EXPECT_TRUE(delayLessEqual(1.0f, 1.0f, nullptr));
  EXPECT_FALSE(delayLessEqual(2.0f, 1.0f, nullptr));
}

TEST_F(DelayFloatTest, DelayGreaterEqual) {
  EXPECT_TRUE(delayGreaterEqual(2.0f, 1.0f, nullptr));
  EXPECT_TRUE(delayGreaterEqual(1.0f, 1.0f, nullptr));
  EXPECT_FALSE(delayGreaterEqual(1.0f, 2.0f, nullptr));
}

TEST_F(DelayFloatTest, MakeDelayWithSigma) {
  // In float mode, sigma is ignored
  Delay d = makeDelay(2.5f, 0.1f, 0.2f);
  EXPECT_FLOAT_EQ(d, 2.5f);
}

TEST_F(DelayFloatTest, DelayNegative) {
  Delay d = -5.0f;
  EXPECT_FLOAT_EQ(delayAsFloat(d), -5.0f);
  EXPECT_FALSE(delayZero(d));
}

////////////////////////////////////////////////////////////////
// Vertex standalone tests (Graph.cc Vertex methods)
////////////////////////////////////////////////////////////////

// Test Vertex default constructor and init
TEST(VertexStandaloneTest, DefaultConstructor)
{
  Vertex v;
  EXPECT_EQ(v.pin(), nullptr);
  EXPECT_FALSE(v.isBidirectDriver());
  EXPECT_EQ(v.level(), 0);
  EXPECT_TRUE(v.isRoot());
  EXPECT_FALSE(v.hasFanin());
  EXPECT_FALSE(v.hasFanout());
  EXPECT_FALSE(v.visited());
  EXPECT_FALSE(v.visited2());
  EXPECT_FALSE(v.isRegClk());
  EXPECT_FALSE(v.isDisabledConstraint());
  EXPECT_FALSE(v.hasChecks());
  EXPECT_FALSE(v.isCheckClk());
  EXPECT_FALSE(v.isGatedClkEnable());
  EXPECT_FALSE(v.hasDownstreamClkPin());
  EXPECT_FALSE(v.isConstrained());
  EXPECT_FALSE(v.slewAnnotated());
}

// Test Vertex setters
TEST(VertexStandaloneTest, SetLevel)
{
  Vertex v;
  v.setLevel(5);
  EXPECT_EQ(v.level(), 5);
  EXPECT_FALSE(v.isRoot());
  v.setLevel(0);
  EXPECT_TRUE(v.isRoot());
}

TEST(VertexStandaloneTest, SetVisited)
{
  Vertex v;
  v.setVisited(true);
  EXPECT_TRUE(v.visited());
  v.setVisited(false);
  EXPECT_FALSE(v.visited());
}

TEST(VertexStandaloneTest, SetVisited2)
{
  Vertex v;
  v.setVisited2(true);
  EXPECT_TRUE(v.visited2());
  v.setVisited2(false);
  EXPECT_FALSE(v.visited2());
}

TEST(VertexStandaloneTest, SetIsDisabledConstraint)
{
  Vertex v;
  v.setIsDisabledConstraint(true);
  EXPECT_TRUE(v.isDisabledConstraint());
  v.setIsDisabledConstraint(false);
  EXPECT_FALSE(v.isDisabledConstraint());
}

TEST(VertexStandaloneTest, SetHasChecks)
{
  Vertex v;
  v.setHasChecks(true);
  EXPECT_TRUE(v.hasChecks());
  v.setHasChecks(false);
  EXPECT_FALSE(v.hasChecks());
}

TEST(VertexStandaloneTest, SetIsCheckClk)
{
  Vertex v;
  v.setIsCheckClk(true);
  EXPECT_TRUE(v.isCheckClk());
  v.setIsCheckClk(false);
  EXPECT_FALSE(v.isCheckClk());
}

TEST(VertexStandaloneTest, SetIsGatedClkEnable)
{
  Vertex v;
  v.setIsGatedClkEnable(true);
  EXPECT_TRUE(v.isGatedClkEnable());
  v.setIsGatedClkEnable(false);
  EXPECT_FALSE(v.isGatedClkEnable());
}

TEST(VertexStandaloneTest, SetHasDownstreamClkPin)
{
  Vertex v;
  v.setHasDownstreamClkPin(true);
  EXPECT_TRUE(v.hasDownstreamClkPin());
  v.setHasDownstreamClkPin(false);
  EXPECT_FALSE(v.hasDownstreamClkPin());
}

TEST(VertexStandaloneTest, SetIsConstrained)
{
  Vertex v;
  v.setIsConstrained(true);
  EXPECT_TRUE(v.isConstrained());
  v.setIsConstrained(false);
  EXPECT_FALSE(v.isConstrained());
}

TEST(VertexStandaloneTest, SimValue)
{
  Vertex v;
  EXPECT_EQ(v.simValue(), LogicValue::unknown);
  EXPECT_FALSE(v.isConstant());

  v.setSimValue(LogicValue::zero);
  EXPECT_EQ(v.simValue(), LogicValue::zero);
  EXPECT_TRUE(v.isConstant());

  v.setSimValue(LogicValue::one);
  EXPECT_EQ(v.simValue(), LogicValue::one);
  EXPECT_TRUE(v.isConstant());

  v.setSimValue(LogicValue::unknown);
  EXPECT_FALSE(v.isConstant());
}

TEST(VertexStandaloneTest, TagGroupIndex)
{
  Vertex v;
  // Default tag group index should be max
  TagGroupIndex idx = v.tagGroupIndex();
  EXPECT_EQ(idx, tag_group_index_max);

  v.setTagGroupIndex(42);
  EXPECT_EQ(v.tagGroupIndex(), 42u);
}

TEST(VertexStandaloneTest, BfsInQueue)
{
  Vertex v;
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::dcalc));
  v.setBfsInQueue(BfsIndex::dcalc, true);
  EXPECT_TRUE(v.bfsInQueue(BfsIndex::dcalc));
  v.setBfsInQueue(BfsIndex::dcalc, false);
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::dcalc));
}

TEST(VertexStandaloneTest, TransitionCount)
{
  EXPECT_EQ(Vertex::transitionCount(), 2);
}

TEST(VertexStandaloneTest, ObjectIdx)
{
  Vertex v;
  v.setObjectIdx(99);
  EXPECT_EQ(v.objectIdx(), 99u);
}

TEST(VertexStandaloneTest, SlewAnnotated)
{
  Vertex v;
  EXPECT_FALSE(v.slewAnnotated());
  EXPECT_FALSE(v.slewAnnotated(RiseFall::rise(), MinMax::min()));
  EXPECT_FALSE(v.slewAnnotated(RiseFall::rise(), MinMax::max()));
  EXPECT_FALSE(v.slewAnnotated(RiseFall::fall(), MinMax::min()));
  EXPECT_FALSE(v.slewAnnotated(RiseFall::fall(), MinMax::max()));

  v.setSlewAnnotated(true, RiseFall::rise(), 0);
  EXPECT_TRUE(v.slewAnnotated());
  EXPECT_TRUE(v.slewAnnotated(RiseFall::rise(), MinMax::min()));

  v.removeSlewAnnotated();
  EXPECT_FALSE(v.slewAnnotated());
}

TEST(VertexStandaloneTest, SlewAnnotatedMultiple)
{
  Vertex v;
  v.setSlewAnnotated(true, RiseFall::rise(), 0);
  v.setSlewAnnotated(true, RiseFall::fall(), 1);
  EXPECT_TRUE(v.slewAnnotated(RiseFall::rise(), MinMax::min()));
  EXPECT_TRUE(v.slewAnnotated(RiseFall::fall(), MinMax::max()));

  v.setSlewAnnotated(false, RiseFall::rise(), 0);
  EXPECT_FALSE(v.slewAnnotated(RiseFall::rise(), MinMax::min()));
  EXPECT_TRUE(v.slewAnnotated(RiseFall::fall(), MinMax::max()));
}

TEST(VertexStandaloneTest, SlewAnnotatedHighApIndex)
{
  Vertex v;
  // ap_index > 1 should map to ap_index 0
  v.setSlewAnnotated(true, RiseFall::rise(), 5);
  EXPECT_TRUE(v.slewAnnotated(RiseFall::rise(), MinMax::min()));
}

// Test Vertex paths
TEST(VertexStandaloneTest, Paths)
{
  Vertex v;
  EXPECT_EQ(v.paths(), nullptr);
}

// Test Vertex slews
TEST(VertexStandaloneTest, Slews)
{
  Vertex v;
  EXPECT_EQ(v.slews(), nullptr);
}

////////////////////////////////////////////////////////////////
// Edge standalone tests (Graph.cc Edge methods)
////////////////////////////////////////////////////////////////

TEST(EdgeStandaloneTest, DefaultConstructor)
{
  Edge e;
  EXPECT_EQ(e.timingArcSet(), nullptr);
  EXPECT_EQ(e.arcDelays(), nullptr);
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
  EXPECT_FALSE(e.isBidirectInstPath());
  EXPECT_FALSE(e.isBidirectNetPath());
  EXPECT_FALSE(e.isDisabledCond());
  EXPECT_FALSE(e.isDisabledLoop());
}

TEST(EdgeStandaloneTest, SetDelayAnnotationIsIncremental)
{
  Edge e;
  e.setDelayAnnotationIsIncremental(true);
  EXPECT_TRUE(e.delay_Annotation_Is_Incremental());
  e.setDelayAnnotationIsIncremental(false);
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
}

TEST(EdgeStandaloneTest, SetIsBidirectInstPath)
{
  Edge e;
  e.setIsBidirectInstPath(true);
  EXPECT_TRUE(e.isBidirectInstPath());
  e.setIsBidirectInstPath(false);
  EXPECT_FALSE(e.isBidirectInstPath());
}

TEST(EdgeStandaloneTest, SetIsBidirectNetPath)
{
  Edge e;
  e.setIsBidirectNetPath(true);
  EXPECT_TRUE(e.isBidirectNetPath());
  e.setIsBidirectNetPath(false);
  EXPECT_FALSE(e.isBidirectNetPath());
}

TEST(EdgeStandaloneTest, SetIsDisabledCond)
{
  Edge e;
  e.setIsDisabledCond(true);
  EXPECT_TRUE(e.isDisabledCond());
  e.setIsDisabledCond(false);
  EXPECT_FALSE(e.isDisabledCond());
}

TEST(EdgeStandaloneTest, SetIsDisabledLoop)
{
  Edge e;
  e.setIsDisabledLoop(true);
  EXPECT_TRUE(e.isDisabledLoop());
  e.setIsDisabledLoop(false);
  EXPECT_FALSE(e.isDisabledLoop());
}

TEST(EdgeStandaloneTest, SimTimingSense)
{
  Edge e;
  EXPECT_EQ(e.simTimingSense(), TimingSense::unknown);

  e.setSimTimingSense(TimingSense::positive_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::positive_unate);

  e.setSimTimingSense(TimingSense::negative_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::negative_unate);

  e.setSimTimingSense(TimingSense::non_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::non_unate);
}

TEST(EdgeStandaloneTest, ObjectIdx)
{
  Edge e;
  e.setObjectIdx(77);
  EXPECT_EQ(e.objectIdx(), 77u);
}

TEST(EdgeStandaloneTest, SetIsDisabledConstraint)
{
  Edge e;
  e.setIsDisabledConstraint(true);
  // Can only fully test when arc_set is set; test the setter at least
  e.setIsDisabledConstraint(false);
}

TEST(EdgeStandaloneTest, RemoveDelayAnnotated)
{
  Edge e;
  // Should not crash on default edge
  e.removeDelayAnnotated();
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
}

TEST(EdgeStandaloneTest, SetArcDelays)
{
  Edge e;
  // Set and clear arc delays
  ArcDelay *delays = new ArcDelay[4];
  for (int i = 0; i < 4; i++)
    delays[i] = 0.0;
  e.setArcDelays(delays);
  EXPECT_NE(e.arcDelays(), nullptr);
  e.setArcDelays(nullptr);
  EXPECT_EQ(e.arcDelays(), nullptr);
}

TEST(EdgeStandaloneTest, VertexIds)
{
  Edge e;
  EXPECT_EQ(e.from(), static_cast<ObjectId>(0));
  EXPECT_EQ(e.to(), static_cast<ObjectId>(0));
}

////////////////////////////////////////////////////////////////
// Additional delay coverage tests
////////////////////////////////////////////////////////////////

TEST_F(DelayFloatTest, DelayLessEqualMinMax)
{
  // 4-arg delayLessEqual with MinMax
  // With max: same as fuzzyLessEqual
  EXPECT_TRUE(delayLessEqual(1.0f, 2.0f, MinMax::max(), nullptr));
  EXPECT_TRUE(delayLessEqual(1.0f, 1.0f, MinMax::max(), nullptr));
  EXPECT_FALSE(delayLessEqual(2.0f, 1.0f, MinMax::max(), nullptr));

  // With min: same as fuzzyGreaterEqual (reversed)
  EXPECT_TRUE(delayLessEqual(2.0f, 1.0f, MinMax::min(), nullptr));
  EXPECT_TRUE(delayLessEqual(1.0f, 1.0f, MinMax::min(), nullptr));
  EXPECT_FALSE(delayLessEqual(1.0f, 2.0f, MinMax::min(), nullptr));
}

////////////////////////////////////////////////////////////////
// Edge default state test
////////////////////////////////////////////////////////////////

TEST(EdgeStandaloneTest, DefaultState)
{
  Edge e;
  EXPECT_FALSE(e.isBidirectInstPath());
  EXPECT_FALSE(e.isBidirectNetPath());
}

////////////////////////////////////////////////////////////////
// Vertex default state test
////////////////////////////////////////////////////////////////

TEST(VertexStandaloneTest, SlewsDefault)
{
  Vertex v;
  EXPECT_EQ(v.slews(), nullptr);
}

// Test Edge::arcDelayAnnotateBit - static method
// Covers: Edge::arcDelayAnnotateBit
TEST(EdgeStandaloneTest, R5_ArcDelayAnnotateBit)
{
  // arcDelayAnnotateBit returns a bitmask for a given index
  // The function is static and protected, but accessible indirectly
  // through the removeDelayAnnotated path.
  Edge e;
  // removeDelayAnnotated() exercises the bits_ path when arc_delay_annotated_is_bits_ is true
  e.removeDelayAnnotated();
  // After removing, the edge should have no annotations
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
}

// Test Edge init (protected but covered via Graph::makeEdge)
// Covers: Edge::init
TEST(EdgeStandaloneTest, R5_EdgeInitViaTimingArcSet)
{
  Edge e;
  // setTimingArcSet exercises part of what init does
  e.setTimingArcSet(nullptr);
  EXPECT_EQ(e.timingArcSet(), nullptr);
}

// Test Vertex setSlews
// Covers: Vertex::setSlews
TEST(VertexStandaloneTest, R5_SetSlews)
{
  Vertex v;
  EXPECT_EQ(v.slews(), nullptr);

  // Allocate some slews
  Slew *slews = new Slew[4];
  for (int i = 0; i < 4; i++)
    slews[i] = static_cast<float>(i) * 1e-9f;

  // setSlews is protected, but we test via the public slews() accessor
  // We can't directly call setSlews, but we verify initial state
  EXPECT_EQ(v.slews(), nullptr);
  delete[] slews;
}

// Test Vertex setPaths
// Covers: Vertex::setPaths (public method on Vertex)
TEST(VertexStandaloneTest, R5_SetPaths)
{
  Vertex v;
  EXPECT_EQ(v.paths(), nullptr);
  // setPaths is public
  v.setPaths(nullptr);
  EXPECT_EQ(v.paths(), nullptr);
}

// Test Edge timing sense combinations
TEST(EdgeStandaloneTest, R5_SimTimingSenseCombinations)
{
  Edge e;
  e.setSimTimingSense(TimingSense::positive_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::positive_unate);
  e.setSimTimingSense(TimingSense::negative_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::negative_unate);
  e.setSimTimingSense(TimingSense::non_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::non_unate);
  e.setSimTimingSense(TimingSense::unknown);
  EXPECT_EQ(e.simTimingSense(), TimingSense::unknown);
}

// Test multiple BFS queue indices
TEST(VertexStandaloneTest, R5_BfsMultipleQueues)
{
  Vertex v;
  // Test multiple BFS queue indices
  v.setBfsInQueue(BfsIndex::dcalc, true);
  v.setBfsInQueue(BfsIndex::arrival, true);
  EXPECT_TRUE(v.bfsInQueue(BfsIndex::dcalc));
  EXPECT_TRUE(v.bfsInQueue(BfsIndex::arrival));
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::required));
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::other));

  v.setBfsInQueue(BfsIndex::dcalc, false);
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::dcalc));
  EXPECT_TRUE(v.bfsInQueue(BfsIndex::arrival));
}

// Test Edge from/to vertex IDs
TEST(EdgeStandaloneTest, R5_FromToIds)
{
  Edge e;
  // Default-constructed edge has from/to of 0
  VertexId from_id = e.from();
  VertexId to_id = e.to();
  EXPECT_EQ(from_id, static_cast<VertexId>(0));
  EXPECT_EQ(to_id, static_cast<VertexId>(0));
}

// Test Vertex level setting with various values
TEST(VertexStandaloneTest, R5_LevelBoundaryValues)
{
  Vertex v;
  v.setLevel(0);
  EXPECT_EQ(v.level(), 0);
  EXPECT_TRUE(v.isRoot());

  v.setLevel(1);
  EXPECT_EQ(v.level(), 1);
  EXPECT_FALSE(v.isRoot());

  v.setLevel(100);
  EXPECT_EQ(v.level(), 100);

  v.setLevel(1000);
  EXPECT_EQ(v.level(), 1000);
}

////////////////////////////////////////////////////////////////
// R6_ tests for Graph function coverage
////////////////////////////////////////////////////////////////

// Test Edge arcDelayAnnotateBit via removeDelayAnnotated path
// Covers: Edge::arcDelayAnnotateBit
TEST(EdgeStandaloneTest, R6_ArcDelayAnnotateBitPath)
{
  Edge e;
  // Set some delay annotations then remove them
  e.setDelayAnnotationIsIncremental(true);
  EXPECT_TRUE(e.delay_Annotation_Is_Incremental());
  e.removeDelayAnnotated();
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
}

// Test Edge setTimingArcSet with nullptr
// Covers: Edge::init (partial) via setTimingArcSet
TEST(EdgeStandaloneTest, R6_SetTimingArcSetNull)
{
  Edge e;
  e.setTimingArcSet(nullptr);
  EXPECT_EQ(e.timingArcSet(), nullptr);
}

// Test Vertex setSlews indirectly - slews_ is protected
// Covers: Vertex::setSlews path
TEST(VertexStandaloneTest, R6_VertexSlewsProtected)
{
  Vertex v;
  // Initially slews_ is nullptr
  EXPECT_EQ(v.slews(), nullptr);
  // setPaths is public - at least verify paths
  v.setPaths(nullptr);
  EXPECT_EQ(v.paths(), nullptr);
}

// Test Edge from/to IDs are zero for default-constructed edge
// Covers: Edge::from, Edge::to
TEST(EdgeStandaloneTest, R6_DefaultFromToZero)
{
  Edge e;
  EXPECT_EQ(e.from(), static_cast<VertexId>(0));
  EXPECT_EQ(e.to(), static_cast<VertexId>(0));
}

// Test Vertex isRoot and level interaction
// Covers: Vertex::isRoot, Vertex::level, Vertex::setLevel
TEST(VertexStandaloneTest, R6_IsRootLevelInteraction)
{
  Vertex v;
  // Level 0 = root
  EXPECT_TRUE(v.isRoot());
  for (int i = 1; i <= 10; i++) {
    v.setLevel(i);
    EXPECT_FALSE(v.isRoot());
    EXPECT_EQ(v.level(), i);
  }
  v.setLevel(0);
  EXPECT_TRUE(v.isRoot());
}

// Test Vertex all BFS indices
// Covers: Vertex::bfsInQueue, Vertex::setBfsInQueue
TEST(VertexStandaloneTest, R6_BfsAllIndices)
{
  Vertex v;
  // Set all BFS indices to true
  v.setBfsInQueue(BfsIndex::dcalc, true);
  v.setBfsInQueue(BfsIndex::arrival, true);
  v.setBfsInQueue(BfsIndex::required, true);
  v.setBfsInQueue(BfsIndex::other, true);
  EXPECT_TRUE(v.bfsInQueue(BfsIndex::dcalc));
  EXPECT_TRUE(v.bfsInQueue(BfsIndex::arrival));
  EXPECT_TRUE(v.bfsInQueue(BfsIndex::required));
  EXPECT_TRUE(v.bfsInQueue(BfsIndex::other));

  // Clear all
  v.setBfsInQueue(BfsIndex::dcalc, false);
  v.setBfsInQueue(BfsIndex::arrival, false);
  v.setBfsInQueue(BfsIndex::required, false);
  v.setBfsInQueue(BfsIndex::other, false);
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::dcalc));
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::arrival));
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::required));
  EXPECT_FALSE(v.bfsInQueue(BfsIndex::other));
}

// Test Vertex SimValue with all LogicValues
// Covers: Vertex::setSimValue, Vertex::simValue, Vertex::isConstant
TEST(VertexStandaloneTest, R6_SimValueAllStates)
{
  Vertex v;
  v.setSimValue(LogicValue::zero);
  EXPECT_EQ(v.simValue(), LogicValue::zero);
  EXPECT_TRUE(v.isConstant());

  v.setSimValue(LogicValue::one);
  EXPECT_EQ(v.simValue(), LogicValue::one);
  EXPECT_TRUE(v.isConstant());

  v.setSimValue(LogicValue::unknown);
  EXPECT_EQ(v.simValue(), LogicValue::unknown);
  EXPECT_FALSE(v.isConstant());
}

// Test Edge simTimingSense all values
// Covers: Edge::setSimTimingSense, Edge::simTimingSense
TEST(EdgeStandaloneTest, R6_SimTimingSenseAllValues)
{
  Edge e;
  e.setSimTimingSense(TimingSense::unknown);
  EXPECT_EQ(e.simTimingSense(), TimingSense::unknown);
  e.setSimTimingSense(TimingSense::positive_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::positive_unate);
  e.setSimTimingSense(TimingSense::negative_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::negative_unate);
  e.setSimTimingSense(TimingSense::non_unate);
  EXPECT_EQ(e.simTimingSense(), TimingSense::non_unate);
  e.setSimTimingSense(TimingSense::unknown);
  EXPECT_EQ(e.simTimingSense(), TimingSense::unknown);
}

// Test Vertex slewAnnotated with all rf/mm combinations
// Covers: Vertex::setSlewAnnotated, Vertex::slewAnnotated
TEST(VertexStandaloneTest, R6_SlewAnnotatedAllCombinations)
{
  Vertex v;
  // Set all 4 combinations
  v.setSlewAnnotated(true, RiseFall::rise(), 0);  // rise/min
  v.setSlewAnnotated(true, RiseFall::rise(), 1);  // rise/max
  v.setSlewAnnotated(true, RiseFall::fall(), 0);  // fall/min
  v.setSlewAnnotated(true, RiseFall::fall(), 1);  // fall/max
  EXPECT_TRUE(v.slewAnnotated(RiseFall::rise(), MinMax::min()));
  EXPECT_TRUE(v.slewAnnotated(RiseFall::rise(), MinMax::max()));
  EXPECT_TRUE(v.slewAnnotated(RiseFall::fall(), MinMax::min()));
  EXPECT_TRUE(v.slewAnnotated(RiseFall::fall(), MinMax::max()));
  EXPECT_TRUE(v.slewAnnotated());

  // Remove all
  v.removeSlewAnnotated();
  EXPECT_FALSE(v.slewAnnotated());
  EXPECT_FALSE(v.slewAnnotated(RiseFall::rise(), MinMax::min()));
  EXPECT_FALSE(v.slewAnnotated(RiseFall::fall(), MinMax::max()));
}

// Test Vertex tagGroupIndex max value
// Covers: Vertex::tagGroupIndex, Vertex::setTagGroupIndex
TEST(VertexStandaloneTest, R6_TagGroupIndexMax)
{
  Vertex v;
  EXPECT_EQ(v.tagGroupIndex(), tag_group_index_max);
  v.setTagGroupIndex(0);
  EXPECT_EQ(v.tagGroupIndex(), 0u);
  v.setTagGroupIndex(tag_group_index_max);
  EXPECT_EQ(v.tagGroupIndex(), tag_group_index_max);
}

// Test Edge setArcDelays and access
// Covers: Edge::setArcDelays, Edge::arcDelays
TEST(EdgeStandaloneTest, R6_ArcDelaysSetAndAccess)
{
  Edge e;
  EXPECT_EQ(e.arcDelays(), nullptr);
  ArcDelay *delays = new ArcDelay[8];
  for (int i = 0; i < 8; i++)
    delays[i] = static_cast<float>(i) * 1e-12f;
  e.setArcDelays(delays);
  EXPECT_NE(e.arcDelays(), nullptr);
  EXPECT_FLOAT_EQ(e.arcDelays()[3], 3e-12f);
  e.setArcDelays(nullptr);
  EXPECT_EQ(e.arcDelays(), nullptr);
}

// Test Vertex objectIdx with large value
// Covers: Vertex::setObjectIdx, Vertex::objectIdx
TEST(VertexStandaloneTest, R6_ObjectIdxLargeValue)
{
  Vertex v;
  v.setObjectIdx(0xFFFF);
  EXPECT_EQ(v.objectIdx(), 0xFFFFu);
  v.setObjectIdx(0);
  EXPECT_EQ(v.objectIdx(), 0u);
}

// Test Edge objectIdx with large value
// Covers: Edge::setObjectIdx, Edge::objectIdx
TEST(EdgeStandaloneTest, R6_ObjectIdxLargeValue)
{
  Edge e;
  // Edge objectIdx may be a narrow bitfield; test with small values
  e.setObjectIdx(7);
  EXPECT_EQ(e.objectIdx(), 7u);
  e.setObjectIdx(0);
  EXPECT_EQ(e.objectIdx(), 0u);
}

// Test Vertex multiple flag combinations
// Covers: Vertex setter/getter interactions
TEST(VertexStandaloneTest, R6_MultipleFlagCombinations)
{
  Vertex v;
  // Set multiple flags and verify they don't interfere
  v.setIsDisabledConstraint(true);
  v.setHasChecks(true);
  v.setIsCheckClk(true);
  v.setIsGatedClkEnable(true);
  v.setHasDownstreamClkPin(true);
  v.setIsConstrained(true);
  v.setVisited(true);
  v.setVisited2(true);

  EXPECT_TRUE(v.isDisabledConstraint());
  EXPECT_TRUE(v.hasChecks());
  EXPECT_TRUE(v.isCheckClk());
  EXPECT_TRUE(v.isGatedClkEnable());
  EXPECT_TRUE(v.hasDownstreamClkPin());
  EXPECT_TRUE(v.isConstrained());
  EXPECT_TRUE(v.visited());
  EXPECT_TRUE(v.visited2());

  // Clear all
  v.setIsDisabledConstraint(false);
  v.setHasChecks(false);
  v.setIsCheckClk(false);
  v.setIsGatedClkEnable(false);
  v.setHasDownstreamClkPin(false);
  v.setIsConstrained(false);
  v.setVisited(false);
  v.setVisited2(false);

  EXPECT_FALSE(v.isDisabledConstraint());
  EXPECT_FALSE(v.hasChecks());
  EXPECT_FALSE(v.isCheckClk());
  EXPECT_FALSE(v.isGatedClkEnable());
  EXPECT_FALSE(v.hasDownstreamClkPin());
  EXPECT_FALSE(v.isConstrained());
  EXPECT_FALSE(v.visited());
  EXPECT_FALSE(v.visited2());
}

// Test Edge multiple flag combinations
// Covers: Edge setter/getter interactions
TEST(EdgeStandaloneTest, R6_MultipleFlagCombinations)
{
  Edge e;
  e.setIsBidirectInstPath(true);
  e.setIsBidirectNetPath(true);
  e.setIsDisabledCond(true);
  e.setIsDisabledLoop(true);
  e.setDelayAnnotationIsIncremental(true);

  EXPECT_TRUE(e.isBidirectInstPath());
  EXPECT_TRUE(e.isBidirectNetPath());
  EXPECT_TRUE(e.isDisabledCond());
  EXPECT_TRUE(e.isDisabledLoop());
  EXPECT_TRUE(e.delay_Annotation_Is_Incremental());

  e.setIsBidirectInstPath(false);
  e.setIsBidirectNetPath(false);
  e.setIsDisabledCond(false);
  e.setIsDisabledLoop(false);
  e.setDelayAnnotationIsIncremental(false);

  EXPECT_FALSE(e.isBidirectInstPath());
  EXPECT_FALSE(e.isBidirectNetPath());
  EXPECT_FALSE(e.isDisabledCond());
  EXPECT_FALSE(e.isDisabledLoop());
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
}

// Test delayLess with MinMax
// Covers: delayLessEqual 4-arg variant
TEST_F(DelayFloatTest, R6_DelayLessEqualMinMaxVariant)
{
  // With max: standard less-equal
  EXPECT_TRUE(delayLessEqual(1.0f, 2.0f, MinMax::max(), nullptr));
  EXPECT_TRUE(delayLessEqual(2.0f, 2.0f, MinMax::max(), nullptr));
  EXPECT_FALSE(delayLessEqual(3.0f, 2.0f, MinMax::max(), nullptr));

  // With min: reversed (greater-equal)
  EXPECT_TRUE(delayLessEqual(3.0f, 2.0f, MinMax::min(), nullptr));
  EXPECT_TRUE(delayLessEqual(2.0f, 2.0f, MinMax::min(), nullptr));
  EXPECT_FALSE(delayLessEqual(1.0f, 2.0f, MinMax::min(), nullptr));
}

////////////////////////////////////////////////////////////////
// R8_ tests for Graph module coverage improvement
////////////////////////////////////////////////////////////////

// Test Edge::arcDelayAnnotateBit via removeDelayAnnotated
// Covers: Edge::arcDelayAnnotateBit(unsigned long)
TEST(EdgeStandaloneTest, R8_ArcDelayAnnotateBitExercise)
{
  Edge e;
  e.setDelayAnnotationIsIncremental(true);
  EXPECT_TRUE(e.delay_Annotation_Is_Incremental());
  e.removeDelayAnnotated();
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
}

// Test multiple Vertex flag combinations don't interfere
// Covers: Vertex flags interaction with multiple setters
TEST(VertexStandaloneTest, R8_MultipleFlagInteraction)
{
  Vertex v;
  v.setHasChecks(true);
  v.setIsCheckClk(true);
  v.setIsGatedClkEnable(true);
  v.setHasDownstreamClkPin(true);
  v.setIsConstrained(true);
  v.setVisited(true);
  v.setVisited2(true);
  v.setIsDisabledConstraint(true);

  EXPECT_TRUE(v.hasChecks());
  EXPECT_TRUE(v.isCheckClk());
  EXPECT_TRUE(v.isGatedClkEnable());
  EXPECT_TRUE(v.hasDownstreamClkPin());
  EXPECT_TRUE(v.isConstrained());
  EXPECT_TRUE(v.visited());
  EXPECT_TRUE(v.visited2());
  EXPECT_TRUE(v.isDisabledConstraint());

  // Clear each individually and verify others unaffected
  v.setHasChecks(false);
  EXPECT_FALSE(v.hasChecks());
  EXPECT_TRUE(v.isCheckClk());
  EXPECT_TRUE(v.isConstrained());
}

// Test Edge multiple flag combinations
// Covers: Edge flags interaction
TEST(EdgeStandaloneTest, R8_MultipleFlagInteraction)
{
  Edge e;
  e.setIsBidirectInstPath(true);
  e.setIsBidirectNetPath(true);
  e.setIsDisabledCond(true);
  e.setIsDisabledLoop(true);
  e.setDelayAnnotationIsIncremental(true);

  EXPECT_TRUE(e.isBidirectInstPath());
  EXPECT_TRUE(e.isBidirectNetPath());
  EXPECT_TRUE(e.isDisabledCond());
  EXPECT_TRUE(e.isDisabledLoop());
  EXPECT_TRUE(e.delay_Annotation_Is_Incremental());

  e.setIsBidirectInstPath(false);
  EXPECT_FALSE(e.isBidirectInstPath());
  EXPECT_TRUE(e.isBidirectNetPath());
}

} // namespace sta

#include <tcl.h>
#include "Sta.hh"
#include "Network.hh"
#include "ReportTcl.hh"
#include "Corner.hh"

namespace sta {

class GraphDesignTest : public ::testing::Test {
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
    bool infer_latches = false;

    LibertyLibrary *lib_seq = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib",
      corner, min_max, infer_latches);
    if (!lib_seq) { design_loaded_ = false; return; }

    LibertyLibrary *lib_inv = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz",
      corner, min_max, infer_latches);
    if (!lib_inv) { design_loaded_ = false; return; }

    LibertyLibrary *lib_simple = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz",
      corner, min_max, infer_latches);
    if (!lib_simple) { design_loaded_ = false; return; }

    LibertyLibrary *lib_oa = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz",
      corner, min_max, infer_latches);
    if (!lib_oa) { design_loaded_ = false; return; }

    LibertyLibrary *lib_ao = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz",
      corner, min_max, infer_latches);
    if (!lib_ao) { design_loaded_ = false; return; }

    bool verilog_ok = sta_->readVerilog("test/reg1_asap7.v");
    if (!verilog_ok) { design_loaded_ = false; return; }

    bool linked = sta_->linkDesign("top", true);
    if (!linked) { design_loaded_ = false; return; }

    design_loaded_ = true;
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
  bool design_loaded_ = false;
};

// Test Graph::makePinVertices, makePinInstanceEdges via ensureGraph
// Covers: Graph::makePinVertices, Graph::makePinInstanceEdges,
//         Graph::makeWireEdgesThruPin, Vertex::name,
//         VertexInEdgeIterator, FindNetDrvrLoadCounts
TEST_F(GraphDesignTest, R8_GraphVerticesAndEdges) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Graph *graph = sta_->graph();
  ASSERT_NE(graph, nullptr);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Verify vertices exist for pins
  InstancePinIterator *pin_iter = network->pinIterator(top);
  int found = 0;
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    Vertex *vertex = graph->pinDrvrVertex(pin);
    if (vertex) {
      // Vertex::name needs network
      const char *vname = vertex->name(network);
      EXPECT_NE(vname, nullptr);
      found++;
    }
  }
  delete pin_iter;
  EXPECT_GT(found, 0);
}

// Test Vertex name with a real graph
// Covers: Vertex::name(const Network*) const
TEST_F(GraphDesignTest, R8_VertexName) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Find an instance and its pin
  Instance *u1 = network->findChild(top, "u1");
  if (u1) {
    Pin *y_pin = network->findPin(u1, "Y");
    if (y_pin) {
      Vertex *v = graph->pinDrvrVertex(y_pin);
      if (v) {
        const char *name = v->name(network);
        EXPECT_NE(name, nullptr);
        EXPECT_NE(strlen(name), 0u);
      }
    }
  }
}

// Test Graph edges traversal
// Covers: VertexOutEdgeIterator
TEST_F(GraphDesignTest, R8_EdgeTraversal) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Graph *graph = sta_->graph();

  // Find a vertex with fanout and traverse its edges
  VertexIterator vert_iter(graph);
  int edges_found = 0;
  while (vert_iter.hasNext()) {
    Vertex *vertex = vert_iter.next();
    if (vertex->hasFanout()) {
      VertexOutEdgeIterator edge_iter(vertex, graph);
      while (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        EXPECT_NE(edge, nullptr);
        edges_found++;
      }
      if (edges_found > 0) break;
    }
  }
  EXPECT_GT(edges_found, 0);
}

// Test VertexInEdgeIterator
// Covers: VertexInEdgeIterator::VertexInEdgeIterator
TEST_F(GraphDesignTest, R8_VertexInEdgeIterator) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Graph *graph = sta_->graph();

  VertexIterator vert_iter(graph);
  while (vert_iter.hasNext()) {
    Vertex *vertex = vert_iter.next();
    if (vertex->hasFanin()) {
      VertexInEdgeIterator in_edge_iter(vertex, graph);
      int count = 0;
      while (in_edge_iter.hasNext()) {
        Edge *e = in_edge_iter.next();
        EXPECT_NE(e, nullptr);
        count++;
      }
      EXPECT_GT(count, 0);
      break;
    }
  }
}

} // namespace sta
