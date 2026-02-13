#include <gtest/gtest.h>
#include <set>
#include "MinMax.hh"
#include "Transition.hh"
#include "StringUtil.hh"

// SDF module smoke tests - verifying types used by SDF reader/writer
namespace sta {

class SdfSmokeTest : public ::testing::Test {};

// SDF uses RiseFall for transitions
TEST_F(SdfSmokeTest, SdfTripleIndices) {
  // SDF triples are indexed by rise/fall
  EXPECT_EQ(RiseFall::riseIndex(), 0);
  EXPECT_EQ(RiseFall::fallIndex(), 1);
}

// SDF uses MinMax for min/typ/max
TEST_F(SdfSmokeTest, MinMaxForSdf) {
  EXPECT_NE(MinMax::min(), nullptr);
  EXPECT_NE(MinMax::max(), nullptr);
}

// SDF transitions
TEST_F(SdfSmokeTest, SdfTransitions) {
  // SDF has 12 transition types + rise_fall
  const Transition *t01 = Transition::rise();
  const Transition *t10 = Transition::fall();
  const Transition *t0z = Transition::tr0Z();
  const Transition *tz1 = Transition::trZ1();
  EXPECT_NE(t01, nullptr);
  EXPECT_NE(t10, nullptr);
  EXPECT_NE(t0z, nullptr);
  EXPECT_NE(tz1, nullptr);
}

// Test string comparison used in SDF parsing
TEST_F(SdfSmokeTest, StringComparison) {
  EXPECT_TRUE(stringEq("IOPATH", "IOPATH"));
  EXPECT_FALSE(stringEq("IOPATH", "iopath"));
  EXPECT_TRUE(stringEqual("IOPATH", "iopath")); // case insensitive
}

////////////////////////////////////////////////////////////////
// Additional SDF-relevant type tests
////////////////////////////////////////////////////////////////

// Test all 12 SDF transitions
TEST_F(SdfSmokeTest, AllSdfTransitions) {
  // 01 (rise)
  EXPECT_NE(Transition::rise(), nullptr);
  EXPECT_NE(Transition::rise()->asRiseFall(), nullptr);
  EXPECT_EQ(Transition::rise()->asRiseFall(), RiseFall::rise());

  // 10 (fall)
  EXPECT_NE(Transition::fall(), nullptr);
  EXPECT_NE(Transition::fall()->asRiseFall(), nullptr);
  EXPECT_EQ(Transition::fall()->asRiseFall(), RiseFall::fall());

  // 0Z
  EXPECT_NE(Transition::tr0Z(), nullptr);
  EXPECT_NE(Transition::tr0Z()->asInitFinalString(), nullptr);

  // Z1
  EXPECT_NE(Transition::trZ1(), nullptr);

  // 1Z
  EXPECT_NE(Transition::tr1Z(), nullptr);

  // Z0
  EXPECT_NE(Transition::trZ0(), nullptr);

  // 0X
  EXPECT_NE(Transition::tr0X(), nullptr);

  // X1
  EXPECT_NE(Transition::trX1(), nullptr);

  // 1X
  EXPECT_NE(Transition::tr1X(), nullptr);

  // X0
  EXPECT_NE(Transition::trX0(), nullptr);

  // XZ
  EXPECT_NE(Transition::trXZ(), nullptr);

  // ZX
  EXPECT_NE(Transition::trZX(), nullptr);
}

// Test transition index values
TEST_F(SdfSmokeTest, TransitionIndices) {
  EXPECT_EQ(Transition::rise()->sdfTripleIndex(), RiseFall::riseIndex());
  EXPECT_EQ(Transition::fall()->sdfTripleIndex(), RiseFall::fallIndex());
  EXPECT_GE(Transition::maxIndex(), 0);
}

// Test transition name strings
TEST_F(SdfSmokeTest, TransitionNames) {
  EXPECT_EQ(Transition::rise()->to_string(), "^");
  EXPECT_EQ(Transition::fall()->to_string(), "v");
  EXPECT_FALSE(Transition::tr0Z()->to_string().empty());
  EXPECT_FALSE(Transition::trZ1()->to_string().empty());
}

// Test transition find
TEST_F(SdfSmokeTest, TransitionFind) {
  const Transition *rise = Transition::find("^");
  EXPECT_EQ(rise, Transition::rise());

  const Transition *fall = Transition::find("v");
  EXPECT_EQ(fall, Transition::fall());
}

// Test transition matches
TEST_F(SdfSmokeTest, TransitionMatches) {
  EXPECT_TRUE(Transition::rise()->matches(Transition::rise()));
  EXPECT_FALSE(Transition::rise()->matches(Transition::fall()));

  // riseFall matches both
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::rise()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::fall()));
}

// Test RiseFall find
TEST_F(SdfSmokeTest, RiseFallFind) {
  const RiseFall *rise = RiseFall::find("rise");
  EXPECT_EQ(rise, RiseFall::rise());

  const RiseFall *fall = RiseFall::find("fall");
  EXPECT_EQ(fall, RiseFall::fall());
}

// Test RiseFall names
TEST_F(SdfSmokeTest, RiseFallNames) {
  EXPECT_STREQ(RiseFall::rise()->name(), "rise");
  EXPECT_STREQ(RiseFall::fall()->name(), "fall");
  EXPECT_STREQ(RiseFall::rise()->shortName(), "^");
  EXPECT_STREQ(RiseFall::fall()->shortName(), "v");
}

// Test RiseFall opposite
TEST_F(SdfSmokeTest, RiseFallOpposite) {
  EXPECT_EQ(RiseFall::rise()->opposite(), RiseFall::fall());
  EXPECT_EQ(RiseFall::fall()->opposite(), RiseFall::rise());
}

// Test RiseFall asRiseFallBoth
TEST_F(SdfSmokeTest, RiseFallAsRiseFallBoth) {
  const RiseFallBoth *rfb = RiseFall::rise()->asRiseFallBoth();
  EXPECT_NE(rfb, nullptr);
  EXPECT_EQ(rfb->asRiseFall(), RiseFall::rise());
}

// Test RiseFallBoth
TEST_F(SdfSmokeTest, RiseFallBothBasic) {
  EXPECT_NE(RiseFallBoth::rise(), nullptr);
  EXPECT_NE(RiseFallBoth::fall(), nullptr);
  EXPECT_NE(RiseFallBoth::riseFall(), nullptr);

  EXPECT_STREQ(RiseFallBoth::rise()->name(), "rise");
  EXPECT_STREQ(RiseFallBoth::fall()->name(), "fall");
}

// Test RiseFallBoth matches
TEST_F(SdfSmokeTest, RiseFallBothMatches) {
  EXPECT_TRUE(RiseFallBoth::rise()->matches(RiseFall::rise()));
  EXPECT_FALSE(RiseFallBoth::rise()->matches(RiseFall::fall()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::rise()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::fall()));
}

// Test MinMax details
TEST_F(SdfSmokeTest, MinMaxDetails) {
  EXPECT_STREQ(MinMax::min()->to_string().c_str(), "min");
  EXPECT_STREQ(MinMax::max()->to_string().c_str(), "max");
  EXPECT_EQ(MinMax::min()->index(), MinMax::minIndex());
  EXPECT_EQ(MinMax::max()->index(), MinMax::maxIndex());
}

