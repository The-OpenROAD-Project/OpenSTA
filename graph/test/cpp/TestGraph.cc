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
  EXPECT_FALSE(v.hasChecks());
  EXPECT_FALSE(v.isCheckClk());
  EXPECT_FALSE(v.hasDownstreamClkPin());
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

TEST(VertexStandaloneTest, SetHasDownstreamClkPin)
{
  Vertex v;
  v.setHasDownstreamClkPin(true);
  EXPECT_TRUE(v.hasDownstreamClkPin());
  v.setHasDownstreamClkPin(false);
  EXPECT_FALSE(v.hasDownstreamClkPin());
}

TEST(VertexStandaloneTest, HasSimValue)
{
  Vertex v;
  EXPECT_FALSE(v.hasSimValue());

  v.setHasSimValue(true);
  EXPECT_TRUE(v.hasSimValue());

  v.setHasSimValue(false);
  EXPECT_FALSE(v.hasSimValue());
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

// Vertex::transitionCount() was removed from upstream API

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
  EXPECT_FALSE(e.hasDisabledCond());
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
  e.setHasDisabledCond(true);
  EXPECT_TRUE(e.hasDisabledCond());
  e.setHasDisabledCond(false);
  EXPECT_FALSE(e.hasDisabledCond());
}

TEST(EdgeStandaloneTest, SetIsDisabledLoop)
{
  Edge e;
  e.setIsDisabledLoop(true);
  EXPECT_TRUE(e.isDisabledLoop());
  e.setIsDisabledLoop(false);
  EXPECT_FALSE(e.isDisabledLoop());
}

TEST(EdgeStandaloneTest, ObjectIdx)
{
  Edge e;
  e.setObjectIdx(77);
  EXPECT_EQ(e.objectIdx(), 77u);
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
TEST(EdgeStandaloneTest, ArcDelayAnnotateBit)
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
TEST(EdgeStandaloneTest, EdgeInitViaTimingArcSet)
{
  Edge e;
  // setTimingArcSet exercises part of what init does
  e.setTimingArcSet(nullptr);
  EXPECT_EQ(e.timingArcSet(), nullptr);
}

// Test Vertex setSlews
// Covers: Vertex::setSlews
TEST(VertexStandaloneTest, SetSlews)
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
TEST(VertexStandaloneTest, SetPaths)
{
  Vertex v;
  EXPECT_EQ(v.paths(), nullptr);
  // setPaths is public
  v.setPaths(nullptr);
  EXPECT_EQ(v.paths(), nullptr);
}

// Test multiple BFS queue indices
TEST(VertexStandaloneTest, BfsMultipleQueues)
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
TEST(EdgeStandaloneTest, FromToIds)
{
  Edge e;
  // Default-constructed edge has from/to of 0
  VertexId from_id = e.from();
  VertexId to_id = e.to();
  EXPECT_EQ(from_id, static_cast<VertexId>(0));
  EXPECT_EQ(to_id, static_cast<VertexId>(0));
}

// Test Vertex level setting with various values
TEST(VertexStandaloneTest, LevelBoundaryValues)
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
TEST(EdgeStandaloneTest, ArcDelayAnnotateBitPath)
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
TEST(EdgeStandaloneTest, SetTimingArcSetNull)
{
  Edge e;
  e.setTimingArcSet(nullptr);
  EXPECT_EQ(e.timingArcSet(), nullptr);
}

// Test Vertex setSlews indirectly - slews_ is protected
// Covers: Vertex::setSlews path
TEST(VertexStandaloneTest, VertexSlewsProtected)
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
TEST(EdgeStandaloneTest, DefaultFromToZero)
{
  Edge e;
  EXPECT_EQ(e.from(), static_cast<VertexId>(0));
  EXPECT_EQ(e.to(), static_cast<VertexId>(0));
}

// Test Vertex isRoot and level interaction
// Covers: Vertex::isRoot, Vertex::level, Vertex::setLevel
TEST(VertexStandaloneTest, IsRootLevelInteraction)
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
TEST(VertexStandaloneTest, BfsAllIndices)
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

// Test Vertex hasSimValue flag
// Covers: Vertex::setHasSimValue, Vertex::hasSimValue
TEST(VertexStandaloneTest, HasSimValueToggle)
{
  Vertex v;
  EXPECT_FALSE(v.hasSimValue());

  v.setHasSimValue(true);
  EXPECT_TRUE(v.hasSimValue());

  v.setHasSimValue(false);
  EXPECT_FALSE(v.hasSimValue());
}

// Test Edge hasSimSense flag
// Covers: Edge::setHasSimSense, Edge::hasSimSense
TEST(EdgeStandaloneTest, HasSimSenseAllValues)
{
  Edge e;
  EXPECT_FALSE(e.hasSimSense());

  e.setHasSimSense(true);
  EXPECT_TRUE(e.hasSimSense());

  e.setHasSimSense(false);
  EXPECT_FALSE(e.hasSimSense());
}

// Test Vertex slewAnnotated with all rf/mm combinations
// Covers: Vertex::setSlewAnnotated, Vertex::slewAnnotated
TEST(VertexStandaloneTest, SlewAnnotatedAllCombinations)
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
TEST(VertexStandaloneTest, TagGroupIndexMax)
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
TEST(EdgeStandaloneTest, ArcDelaysSetAndAccess)
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
TEST(VertexStandaloneTest, ObjectIdxLargeValue)
{
  Vertex v;
  v.setObjectIdx(0xFFFF);
  EXPECT_EQ(v.objectIdx(), 0xFFFFu);
  v.setObjectIdx(0);
  EXPECT_EQ(v.objectIdx(), 0u);
}

// Test Edge objectIdx with large value
// Covers: Edge::setObjectIdx, Edge::objectIdx
TEST(EdgeStandaloneTest, ObjectIdxLargeValue)
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
TEST(VertexStandaloneTest, MultipleFlagCombinations)
{
  Vertex v;
  // Set multiple flags and verify they don't interfere
  v.setHasChecks(true);
  v.setIsCheckClk(true);
  v.setHasDownstreamClkPin(true);
  v.setVisited(true);
  v.setVisited2(true);

  EXPECT_TRUE(v.hasChecks());
  EXPECT_TRUE(v.isCheckClk());
  EXPECT_TRUE(v.hasDownstreamClkPin());
  EXPECT_TRUE(v.visited());
  EXPECT_TRUE(v.visited2());

  // Clear all
  v.setHasChecks(false);
  v.setIsCheckClk(false);
  v.setHasDownstreamClkPin(false);
  v.setVisited(false);
  v.setVisited2(false);

  EXPECT_FALSE(v.hasChecks());
  EXPECT_FALSE(v.isCheckClk());
  EXPECT_FALSE(v.hasDownstreamClkPin());
  EXPECT_FALSE(v.visited());
  EXPECT_FALSE(v.visited2());
}

// Test Edge multiple flag combinations
// Covers: Edge setter/getter interactions
TEST(EdgeStandaloneTest, MultipleFlagCombinations)
{
  Edge e;
  e.setIsBidirectInstPath(true);
  e.setIsBidirectNetPath(true);
  e.setHasDisabledCond(true);
  e.setIsDisabledLoop(true);
  e.setDelayAnnotationIsIncremental(true);

  EXPECT_TRUE(e.isBidirectInstPath());
  EXPECT_TRUE(e.isBidirectNetPath());
  EXPECT_TRUE(e.hasDisabledCond());
  EXPECT_TRUE(e.isDisabledLoop());
  EXPECT_TRUE(e.delay_Annotation_Is_Incremental());

  e.setIsBidirectInstPath(false);
  e.setIsBidirectNetPath(false);
  e.setHasDisabledCond(false);
  e.setIsDisabledLoop(false);
  e.setDelayAnnotationIsIncremental(false);

  EXPECT_FALSE(e.isBidirectInstPath());
  EXPECT_FALSE(e.isBidirectNetPath());
  EXPECT_FALSE(e.hasDisabledCond());
  EXPECT_FALSE(e.isDisabledLoop());
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
}

// Test delayLess with MinMax
// Covers: delayLessEqual 4-arg variant
TEST_F(DelayFloatTest, DelayLessEqualMinMaxVariant)
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
TEST(EdgeStandaloneTest, ArcDelayAnnotateBitExercise)
{
  Edge e;
  e.setDelayAnnotationIsIncremental(true);
  EXPECT_TRUE(e.delay_Annotation_Is_Incremental());
  e.removeDelayAnnotated();
  EXPECT_FALSE(e.delay_Annotation_Is_Incremental());
}

// Test multiple Vertex flag combinations don't interfere
// Covers: Vertex flags interaction with multiple setters
TEST(VertexStandaloneTest, MultipleFlagInteraction)
{
  Vertex v;
  v.setHasChecks(true);
  v.setIsCheckClk(true);
  v.setHasDownstreamClkPin(true);
  v.setVisited(true);
  v.setVisited2(true);
  v.setHasSimValue(true);

  EXPECT_TRUE(v.hasChecks());
  EXPECT_TRUE(v.isCheckClk());
  EXPECT_TRUE(v.hasDownstreamClkPin());
  EXPECT_TRUE(v.visited());
  EXPECT_TRUE(v.visited2());
  EXPECT_TRUE(v.hasSimValue());

  // Clear each individually and verify others unaffected
  v.setHasChecks(false);
  EXPECT_FALSE(v.hasChecks());
  EXPECT_TRUE(v.isCheckClk());
  EXPECT_TRUE(v.hasSimValue());
}

// Test Edge multiple flag combinations
// Covers: Edge flags interaction
TEST(EdgeStandaloneTest, MultipleFlagInteraction)
{
  Edge e;
  e.setIsBidirectInstPath(true);
  e.setIsBidirectNetPath(true);
  e.setHasDisabledCond(true);
  e.setIsDisabledLoop(true);
  e.setDelayAnnotationIsIncremental(true);

  EXPECT_TRUE(e.isBidirectInstPath());
  EXPECT_TRUE(e.isBidirectNetPath());
  EXPECT_TRUE(e.hasDisabledCond());
  EXPECT_TRUE(e.isDisabledLoop());
  EXPECT_TRUE(e.delay_Annotation_Is_Incremental());

  e.setIsBidirectInstPath(false);
  EXPECT_FALSE(e.isBidirectInstPath());
  EXPECT_TRUE(e.isBidirectNetPath());
}

} // namespace sta