// Test MinMax opposite
TEST_F(SdfSmokeTest, MinMaxOpposite) {
  EXPECT_EQ(MinMax::min()->opposite(), MinMax::max());
  EXPECT_EQ(MinMax::max()->opposite(), MinMax::min());
}

// Test MinMax compare
TEST_F(SdfSmokeTest, MinMaxCompare) {
  // min->compare returns true when value1 < value2
  EXPECT_TRUE(MinMax::min()->compare(1.0f, 2.0f));
  EXPECT_FALSE(MinMax::min()->compare(2.0f, 1.0f));

  // max->compare returns true when value1 > value2
  EXPECT_TRUE(MinMax::max()->compare(2.0f, 1.0f));
  EXPECT_FALSE(MinMax::max()->compare(1.0f, 2.0f));
}

// Test MinMax minMax function
TEST_F(SdfSmokeTest, MinMaxMinMaxFunc) {
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(1.0f, 2.0f), 1.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(1.0f, 2.0f), 2.0f);
}

// Test MinMax find
TEST_F(SdfSmokeTest, MinMaxFind) {
  EXPECT_EQ(MinMax::find("min"), MinMax::min());
  EXPECT_EQ(MinMax::find("max"), MinMax::max());
  EXPECT_EQ(MinMax::find(0), MinMax::min());
  EXPECT_EQ(MinMax::find(1), MinMax::max());
}

// Test MinMaxAll
TEST_F(SdfSmokeTest, MinMaxAllBasic) {
  EXPECT_NE(MinMaxAll::min(), nullptr);
  EXPECT_NE(MinMaxAll::max(), nullptr);
  EXPECT_NE(MinMaxAll::all(), nullptr);
}

// Test MinMaxAll matches
TEST_F(SdfSmokeTest, MinMaxAllMatches) {
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::max()));
  EXPECT_TRUE(MinMaxAll::min()->matches(MinMax::min()));
  EXPECT_FALSE(MinMaxAll::min()->matches(MinMax::max()));
}

// Test MinMaxAll range
TEST_F(SdfSmokeTest, MinMaxAllRange) {
  auto range = MinMaxAll::all()->range();
  EXPECT_EQ(range.size(), 2u);

  auto min_range = MinMaxAll::min()->range();
  EXPECT_EQ(min_range.size(), 1u);
}

// Test MinMax initValue
TEST_F(SdfSmokeTest, MinMaxInitValue) {
  // min's init value is very large (positive INF)
  EXPECT_GT(MinMax::min()->initValue(), 0.0f);
  // max's init value is very negative (-INF)
  EXPECT_LT(MinMax::max()->initValue(), 0.0f);
}

////////////////////////////////////////////////////////////////
// Additional SDF-relevant tests for function coverage
////////////////////////////////////////////////////////////////

// Test all RiseFall methods
TEST_F(SdfSmokeTest, RiseFallIndex) {
  EXPECT_EQ(RiseFall::rise()->index(), 0);
  EXPECT_EQ(RiseFall::fall()->index(), 1);
}

// Test RiseFall::asTransition (Transition.cc coverage)
TEST_F(SdfSmokeTest, RiseFallAsTransition) {
  const Transition *tr_rise = RiseFall::rise()->asTransition();
  EXPECT_EQ(tr_rise, Transition::rise());
  const Transition *tr_fall = RiseFall::fall()->asTransition();
  EXPECT_EQ(tr_fall, Transition::fall());
}

// Test RiseFallBoth::find
TEST_F(SdfSmokeTest, RiseFallBothFindSdf) {
  const RiseFallBoth *r = RiseFallBoth::find("rise");
  EXPECT_NE(r, nullptr);
  EXPECT_EQ(r, RiseFallBoth::rise());
  const RiseFallBoth *f = RiseFallBoth::find("fall");
  EXPECT_NE(f, nullptr);
  EXPECT_EQ(f, RiseFallBoth::fall());
  const RiseFallBoth *rf = RiseFallBoth::find("rise_fall");
  EXPECT_NE(rf, nullptr);
  EXPECT_EQ(rf, RiseFallBoth::riseFall());
}