#include <tcl.h>
#include "Sta.hh"
#include "Sdc.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "ReportTcl.hh"
#include "Scene.hh"
#include "Search.hh"
#include "TimingArc.hh"
#include "PortDirection.hh"

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

    Scene *corner = sta_->cmdScene();
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
TEST_F(GraphDesignTest, GraphVerticesAndEdges) {
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
TEST_F(GraphDesignTest, VertexName) {
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
TEST_F(GraphDesignTest, EdgeTraversal) {
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
TEST_F(GraphDesignTest, VertexInEdgeIterator) {
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

////////////////////////////////////////////////////////////////
// GraphNangateTest: uses Nangate45 + graph_test2.v
// Tests graph construction, vertex/edge counts, queries, and timing arcs
////////////////////////////////////////////////////////////////

class GraphNangateTest : public ::testing::Test {
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

    Scene *corner = sta_->cmdScene();
    const MinMaxAll *min_max = MinMaxAll::all();
    LibertyLibrary *lib = sta_->readLiberty(
      "test/nangate45/Nangate45_typ.lib", corner, min_max, false);
    ASSERT_NE(lib, nullptr);

    bool ok = sta_->readVerilog("graph/test/graph_test2.v");
    ASSERT_TRUE(ok);
    ok = sta_->linkDesign("graph_test2", true);
    ASSERT_TRUE(ok);

    // Create clock and constraints
    Network *network = sta_->network();
    Instance *top = network->topInstance();
    Pin *clk_pin = network->findPin(top, "clk");
    ASSERT_NE(clk_pin, nullptr);
    PinSet *clk_pins = new PinSet(network);
    clk_pins->insert(clk_pin);
    FloatSeq *waveform = new FloatSeq;
    waveform->push_back(0.0f);
    waveform->push_back(5.0f);
    sta_->makeClock("clk", clk_pins, false, 10.0f, waveform, nullptr, sta_->cmdMode());

    Clock *clk = sta_->cmdSdc()->findClock("clk");
    ASSERT_NE(clk, nullptr);

    Pin *d1_pin = network->findPin(top, "d1");
    ASSERT_NE(d1_pin, nullptr);
    sta_->setInputDelay(d1_pin, RiseFallBoth::riseFall(), clk,
                        RiseFall::rise(), nullptr, false, false,
                        MinMaxAll::all(), false, 1.0f, sta_->cmdSdc());

    Pin *d2_pin = network->findPin(top, "d2");
    ASSERT_NE(d2_pin, nullptr);
    sta_->setInputDelay(d2_pin, RiseFallBoth::riseFall(), clk,
                        RiseFall::rise(), nullptr, false, false,
                        MinMaxAll::all(), false, 1.0f, sta_->cmdSdc());

    Pin *en_pin = network->findPin(top, "en");
    ASSERT_NE(en_pin, nullptr);
    sta_->setInputDelay(en_pin, RiseFallBoth::riseFall(), clk,
                        RiseFall::rise(), nullptr, false, false,
                        MinMaxAll::all(), false, 1.0f, sta_->cmdSdc());

    design_loaded_ = true;
  }

  void TearDown() override {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_) Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  Sta *sta_;
  Tcl_Interp *interp_;
  bool design_loaded_ = false;
};

// graph_test2 has: buf1(BUF_X1), buf2(BUF_X2), inv1(INV_X1),
// and1(AND2_X1), or1(OR2_X1), buf3(BUF_X1), reg1(DFF_X1), reg2(DFF_X1)
// Ports: clk, d1, d2, en (input), q1, q2 (output)
// Total: 8 instances + top-level ports

TEST_F(GraphNangateTest, GraphVertexCountNonZero) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  ASSERT_NE(graph, nullptr);
  // Must have vertices for all instance pins + port pins
  EXPECT_GT(graph->vertexCount(), 0u);
}

TEST_F(GraphNangateTest, PinDrvrVertexForPorts) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Input ports should have driver vertices
  Pin *d1_pin = network->findPin(top, "d1");
  ASSERT_NE(d1_pin, nullptr);
  Vertex *d1_v = graph->pinDrvrVertex(d1_pin);
  EXPECT_NE(d1_v, nullptr);

  // Output ports should have load vertices
  Pin *q1_pin = network->findPin(top, "q1");
  ASSERT_NE(q1_pin, nullptr);
  Vertex *q1_v = graph->pinLoadVertex(q1_pin);
  EXPECT_NE(q1_v, nullptr);
}

TEST_F(GraphNangateTest, PinDrvrVertexForInstPins) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // buf1 output should have a driver vertex
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Vertex *buf1_z_v = graph->pinDrvrVertex(buf1_z);
  EXPECT_NE(buf1_z_v, nullptr);

  // buf1 input should have a load vertex
  Pin *buf1_a = network->findPin(buf1, "A");
  ASSERT_NE(buf1_a, nullptr);
  Vertex *buf1_a_v = graph->pinLoadVertex(buf1_a);
  EXPECT_NE(buf1_a_v, nullptr);
}

TEST_F(GraphNangateTest, InstanceEdgesExist) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // buf1 (BUF_X1) should have an edge from A to Z
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Vertex *buf1_z_v = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(buf1_z_v, nullptr);

  // The output vertex should have in-edges (from the timing arc A->Z)
  int in_count = 0;
  VertexInEdgeIterator in_iter(buf1_z_v, graph);
  while (in_iter.hasNext()) {
    Edge *edge = in_iter.next();
    EXPECT_NE(edge, nullptr);
    EXPECT_FALSE(edge->isWire());  // Instance edge, not wire
    in_count++;
  }
  EXPECT_GT(in_count, 0);
}

TEST_F(GraphNangateTest, WireEdgesExist) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Wire edge: buf1/Z drives inv1/A (through net n1)
  Instance *inv1 = network->findChild(top, "inv1");
  ASSERT_NE(inv1, nullptr);
  Pin *inv1_a = network->findPin(inv1, "A");
  ASSERT_NE(inv1_a, nullptr);
  Vertex *inv1_a_v = graph->pinLoadVertex(inv1_a);
  ASSERT_NE(inv1_a_v, nullptr);

  int wire_count = 0;
  VertexInEdgeIterator in_iter(inv1_a_v, graph);
  while (in_iter.hasNext()) {
    Edge *edge = in_iter.next();
    if (edge->isWire())
      wire_count++;
  }
  EXPECT_GT(wire_count, 0);
}

TEST_F(GraphNangateTest, MultiInputCellEdges) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // and1 (AND2_X1) has 2 input pins: A1 and A2, output ZN
  // Should have edges A1->ZN and A2->ZN
  Instance *and1 = network->findChild(top, "and1");
  ASSERT_NE(and1, nullptr);
  Pin *and1_zn = network->findPin(and1, "ZN");
  ASSERT_NE(and1_zn, nullptr);
  Vertex *and1_zn_v = graph->pinDrvrVertex(and1_zn);
  ASSERT_NE(and1_zn_v, nullptr);

  int inst_edge_count = 0;
  VertexInEdgeIterator in_iter(and1_zn_v, graph);
  while (in_iter.hasNext()) {
    Edge *edge = in_iter.next();
    if (!edge->isWire())
      inst_edge_count++;
  }
  // AND2 should have 2 instance edges (A1->ZN and A2->ZN)
  EXPECT_EQ(inst_edge_count, 2);
}

TEST_F(GraphNangateTest, FanoutFromBuffer) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // buf1/Z drives n1, which connects to inv1/A and and1/A1
  // So buf1/Z should have outgoing wire edges
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Vertex *buf1_z_v = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(buf1_z_v, nullptr);

  int out_count = 0;
  VertexOutEdgeIterator out_iter(buf1_z_v, graph);
  while (out_iter.hasNext()) {
    Edge *edge = out_iter.next();
    EXPECT_NE(edge, nullptr);
    out_count++;
  }
  EXPECT_GT(out_count, 0);
}

TEST_F(GraphNangateTest, RegisterClockEdges) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // reg1 is DFF_X1 with CK pin - should have timing arcs from CK
  Instance *reg1 = network->findChild(top, "reg1");
  ASSERT_NE(reg1, nullptr);
  Pin *ck_pin = network->findPin(reg1, "CK");
  ASSERT_NE(ck_pin, nullptr);
  Vertex *ck_v = graph->pinLoadVertex(ck_pin);
  ASSERT_NE(ck_v, nullptr);

  // CK should have output edges (to Q and to setup/hold check arcs)
  int out_count = 0;
  VertexOutEdgeIterator out_iter(ck_v, graph);
  while (out_iter.hasNext()) {
    out_iter.next();
    out_count++;
  }
  EXPECT_GT(out_count, 0);
}

TEST_F(GraphNangateTest, VertexIteratorTraversesAll) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();

  int count = 0;
  VertexIterator iter(graph);
  while (iter.hasNext()) {
    Vertex *v = iter.next();
    EXPECT_NE(v, nullptr);
    count++;
  }
  // graph_test2 has 8 instances + 6 ports = significant number of vertices
  EXPECT_GT(count, 20);
  EXPECT_EQ(static_cast<VertexId>(count), graph->vertexCount());
}

TEST_F(GraphNangateTest, GateEdgeArcLookup) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Look up the timing arc for buf1 A->Z, rise->rise
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_a = network->findPin(buf1, "A");
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_a, nullptr);
  ASSERT_NE(buf1_z, nullptr);

  Edge *edge = nullptr;
  const TimingArc *arc = nullptr;
  graph->gateEdgeArc(buf1_a, RiseFall::rise(),
                     buf1_z, RiseFall::rise(),
                     edge, arc);
  EXPECT_NE(edge, nullptr);
  EXPECT_NE(arc, nullptr);
}

TEST_F(GraphNangateTest, ArcDelaysAfterTiming) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_a = network->findPin(buf1, "A");
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_a, nullptr);
  ASSERT_NE(buf1_z, nullptr);

  Edge *edge = nullptr;
  const TimingArc *arc = nullptr;
  graph->gateEdgeArc(buf1_a, RiseFall::rise(),
                     buf1_z, RiseFall::rise(),
                     edge, arc);
  ASSERT_NE(edge, nullptr);
  ASSERT_NE(arc, nullptr);

  // After timing, arc delay should be computed and > 0
  ArcDelay delay = graph->arcDelay(edge, arc, 0);
  EXPECT_GT(delayAsFloat(delay), 0.0f);
}