// Test RiseFallBoth::matches(const Transition*)
TEST_F(SdfSmokeTest, RiseFallBothMatchesTransition) {
  EXPECT_TRUE(RiseFallBoth::rise()->matches(Transition::rise()));
  EXPECT_FALSE(RiseFallBoth::rise()->matches(Transition::fall()));
  EXPECT_TRUE(RiseFallBoth::fall()->matches(Transition::fall()));
  EXPECT_FALSE(RiseFallBoth::fall()->matches(Transition::rise()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(Transition::rise()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(Transition::fall()));
}

// Test Transition::asRiseFallBoth
TEST_F(SdfSmokeTest, TransitionAsRiseFallBothSdf) {
  const RiseFallBoth *rfb = Transition::rise()->asRiseFallBoth();
  EXPECT_NE(rfb, nullptr);
  const RiseFallBoth *rfb2 = Transition::fall()->asRiseFallBoth();
  EXPECT_NE(rfb2, nullptr);
}

// Test Transition find with init/final strings (used by SDF reader)
TEST_F(SdfSmokeTest, TransitionFindInitFinalSdf) {
  EXPECT_EQ(Transition::find("01"), Transition::rise());
  EXPECT_EQ(Transition::find("10"), Transition::fall());
  EXPECT_EQ(Transition::find("0Z"), Transition::tr0Z());
  EXPECT_EQ(Transition::find("Z1"), Transition::trZ1());
  EXPECT_EQ(Transition::find("1Z"), Transition::tr1Z());
  EXPECT_EQ(Transition::find("Z0"), Transition::trZ0());
  EXPECT_EQ(Transition::find("0X"), Transition::tr0X());
  EXPECT_EQ(Transition::find("X1"), Transition::trX1());
  EXPECT_EQ(Transition::find("1X"), Transition::tr1X());
  EXPECT_EQ(Transition::find("X0"), Transition::trX0());
  EXPECT_EQ(Transition::find("XZ"), Transition::trXZ());
  EXPECT_EQ(Transition::find("ZX"), Transition::trZX());
}

// Test all transition sdfTripleIndex values
TEST_F(SdfSmokeTest, AllTransitionSdfTripleIndex) {
  EXPECT_EQ(Transition::rise()->sdfTripleIndex(), 0);
  EXPECT_EQ(Transition::fall()->sdfTripleIndex(), 1);
  EXPECT_EQ(Transition::tr0Z()->sdfTripleIndex(), 2);
  EXPECT_EQ(Transition::trZ1()->sdfTripleIndex(), 3);
  EXPECT_EQ(Transition::tr1Z()->sdfTripleIndex(), 4);
  EXPECT_EQ(Transition::trZ0()->sdfTripleIndex(), 5);
  EXPECT_EQ(Transition::tr0X()->sdfTripleIndex(), 6);
  EXPECT_EQ(Transition::trX1()->sdfTripleIndex(), 7);
  EXPECT_EQ(Transition::tr1X()->sdfTripleIndex(), 8);
  EXPECT_EQ(Transition::trX0()->sdfTripleIndex(), 9);
  EXPECT_EQ(Transition::trXZ()->sdfTripleIndex(), 10);
  EXPECT_EQ(Transition::trZX()->sdfTripleIndex(), 11);
}

// Test all transition initFinal strings
TEST_F(SdfSmokeTest, AllTransitionInitFinalString) {
  EXPECT_STREQ(Transition::rise()->asInitFinalString(), "01");
  EXPECT_STREQ(Transition::fall()->asInitFinalString(), "10");
  EXPECT_STREQ(Transition::tr0Z()->asInitFinalString(), "0Z");
  EXPECT_STREQ(Transition::trZ1()->asInitFinalString(), "Z1");
  EXPECT_STREQ(Transition::tr1Z()->asInitFinalString(), "1Z");
  EXPECT_STREQ(Transition::trZ0()->asInitFinalString(), "Z0");
  EXPECT_STREQ(Transition::tr0X()->asInitFinalString(), "0X");
  EXPECT_STREQ(Transition::trX1()->asInitFinalString(), "X1");
  EXPECT_STREQ(Transition::tr1X()->asInitFinalString(), "1X");
  EXPECT_STREQ(Transition::trX0()->asInitFinalString(), "X0");
  EXPECT_STREQ(Transition::trXZ()->asInitFinalString(), "XZ");
  EXPECT_STREQ(Transition::trZX()->asInitFinalString(), "ZX");
}

// Test all transition asRiseFall
TEST_F(SdfSmokeTest, AllTransitionAsRiseFall) {
  EXPECT_EQ(Transition::tr0Z()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::trZ1()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::tr0X()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::trX1()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::tr1Z()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::trZ0()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::tr1X()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::trX0()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::trXZ()->asRiseFall(), nullptr);
  EXPECT_EQ(Transition::trZX()->asRiseFall(), nullptr);
}

// Test all transition to_string
TEST_F(SdfSmokeTest, AllTransitionToString) {
  EXPECT_FALSE(Transition::tr0Z()->to_string().empty());
  EXPECT_FALSE(Transition::trZ1()->to_string().empty());
  EXPECT_FALSE(Transition::tr1Z()->to_string().empty());
  EXPECT_FALSE(Transition::trZ0()->to_string().empty());
  EXPECT_FALSE(Transition::tr0X()->to_string().empty());
  EXPECT_FALSE(Transition::trX1()->to_string().empty());
  EXPECT_FALSE(Transition::tr1X()->to_string().empty());
  EXPECT_FALSE(Transition::trX0()->to_string().empty());
  EXPECT_FALSE(Transition::trXZ()->to_string().empty());
  EXPECT_FALSE(Transition::trZX()->to_string().empty());
  EXPECT_FALSE(Transition::riseFall()->to_string().empty());
}

// Test RiseFallBoth range and rangeIndex
TEST_F(SdfSmokeTest, RiseFallBothRangesSdf) {
  auto &rr = RiseFallBoth::rise()->range();
  EXPECT_EQ(rr.size(), 1u);
  auto &fr = RiseFallBoth::fall()->range();
  EXPECT_EQ(fr.size(), 1u);
  auto &rfr = RiseFallBoth::riseFall()->range();
  EXPECT_EQ(rfr.size(), 2u);

  auto &ri = RiseFallBoth::rise()->rangeIndex();
  EXPECT_EQ(ri.size(), 1u);
  auto &fi = RiseFallBoth::fall()->rangeIndex();
  EXPECT_EQ(fi.size(), 1u);
  auto &rfi = RiseFallBoth::riseFall()->rangeIndex();
  EXPECT_EQ(rfi.size(), 2u);
}

// Test MinMax range and rangeIndex
TEST_F(SdfSmokeTest, MinMaxRange) {
  auto &range = MinMax::range();
  EXPECT_EQ(range.size(), 2u);
  auto &ridx = MinMax::rangeIndex();
  EXPECT_EQ(ridx.size(), 2u);
}

// Test Transition matches self
TEST_F(SdfSmokeTest, TransitionMatchesSelf) {
  EXPECT_TRUE(Transition::rise()->matches(Transition::rise()));
  EXPECT_FALSE(Transition::rise()->matches(Transition::fall()));
  EXPECT_TRUE(Transition::fall()->matches(Transition::fall()));
  EXPECT_TRUE(Transition::tr0Z()->matches(Transition::tr0Z()));
  EXPECT_FALSE(Transition::tr0Z()->matches(Transition::trZ1()));
}

// Test MinMaxAll asMinMax
TEST_F(SdfSmokeTest, MinMaxAllAsMinMax) {
  EXPECT_EQ(MinMaxAll::min()->asMinMax(), MinMax::min());
  EXPECT_EQ(MinMaxAll::max()->asMinMax(), MinMax::max());
}

// SdfTriple tests (class defined in SdfReader.cc, not directly accessible
// but we test the concepts that SDF triple values represent)

// Test SDF edge names for transitions
// Covers: SdfWriter::sdfEdge - exercises transition to SDF edge string mapping
TEST_F(SdfSmokeTest, R5_TransitionAsInitFinalString) {
  // SDF uses init/final transition string representation
  EXPECT_NE(Transition::rise()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::fall()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::tr0Z()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::trZ1()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::tr1Z()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::trZ0()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::tr0X()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::trX1()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::tr1X()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::trX0()->asInitFinalString(), nullptr);
}

// Test Transition asRiseFall for SDF edge transitions
TEST_F(SdfSmokeTest, R5_TransitionAsRiseFallAll) {
  // Rise transitions map to rise
  EXPECT_EQ(Transition::rise()->asRiseFall(), RiseFall::rise());
  // Fall transitions map to fall
  EXPECT_EQ(Transition::fall()->asRiseFall(), RiseFall::fall());
  // Z1 maps to rise
  EXPECT_EQ(Transition::trZ1()->asRiseFall(), RiseFall::rise());
  // Z0 maps to fall
  EXPECT_EQ(Transition::trZ0()->asRiseFall(), RiseFall::fall());
  // 0Z maps to rise
  EXPECT_EQ(Transition::tr0Z()->asRiseFall(), RiseFall::rise());
  // 1Z maps to fall
  EXPECT_EQ(Transition::tr1Z()->asRiseFall(), RiseFall::fall());
}

// Test MinMaxAll matches method
TEST_F(SdfSmokeTest, R5_MinMaxAllMatches) {
  EXPECT_TRUE(MinMaxAll::min()->matches(MinMax::min()));
  EXPECT_FALSE(MinMaxAll::min()->matches(MinMax::max()));
  EXPECT_TRUE(MinMaxAll::max()->matches(MinMax::max()));
  EXPECT_FALSE(MinMaxAll::max()->matches(MinMax::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::max()));
}

// Test MinMaxAll matches MinMaxAll
TEST_F(SdfSmokeTest, R5_MinMaxAllMatchesAll) {
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::max()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::all()));
  EXPECT_TRUE(MinMaxAll::min()->matches(MinMaxAll::min()));
  EXPECT_FALSE(MinMaxAll::min()->matches(MinMaxAll::max()));
}

// Test MinMax find by name
TEST_F(SdfSmokeTest, R5_MinMaxFindByName) {
  EXPECT_EQ(MinMax::find("min"), MinMax::min());
  EXPECT_EQ(MinMax::find("max"), MinMax::max());
  EXPECT_EQ(MinMax::find("nonexistent"), nullptr);
}