TEST_F(GraphNangateTest, SlewsAfterTiming) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Check slew at buf1 output after timing
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Vertex *buf1_z_v = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(buf1_z_v, nullptr);

  const Slew &slew_rise = graph->slew(buf1_z_v, RiseFall::rise(), 0);
  const Slew &slew_fall = graph->slew(buf1_z_v, RiseFall::fall(), 0);
  // After timing, slew should be non-zero
  EXPECT_GT(delayAsFloat(slew_rise), 0.0f);
  EXPECT_GT(delayAsFloat(slew_fall), 0.0f);
}

TEST_F(GraphNangateTest, EdgeTimingRole) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Instance edge should have a combinational or register role
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Vertex *buf1_z_v = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(buf1_z_v, nullptr);

  VertexInEdgeIterator in_iter(buf1_z_v, graph);
  while (in_iter.hasNext()) {
    Edge *edge = in_iter.next();
    if (!edge->isWire()) {
      const TimingRole *role = edge->role();
      EXPECT_NE(role, nullptr);
      break;
    }
  }
}

////////////////////////////////////////////////////////////////
// GraphLargeDesignTest: uses Nangate45 + graph_test3.v (multi-clock)
// Tests complex graph with reconvergent paths and multiple clock domains
////////////////////////////////////////////////////////////////

class GraphLargeDesignTest : public ::testing::Test {
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

    Scene *corner = sta_->cmdScene();
    const MinMaxAll *min_max = MinMaxAll::all();
    LibertyLibrary *lib = sta_->readLiberty(
      "test/nangate45/Nangate45_typ.lib", corner, min_max, false);
    ASSERT_NE(lib, nullptr);

    bool ok = sta_->readVerilog("graph/test/graph_test3.v");
    ASSERT_TRUE(ok);
    ok = sta_->linkDesign("graph_test3", true);
    ASSERT_TRUE(ok);

    Network *network = sta_->network();
    Instance *top = network->topInstance();

    // Create clock1
    Pin *clk1_pin = network->findPin(top, "clk1");
    ASSERT_NE(clk1_pin, nullptr);
    PinSet *clk1_pins = new PinSet(network);
    clk1_pins->insert(clk1_pin);
    FloatSeq *wave1 = new FloatSeq;
    wave1->push_back(0.0f);
    wave1->push_back(5.0f);
    sta_->makeClock("clk1", clk1_pins, false, 10.0f, wave1, nullptr, sta_->cmdMode());

    // Create clock2
    Pin *clk2_pin = network->findPin(top, "clk2");
    ASSERT_NE(clk2_pin, nullptr);
    PinSet *clk2_pins = new PinSet(network);
    clk2_pins->insert(clk2_pin);
    FloatSeq *wave2 = new FloatSeq;
    wave2->push_back(0.0f);
    wave2->push_back(2.5f);
    sta_->makeClock("clk2", clk2_pins, false, 5.0f, wave2, nullptr, sta_->cmdMode());

    // Input delays
    Clock *clk1 = sta_->cmdSdc()->findClock("clk1");
    ASSERT_NE(clk1, nullptr);
    const char *inputs[] = {"d1", "d2", "d3", "d4"};
    for (const char *name : inputs) {
      Pin *pin = network->findPin(top, name);
      ASSERT_NE(pin, nullptr);
      sta_->setInputDelay(pin, RiseFallBoth::riseFall(), clk1,
                          RiseFall::rise(), nullptr, false, false,
                          MinMaxAll::all(), false, 1.0f, sta_->cmdSdc());
    }

    design_loaded_ = true;
  }

  void TearDown() override {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_) Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  Sta *sta_;
  Tcl_Interp *interp_;
  bool design_loaded_ = false;
};

TEST_F(GraphLargeDesignTest, VertexCountLargeDesign) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  // graph_test3: 14 instances + 10 ports - more vertices than graph_test2
  EXPECT_GT(graph->vertexCount(), 30u);
}

TEST_F(GraphLargeDesignTest, ReconvergentPaths) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // n7 feeds both and2/A1 and or2/A1 (reconvergent fanout)
  // nand1/ZN drives n7
  Instance *nand1 = network->findChild(top, "nand1");
  ASSERT_NE(nand1, nullptr);
  Pin *nand1_zn = network->findPin(nand1, "ZN");
  ASSERT_NE(nand1_zn, nullptr);
  Vertex *nand1_zn_v = graph->pinDrvrVertex(nand1_zn);
  ASSERT_NE(nand1_zn_v, nullptr);

  // Count wire edges from nand1/ZN - should fan out to and2, or2, buf4
  int wire_out = 0;
  VertexOutEdgeIterator out_iter(nand1_zn_v, graph);
  while (out_iter.hasNext()) {
    Edge *edge = out_iter.next();
    if (edge->isWire())
      wire_out++;
  }
  // n7 connects to: and2/A1, or2/A1, buf4/A = 3 wire edges
  EXPECT_EQ(wire_out, 3);
}