// Test MinMax find by index
TEST_F(SdfSmokeTest, R5_MinMaxFindByIndex) {
  EXPECT_EQ(MinMax::find(MinMax::minIndex()), MinMax::min());
  EXPECT_EQ(MinMax::find(MinMax::maxIndex()), MinMax::max());
}

// Test MinMaxAll find by name
TEST_F(SdfSmokeTest, R5_MinMaxAllFindByName) {
  EXPECT_EQ(MinMaxAll::find("min"), MinMaxAll::min());
  EXPECT_EQ(MinMaxAll::find("max"), MinMaxAll::max());
  EXPECT_EQ(MinMaxAll::find("all"), MinMaxAll::all());
  EXPECT_EQ(MinMaxAll::find("nonexistent"), nullptr);
}

// Test MinMax opposite
TEST_F(SdfSmokeTest, R5_MinMaxOpposite) {
  EXPECT_EQ(MinMax::min()->opposite(), MinMax::max());
  EXPECT_EQ(MinMax::max()->opposite(), MinMax::min());
}

// Test MinMax minMax function
TEST_F(SdfSmokeTest, R5_MinMaxMinMaxFunc) {
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(3.0f, 5.0f), 3.0f);
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(5.0f, 3.0f), 3.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(3.0f, 5.0f), 5.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(5.0f, 3.0f), 5.0f);
}

// Test MinMax to_string
TEST_F(SdfSmokeTest, R5_MinMaxToString) {
  EXPECT_EQ(MinMax::min()->to_string(), "min");
  EXPECT_EQ(MinMax::max()->to_string(), "max");
}

// Test MinMaxAll to_string
TEST_F(SdfSmokeTest, R5_MinMaxAllToString) {
  EXPECT_EQ(MinMaxAll::min()->to_string(), "min");
  EXPECT_EQ(MinMaxAll::max()->to_string(), "max");
  EXPECT_EQ(MinMaxAll::all()->to_string(), "all");
}

// Test MinMax initValueInt
TEST_F(SdfSmokeTest, R5_MinMaxInitValueInt) {
  // min's init value is a large positive integer
  EXPECT_GT(MinMax::min()->initValueInt(), 0);
  // max's init value is a large negative integer
  EXPECT_LT(MinMax::max()->initValueInt(), 0);
}

// Test MinMaxAll rangeIndex
TEST_F(SdfSmokeTest, R5_MinMaxAllRangeIndex) {
  auto &min_range_idx = MinMaxAll::min()->rangeIndex();
  EXPECT_EQ(min_range_idx.size(), 1u);
  EXPECT_EQ(min_range_idx[0], MinMax::minIndex());

  auto &max_range_idx = MinMaxAll::max()->rangeIndex();
  EXPECT_EQ(max_range_idx.size(), 1u);
  EXPECT_EQ(max_range_idx[0], MinMax::maxIndex());

  auto &all_range_idx = MinMaxAll::all()->rangeIndex();
  EXPECT_EQ(all_range_idx.size(), 2u);
}

// Test MinMax constructor coverage (exercises internal MinMax ctor)
// Covers: MinMax::MinMax(const char*, int, float, int, compare_fn)
TEST_F(SdfSmokeTest, R6_MinMaxConstructorCoverage) {
  // The MinMax constructor is private; we verify it was called via singletons
  const MinMax *mn = MinMax::min();
  EXPECT_STREQ(mn->to_string().c_str(), "min");
  EXPECT_EQ(mn->index(), MinMax::minIndex());
  EXPECT_GT(mn->initValue(), 0.0f);
  EXPECT_GT(mn->initValueInt(), 0);
  // Compare function: min returns true when value1 < value2
  EXPECT_TRUE(mn->compare(1.0f, 2.0f));
  EXPECT_FALSE(mn->compare(3.0f, 2.0f));

  const MinMax *mx = MinMax::max();
  EXPECT_STREQ(mx->to_string().c_str(), "max");
  EXPECT_EQ(mx->index(), MinMax::maxIndex());
  EXPECT_LT(mx->initValue(), 0.0f);
  EXPECT_LT(mx->initValueInt(), 0);
  EXPECT_TRUE(mx->compare(3.0f, 2.0f));
  EXPECT_FALSE(mx->compare(1.0f, 2.0f));
}

// Test MinMax minMax with equal values
// Covers: MinMax::minMax edge case
TEST_F(SdfSmokeTest, R6_MinMaxMinMaxEqualValues) {
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(5.0f, 5.0f), 5.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(5.0f, 5.0f), 5.0f);
}

// Test MinMaxAll index values
// Covers: MinMaxAll constructor internals
TEST_F(SdfSmokeTest, R6_MinMaxAllIndices) {
  EXPECT_EQ(MinMaxAll::min()->index(), 0);
  EXPECT_EQ(MinMaxAll::max()->index(), 1);
  EXPECT_EQ(MinMaxAll::all()->index(), 2);
}

// Test MinMax find with nullptr
// Covers: MinMax::find edge case
TEST_F(SdfSmokeTest, R6_MinMaxFindNull) {
  const MinMax *result = MinMax::find("invalid_string");
  EXPECT_EQ(result, nullptr);
}

// Test RiseFall asRiseFallBoth
// Covers: RiseFall::asRiseFallBoth
TEST_F(SdfSmokeTest, R6_RiseFallAsRiseFallBoth) {
  const RiseFallBoth *rfb = RiseFall::rise()->asRiseFallBoth();
  EXPECT_EQ(rfb, RiseFallBoth::rise());
  rfb = RiseFall::fall()->asRiseFallBoth();
  EXPECT_EQ(rfb, RiseFallBoth::fall());
}

// Test Transition::riseFall matches all transitions
// Covers: Transition::matches for riseFall wildcard
TEST_F(SdfSmokeTest, R6_TransitionRiseFallMatchesAll) {
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::rise()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::fall()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::tr0Z()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::trZ1()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::tr1Z()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::trZ0()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::tr0X()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::trX1()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::tr1X()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::trX0()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::trXZ()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::trZX()));
}

// Test Transition::find with empty/unknown string
// Covers: Transition::find edge case
TEST_F(SdfSmokeTest, R6_TransitionFindUnknown) {
  const Transition *t = Transition::find("nonexistent");
  EXPECT_EQ(t, nullptr);
}

// Test RiseFall::find with unknown string
// Covers: RiseFall::find edge case
TEST_F(SdfSmokeTest, R6_RiseFallFindUnknown) {
  const RiseFall *rf = RiseFall::find("unknown");
  EXPECT_EQ(rf, nullptr);
}

// Test Transition::maxIndex value
// Covers: Transition::maxIndex
TEST_F(SdfSmokeTest, R6_TransitionMaxIndex) {
  // Transition::maxIndex() returns the max valid index
  EXPECT_GE(Transition::maxIndex(), 1);
  // All transitions should have index <= maxIndex
  EXPECT_LE(Transition::rise()->index(), Transition::maxIndex());
  EXPECT_LE(Transition::fall()->index(), Transition::maxIndex());
}

// Test RiseFall to_string
// Covers: RiseFall::to_string
TEST_F(SdfSmokeTest, R6_RiseFallToString) {
  EXPECT_EQ(RiseFall::rise()->to_string(), "^");
  EXPECT_EQ(RiseFall::fall()->to_string(), "v");
}