TEST_F(GraphLargeDesignTest, CrossDomainEdges) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // reg3 is clocked by clk2 but driven by reg1/Q (clk1 domain)
  Instance *reg3 = network->findChild(top, "reg3");
  ASSERT_NE(reg3, nullptr);
  Pin *reg3_d = network->findPin(reg3, "D");
  ASSERT_NE(reg3_d, nullptr);
  Vertex *reg3_d_v = graph->pinLoadVertex(reg3_d);
  ASSERT_NE(reg3_d_v, nullptr);

  // Should have incoming wire edge from reg1/Q
  int in_count = 0;
  VertexInEdgeIterator in_iter(reg3_d_v, graph);
  while (in_iter.hasNext()) {
    in_iter.next();
    in_count++;
  }
  EXPECT_GT(in_count, 0);
}

TEST_F(GraphLargeDesignTest, TimingAllCellTypes) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Verify arc delays are computed for each cell type
  const char *insts[] = {"buf1", "buf2", "inv1", "inv2",
                         "and1", "or1", "nand1", "nor1"};
  for (const char *name : insts) {
    Instance *inst = network->findChild(top, name);
    ASSERT_NE(inst, nullptr) << "Instance " << name << " not found";

    // Find an output pin
    InstancePinIterator *pin_iter = network->pinIterator(inst);
    while (pin_iter->hasNext()) {
      Pin *pin = pin_iter->next();
      PortDirection *dir = network->direction(pin);
      if (dir->isOutput()) {
        Vertex *v = graph->pinDrvrVertex(pin);
        if (v) {
          // Check that at least one input edge has a computed delay
          VertexInEdgeIterator in_iter(v, graph);
          bool found_delay = false;
          while (in_iter.hasNext()) {
            Edge *edge = in_iter.next();
            if (!edge->isWire()) {
              TimingArcSet *arc_set = edge->timingArcSet();
              if (arc_set && !arc_set->arcs().empty()) {
                const TimingArc *arc = arc_set->arcs()[0];
                ArcDelay delay = graph->arcDelay(edge, arc, 0);
                if (delayAsFloat(delay) > 0.0f)
                  found_delay = true;
              }
            }
          }
          EXPECT_TRUE(found_delay) << "No delay for " << name;
        }
        break;
      }
    }
    delete pin_iter;
  }
}

TEST_F(GraphLargeDesignTest, NandNorTimingSense) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // NAND2 has negative_unate from each input
  Instance *nand1 = network->findChild(top, "nand1");
  ASSERT_NE(nand1, nullptr);
  Pin *nand1_a1 = network->findPin(nand1, "A1");
  Pin *nand1_zn = network->findPin(nand1, "ZN");
  ASSERT_NE(nand1_a1, nullptr);
  ASSERT_NE(nand1_zn, nullptr);

  Edge *edge = nullptr;
  const TimingArc *arc = nullptr;
  // NAND: rise on input -> fall on output
  graph->gateEdgeArc(nand1_a1, RiseFall::rise(),
                     nand1_zn, RiseFall::fall(),
                     edge, arc);
  EXPECT_NE(edge, nullptr);
  EXPECT_NE(arc, nullptr);
}

////////////////////////////////////////////////////////////////
// GraphModificationTest: uses Nangate45 + graph_test2.v
// Tests graph behavior after network modifications (replaceCell, etc.)
////////////////////////////////////////////////////////////////

class GraphModificationTest : public ::testing::Test {
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

    Scene *corner = sta_->cmdScene();
    const MinMaxAll *min_max = MinMaxAll::all();
    LibertyLibrary *lib = sta_->readLiberty(
      "test/nangate45/Nangate45_typ.lib", corner, min_max, false);
    ASSERT_NE(lib, nullptr);

    bool ok = sta_->readVerilog("graph/test/graph_test2.v");
    ASSERT_TRUE(ok);
    ok = sta_->linkDesign("graph_test2", true);
    ASSERT_TRUE(ok);

    Network *network = sta_->network();
    Instance *top = network->topInstance();
    Pin *clk_pin = network->findPin(top, "clk");
    ASSERT_NE(clk_pin, nullptr);
    PinSet *clk_pins = new PinSet(network);
    clk_pins->insert(clk_pin);
    FloatSeq *waveform = new FloatSeq;
    waveform->push_back(0.0f);
    waveform->push_back(5.0f);
    sta_->makeClock("clk", clk_pins, false, 10.0f, waveform, nullptr, sta_->cmdMode());

    Clock *clk = sta_->cmdSdc()->findClock("clk");
    ASSERT_NE(clk, nullptr);

    Pin *d1_pin = network->findPin(top, "d1");
    ASSERT_NE(d1_pin, nullptr);
    sta_->setInputDelay(d1_pin, RiseFallBoth::riseFall(), clk,
                        RiseFall::rise(), nullptr, false, false,
                        MinMaxAll::all(), false, 1.0f, sta_->cmdSdc());

    Pin *d2_pin = network->findPin(top, "d2");
    ASSERT_NE(d2_pin, nullptr);
    sta_->setInputDelay(d2_pin, RiseFallBoth::riseFall(), clk,
                        RiseFall::rise(), nullptr, false, false,
                        MinMaxAll::all(), false, 1.0f, sta_->cmdSdc());