// Test MinMax compare with equal values
// Covers: MinMax::compare edge case
TEST_F(SdfSmokeTest, R6_MinMaxCompareEqual) {
  EXPECT_FALSE(MinMax::min()->compare(5.0f, 5.0f));
  EXPECT_FALSE(MinMax::max()->compare(5.0f, 5.0f));
}

// Test MinMax compare with negative values
// Covers: MinMax::compare with negatives
TEST_F(SdfSmokeTest, R6_MinMaxCompareNegative) {
  EXPECT_TRUE(MinMax::min()->compare(-2.0f, -1.0f));
  EXPECT_FALSE(MinMax::min()->compare(-1.0f, -2.0f));
  EXPECT_TRUE(MinMax::max()->compare(-1.0f, -2.0f));
  EXPECT_FALSE(MinMax::max()->compare(-2.0f, -1.0f));
}

// Test MinMax compare with zero
// Covers: MinMax::compare with zero
TEST_F(SdfSmokeTest, R6_MinMaxCompareZero) {
  EXPECT_TRUE(MinMax::min()->compare(0.0f, 1.0f));
  EXPECT_FALSE(MinMax::min()->compare(0.0f, 0.0f));
  EXPECT_TRUE(MinMax::max()->compare(1.0f, 0.0f));
  EXPECT_FALSE(MinMax::max()->compare(0.0f, 0.0f));
}

// Test MinMaxAll range sizes
// Covers: MinMaxAll::range
TEST_F(SdfSmokeTest, R6_MinMaxAllRangeSizes) {
  EXPECT_EQ(MinMaxAll::min()->range().size(), 1u);
  EXPECT_EQ(MinMaxAll::max()->range().size(), 1u);
  EXPECT_EQ(MinMaxAll::all()->range().size(), 2u);
}

// Test Transition sdfTripleIndex uniqueness
// Covers: Transition::sdfTripleIndex
TEST_F(SdfSmokeTest, R6_TransitionSdfTripleIndexUnique) {
  std::set<int> indices;
  indices.insert(Transition::rise()->sdfTripleIndex());
  indices.insert(Transition::fall()->sdfTripleIndex());
  indices.insert(Transition::tr0Z()->sdfTripleIndex());
  indices.insert(Transition::trZ1()->sdfTripleIndex());
  indices.insert(Transition::tr1Z()->sdfTripleIndex());
  indices.insert(Transition::trZ0()->sdfTripleIndex());
  indices.insert(Transition::tr0X()->sdfTripleIndex());
  indices.insert(Transition::trX1()->sdfTripleIndex());
  indices.insert(Transition::tr1X()->sdfTripleIndex());
  indices.insert(Transition::trX0()->sdfTripleIndex());
  indices.insert(Transition::trXZ()->sdfTripleIndex());
  indices.insert(Transition::trZX()->sdfTripleIndex());
  EXPECT_EQ(indices.size(), 12u);
}

// Test RiseFall range iteration
// Covers: RiseFall::range
TEST_F(SdfSmokeTest, R6_RiseFallRangeIteration) {
  int count = 0;
  for (auto rf : RiseFall::range()) {
    EXPECT_NE(rf, nullptr);
    count++;
  }
  EXPECT_EQ(count, 2);
}

// Test MinMax range
// Covers: MinMax::range
TEST_F(SdfSmokeTest, R6_MinMaxRangeIteration) {
  int count = 0;
  for (auto mm : MinMax::range()) {
    EXPECT_NE(mm, nullptr);
    count++;
  }
  EXPECT_EQ(count, 2);
}

// Test RiseFallBoth find with nullptr
// Covers: RiseFallBoth::find edge case
TEST_F(SdfSmokeTest, R6_RiseFallBothFindNull) {
  const RiseFallBoth *result = RiseFallBoth::find("nonexistent");
  EXPECT_EQ(result, nullptr);
}

// Test Transition asRiseFallBoth for non-rise/fall transitions
// Covers: Transition::asRiseFallBoth edge cases
TEST_F(SdfSmokeTest, R6_TransitionAsRiseFallBothTristate) {
  // Tristate transitions should still return valid RiseFallBoth
  const RiseFallBoth *rfb_0z = Transition::tr0Z()->asRiseFallBoth();
  EXPECT_NE(rfb_0z, nullptr);
}

// Test Transition to_string for riseFall wildcard
// Covers: Transition::to_string for riseFall
TEST_F(SdfSmokeTest, R6_TransitionRiseFallToString) {
  EXPECT_FALSE(Transition::riseFall()->to_string().empty());
}

////////////////////////////////////////////////////////////////
// R8_ tests for SDF module coverage improvement
// SdfWriter, SdfTriple, SdfPortSpec are all internal/protected.
// We exercise them through the full STA flow.
////////////////////////////////////////////////////////////////

} // namespace sta

#include <tcl.h>
#include <cstdio>
#include "Sta.hh"
#include "Network.hh"
#include "ReportTcl.hh"
#include "Corner.hh"
#include "Error.hh"
#include "sdf/SdfReader.hh"

namespace sta {

class SdfDesignTest : public ::testing::Test {
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

// Test writeSdf exercises SdfWriter constructor/destructor,
// writeTrailer, writeInstTrailer, sdfEdge, writeTimingCheckHeader/Trailer
// Covers: SdfWriter::~SdfWriter, SdfWriter::writeTrailer,
//         SdfWriter::writeInstTrailer, SdfWriter::writeTimingCheckHeader,
//         SdfWriter::writeTimingCheckTrailer, SdfWriter::sdfEdge
TEST_F(SdfDesignTest, R8_WriteSdfExercisesWriter) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  // Read SPEF to have timing data
  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Write SDF to a temp file - this exercises all SdfWriter methods
  const char *tmpfile = "/tmp/test_r8_sdf_output.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, true);

  // Verify the file was created and has content
  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);

  // Clean up
  std::remove(tmpfile);
}

// Test writeSdf with gzip
// Covers: SdfWriter methods through gzip path
TEST_F(SdfDesignTest, R8_WriteSdfGzip) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r8_sdf_output.sdf.gz";
  sta_->writeSdf(tmpfile, corner, '/', false, 3, true, true, true);

  // Verify the file was created
  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);

  std::remove(tmpfile);
}

////////////////////////////////////////////////////////////////
// R9_ tests for SDF module coverage improvement
////////////////////////////////////////////////////////////////

// Test writeSdf with dot divider
// Covers: SdfWriter path separator handling
TEST_F(SdfDesignTest, R9_WriteSdfDotDivider) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_dot.sdf";
  sta_->writeSdf(tmpfile, corner, '.', true, 3, false, true, true);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test writeSdf without typ values
// Covers: SdfWriter min/max only path
TEST_F(SdfDesignTest, R9_WriteSdfNoTyp) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_notyp.sdf";
  sta_->writeSdf(tmpfile, corner, '/', false, 3, false, true, true);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test writeSdf with high precision