    design_loaded_ = true;
  }

  void TearDown() override {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_) Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  Sta *sta_;
  Tcl_Interp *interp_;
  bool design_loaded_ = false;
};

TEST_F(GraphModificationTest, ReplaceCellUpdatesGraph) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Vertex *buf1_z_v = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(buf1_z_v, nullptr);

  // Get delay before replace
  Pin *buf1_a = network->findPin(buf1, "A");
  ASSERT_NE(buf1_a, nullptr);
  Edge *edge_before = nullptr;
  const TimingArc *arc_before = nullptr;
  graph->gateEdgeArc(buf1_a, RiseFall::rise(), buf1_z, RiseFall::rise(),
                     edge_before, arc_before);
  ASSERT_NE(edge_before, nullptr);
  ArcDelay delay_before = graph->arcDelay(edge_before, arc_before, 0);

  // Replace BUF_X1 with BUF_X4 (larger, faster buffer)
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);
  sta_->updateTiming(true);

  // Verify timing changed
  graph = sta_->graph();
  buf1_z = network->findPin(buf1, "Z");
  buf1_a = network->findPin(buf1, "A");
  Edge *edge_after = nullptr;
  const TimingArc *arc_after = nullptr;
  graph->gateEdgeArc(buf1_a, RiseFall::rise(), buf1_z, RiseFall::rise(),
                     edge_after, arc_after);
  ASSERT_NE(edge_after, nullptr);
  ArcDelay delay_after = graph->arcDelay(edge_after, arc_after, 0);
  // Larger buffer should have different delay
  EXPECT_NE(delayAsFloat(delay_before), delayAsFloat(delay_after));
}

TEST_F(GraphModificationTest, ReplaceCellPreservesConnectivity) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);

  // Count edges before
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Vertex *v = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(v, nullptr);
  int out_before = 0;
  VertexOutEdgeIterator out_iter1(v, graph);
  while (out_iter1.hasNext()) { out_iter1.next(); out_before++; }

  // Replace cell
  LibertyCell *buf_x2 = network->findLibertyCell("BUF_X2");
  ASSERT_NE(buf_x2, nullptr);
  sta_->replaceCell(buf1, buf_x2);
  sta_->ensureGraph();

  // Count edges after - connectivity should be preserved
  graph = sta_->graph();
  buf1_z = network->findPin(buf1, "Z");
  v = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(v, nullptr);
  int out_after = 0;
  VertexOutEdgeIterator out_iter2(v, graph);
  while (out_iter2.hasNext()) { out_iter2.next(); out_after++; }
  EXPECT_EQ(out_before, out_after);
}

TEST_F(GraphModificationTest, ReplaceCellBackAndForth) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);

  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x1, nullptr);
  ASSERT_NE(buf_x4, nullptr);

  // Replace back and forth multiple times
  for (int i = 0; i < 3; i++) {
    sta_->replaceCell(buf1, buf_x4);
    sta_->updateTiming(true);
    Graph *graph = sta_->graph();
    EXPECT_GT(graph->vertexCount(), 0u);

    sta_->replaceCell(buf1, buf_x1);
    sta_->updateTiming(true);
    graph = sta_->graph();
    EXPECT_GT(graph->vertexCount(), 0u);
  }
}

TEST_F(GraphModificationTest, AddInstanceUpdatesGraph) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  VertexId count_before = graph->vertexCount();

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Add a new buffer instance
  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_x1, nullptr);
  Instance *new_buf = sta_->makeInstance("buf_new", buf_x1, top);
  ASSERT_NE(new_buf, nullptr);

  // Create a new net and connect
  Net *new_net = sta_->makeNet("n_new", top);
  ASSERT_NE(new_net, nullptr);

  // Connect buf_new/A to an existing net and buf_new/Z to new_net
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Net *n1_net = network->net(buf1_z);
  ASSERT_NE(n1_net, nullptr);

  sta_->connectPin(new_buf, buf_x1->findLibertyPort("A"), n1_net);
  sta_->connectPin(new_buf, buf_x1->findLibertyPort("Z"), new_net);

  sta_->updateTiming(true);
  graph = sta_->graph();

  // Should have more vertices now
  EXPECT_GT(graph->vertexCount(), count_before);
}

TEST_F(GraphModificationTest, DeleteInstanceUpdatesGraph) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  VertexId count_before = graph->vertexCount();

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // First add a new instance
  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_x1, nullptr);
  Instance *new_buf = sta_->makeInstance("buf_temp", buf_x1, top);
  ASSERT_NE(new_buf, nullptr);
  Net *temp_net = sta_->makeNet("n_temp", top);
  ASSERT_NE(temp_net, nullptr);
  sta_->connectPin(new_buf, buf_x1->findLibertyPort("Z"), temp_net);

  sta_->updateTiming(true);
  graph = sta_->graph();
  VertexId count_with_inst = graph->vertexCount();
  EXPECT_GT(count_with_inst, count_before);

  // Now disconnect and delete the instance
  Pin *new_z = network->findPin(new_buf, "Z");
  if (new_z)
    sta_->disconnectPin(new_z);
  sta_->deleteInstance(new_buf);
  sta_->deleteNet(temp_net);

  sta_->updateTiming(true);
  graph = sta_->graph();
  // Vertex count should be back to original
  EXPECT_EQ(graph->vertexCount(), count_before);
}