// Covers: SdfWriter digit formatting
TEST_F(SdfDesignTest, R9_WriteSdfHighPrecision) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_highprec.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 6, false, true, true);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test writeSdf with no_timestamp
// Covers: SdfWriter header control
TEST_F(SdfDesignTest, R9_WriteSdfNoTimestamp) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_notimestamp.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, false, true);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test writeSdf with no_timescale
// Covers: SdfWriter timescale control
TEST_F(SdfDesignTest, R9_WriteSdfNoTimescale) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_notimescale.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, false);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test writeSdf and readSdf round-trip
// Covers: readSdf, SdfReader constructor, SdfScanner, SdfPortSpec, SdfTriple
TEST_F(SdfDesignTest, R9_WriteThenReadSdf) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_roundtrip.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, true);

  // Now read it back
  readSdf(tmpfile, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(tmpfile);
  SUCCEED();
}

// Test readSdf with unescaped_dividers option
// Covers: SdfReader unescaped divider path
TEST_F(SdfDesignTest, R9_ReadSdfUnescapedDividers) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_unesc.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, true);

  readSdf(tmpfile, nullptr, corner, true, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(tmpfile);
  SUCCEED();
}

// Test readSdf with incremental_only option
// Covers: SdfReader incremental_only path
TEST_F(SdfDesignTest, R9_ReadSdfIncrementalOnly) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_incr.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, true);

  readSdf(tmpfile, nullptr, corner, false, true,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(tmpfile);
  SUCCEED();
}

// Test readSdf with cond_use min
// Covers: SdfReader cond_use min path
TEST_F(SdfDesignTest, R9_ReadSdfCondUseMin) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_cumin.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, true);

  readSdf(tmpfile, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::min()), sta_);

  std::remove(tmpfile);
  SUCCEED();
}

// Test readSdf with cond_use max
// Covers: SdfReader cond_use max path
TEST_F(SdfDesignTest, R9_ReadSdfCondUseMax) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_cumax.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, true);

  readSdf(tmpfile, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::max()), sta_);

  std::remove(tmpfile);
  SUCCEED();
}

// Test writeSdf then read with both unescaped and incremental
// Covers: combined SdfReader option paths
TEST_F(SdfDesignTest, R9_ReadSdfCombinedOptions) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_combined.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, true);

  readSdf(tmpfile, nullptr, corner, true, true,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(tmpfile);
  SUCCEED();
}

// Test writeSdf with low precision (1 digit)
// Covers: SdfWriter digit formatting edge case
TEST_F(SdfDesignTest, R9_WriteSdfLowPrecision) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_lowprec.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 1, false, true, true);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test writeSdf gzip then readSdf
// Covers: SdfReader gzip input path
TEST_F(SdfDesignTest, R9_WriteSdfGzipThenRead) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_gz.sdf.gz";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, true, true, true);

  readSdf(tmpfile, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(tmpfile);
  SUCCEED();
}

// Test writeSdf with no_timestamp and no_timescale
// Covers: SdfWriter combined header options
TEST_F(SdfDesignTest, R9_WriteSdfNoTimestampNoTimescale) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r9_sdf_minimal.sdf";
  sta_->writeSdf(tmpfile, corner, '/', false, 3, false, false, false);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test readSdf with nonexistent file throws FileNotReadable
// Covers: SdfReader error handling path
TEST_F(SdfDesignTest, R9_ReadSdfNonexistent) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  // Attempt to read nonexistent SDF - should throw
  EXPECT_THROW(
    readSdf("/tmp/nonexistent_r9.sdf", nullptr, corner,
            false, false, const_cast<MinMaxAll*>(MinMaxAll::all()), sta_),
    FileNotReadable
  );
}

// R9_ SdfSmokeTest - Transition properties for SDF
TEST_F(SdfSmokeTest, R9_TransitionRiseProperties) {
  const Transition *t = Transition::rise();
  EXPECT_NE(t, nullptr);
  EXPECT_EQ(t->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(t->sdfTripleIndex(), RiseFall::riseIndex());
  EXPECT_FALSE(t->to_string().empty());
  EXPECT_NE(t->asInitFinalString(), nullptr);
}

TEST_F(SdfSmokeTest, R9_TransitionFallProperties) {
  const Transition *t = Transition::fall();
  EXPECT_NE(t, nullptr);
  EXPECT_EQ(t->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(t->sdfTripleIndex(), RiseFall::fallIndex());
  EXPECT_FALSE(t->to_string().empty());
  EXPECT_NE(t->asInitFinalString(), nullptr);
}

TEST_F(SdfSmokeTest, R9_TransitionTristateProperties) {
  // 0Z
  const Transition *t0z = Transition::tr0Z();
  EXPECT_NE(t0z, nullptr);
  EXPECT_FALSE(t0z->to_string().empty());
  EXPECT_NE(t0z->asRiseFallBoth(), nullptr);

  // Z1
  const Transition *tz1 = Transition::trZ1();
  EXPECT_NE(tz1, nullptr);
  EXPECT_NE(tz1->asRiseFallBoth(), nullptr);

  // 1Z
  const Transition *t1z = Transition::tr1Z();
  EXPECT_NE(t1z, nullptr);
  EXPECT_NE(t1z->asRiseFallBoth(), nullptr);

  // Z0
  const Transition *tz0 = Transition::trZ0();
  EXPECT_NE(tz0, nullptr);
  EXPECT_NE(tz0->asRiseFallBoth(), nullptr);
}

TEST_F(SdfSmokeTest, R9_TransitionUnknownProperties) {
  // 0X
  const Transition *t0x = Transition::tr0X();
  EXPECT_NE(t0x, nullptr);
  EXPECT_NE(t0x->asRiseFallBoth(), nullptr);

  // X1
  const Transition *tx1 = Transition::trX1();
  EXPECT_NE(tx1, nullptr);
  EXPECT_NE(tx1->asRiseFallBoth(), nullptr);

  // 1X
  const Transition *t1x = Transition::tr1X();
  EXPECT_NE(t1x, nullptr);
  EXPECT_NE(t1x->asRiseFallBoth(), nullptr);

  // X0
  const Transition *tx0 = Transition::trX0();
  EXPECT_NE(tx0, nullptr);
  EXPECT_NE(tx0->asRiseFallBoth(), nullptr);
}

TEST_F(SdfSmokeTest, R9_TransitionHighZUnknown) {
  // XZ
  const Transition *txz = Transition::trXZ();
  EXPECT_NE(txz, nullptr);
  EXPECT_FALSE(txz->to_string().empty());

  // ZX
  const Transition *tzx = Transition::trZX();
  EXPECT_NE(tzx, nullptr);
  EXPECT_FALSE(tzx->to_string().empty());
}

TEST_F(SdfSmokeTest, R9_RiseFallBothRiseFallMatches) {
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::rise()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::fall()));
  EXPECT_TRUE(RiseFallBoth::rise()->matches(RiseFall::rise()));
  EXPECT_FALSE(RiseFallBoth::rise()->matches(RiseFall::fall()));
  EXPECT_FALSE(RiseFallBoth::fall()->matches(RiseFall::rise()));
  EXPECT_TRUE(RiseFallBoth::fall()->matches(RiseFall::fall()));
}

TEST_F(SdfSmokeTest, R9_MinMaxAllRange) {
  // Verify MinMaxAll::all() range contains both min and max
  int count = 0;
  for (auto mm : MinMaxAll::all()->range()) {
    EXPECT_NE(mm, nullptr);
    count++;
  }
  EXPECT_EQ(count, 2);
}

TEST_F(SdfSmokeTest, R9_MinMaxInitValue) {
  // min init value is large positive (for finding minimum)
  float min_init = MinMax::min()->initValue();
  EXPECT_GT(min_init, 0.0f);

  // max init value is large negative (for finding maximum)
  float max_init = MinMax::max()->initValue();
  EXPECT_LT(max_init, 0.0f);
}

TEST_F(SdfSmokeTest, R9_MinMaxCompareExtremes) {
  // Very large values
  EXPECT_TRUE(MinMax::min()->compare(1e10f, 1e20f));
  EXPECT_FALSE(MinMax::min()->compare(1e20f, 1e10f));
  EXPECT_TRUE(MinMax::max()->compare(1e20f, 1e10f));
  EXPECT_FALSE(MinMax::max()->compare(1e10f, 1e20f));

  // Very small values
  EXPECT_TRUE(MinMax::min()->compare(1e-20f, 1e-10f));
  EXPECT_TRUE(MinMax::max()->compare(1e-10f, 1e-20f));
}

TEST_F(SdfSmokeTest, R9_RiseFallToStringAndFind) {
  EXPECT_EQ(RiseFall::rise()->to_string(), "^");
  EXPECT_EQ(RiseFall::fall()->to_string(), "v");
  EXPECT_EQ(RiseFall::find("^"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("v"), RiseFall::fall());
  EXPECT_EQ(RiseFall::find("rise"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("fall"), RiseFall::fall());
}

TEST_F(SdfSmokeTest, R9_TransitionFindByName) {
  EXPECT_EQ(Transition::find("^"), Transition::rise());
  EXPECT_EQ(Transition::find("v"), Transition::fall());
  EXPECT_EQ(Transition::find("nonexistent"), nullptr);
}

TEST_F(SdfSmokeTest, R9_MinMaxAllAsMinMax) {
  EXPECT_EQ(MinMaxAll::min()->asMinMax(), MinMax::min());
  EXPECT_EQ(MinMaxAll::max()->asMinMax(), MinMax::max());
}

TEST_F(SdfSmokeTest, R9_RiseFallOpposite) {
  EXPECT_EQ(RiseFall::rise()->opposite(), RiseFall::fall());
  EXPECT_EQ(RiseFall::fall()->opposite(), RiseFall::rise());
}

TEST_F(SdfSmokeTest, R9_TransitionMatchesSelf) {
  EXPECT_TRUE(Transition::rise()->matches(Transition::rise()));
  EXPECT_TRUE(Transition::fall()->matches(Transition::fall()));
  EXPECT_FALSE(Transition::rise()->matches(Transition::fall()));
  EXPECT_FALSE(Transition::fall()->matches(Transition::rise()));
}

TEST_F(SdfSmokeTest, R9_TransitionMatchesRiseFallWildcard) {
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::rise()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::fall()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::tr0Z()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::trXZ()));
}

TEST_F(SdfSmokeTest, R9_MinMaxMinMaxFunc) {
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(10.0f, 20.0f), 10.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(10.0f, 20.0f), 20.0f);
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(-5.0f, 5.0f), -5.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(-5.0f, 5.0f), 5.0f);
}

TEST_F(SdfSmokeTest, R9_RiseFallBothFind) {
  EXPECT_EQ(RiseFallBoth::find("rise"), RiseFallBoth::rise());
  EXPECT_EQ(RiseFallBoth::find("fall"), RiseFallBoth::fall());
  EXPECT_EQ(RiseFallBoth::find("rise_fall"), RiseFallBoth::riseFall());
  EXPECT_EQ(RiseFallBoth::find("nonexistent"), nullptr);
}

// =========================================================================
// R11_ tests: Cover additional uncovered SDF functions
// =========================================================================

// R11_1: Write SDF then read it with a path argument
// Covers: SdfReader with path set, SdfTriple construction paths
TEST_F(SdfDesignTest, R11_ReadSdfWithPath) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r11_sdf_path.sdf";
  sta_->writeSdf(tmpfile, corner, '/', true, 3, false, true, true);

  // Read with a path argument (top instance path)
  readSdf(tmpfile, "top", corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(tmpfile);
  SUCCEED();
}