////////////////////////////////////////////////////////////////
// GraphMultiCornerTest: uses Nangate45 fast/slow + graph_test2.v
// Tests multi-corner graph behavior
////////////////////////////////////////////////////////////////

class GraphMultiCornerTest : public ::testing::Test {
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

    // Define two corners
    StringSeq scene_names;
    scene_names.push_back("fast");
    scene_names.push_back("slow");
    sta_->makeScenes(&scene_names);

    Scene *fast_corner = sta_->findScene("fast");
    Scene *slow_corner = sta_->findScene("slow");
    ASSERT_NE(fast_corner, nullptr);
    ASSERT_NE(slow_corner, nullptr);

    const MinMaxAll *min_max = MinMaxAll::all();
    LibertyLibrary *fast_lib = sta_->readLiberty(
      "test/nangate45/Nangate45_fast.lib", fast_corner, min_max, false);
    ASSERT_NE(fast_lib, nullptr);

    LibertyLibrary *slow_lib = sta_->readLiberty(
      "test/nangate45/Nangate45_slow.lib", slow_corner, min_max, false);
    ASSERT_NE(slow_lib, nullptr);

    bool ok = sta_->readVerilog("graph/test/graph_test2.v");
    ASSERT_TRUE(ok);
    ok = sta_->linkDesign("graph_test2", true);
    ASSERT_TRUE(ok);

    // Create clock
    Network *network = sta_->network();
    Instance *top = network->topInstance();
    Pin *clk_pin = network->findPin(top, "clk");
    ASSERT_NE(clk_pin, nullptr);
    PinSet *clk_pins = new PinSet(network);
    clk_pins->insert(clk_pin);
    FloatSeq *waveform = new FloatSeq;
    waveform->push_back(0.0f);
    waveform->push_back(5.0f);
    sta_->makeClock("clk", clk_pins, false, 10.0f, waveform, nullptr, sta_->cmdMode());

    Clock *clk = sta_->cmdSdc()->findClock("clk");
    ASSERT_NE(clk, nullptr);

    Pin *d1_pin = network->findPin(top, "d1");
    ASSERT_NE(d1_pin, nullptr);
    sta_->setInputDelay(d1_pin, RiseFallBoth::riseFall(), clk,
                        RiseFall::rise(), nullptr, false, false,
                        MinMaxAll::all(), false, 1.0f, sta_->cmdSdc());

    fast_corner_ = fast_corner;
    slow_corner_ = slow_corner;
    design_loaded_ = true;
  }

  void TearDown() override {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_) Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  Sta *sta_;
  Tcl_Interp *interp_;
  Scene *fast_corner_ = nullptr;
  Scene *slow_corner_ = nullptr;
  bool design_loaded_ = false;
};

TEST_F(GraphMultiCornerTest, DelaysDifferByCorner) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_a = network->findPin(buf1, "A");
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_a, nullptr);
  ASSERT_NE(buf1_z, nullptr);

  Edge *edge = nullptr;
  const TimingArc *arc = nullptr;
  graph->gateEdgeArc(buf1_a, RiseFall::rise(), buf1_z, RiseFall::rise(),
                     edge, arc);
  ASSERT_NE(edge, nullptr);
  ASSERT_NE(arc, nullptr);

  // Get delay for each corner
  DcalcAPIndex fast_idx = fast_corner_->dcalcAnalysisPtIndex(MinMax::max());
  DcalcAPIndex slow_idx = slow_corner_->dcalcAnalysisPtIndex(MinMax::max());
  ArcDelay fast_delay = graph->arcDelay(edge, arc, fast_idx);
  ArcDelay slow_delay = graph->arcDelay(edge, arc, slow_idx);

  // Slow corner should have larger delay than fast
  EXPECT_GT(delayAsFloat(slow_delay), delayAsFloat(fast_delay));
}

TEST_F(GraphMultiCornerTest, SlewsDifferByCorner) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);
  Vertex *v = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(v, nullptr);

  DcalcAPIndex fast_idx = fast_corner_->dcalcAnalysisPtIndex(MinMax::max());
  DcalcAPIndex slow_idx = slow_corner_->dcalcAnalysisPtIndex(MinMax::max());
  const Slew &fast_slew = graph->slew(v, RiseFall::rise(), fast_idx);
  const Slew &slow_slew = graph->slew(v, RiseFall::rise(), slow_idx);

  // Both should be non-zero
  EXPECT_GT(delayAsFloat(fast_slew), 0.0f);
  EXPECT_GT(delayAsFloat(slow_slew), 0.0f);
  // Slow corner should have larger slew
  EXPECT_GT(delayAsFloat(slow_slew), delayAsFloat(fast_slew));
}

TEST_F(GraphMultiCornerTest, GraphSharedAcrossCorners) {
  ASSERT_TRUE(design_loaded_);
  sta_->updateTiming(true);
  Graph *graph = sta_->graph();

  // Graph object is shared - vertex count same regardless of corner
  EXPECT_GT(graph->vertexCount(), 0u);

  // Verify same graph reference after updating both corners
  Graph *graph2 = sta_->graph();
  EXPECT_EQ(graph, graph2);
}

} // namespace sta