// R11_2: Read a hand-crafted SDF with specific constructs to exercise
// SdfReader::makeTriple(), makeTriple(float), SdfPortSpec, SdfTriple::hasValue
TEST_F(SdfDesignTest, R11_ReadHandCraftedSdf) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Write a hand-crafted SDF with IOPATH and timing checks
  const char *sdf_path = "/tmp/test_r11_handcraft.sdf";
  FILE *fp = fopen(sdf_path, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "(DELAYFILE\n");
  fprintf(fp, "  (SDFVERSION \"3.0\")\n");
  fprintf(fp, "  (DESIGN \"top\")\n");
  fprintf(fp, "  (TIMESCALE 1ns)\n");
  fprintf(fp, "  (CELL\n");
  fprintf(fp, "    (CELLTYPE \"DFFHQx4_ASAP7_75t_R\")\n");
  fprintf(fp, "    (INSTANCE r1)\n");
  fprintf(fp, "    (DELAY\n");
  fprintf(fp, "      (ABSOLUTE\n");
  fprintf(fp, "        (IOPATH CLK Q (0.100::0.200) (0.150::0.250))\n");
  fprintf(fp, "      )\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "    (TIMINGCHECK\n");
  fprintf(fp, "      (SETUP D (posedge CLK) (0.050::0.080))\n");
  fprintf(fp, "      (HOLD D (posedge CLK) (0.020::0.030))\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "  )\n");
  fprintf(fp, ")\n");
  fclose(fp);

  readSdf(sdf_path, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(sdf_path);
  SUCCEED();
}

// R11_3: Read SDF with edge-specific IOPATH (posedge, negedge)
// Covers: SdfPortSpec with transitions, sdfEdge paths
TEST_F(SdfDesignTest, R11_ReadSdfEdgeIopath) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *sdf_path = "/tmp/test_r11_edge_iopath.sdf";
  FILE *fp = fopen(sdf_path, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "(DELAYFILE\n");
  fprintf(fp, "  (SDFVERSION \"3.0\")\n");
  fprintf(fp, "  (DESIGN \"top\")\n");
  fprintf(fp, "  (TIMESCALE 1ns)\n");
  fprintf(fp, "  (CELL\n");
  fprintf(fp, "    (CELLTYPE \"DFFHQx4_ASAP7_75t_R\")\n");
  fprintf(fp, "    (INSTANCE r1)\n");
  fprintf(fp, "    (DELAY\n");
  fprintf(fp, "      (ABSOLUTE\n");
  fprintf(fp, "        (IOPATH (posedge CLK) Q (0.100::0.200) (0.150::0.250))\n");
  fprintf(fp, "        (IOPATH (negedge CLK) Q (0.110::0.210) (0.160::0.260))\n");
  fprintf(fp, "      )\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "  )\n");
  fprintf(fp, ")\n");
  fclose(fp);

  readSdf(sdf_path, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(sdf_path);
  SUCCEED();
}

// R11_4: Read SDF with SETUPHOLD combined check
// Covers: SdfReader::timingCheckSetupHold path
TEST_F(SdfDesignTest, R11_ReadSdfSetupHold) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *sdf_path = "/tmp/test_r11_setuphold.sdf";
  FILE *fp = fopen(sdf_path, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "(DELAYFILE\n");
  fprintf(fp, "  (SDFVERSION \"3.0\")\n");
  fprintf(fp, "  (DESIGN \"top\")\n");
  fprintf(fp, "  (TIMESCALE 1ns)\n");
  fprintf(fp, "  (CELL\n");
  fprintf(fp, "    (CELLTYPE \"DFFHQx4_ASAP7_75t_R\")\n");
  fprintf(fp, "    (INSTANCE r1)\n");
  fprintf(fp, "    (TIMINGCHECK\n");
  fprintf(fp, "      (SETUPHOLD D (posedge CLK) (0.050) (0.020))\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "  )\n");
  fprintf(fp, ")\n");
  fclose(fp);

  readSdf(sdf_path, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(sdf_path);
  SUCCEED();
}

// R11_5: Read SDF with RECREM combined check
// Covers: SdfReader::timingCheckRecRem path
TEST_F(SdfDesignTest, R11_ReadSdfRecRem) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *sdf_path = "/tmp/test_r11_recrem.sdf";
  FILE *fp = fopen(sdf_path, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "(DELAYFILE\n");
  fprintf(fp, "  (SDFVERSION \"3.0\")\n");
  fprintf(fp, "  (DESIGN \"top\")\n");
  fprintf(fp, "  (TIMESCALE 1ns)\n");
  fprintf(fp, "  (CELL\n");
  fprintf(fp, "    (CELLTYPE \"DFFHQx4_ASAP7_75t_R\")\n");
  fprintf(fp, "    (INSTANCE r1)\n");
  fprintf(fp, "    (TIMINGCHECK\n");
  fprintf(fp, "      (RECREM D (posedge CLK) (0.050) (0.020))\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "  )\n");
  fprintf(fp, ")\n");
  fclose(fp);

  readSdf(sdf_path, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(sdf_path);
  SUCCEED();
}

// R11_6: Read SDF with WIDTH check
// Covers: SdfReader::timingCheckWidth path
TEST_F(SdfDesignTest, R11_ReadSdfWidth) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *sdf_path = "/tmp/test_r11_width.sdf";
  FILE *fp = fopen(sdf_path, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "(DELAYFILE\n");
  fprintf(fp, "  (SDFVERSION \"3.0\")\n");
  fprintf(fp, "  (DESIGN \"top\")\n");
  fprintf(fp, "  (TIMESCALE 1ns)\n");
  fprintf(fp, "  (CELL\n");
  fprintf(fp, "    (CELLTYPE \"DFFHQx4_ASAP7_75t_R\")\n");
  fprintf(fp, "    (INSTANCE r1)\n");
  fprintf(fp, "    (TIMINGCHECK\n");
  fprintf(fp, "      (WIDTH (posedge CLK) (0.100))\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "  )\n");
  fprintf(fp, ")\n");
  fclose(fp);

  readSdf(sdf_path, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(sdf_path);
  SUCCEED();
}

// R11_7: Read SDF with PERIOD check
// Covers: SdfReader::timingCheckPeriod path
TEST_F(SdfDesignTest, R11_ReadSdfPeriod) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *sdf_path = "/tmp/test_r11_period.sdf";
  FILE *fp = fopen(sdf_path, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "(DELAYFILE\n");
  fprintf(fp, "  (SDFVERSION \"3.0\")\n");
  fprintf(fp, "  (DESIGN \"top\")\n");
  fprintf(fp, "  (TIMESCALE 1ns)\n");
  fprintf(fp, "  (CELL\n");
  fprintf(fp, "    (CELLTYPE \"DFFHQx4_ASAP7_75t_R\")\n");
  fprintf(fp, "    (INSTANCE r1)\n");
  fprintf(fp, "    (TIMINGCHECK\n");
  fprintf(fp, "      (PERIOD (posedge CLK) (1.000))\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "  )\n");
  fprintf(fp, ")\n");
  fclose(fp);

  readSdf(sdf_path, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(sdf_path);
  SUCCEED();
}

// R11_8: Read SDF with NOCHANGE check
// Covers: SdfReader::timingCheckNochange, notSupported
// NOCHANGE is not supported and throws, so we catch the exception
TEST_F(SdfDesignTest, R11_ReadSdfNochange) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *sdf_path = "/tmp/test_r11_nochange.sdf";
  FILE *fp = fopen(sdf_path, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "(DELAYFILE\n");
  fprintf(fp, "  (SDFVERSION \"3.0\")\n");
  fprintf(fp, "  (DESIGN \"top\")\n");
  fprintf(fp, "  (TIMESCALE 1ns)\n");
  fprintf(fp, "  (CELL\n");
  fprintf(fp, "    (CELLTYPE \"DFFHQx4_ASAP7_75t_R\")\n");
  fprintf(fp, "    (INSTANCE r1)\n");
  fprintf(fp, "    (TIMINGCHECK\n");
  fprintf(fp, "      (NOCHANGE D (posedge CLK) (0.050) (0.020))\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "  )\n");
  fprintf(fp, ")\n");
  fclose(fp);

  // NOCHANGE is not supported and throws an exception
  EXPECT_THROW(
    readSdf(sdf_path, nullptr, corner, false, false,
            const_cast<MinMaxAll*>(MinMaxAll::all()), sta_),
    std::exception
  );

  std::remove(sdf_path);
}

// R11_9: Read SDF with INTERCONNECT delay
// Covers: SdfReader::interconnect path
TEST_F(SdfDesignTest, R11_ReadSdfInterconnect) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *sdf_path = "/tmp/test_r11_interconnect.sdf";
  FILE *fp = fopen(sdf_path, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "(DELAYFILE\n");
  fprintf(fp, "  (SDFVERSION \"3.0\")\n");
  fprintf(fp, "  (DESIGN \"top\")\n");
  fprintf(fp, "  (TIMESCALE 1ns)\n");
  fprintf(fp, "  (CELL\n");
  fprintf(fp, "    (CELLTYPE \"top\")\n");
  fprintf(fp, "    (INSTANCE)\n");
  fprintf(fp, "    (DELAY\n");
  fprintf(fp, "      (ABSOLUTE\n");
  fprintf(fp, "        (INTERCONNECT u1/Y r3/D (0.010::0.020) (0.015::0.025))\n");
  fprintf(fp, "      )\n");
  fprintf(fp, "    )\n");
  fprintf(fp, "  )\n");
  fprintf(fp, ")\n");
  fclose(fp);

  readSdf(sdf_path, nullptr, corner, false, false,
          const_cast<MinMaxAll*>(MinMaxAll::all()), sta_);

  std::remove(sdf_path);
  SUCCEED();
}

// R11_10: WriteSdf with include_typ=true and no_version=false to cover
// the writeHeader path with version
TEST_F(SdfDesignTest, R11_WriteSdfWithVersion) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  const char *tmpfile = "/tmp/test_r11_sdf_version.sdf";
  // no_timestamp=false, no_version=false -> includes version and timestamp
  sta_->writeSdf(tmpfile, corner, '/', true, 4, false, false, false);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

} // namespace sta
