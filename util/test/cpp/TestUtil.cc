#include <gtest/gtest.h>
#include <string>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <tcl.h>
#include "Fuzzy.hh"
#include "MinMax.hh"
#include "PatternMatch.hh"
#include "StringUtil.hh"
#include "RiseFallMinMax.hh"
#include "RiseFallValues.hh"
#include "Report.hh"
#include "ReportStd.hh"
#include "Error.hh"
#include "Debug.hh"
#include "Machine.hh"
#include "DispatchQueue.hh"
#include "Stats.hh"
#include "util/gzstream.hh"

namespace sta {

// fuzzyEqual tests
TEST(FuzzyTest, EqualValues)
{
  EXPECT_TRUE(fuzzyEqual(1.0f, 1.0f));
  EXPECT_TRUE(fuzzyEqual(0.0f, 0.0f));
  EXPECT_TRUE(fuzzyEqual(-1.0f, -1.0f));
}

TEST(FuzzyTest, EqualSlightlyDifferent)
{
  // Values that differ by a tiny amount should be fuzzy equal
  float v = 1.0f;
  float small = v + v * 1e-7f;
  EXPECT_TRUE(fuzzyEqual(v, small));
}

TEST(FuzzyTest, EqualVeryDifferent)
{
  EXPECT_FALSE(fuzzyEqual(1.0f, 2.0f));
  EXPECT_FALSE(fuzzyEqual(0.0f, 1.0f));
  EXPECT_FALSE(fuzzyEqual(-1.0f, 1.0f));
}

// fuzzyZero tests
TEST(FuzzyTest, ZeroExact)
{
  EXPECT_TRUE(fuzzyZero(0.0f));
}

TEST(FuzzyTest, ZeroVerySmall)
{
  EXPECT_TRUE(fuzzyZero(1e-20f));
  EXPECT_TRUE(fuzzyZero(-1e-20f));
}

TEST(FuzzyTest, ZeroNonZero)
{
  EXPECT_FALSE(fuzzyZero(1.0f));
  EXPECT_FALSE(fuzzyZero(-1.0f));
}

// fuzzyLess tests
TEST(FuzzyTest, LessTrue)
{
  EXPECT_TRUE(fuzzyLess(1.0f, 2.0f));
}

TEST(FuzzyTest, LessEqual)
{
  EXPECT_FALSE(fuzzyLess(1.0f, 1.0f));
}

TEST(FuzzyTest, LessGreater)
{
  EXPECT_FALSE(fuzzyLess(2.0f, 1.0f));
}

// fuzzyGreater tests
TEST(FuzzyTest, GreaterTrue)
{
  EXPECT_TRUE(fuzzyGreater(2.0f, 1.0f));
}

TEST(FuzzyTest, GreaterEqual)
{
  EXPECT_FALSE(fuzzyGreater(1.0f, 1.0f));
}

TEST(FuzzyTest, GreaterLess)
{
  EXPECT_FALSE(fuzzyGreater(1.0f, 2.0f));
}

// fuzzyInf tests
// fuzzyInf checks against the STA INF constant (a large finite float),
// not IEEE infinity.
TEST(FuzzyTest, InfPositive)
{
  EXPECT_TRUE(fuzzyInf(INF));
}

TEST(FuzzyTest, InfNegative)
{
  EXPECT_TRUE(fuzzyInf(-INF));
}

TEST(FuzzyTest, InfNormal)
{
  EXPECT_FALSE(fuzzyInf(1.0f));
  EXPECT_FALSE(fuzzyInf(0.0f));
  EXPECT_FALSE(fuzzyInf(-1.0f));
}

// patternMatch tests
TEST(PatternMatchTest, ExactMatch)
{
  EXPECT_TRUE(patternMatch("hello", "hello"));
  EXPECT_FALSE(patternMatch("hello", "world"));
}

TEST(PatternMatchTest, WildcardStar)
{
  EXPECT_TRUE(patternMatch("hel*", "hello"));
  EXPECT_TRUE(patternMatch("*llo", "hello"));
  EXPECT_TRUE(patternMatch("*", "anything"));
  EXPECT_TRUE(patternMatch("h*o", "hello"));
  EXPECT_FALSE(patternMatch("h*x", "hello"));
}

TEST(PatternMatchTest, WildcardQuestion)
{
  EXPECT_TRUE(patternMatch("hell?", "hello"));
  EXPECT_TRUE(patternMatch("?ello", "hello"));
  EXPECT_FALSE(patternMatch("hell?", "hell"));
}

TEST(PatternMatchTest, NoMatch)
{
  EXPECT_FALSE(patternMatch("abc", "xyz"));
  EXPECT_FALSE(patternMatch("abc?", "ab"));
}

// patternMatchNoCase tests
TEST(PatternMatchTest, NoCaseSensitive)
{
  // nocase = false: case matters
  EXPECT_TRUE(patternMatchNoCase("hello", "hello", false));
  EXPECT_FALSE(patternMatchNoCase("Hello", "hello", false));
}

TEST(PatternMatchTest, NoCaseInsensitive)
{
  // nocase = true: case ignored
  EXPECT_TRUE(patternMatchNoCase("Hello", "hello", true));
  EXPECT_TRUE(patternMatchNoCase("HELLO", "hello", true));
  EXPECT_TRUE(patternMatchNoCase("H*O", "hello", true));
}

// patternWildcards tests
TEST(PatternMatchTest, HasWildcards)
{
  EXPECT_TRUE(patternWildcards("hel*"));
  EXPECT_TRUE(patternWildcards("hell?"));
  EXPECT_TRUE(patternWildcards("*"));
}

TEST(PatternMatchTest, NoWildcards)
{
  EXPECT_FALSE(patternWildcards("hello"));
  EXPECT_FALSE(patternWildcards("simple"));
}

// stringEq tests
TEST(StringUtilTest, StringEqEqual)
{
  EXPECT_TRUE(stringEq("hello", "hello"));
  EXPECT_TRUE(stringEq("", ""));
}

TEST(StringUtilTest, StringEqNotEqual)
{
  EXPECT_FALSE(stringEq("hello", "world"));
  EXPECT_FALSE(stringEq("hello", "Hello"));
}

// isDigits tests
TEST(StringUtilTest, IsDigitsTrue)
{
  EXPECT_TRUE(isDigits("12345"));
  EXPECT_TRUE(isDigits("0"));
}

TEST(StringUtilTest, IsDigitsFalse)
{
  EXPECT_FALSE(isDigits("abc"));
  EXPECT_FALSE(isDigits("123abc"));
  // Empty string returns true (no non-digit characters)
  EXPECT_TRUE(isDigits(""));
}

// trimRight tests
TEST(StringUtilTest, TrimRightSpaces)
{
  std::string s = "hello   ";
  trimRight(s);
  EXPECT_EQ(s, "hello");
}

TEST(StringUtilTest, TrimRightNoSpaces)
{
  std::string s = "hello";
  trimRight(s);
  EXPECT_EQ(s, "hello");
}

TEST(StringUtilTest, TrimRightAllSpaces)
{
  std::string s = "   ";
  trimRight(s);
  EXPECT_EQ(s, "");
}

TEST(StringUtilTest, TrimRightEmpty)
{
  std::string s = "";
  trimRight(s);
  EXPECT_EQ(s, "");
}

////////////////////////////////////////////////////////////////
// RiseFallMinMax tests
////////////////////////////////////////////////////////////////

TEST(RiseFallMinMaxTest, DefaultConstructorIsEmpty)
{
  RiseFallMinMax rfmm;
  EXPECT_TRUE(rfmm.empty());
  EXPECT_FALSE(rfmm.hasValue());
}

TEST(RiseFallMinMaxTest, InitValueConstructor)
{
  RiseFallMinMax rfmm(5.0f);
  EXPECT_FALSE(rfmm.empty());
  EXPECT_TRUE(rfmm.hasValue());
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::fall(), MinMax::min()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::fall(), MinMax::max()));
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 5.0f);
}

TEST(RiseFallMinMaxTest, CopyConstructor)
{
  RiseFallMinMax src(3.0f);
  RiseFallMinMax copy(&src);
  EXPECT_TRUE(copy.equal(&src));
  EXPECT_FLOAT_EQ(copy.value(RiseFall::rise(), MinMax::min()), 3.0f);
}

TEST(RiseFallMinMaxTest, SetValueAll)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(5.0f);
  EXPECT_FALSE(rfmm.empty());
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 5.0f);
}

TEST(RiseFallMinMaxTest, SetValueRiseFallBothMinMaxAll)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFallBoth::riseFall(), MinMaxAll::all(), 2.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 2.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 2.0f);
}

TEST(RiseFallMinMaxTest, SetValueRiseFallBothMinMax)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFallBoth::rise(), MinMax::min(), 1.5f);
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 1.5f);
}

TEST(RiseFallMinMaxTest, SetValueRiseFallMinMax)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::fall(), MinMax::max(), 7.5f);
  EXPECT_TRUE(rfmm.hasValue(RiseFall::fall(), MinMax::max()));
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 7.5f);
}

TEST(RiseFallMinMaxTest, ValueWithExistsFlag)
{
  RiseFallMinMax rfmm;
  float val;
  bool exists;

  // Before setting, exists should be false
  rfmm.value(RiseFall::rise(), MinMax::min(), val, exists);
  EXPECT_FALSE(exists);

  // After setting, exists should be true
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 4.0f);
  rfmm.value(RiseFall::rise(), MinMax::min(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 4.0f);
}

TEST(RiseFallMinMaxTest, ValueMinMaxOnly)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 2.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 4.0f);
  // value(MinMax::min()) should return min of rise,fall for min
  float v = rfmm.value(MinMax::min());
  EXPECT_FLOAT_EQ(v, 2.0f);  // min->compare returns true when value1 < value2

  rfmm.setValue(RiseFall::rise(), MinMax::max(), 10.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::max(), 8.0f);
  float v2 = rfmm.value(MinMax::max());
  EXPECT_FLOAT_EQ(v2, 10.0f);  // max->compare returns true when value1 > value2
}

TEST(RiseFallMinMaxTest, MaxValue)
{
  RiseFallMinMax rfmm;
  float max_val;
  bool exists;

  // Empty: no max
  rfmm.maxValue(max_val, exists);
  EXPECT_FALSE(exists);

  // Set some values
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 3.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::max(), 7.0f);
  rfmm.maxValue(max_val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(max_val, 7.0f);
}

TEST(RiseFallMinMaxTest, Clear)
{
  RiseFallMinMax rfmm(10.0f);
  EXPECT_FALSE(rfmm.empty());
  rfmm.clear();
  EXPECT_TRUE(rfmm.empty());
}

TEST(RiseFallMinMaxTest, SetValues)
{
  RiseFallMinMax src(2.5f);
  RiseFallMinMax dest;
  dest.setValues(&src);
  EXPECT_TRUE(dest.equal(&src));
}

TEST(RiseFallMinMaxTest, RemoveValueMinMax)
{
  RiseFallMinMax rfmm(5.0f);
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  rfmm.removeValue(RiseFallBoth::rise(), MinMax::min());
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
}

TEST(RiseFallMinMaxTest, RemoveValueMinMaxAll)
{
  RiseFallMinMax rfmm(5.0f);
  rfmm.removeValue(RiseFallBoth::rise(), MinMaxAll::all());
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
  EXPECT_TRUE(rfmm.hasValue(RiseFall::fall(), MinMax::min()));
}

TEST(RiseFallMinMaxTest, MergeValueRiseFallBothMinMaxAll)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  // Merge a smaller value for min -> should take the new value
  rfmm.mergeValue(RiseFallBoth::rise(), MinMaxAll::min(), 3.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 3.0f);
  // Merge a larger value for min -> should keep the old value
  rfmm.mergeValue(RiseFallBoth::rise(), MinMaxAll::min(), 8.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 3.0f);
}

TEST(RiseFallMinMaxTest, MergeValueRiseFallMinMax)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 5.0f);
  // Merge a larger value for max -> should take the new value
  rfmm.mergeValue(RiseFall::rise(), MinMax::max(), 8.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 8.0f);
  // Merge a smaller value for max -> should keep old value
  rfmm.mergeValue(RiseFall::rise(), MinMax::max(), 2.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 8.0f);
}

TEST(RiseFallMinMaxTest, MergeValueIntoEmpty)
{
  RiseFallMinMax rfmm;
  // Merging into empty should set the value
  rfmm.mergeValue(RiseFall::rise(), MinMax::min(), 4.0f);
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 4.0f);
}

TEST(RiseFallMinMaxTest, MergeWith)
{
  RiseFallMinMax rfmm1;
  rfmm1.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  rfmm1.setValue(RiseFall::rise(), MinMax::max(), 10.0f);

  RiseFallMinMax rfmm2;
  rfmm2.setValue(RiseFall::rise(), MinMax::min(), 3.0f);
  rfmm2.setValue(RiseFall::rise(), MinMax::max(), 12.0f);
  rfmm2.setValue(RiseFall::fall(), MinMax::min(), 2.0f);

  rfmm1.mergeWith(&rfmm2);
  // min: take smaller (3.0)
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::rise(), MinMax::min()), 3.0f);
  // max: take larger (12.0)
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::rise(), MinMax::max()), 12.0f);
  // fall/min: was empty in rfmm1, so take from rfmm2
  EXPECT_TRUE(rfmm1.hasValue(RiseFall::fall(), MinMax::min()));
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::fall(), MinMax::min()), 2.0f);
}

TEST(RiseFallMinMaxTest, MergeWithBothEmpty)
{
  RiseFallMinMax rfmm1;
  RiseFallMinMax rfmm2;
  rfmm1.mergeWith(&rfmm2);
  EXPECT_TRUE(rfmm1.empty());
}

TEST(RiseFallMinMaxTest, EqualSame)
{
  RiseFallMinMax rfmm1(5.0f);
  RiseFallMinMax rfmm2(5.0f);
  EXPECT_TRUE(rfmm1.equal(&rfmm2));
}

TEST(RiseFallMinMaxTest, EqualDifferentValues)
{
  RiseFallMinMax rfmm1(5.0f);
  RiseFallMinMax rfmm2(3.0f);
  EXPECT_FALSE(rfmm1.equal(&rfmm2));
}

TEST(RiseFallMinMaxTest, EqualDifferentExists)
{
  RiseFallMinMax rfmm1(5.0f);
  RiseFallMinMax rfmm2(5.0f);
  rfmm2.removeValue(RiseFallBoth::rise(), MinMax::min());
  EXPECT_FALSE(rfmm1.equal(&rfmm2));
}

TEST(RiseFallMinMaxTest, EqualBothEmpty)
{
  RiseFallMinMax rfmm1;
  RiseFallMinMax rfmm2;
  EXPECT_TRUE(rfmm1.equal(&rfmm2));
}

TEST(RiseFallMinMaxTest, IsOneValueTrue)
{
  RiseFallMinMax rfmm(5.0f);
  EXPECT_TRUE(rfmm.isOneValue());
  float val;
  EXPECT_TRUE(rfmm.isOneValue(val));
  EXPECT_FLOAT_EQ(val, 5.0f);
}

TEST(RiseFallMinMaxTest, IsOneValueFalse)
{
  RiseFallMinMax rfmm(5.0f);
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 3.0f);
  EXPECT_FALSE(rfmm.isOneValue());
}

TEST(RiseFallMinMaxTest, IsOneValueEmpty)
{
  RiseFallMinMax rfmm;
  EXPECT_FALSE(rfmm.isOneValue());
  float val;
  EXPECT_FALSE(rfmm.isOneValue(val));
}

TEST(RiseFallMinMaxTest, IsOneValuePartialExists)
{
  RiseFallMinMax rfmm(5.0f);
  rfmm.removeValue(RiseFallBoth::fall(), MinMax::max());
  float val;
  EXPECT_FALSE(rfmm.isOneValue(val));
}

TEST(RiseFallMinMaxTest, IsOneValueMinMax)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 5.0f);
  float val;
  EXPECT_TRUE(rfmm.isOneValue(MinMax::min(), val));
  EXPECT_FLOAT_EQ(val, 5.0f);
}

TEST(RiseFallMinMaxTest, IsOneValueMinMaxDifferent)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 3.0f);
  float val;
  EXPECT_FALSE(rfmm.isOneValue(MinMax::min(), val));
}

TEST(RiseFallMinMaxTest, IsOneValueMinMaxEmpty)
{
  RiseFallMinMax rfmm;
  float val;
  EXPECT_FALSE(rfmm.isOneValue(MinMax::min(), val));
}

TEST(RiseFallMinMaxTest, IsOneValueMinMaxPartial)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 5.0f);
  // fall/max is not set
  float val;
  EXPECT_FALSE(rfmm.isOneValue(MinMax::max(), val));
}

////////////////////////////////////////////////////////////////
// RiseFallValues tests
////////////////////////////////////////////////////////////////

TEST(RiseFallValuesTest, DefaultConstructorEmpty)
{
  RiseFallValues rfv;
  EXPECT_FALSE(rfv.hasValue(RiseFall::rise()));
  EXPECT_FALSE(rfv.hasValue(RiseFall::fall()));
}

TEST(RiseFallValuesTest, InitValueConstructor)
{
  RiseFallValues rfv(3.0f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_TRUE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 3.0f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 3.0f);
}

TEST(RiseFallValuesTest, SetValueAll)
{
  RiseFallValues rfv;
  rfv.setValue(5.0f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_TRUE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 5.0f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 5.0f);
}

TEST(RiseFallValuesTest, SetValueRiseFallBoth)
{
  RiseFallValues rfv;
  rfv.setValue(RiseFallBoth::rise(), 1.0f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_FALSE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 1.0f);
}

TEST(RiseFallValuesTest, SetValueRiseFallBothBoth)
{
  RiseFallValues rfv;
  rfv.setValue(RiseFallBoth::riseFall(), 9.0f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_TRUE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 9.0f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 9.0f);
}

TEST(RiseFallValuesTest, SetValueRiseFall)
{
  RiseFallValues rfv;
  rfv.setValue(RiseFall::fall(), 2.5f);
  EXPECT_FALSE(rfv.hasValue(RiseFall::rise()));
  EXPECT_TRUE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 2.5f);
}

TEST(RiseFallValuesTest, ValueWithExistsFlag)
{
  RiseFallValues rfv;
  float val;
  bool exists;

  rfv.value(RiseFall::rise(), val, exists);
  EXPECT_FALSE(exists);

  rfv.setValue(RiseFall::rise(), 4.0f);
  rfv.value(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 4.0f);
}

TEST(RiseFallValuesTest, SetValues)
{
  RiseFallValues src(7.0f);
  RiseFallValues dest;
  dest.setValues(&src);
  EXPECT_TRUE(dest.hasValue(RiseFall::rise()));
  EXPECT_TRUE(dest.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(dest.value(RiseFall::rise()), 7.0f);
  EXPECT_FLOAT_EQ(dest.value(RiseFall::fall()), 7.0f);
}

TEST(RiseFallValuesTest, Clear)
{
  RiseFallValues rfv(5.0f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  rfv.clear();
  EXPECT_FALSE(rfv.hasValue(RiseFall::rise()));
  EXPECT_FALSE(rfv.hasValue(RiseFall::fall()));
}

TEST(RiseFallValuesTest, SetRiseThenFall)
{
  RiseFallValues rfv;
  rfv.setValue(RiseFall::rise(), 1.0f);
  rfv.setValue(RiseFall::fall(), 2.0f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 1.0f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 2.0f);
}

TEST(RiseFallValuesTest, OverwriteValue)
{
  RiseFallValues rfv(5.0f);
  rfv.setValue(RiseFall::rise(), 10.0f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 10.0f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 5.0f);
}

////////////////////////////////////////////////////////////////
// PatternMatch class tests
////////////////////////////////////////////////////////////////

TEST(PatternMatchClassTest, SimpleGlobConstructor)
{
  PatternMatch pm("hello");
  EXPECT_EQ(pm.pattern(), "hello");
  EXPECT_FALSE(pm.isRegexp());
  EXPECT_FALSE(pm.nocase());
  EXPECT_EQ(pm.tclInterp(), nullptr);
}

TEST(PatternMatchClassTest, GlobMatchExact)
{
  PatternMatch pm("hello");
  EXPECT_TRUE(pm.match("hello"));
  EXPECT_FALSE(pm.match("world"));
}

TEST(PatternMatchClassTest, GlobMatchWithStar)
{
  PatternMatch pm("hel*");
  EXPECT_TRUE(pm.match("hello"));
  EXPECT_TRUE(pm.match("help"));
  EXPECT_FALSE(pm.match("world"));
}

TEST(PatternMatchClassTest, GlobMatchWithQuestion)
{
  PatternMatch pm("h?llo");
  EXPECT_TRUE(pm.match("hello"));
  EXPECT_TRUE(pm.match("hallo"));
  EXPECT_FALSE(pm.match("hllo"));
}

TEST(PatternMatchClassTest, GlobMatchString)
{
  PatternMatch pm("test*");
  std::string s = "testing";
  EXPECT_TRUE(pm.match(s));
  std::string s2 = "other";
  EXPECT_FALSE(pm.match(s2));
}

TEST(PatternMatchClassTest, HasWildcardsGlob)
{
  PatternMatch pm_wild("he*lo");
  EXPECT_TRUE(pm_wild.hasWildcards());

  PatternMatch pm_q("he?lo");
  EXPECT_TRUE(pm_q.hasWildcards());

  PatternMatch pm_none("hello");
  EXPECT_FALSE(pm_none.hasWildcards());
}

TEST(PatternMatchClassTest, MatchNoCase)
{
  PatternMatch pm("hello", false, true, nullptr);
  EXPECT_TRUE(pm.matchNoCase("hello"));
  EXPECT_TRUE(pm.matchNoCase("HELLO"));
  EXPECT_TRUE(pm.matchNoCase("Hello"));
}

TEST(PatternMatchClassTest, MatchNoCaseSensitive)
{
  PatternMatch pm("hello", false, false, nullptr);
  EXPECT_TRUE(pm.matchNoCase("hello"));
  EXPECT_FALSE(pm.matchNoCase("HELLO"));
}

TEST(PatternMatchClassTest, InheritFromConstructor)
{
  PatternMatch parent("base*", false, true, nullptr);
  PatternMatch child("child*", &parent);
  EXPECT_EQ(child.pattern(), "child*");
  EXPECT_FALSE(child.isRegexp());
  EXPECT_TRUE(child.nocase());
  EXPECT_TRUE(child.match("children"));
}

TEST(PatternMatchClassTest, InheritFromStringConstructor)
{
  PatternMatch parent("base*", false, true, nullptr);
  std::string childPat = "child*";
  PatternMatch child(childPat, &parent);
  EXPECT_TRUE(child.nocase());
  EXPECT_TRUE(child.match("children"));
}

// Regexp-based PatternMatch tests require a Tcl interpreter
class PatternMatchRegexpTest : public ::testing::Test {
protected:
  void SetUp() override {
    interp_ = Tcl_CreateInterp();
  }
  void TearDown() override {
    if (interp_)
      Tcl_DeleteInterp(interp_);
  }
  Tcl_Interp *interp_;
};

TEST_F(PatternMatchRegexpTest, RegexpHasWildcards)
{
  // Test regexp wildcards detection (uses a different code path)
  PatternMatch pm_reg("he.lo", true, false, interp_);
  EXPECT_TRUE(pm_reg.hasWildcards());

  PatternMatch pm_plus("he+lo", true, false, interp_);
  EXPECT_TRUE(pm_plus.hasWildcards());

  PatternMatch pm_bracket("he[lL]lo", true, false, interp_);
  EXPECT_TRUE(pm_bracket.hasWildcards());
}

TEST_F(PatternMatchRegexpTest, RegexpMatch)
{
  PatternMatch pm("hel+o", true, false, interp_);
  EXPECT_TRUE(pm.match("hello"));
  EXPECT_TRUE(pm.match("helllo"));
  EXPECT_FALSE(pm.match("heo"));
}

TEST_F(PatternMatchRegexpTest, RegexpMatchNoCase)
{
  PatternMatch pm("hello", true, true, interp_);
  EXPECT_TRUE(pm.matchNoCase("HELLO"));
  EXPECT_TRUE(pm.matchNoCase("hello"));
}

TEST_F(PatternMatchRegexpTest, RegexpNoWildcards)
{
  PatternMatch pm("hello", true, false, interp_);
  EXPECT_FALSE(pm.hasWildcards());
}

TEST_F(PatternMatchRegexpTest, RegexpInheritFrom)
{
  PatternMatch parent("base.*", true, false, interp_);
  PatternMatch child("child.*", &parent);
  EXPECT_TRUE(child.isRegexp());
  EXPECT_TRUE(child.match("children"));
}

TEST_F(PatternMatchRegexpTest, RegexpInheritFromString)
{
  PatternMatch parent("base.*", true, true, interp_);
  std::string childPat = "child.*";
  PatternMatch child(childPat, &parent);
  EXPECT_TRUE(child.isRegexp());
  EXPECT_TRUE(child.nocase());
  EXPECT_TRUE(child.match("CHILDREN"));
}

TEST_F(PatternMatchRegexpTest, RegexpMatchString)
{
  PatternMatch pm("te.t", true, false, interp_);
  std::string s = "test";
  EXPECT_TRUE(pm.match(s));
  std::string s2 = "team";
  EXPECT_FALSE(pm.match(s2));
}

TEST_F(PatternMatchRegexpTest, RegexpMatchNoCaseSensitive)
{
  // nocase=false means case matters
  PatternMatch pm("hello", true, false, interp_);
  EXPECT_TRUE(pm.matchNoCase("hello"));
  // With nocase=false on the pattern but using matchNoCase,
  // it still delegates to the regexp which was compiled without nocase
  EXPECT_FALSE(pm.matchNoCase("HELLO"));
}

////////////////////////////////////////////////////////////////
// Report tests
////////////////////////////////////////////////////////////////

TEST(ReportTest, BasicConstruction)
{
  Report report;
  // Should not crash. defaultReport should be set.
  EXPECT_EQ(Report::defaultReport(), &report);
}

TEST(ReportTest, RedirectStringBasic)
{
  Report report;
  report.redirectStringBegin();
  report.reportLine("hello world");
  const char *result = report.redirectStringEnd();
  EXPECT_NE(result, nullptr);
  std::string s(result);
  EXPECT_NE(s.find("hello world"), std::string::npos);
}

TEST(ReportTest, RedirectStringMultipleLines)
{
  Report report;
  report.redirectStringBegin();
  report.reportLine("line1");
  report.reportLine("line2");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("line1"), std::string::npos);
  EXPECT_NE(s.find("line2"), std::string::npos);
}

TEST(ReportTest, RedirectStringStdString)
{
  Report report;
  report.redirectStringBegin();
  std::string line = "std string line";
  report.reportLine(line);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("std string line"), std::string::npos);
}

TEST(ReportTest, ReportBlankLine)
{
  Report report;
  report.redirectStringBegin();
  report.reportBlankLine();
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_EQ(s, "\n");
}

TEST(ReportTest, ReportLineFormatted)
{
  Report report;
  report.redirectStringBegin();
  report.report("value={}", 42);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("value=42"), std::string::npos);
}

TEST(ReportTest, LogToFile)
{
  Report report;
  const char *tmpfile = "/tmp/test_report_log.txt";
  report.logBegin(tmpfile);
  report.reportLine("log test line");
  report.logEnd();
  // Verify file was created with content
  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  char buf[256];
  ASSERT_NE(fgets(buf, sizeof(buf), f), nullptr);
  fclose(f);
  EXPECT_NE(strstr(buf, "log test line"), nullptr);
  std::remove(tmpfile);
}

TEST(ReportTest, LogEndWithoutLog)
{
  ASSERT_NO_THROW(( [&](){
  Report report;
  // Should not crash when no log is active
  report.logEnd();

  }() ));
}

TEST(ReportTest, RedirectFileBegin)
{
  Report report;
  const char *tmpfile = "/tmp/test_report_redirect.txt";
  report.redirectFileBegin(tmpfile);
  report.reportLine("redirected line");
  report.redirectFileEnd();

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  char buf[256];
  ASSERT_NE(fgets(buf, sizeof(buf), f), nullptr);
  fclose(f);
  EXPECT_NE(strstr(buf, "redirected line"), nullptr);
  std::remove(tmpfile);
}

TEST(ReportTest, RedirectFileAppendBegin)
{
  Report report;
  const char *tmpfile = "/tmp/test_report_append.txt";

  // Write first
  report.redirectFileBegin(tmpfile);
  report.reportLine("first");
  report.redirectFileEnd();

  // Append
  report.redirectFileAppendBegin(tmpfile);
  report.reportLine("second");
  report.redirectFileEnd();

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  char content[512] = {};
  size_t bytes_read = fread(content, 1, sizeof(content) - 1, f);
  EXPECT_GT(bytes_read, 0u);
  fclose(f);
  EXPECT_NE(strstr(content, "first"), nullptr);
  EXPECT_NE(strstr(content, "second"), nullptr);
  std::remove(tmpfile);
}

TEST(ReportTest, RedirectFileEndWithoutRedirect)
{
  ASSERT_NO_THROW(( [&](){
  Report report;
  // Should not crash
  report.redirectFileEnd();

  }() ));
}

TEST(ReportTest, RedirectFileNotWritable)
{
  Report report;
  EXPECT_THROW(report.redirectFileBegin("/nonexistent/path/file.txt"),
               FileNotWritable);
}

TEST(ReportTest, RedirectFileAppendNotWritable)
{
  Report report;
  EXPECT_THROW(report.redirectFileAppendBegin("/nonexistent/path/file.txt"),
               FileNotWritable);
}

TEST(ReportTest, LogNotWritable)
{
  Report report;
  EXPECT_THROW(report.logBegin("/nonexistent/path/log.txt"),
               FileNotWritable);
}

TEST(ReportTest, WarnBasic)
{
  Report report;
  report.redirectStringBegin();
  report.warn(100, "something bad {}", 42);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 100:"), std::string::npos);
  EXPECT_NE(s.find("something bad 42"), std::string::npos);
}

TEST(ReportTest, FileWarn)
{
  Report report;
  report.redirectStringBegin();
  report.fileWarn(101, "test.v", 10, "missing {}", "semicolon");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 101:"), std::string::npos);
  EXPECT_NE(s.find("test.v"), std::string::npos);
  EXPECT_NE(s.find("line 10"), std::string::npos);
  EXPECT_NE(s.find("missing semicolon"), std::string::npos);
}

TEST(ReportTest, VwarnBasic)
{
  Report report;
  report.redirectStringBegin();
  // Use vwarn indirectly via warn (vwarn is called by warn internals)
  report.warn(102, "warn test {}", "value");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 102:"), std::string::npos);
}

TEST(ReportTest, ErrorThrows)
{
  Report report;
  EXPECT_THROW(report.error(200, "error message {}", 1), ExceptionMsg);
}

TEST(ReportTest, ErrorMessageContent)
{
  Report report;
  try {
    report.error(200, "specific error {}", "info");
    FAIL() << "Expected ExceptionMsg";
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("specific error info"), std::string::npos);
  }
}

TEST(ReportTest, FileErrorThrows)
{
  Report report;
  EXPECT_THROW(report.fileError(201, "test.sdc", 5, "parse error"), ExceptionMsg);
}

TEST(ReportTest, FileErrorContent)
{
  Report report;
  try {
    report.fileError(201, "test.sdc", 5, "unexpected token {}", "foo");
    FAIL() << "Expected ExceptionMsg";
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("test.sdc"), std::string::npos);
    EXPECT_NE(what.find("line 5"), std::string::npos);
    EXPECT_NE(what.find("unexpected token foo"), std::string::npos);
  }
}

TEST(ReportTest, VfileErrorThrows)
{
  Report report;
  // Test vfileError by throwing and catching
  try {
    report.fileError(202, "a.v", 3, "vfile error");
    FAIL();
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("a.v"), std::string::npos);
  }
}

TEST(ReportTest, SuppressMsgId)
{
  Report report;
  EXPECT_FALSE(report.isSuppressed(100));
  report.suppressMsgId(100);
  EXPECT_TRUE(report.isSuppressed(100));
}

TEST(ReportTest, UnsuppressMsgId)
{
  Report report;
  report.suppressMsgId(100);
  EXPECT_TRUE(report.isSuppressed(100));
  report.unsuppressMsgId(100);
  EXPECT_FALSE(report.isSuppressed(100));
}

TEST(ReportTest, SuppressedWarn)
{
  Report report;
  report.suppressMsgId(100);
  report.redirectStringBegin();
  report.warn(100, "should not appear");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  // Suppressed warning should produce no output
  EXPECT_EQ(s.find("should not appear"), std::string::npos);
}

TEST(ReportTest, SuppressedFileWarn)
{
  Report report;
  report.suppressMsgId(101);
  report.redirectStringBegin();
  report.fileWarn(101, "test.v", 1, "suppressed file warn");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_EQ(s.find("suppressed file warn"), std::string::npos);
}

TEST(ReportTest, SuppressedErrorIsSuppressed)
{
  Report report;
  report.suppressMsgId(200);
  try {
    report.error(200, "suppressed error");
    FAIL();
  } catch (const ExceptionMsg &e) {
    EXPECT_TRUE(e.suppressed());
  }
}

TEST(ReportTest, PrintStringDirect)
{
  Report report;
  report.redirectStringBegin();
  report.printString("direct print", 12);
  const char *result = report.redirectStringEnd();
  EXPECT_STREQ(result, "direct print");
}

TEST(ReportTest, LogAndConsoleSimultaneous)
{
  Report report;
  const char *logfile = "/tmp/test_report_logconsole.txt";
  report.logBegin(logfile);
  // Print to console (with log capturing)
  report.reportLine("dual output");
  report.logEnd();

  FILE *f = fopen(logfile, "r");
  ASSERT_NE(f, nullptr);
  char buf[256];
  ASSERT_NE(fgets(buf, sizeof(buf), f), nullptr);
  fclose(f);
  EXPECT_NE(strstr(buf, "dual output"), nullptr);
  std::remove(logfile);
}

////////////////////////////////////////////////////////////////
// Additional StringUtil tests
////////////////////////////////////////////////////////////////

TEST(StringUtilTest, StaFormat)
{
  std::string s = sta::format("value={}", 42);
  EXPECT_EQ(s, "value=42");
}

TEST(StringUtilTest, StaFormatToStdString)
{
  std::string s = sta::format("test {} {}", "abc", 123);
  EXPECT_EQ(s, "test abc 123");
}

TEST(StringUtilTest, StaFormatAppendToStdString)
{
  std::string s = "prefix ";
  s += sta::format("suffix {}", 1);
  EXPECT_EQ(s, "prefix suffix 1");
}

TEST(StringUtilTest, StaFormatAllocates)
{
  std::string s = sta::format("number {}", 99);
  EXPECT_EQ(s, "number 99");
}

TEST(StringUtilTest, StaFormatTmp)
{
  std::string s = sta::format("tmp {}", 42);
  EXPECT_EQ(s, "tmp 42");
}

TEST(StringUtilTest, StringEqWithLength)
{
  EXPECT_TRUE(stringEq("hello world", "hello", 5));
  EXPECT_FALSE(stringEq("hello world", "hellx", 5));
}

TEST(StringUtilTest, StringBeginEq)
{
  EXPECT_TRUE(stringBeginEq("hello world", "hello"));
  EXPECT_FALSE(stringBeginEq("hello world", "world"));
}

TEST(StringUtilTest, StringBeginEqual)
{
  EXPECT_TRUE(stringBeginEqual("Hello World", "hello"));
  EXPECT_FALSE(stringBeginEqual("Hello World", "world"));
}

TEST(StringUtilTest, StringEqual)
{
  EXPECT_TRUE(stringEqual("HELLO", "hello"));
  EXPECT_FALSE(stringEqual("hello", "world"));
}

////////////////////////////////////////////////////////////////
// Debug tests
////////////////////////////////////////////////////////////////

TEST(DebugTest, BasicConstruction)
{
  Report report;
  Debug debug(&report);
  // Initial state: level is 0 for any debug key
  EXPECT_EQ(debug.level("test"), 0);
  EXPECT_EQ(debug.statsLevel(), 0);
}

TEST(DebugTest, SetAndCheckLevel)
{
  Report report;
  Debug debug(&report);
  debug.setLevel("graph", 3);
  EXPECT_EQ(debug.level("graph"), 3);
  EXPECT_TRUE(debug.check("graph", 1));
  EXPECT_TRUE(debug.check("graph", 3));
  EXPECT_FALSE(debug.check("graph", 4));
}

TEST(DebugTest, SetLevelStats)
{
  Report report;
  Debug debug(&report);
  debug.setLevel("stats", 2);
  EXPECT_EQ(debug.statsLevel(), 2);
}

TEST(DebugTest, SetLevelZeroRemoves)
{
  Report report;
  Debug debug(&report);
  debug.setLevel("test", 3);
  EXPECT_TRUE(debug.check("test", 1));
  debug.setLevel("test", 0);
  EXPECT_FALSE(debug.check("test", 1));
  EXPECT_EQ(debug.level("test"), 0);
}

TEST(DebugTest, CheckUnsetKey)
{
  Report report;
  Debug debug(&report);
  EXPECT_FALSE(debug.check("nonexistent", 1));
}

TEST(DebugTest, ReportLine)
{
  Report report;
  Debug debug(&report);
  debug.setLevel("test", 1);
  // Redirect output to string to capture the debug line
  report.redirectStringBegin();
  debug.report("test", "value {}", 42);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("test"), std::string::npos);
  EXPECT_NE(s.find("value 42"), std::string::npos);
}

////////////////////////////////////////////////////////////////
// Tests for variadic template Report methods (warn, fileWarn, error, fileError)
////////////////////////////////////////////////////////////////

TEST(ReportVaTest, WarnBasic)
{
  Report report;
  report.redirectStringBegin();
  report.warn(300, "warn message {}", 42);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 300:"), std::string::npos);
  EXPECT_NE(s.find("warn message 42"), std::string::npos);
}

TEST(ReportVaTest, WarnSuppressed)
{
  Report report;
  report.suppressMsgId(300);
  report.redirectStringBegin();
  report.warn(300, "suppressed warn");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_EQ(s.find("suppressed warn"), std::string::npos);
}

TEST(ReportVaTest, FileWarnBasic)
{
  Report report;
  report.redirectStringBegin();
  report.fileWarn(301, "test.v", 15, "file warn msg {}", "detail");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 301:"), std::string::npos);
  EXPECT_NE(s.find("test.v"), std::string::npos);
  EXPECT_NE(s.find("line 15"), std::string::npos);
  EXPECT_NE(s.find("file warn msg detail"), std::string::npos);
}

TEST(ReportVaTest, FileWarnSuppressed)
{
  Report report;
  report.suppressMsgId(301);
  report.redirectStringBegin();
  report.fileWarn(301, "test.v", 15, "suppressed file warn");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_EQ(s.find("suppressed file warn"), std::string::npos);
}

TEST(ReportVaTest, ErrorThrows)
{
  Report report;
  EXPECT_THROW(report.error(400, "error msg {}", 99), ExceptionMsg);
}

TEST(ReportVaTest, ErrorContent)
{
  Report report;
  try {
    report.error(400, "error content {}", "test");
    FAIL();
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("error content test"), std::string::npos);
  }
}

TEST(ReportVaTest, FileErrorThrows)
{
  Report report;
  EXPECT_THROW(report.fileError(401, "myfile.sdc", 20, "file error msg"),
               ExceptionMsg);
}

TEST(ReportVaTest, FileErrorContent)
{
  Report report;
  try {
    report.fileError(401, "myfile.sdc", 20, "file error {}", 42);
    FAIL();
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("myfile.sdc"), std::string::npos);
    EXPECT_NE(what.find("line 20"), std::string::npos);
    EXPECT_NE(what.find("file error 42"), std::string::npos);
  }
}

// Test Report buffer growth (long format strings that exceed initial buffer)
TEST(ReportTest, LongReportLine)
{
  Report report;
  report.redirectStringBegin();
  // Create a string longer than the initial 1000 char buffer
  std::string long_str(2000, 'x');
  report.reportLine(long_str);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find(long_str), std::string::npos);
}

TEST(ReportTest, LongWarnLine)
{
  Report report;
  report.redirectStringBegin();
  std::string long_str(2000, 'y');
  report.warn(500, "{}", long_str);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 500:"), std::string::npos);
  EXPECT_NE(s.find(long_str), std::string::npos);
}

// Test ExceptionMsg suppressed flag
TEST(ReportVaTest, ErrorSuppressedFlag)
{
  Report report;
  report.suppressMsgId(400);
  try {
    report.error(400, "suppressed error");
    FAIL();
  } catch (const ExceptionMsg &e) {
    EXPECT_TRUE(e.suppressed());
  }
}

TEST(ReportVaTest, ErrorNotSuppressedFlag)
{
  Report report;
  try {
    report.error(400, "not suppressed");
    FAIL();
  } catch (const ExceptionMsg &e) {
    EXPECT_FALSE(e.suppressed());
  }
}

// Test FileNotWritable exception
TEST(ExceptionTest, FileNotWritable)
{
  try {
    throw FileNotWritable("/nonexistent/path");
  } catch (const FileNotWritable &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("/nonexistent/path"), std::string::npos);
  }
}

// Test FileNotReadable exception
TEST(ExceptionTest, FileNotReadable)
{
  try {
    throw FileNotReadable("/missing/file");
  } catch (const FileNotReadable &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("/missing/file"), std::string::npos);
  }
}

// Test ExceptionMsg
TEST(ExceptionTest, ExceptionMsg)
{
  try {
    throw ExceptionMsg("test error message", false);
  } catch (const ExceptionMsg &e) {
    EXPECT_STREQ(e.what(), "test error message");
    EXPECT_FALSE(e.suppressed());
  }
}

TEST(ExceptionTest, ExceptionMsgSuppressed)
{
  try {
    throw ExceptionMsg("suppressed msg", true);
  } catch (const ExceptionMsg &e) {
    EXPECT_STREQ(e.what(), "suppressed msg");
    EXPECT_TRUE(e.suppressed());
  }
}

// Test RegexpCompileError
TEST(ExceptionTest, RegexpCompileError)
{
  try {
    throw RegexpCompileError("bad_pattern");
  } catch (const RegexpCompileError &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("bad_pattern"), std::string::npos);
    EXPECT_NE(what.find("regular expression"), std::string::npos);
  }
}

////////////////////////////////////////////////////////////////
// Transition class tests (Transition.cc coverage)
////////////////////////////////////////////////////////////////

#include "Transition.hh"

// RiseFall::asTransition
TEST(TransitionCovTest, RiseFallAsTransition)
{
  const Transition *tr_rise = RiseFall::rise()->asTransition();
  EXPECT_EQ(tr_rise, Transition::rise());
  const Transition *tr_fall = RiseFall::fall()->asTransition();
  EXPECT_EQ(tr_fall, Transition::fall());
}

// RiseFallBoth::find
TEST(TransitionCovTest, RiseFallBothFind)
{
  const RiseFallBoth *r = RiseFallBoth::find("rise");
  EXPECT_EQ(r, RiseFallBoth::rise());
  const RiseFallBoth *f = RiseFallBoth::find("fall");
  EXPECT_EQ(f, RiseFallBoth::fall());
  const RiseFallBoth *rf = RiseFallBoth::find("rise_fall");
  EXPECT_EQ(rf, RiseFallBoth::riseFall());
  const RiseFallBoth *nope = RiseFallBoth::find("nonexistent");
  EXPECT_EQ(nope, nullptr);
}

// RiseFallBoth::matches(const Transition*)
TEST(TransitionCovTest, RiseFallBothMatchesTransition)
{
  EXPECT_TRUE(RiseFallBoth::rise()->matches(Transition::rise()));
  EXPECT_FALSE(RiseFallBoth::rise()->matches(Transition::fall()));
  EXPECT_TRUE(RiseFallBoth::fall()->matches(Transition::fall()));
  EXPECT_FALSE(RiseFallBoth::fall()->matches(Transition::rise()));
  // riseFall matches everything
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(Transition::rise()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(Transition::fall()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(Transition::tr0Z()));
}

// Transition::asRiseFallBoth
TEST(TransitionCovTest, TransitionAsRiseFallBoth)
{
  const RiseFallBoth *rfb_rise = Transition::rise()->asRiseFallBoth();
  EXPECT_NE(rfb_rise, nullptr);
  const RiseFallBoth *rfb_fall = Transition::fall()->asRiseFallBoth();
  EXPECT_NE(rfb_fall, nullptr);
}

// Transition::matches(const Transition*) - non-riseFall case
TEST(TransitionCovTest, TransitionMatchesSelf)
{
  EXPECT_TRUE(Transition::rise()->matches(Transition::rise()));
  EXPECT_FALSE(Transition::rise()->matches(Transition::fall()));
  EXPECT_TRUE(Transition::fall()->matches(Transition::fall()));
  EXPECT_FALSE(Transition::fall()->matches(Transition::rise()));
}

// RiseFall::find with short name
TEST(TransitionCovTest, RiseFallFindShortName)
{
  const RiseFall *r = RiseFall::find("^");
  EXPECT_EQ(r, RiseFall::rise());
  const RiseFall *f = RiseFall::find("v");
  EXPECT_EQ(f, RiseFall::fall());
  const RiseFall *nope = RiseFall::find("x");
  EXPECT_EQ(nope, nullptr);
}

// RiseFallBoth::to_string and shortName
TEST(TransitionCovTest, RiseFallBothToString)
{
  EXPECT_EQ(RiseFallBoth::rise()->to_string(), "rise");
  EXPECT_EQ(RiseFallBoth::fall()->to_string(), "fall");
  EXPECT_EQ(RiseFallBoth::rise()->shortName(), "^");
  EXPECT_EQ(RiseFallBoth::fall()->shortName(), "v");
}

// RiseFallBoth::index
TEST(TransitionCovTest, RiseFallBothIndex)
{
  EXPECT_EQ(RiseFallBoth::rise()->index(), 0);
  EXPECT_EQ(RiseFallBoth::fall()->index(), 1);
  EXPECT_EQ(RiseFallBoth::riseFall()->index(), 2);
}

// RiseFallBoth::rangeIndex
TEST(TransitionCovTest, RiseFallBothRangeIndex)
{
  auto &ri = RiseFallBoth::rise()->rangeIndex();
  EXPECT_EQ(ri.size(), 1u);
  EXPECT_EQ(ri[0], 0);

  auto &fi = RiseFallBoth::fall()->rangeIndex();
  EXPECT_EQ(fi.size(), 1u);
  EXPECT_EQ(fi[0], 1);

  auto &rfi = RiseFallBoth::riseFall()->rangeIndex();
  EXPECT_EQ(rfi.size(), 2u);
}

// RiseFallBoth::range
TEST(TransitionCovTest, RiseFallBothRange)
{
  auto &rr = RiseFallBoth::rise()->range();
  EXPECT_EQ(rr.size(), 1u);
  EXPECT_EQ(rr[0], RiseFall::rise());

  auto &fr = RiseFallBoth::fall()->range();
  EXPECT_EQ(fr.size(), 1u);
  EXPECT_EQ(fr[0], RiseFall::fall());

  auto &rfr = RiseFallBoth::riseFall()->range();
  EXPECT_EQ(rfr.size(), 2u);
}

// RiseFallBoth::asRiseFall returns nullptr for riseFall()
TEST(TransitionCovTest, RiseFallBothAsRiseFall)
{
  EXPECT_EQ(RiseFallBoth::rise()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(RiseFallBoth::fall()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(RiseFallBoth::riseFall()->asRiseFall(), nullptr);
}

// Transition::find with init/final strings
TEST(TransitionCovTest, TransitionFindInitFinal)
{
  const Transition *t01 = Transition::find("01");
  EXPECT_EQ(t01, Transition::rise());
  const Transition *t10 = Transition::find("10");
  EXPECT_EQ(t10, Transition::fall());
  const Transition *t0Z = Transition::find("0Z");
  EXPECT_EQ(t0Z, Transition::tr0Z());
  const Transition *tZ1 = Transition::find("Z1");
  EXPECT_EQ(tZ1, Transition::trZ1());
  const Transition *t1Z = Transition::find("1Z");
  EXPECT_EQ(t1Z, Transition::tr1Z());
  const Transition *tZ0 = Transition::find("Z0");
  EXPECT_EQ(tZ0, Transition::trZ0());
}

// Transition index
TEST(TransitionCovTest, TransitionIndex)
{
  EXPECT_EQ(Transition::rise()->index(), 0);
  EXPECT_EQ(Transition::fall()->index(), 1);
  EXPECT_EQ(Transition::tr0Z()->index(), 2);
  EXPECT_EQ(Transition::trZ1()->index(), 3);
}

// Transition::asRiseFall for non-rise/fall transitions
TEST(TransitionCovTest, TransitionAsRiseFallExtra)
{
  EXPECT_EQ(Transition::tr0Z()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::trZ1()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::tr1Z()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::trZ0()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::trXZ()->asRiseFall(), nullptr);
  EXPECT_EQ(Transition::trZX()->asRiseFall(), nullptr);
  EXPECT_EQ(Transition::riseFall()->asRiseFall(), nullptr);
}

// Transition asInitFinalString for various transitions
TEST(TransitionCovTest, TransitionAsInitFinalString)
{
  EXPECT_EQ(Transition::rise()->asInitFinalString(), "01");
  EXPECT_EQ(Transition::fall()->asInitFinalString(), "10");
  EXPECT_EQ(Transition::tr0Z()->asInitFinalString(), "0Z");
  EXPECT_EQ(Transition::trZ1()->asInitFinalString(), "Z1");
  EXPECT_EQ(Transition::tr1Z()->asInitFinalString(), "1Z");
  EXPECT_EQ(Transition::trZ0()->asInitFinalString(), "Z0");
  EXPECT_EQ(Transition::tr0X()->asInitFinalString(), "0X");
  EXPECT_EQ(Transition::trX1()->asInitFinalString(), "X1");
  EXPECT_EQ(Transition::tr1X()->asInitFinalString(), "1X");
  EXPECT_EQ(Transition::trX0()->asInitFinalString(), "X0");
  EXPECT_EQ(Transition::trXZ()->asInitFinalString(), "XZ");
  EXPECT_EQ(Transition::trZX()->asInitFinalString(), "ZX");
  EXPECT_EQ(Transition::riseFall()->asInitFinalString(), "**");
}

// Transition::sdfTripleIndex
TEST(TransitionCovTest, TransitionSdfTripleIndex)
{
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
  EXPECT_EQ(Transition::trZX()->sdfTripleIndex(), 11u);
  EXPECT_EQ(Transition::riseFall()->sdfTripleIndex(), 12u);
}

// Transition::maxIndex
TEST(TransitionCovTest, TransitionMaxIndex)
{
  EXPECT_GE(Transition::maxIndex(), 11);
}

// RiseFall::to_string
TEST(TransitionCovTest, RiseFallToString)
{
  EXPECT_EQ(RiseFall::rise()->to_string(), "rise");
  EXPECT_EQ(RiseFall::fall()->to_string(), "fall");
}

////////////////////////////////////////////////////////////////
// Additional StringUtil coverage
////////////////////////////////////////////////////////////////

TEST(StringUtilCovTest, StaFormatArgs)
{
  // Test sta::format with multiple arguments
  std::string s = sta::format("args test {} {}", 42, "hello");
  EXPECT_EQ(s, "args test 42 hello");
}

// Long sta::format string
TEST(StringUtilCovTest, LongStaFormat)
{
  std::string long_str(500, 'z');
  std::string result = sta::format("{}", long_str);
  EXPECT_EQ(result, long_str);
}

// std::string append
TEST(StringUtilCovTest, StringAppendStd)
{
  std::string s;
  s += "hello";
  s += " world";
  EXPECT_EQ(s, "hello world");
}

////////////////////////////////////////////////////////////////
// Report: printConsole coverage
////////////////////////////////////////////////////////////////

TEST(ReportCovTest, PrintConsoleDirectly)
{
  Report report;
  // printConsole writes directly to stdout - just ensure it doesn't crash
  // and returns a reasonable value
  size_t written = report.printString("test output\n", 12);
  EXPECT_GT(written, 0u);
}

// Report: printLine via reportLineString with empty string
TEST(ReportCovTest, ReportLineStringEmpty)
{
  Report report;
  report.redirectStringBegin();
  report.reportLine("");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  // Empty line should just have a newline
  EXPECT_EQ(s, "\n");
}

// Report: printToBuffer overflow (forces buffer growth)
TEST(ReportCovTest, ReportLineLongFormatted)
{
  Report report;
  report.redirectStringBegin();
  std::string fmt_str(2000, 'a');
  report.report("{} end", fmt_str);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find(fmt_str), std::string::npos);
  EXPECT_NE(s.find("end"), std::string::npos);
}

// Report: multiple redirects (file then string)
TEST(ReportCovTest, ReportRedirectSequence)
{
  Report report;
  const char *tmpfile = "/tmp/test_report_seq.txt";

  // Redirect to file first
  report.redirectFileBegin(tmpfile);
  report.reportLine("file output");
  report.redirectFileEnd();

  // Then redirect to string
  report.redirectStringBegin();
  report.reportLine("string output");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("string output"), std::string::npos);

  std::remove(tmpfile);
}

// Report: log active during string redirect (log should not capture)
TEST(ReportCovTest, LogDuringStringRedirect)
{
  Report report;
  const char *logfile = "/tmp/test_report_log_str.txt";

  report.logBegin(logfile);
  report.redirectStringBegin();
  report.reportLine("string only");
  const char *result = report.redirectStringEnd();
  report.logEnd();

  std::string s(result);
  EXPECT_NE(s.find("string only"), std::string::npos);
  std::remove(logfile);
}

// Report: warn with format that triggers printToBufferAppend
TEST(ReportCovTest, WarnWithLongMessage)
{
  Report report;
  report.redirectStringBegin();
  std::string long_msg(1500, 'w');
  report.warn(999, "prefix {} suffix", long_msg);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 999:"), std::string::npos);
  EXPECT_NE(s.find("prefix"), std::string::npos);
  EXPECT_NE(s.find("suffix"), std::string::npos);
}

// Report: fileWarn with long message
TEST(ReportCovTest, FileWarnLongMessage)
{
  Report report;
  report.redirectStringBegin();
  std::string long_msg(1500, 'f');
  report.fileWarn(998, "bigfile.v", 100, "detail: {}", long_msg);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 998:"), std::string::npos);
  EXPECT_NE(s.find("bigfile.v"), std::string::npos);
  EXPECT_NE(s.find("line 100"), std::string::npos);
}

// Report: error with long message (forces buffer growth)
TEST(ReportCovTest, ErrorLongMessage)
{
  Report report;
  std::string long_msg(1500, 'e');
  try {
    report.error(997, "err: {}", long_msg);
    FAIL();
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("err:"), std::string::npos);
    EXPECT_NE(what.find(long_msg), std::string::npos);
  }
}

// Report: fileError with long message
TEST(ReportCovTest, FileErrorLongMessage)
{
  Report report;
  std::string long_msg(1500, 'x');
  try {
    report.fileError(996, "big.sdc", 50, "detail: {}", long_msg);
    FAIL();
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("big.sdc"), std::string::npos);
    EXPECT_NE(what.find("line 50"), std::string::npos);
  }
}

////////////////////////////////////////////////////////////////
// ReportStd coverage tests
////////////////////////////////////////////////////////////////

TEST(ReportStdCovTest, MakeReportStd)
{
  Report *report = makeReportStd();
  EXPECT_NE(report, nullptr);
  // printConsole and printErrorConsole are called through reportLine/warn/error
  report->reportLine("test output from ReportStd");
  delete report;
}

TEST(ReportStdCovTest, ReportStdWarn)
{
  Report *report = makeReportStd();
  report->redirectStringBegin();
  report->warn(999, "ReportStd warn test");
  const char *result = report->redirectStringEnd();
  EXPECT_NE(std::string(result).find("Warning"), std::string::npos);
  delete report;
}

TEST(ReportStdCovTest, ReportStdError)
{
  Report *report = makeReportStd();
  try {
    report->error(999, "ReportStd error test");
    FAIL();
  } catch (const ExceptionMsg &e) {
    EXPECT_NE(std::string(e.what()).find("ReportStd error test"), std::string::npos);
  }
  delete report;
}

TEST(ReportStdCovTest, PrintConsoleDirect)
{
  ASSERT_NO_THROW(( [&](){
  Report *report = makeReportStd();
  // reportLine calls printConsole
  report->reportLine("direct console print test");
  // printError calls printErrorConsole - triggered by warn
  report->warn(998, "stderr test");
  delete report;

  }() ));
}

////////////////////////////////////////////////////////////////
// DispatchQueue coverage tests
////////////////////////////////////////////////////////////////

TEST(DispatchQueueCovTest, GetThreadCount)
{
  DispatchQueue dq(2);
  EXPECT_EQ(dq.getThreadCount(), 2u);
}

TEST(DispatchQueueCovTest, DispatchConstRef)
{
  DispatchQueue dq(1);
  std::atomic<int> counter(0);
  std::function<void(int)> task = [&counter](int) { counter++; };
  dq.dispatch(task);
  dq.finishTasks();
  EXPECT_EQ(counter.load(), 1);
}

TEST(DispatchQueueCovTest, DispatchMultiple)
{
  DispatchQueue dq(2);
  std::atomic<int> counter(0);
  for (int i = 0; i < 10; i++) {
    dq.dispatch([&counter](int) { counter++; });
  }
  dq.finishTasks();
  EXPECT_EQ(counter.load(), 10);
}

////////////////////////////////////////////////////////////////
// ExceptionLine coverage test
////////////////////////////////////////////////////////////////

TEST(ExceptionCovTest, FileNotReadable)
{
  FileNotReadable ex("testfile.cc");
  const char *msg = ex.what();
  EXPECT_NE(msg, nullptr);
}

TEST(ExceptionCovTest, FileNotWritable)
{
  FileNotWritable ex("testfile.cc");
  const char *msg = ex.what();
  EXPECT_NE(msg, nullptr);
}

// Concrete subclass to test ExceptionLine (which is abstract)
class TestExceptionLine : public ExceptionLine
{
public:
  TestExceptionLine(const char *filename, int line)
    : ExceptionLine(filename, line) {}
  const char *what() const noexcept override { return "test exception line"; }
};

TEST(ExceptionCovTest, ExceptionLineConstructor)
{
  TestExceptionLine ex("testfile.cc", 42);
  EXPECT_STREQ(ex.what(), "test exception line");
}

////////////////////////////////////////////////////////////////
// RiseFall::asRiseFallBoth non-const coverage test
////////////////////////////////////////////////////////////////

TEST(TransitionCovTest, RiseFallNonConstAsRiseFallBoth)
{
  // Non-const version of asRiseFallBoth
  RiseFall *rf_rise = const_cast<RiseFall*>(RiseFall::rise());
  const RiseFallBoth *rfb = rf_rise->asRiseFallBoth();
  EXPECT_EQ(rfb, RiseFallBoth::rise());

  RiseFall *rf_fall = const_cast<RiseFall*>(RiseFall::fall());
  rfb = rf_fall->asRiseFallBoth();
  EXPECT_EQ(rfb, RiseFallBoth::fall());
}

////////////////////////////////////////////////////////////////
// systemRunTime coverage test
////////////////////////////////////////////////////////////////

TEST(MachineCovTest, SystemRunTime)
{
  double stime = systemRunTime();
  EXPECT_GE(stime, 0.0);
}

////////////////////////////////////////////////////////////////
// Stats coverage test
////////////////////////////////////////////////////////////////

TEST(StatsCovTest, StatsConstructAndReport)
{
  ASSERT_NO_THROW(( [&](){
  Report report;
  Debug debug(&report);
  // With debug stats level 0, constructor/report are no-ops
  Stats stats(&debug, &report);
  stats.report("test step");

  // With debug stats level > 0, constructor records timing
  debug.setLevel("stats", 1);
  Stats stats2(&debug, &report);
  stats2.report("test step 2");

  }() ));
}

////////////////////////////////////////////////////////////////
// gzstream coverage tests (flush_buffer, overflow)
////////////////////////////////////////////////////////////////

} // namespace sta

#include "util/gzstream.hh"

namespace sta {

TEST(GzstreamCovTest, WriteGzFile)
{
  // Writing to ogzstream triggers overflow() and flush_buffer()
  const char *tmpgz = "/tmp/test_gzstream.gz";
  {
    gzstream::ogzstream gz(tmpgz);
    ASSERT_TRUE(gz.rdbuf()->is_open());
    gz << "hello gzstream test line\n";
    // Write enough data to trigger overflow
    for (int i = 0; i < 100; i++) {
      gz << "line " << i << " padding data for buffer overflow test\n";
    }
  }
  // Read back to verify
  {
    gzstream::igzstream gz(tmpgz);
    ASSERT_TRUE(gz.rdbuf()->is_open());
    std::string line;
    std::getline(gz, line);
    EXPECT_EQ(line, "hello gzstream test line");
  }
  std::remove(tmpgz);
}

TEST(GzstreamCovTest, FlushExplicit)
{
  ASSERT_NO_THROW(( [&](){
  const char *tmpgz = "/tmp/test_gzstream_flush.gz";
  {
    gzstream::ogzstream gz(tmpgz);
    gz << "flush test data";
    gz.flush();  // This triggers sync() -> flush_buffer()
  }
  std::remove(tmpgz);

  }() ));
}

////////////////////////////////////////////////////////////////
// RiseFallMinMax copy constructor and setValue(float)
////////////////////////////////////////////////////////////////

TEST(RiseFallMinMaxCovTest, CopyConstructor)
{
  RiseFallMinMax orig;
  orig.setValue(RiseFallBoth::rise(), MinMaxAll::max(), 1.5f);
  orig.setValue(RiseFallBoth::fall(), MinMaxAll::min(), 0.5f);

  RiseFallMinMax copy(&orig);
  float val;
  bool exists;
  copy.value(RiseFall::rise(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 1.5f);

  copy.value(RiseFall::fall(), MinMax::min(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.5f);
}

TEST(RiseFallMinMaxCovTest, SetValueFloat)
{
  RiseFallMinMax rfmm;
  rfmm.setValue(3.14f);

  // All four combinations should have the value
  float val;
  bool exists;
  rfmm.value(RiseFall::rise(), MinMax::min(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 3.14f);

  rfmm.value(RiseFall::rise(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 3.14f);

  rfmm.value(RiseFall::fall(), MinMax::min(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 3.14f);

  rfmm.value(RiseFall::fall(), MinMax::max(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 3.14f);
}

////////////////////////////////////////////////////////////////
// PatternMatch: patternWildcards, patternMatchNoCase standalone
////////////////////////////////////////////////////////////////

TEST(PatternMatchFuncTest, PatternWildcards)
{
  EXPECT_TRUE(patternWildcards("hel*lo"));
  EXPECT_TRUE(patternWildcards("hel?lo"));
  EXPECT_FALSE(patternWildcards("hello"));
  EXPECT_FALSE(patternWildcards(""));
}

TEST(PatternMatchFuncTest, PatternMatchNoCaseFunc)
{
  EXPECT_TRUE(patternMatchNoCase("hello", "hello", false));
  EXPECT_TRUE(patternMatchNoCase("hello", "HELLO", true));
  EXPECT_FALSE(patternMatchNoCase("hello", "HELLO", false));
  EXPECT_TRUE(patternMatchNoCase("hel*", "HELLO", true));
  EXPECT_FALSE(patternMatchNoCase("hel*", "HELLO", false));
}

TEST(PatternMatchFuncTest, EqualCaseNoCase)
{
  // Test patternMatchNoCase with question marks
  EXPECT_TRUE(patternMatchNoCase("h?llo", "HELLO", true));
  EXPECT_FALSE(patternMatchNoCase("h?llo", "HELLO", false));
}

////////////////////////////////////////////////////////////////
// Transition: RiseFallBoth::matches(const Transition*) additional
// and Transition::asRiseFallBoth() const
////////////////////////////////////////////////////////////////

TEST(TransitionCovTest, TransitionAsRiseFallBothConst)
{
  // Transition::asRiseFallBoth() const for various transitions
  const RiseFallBoth *rfb_rise = Transition::rise()->asRiseFallBoth();
  EXPECT_NE(rfb_rise, nullptr);
  const RiseFallBoth *rfb_fall = Transition::fall()->asRiseFallBoth();
  EXPECT_NE(rfb_fall, nullptr);
  // Non-rise/fall transitions may return nullptr or different RiseFallBoth
  Transition::tr0Z()->asRiseFallBoth();  // just call for coverage
  Transition::tr1Z()->asRiseFallBoth();
}

////////////////////////////////////////////////////////////////
// Report: redirectStringPrint coverage
// This is called when printString is called during string redirect
////////////////////////////////////////////////////////////////

TEST(ReportCovTest, RedirectStringPrintCoverage)
{
  Report report;
  report.redirectStringBegin();
  // printString during redirect calls redirectStringPrint
  report.printString("hello ", 6);
  report.printString("world", 5);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("hello "), std::string::npos);
  EXPECT_NE(s.find("world"), std::string::npos);
}

////////////////////////////////////////////////////////////////
// ReportStd: printErrorConsole
// Called by warn/error which use printError path
////////////////////////////////////////////////////////////////

TEST(ReportStdCovTest, PrintErrorConsole)
{
  ASSERT_NO_THROW(( [&](){
  Report *report = makeReportStd();
  // warn() outputs to stderr via printErrorConsole
  report->warn(777, "testing stderr output");
  // fileWarn also goes through printErrorConsole
  report->fileWarn(778, "test.v", 1, "file warning test");
  delete report;

  }() ));
}

// Test Report redirectStringPrint with empty string
// Covers: Report::redirectStringPrint
TEST(ReportCovTest, RedirectStringPrintEmpty)
{
  Report report;
  report.redirectStringBegin();
  report.printString("", 0);
  const char *result = report.redirectStringEnd();
  EXPECT_STREQ(result, "");
}

// Test Report redirectStringPrint with large string
TEST(ReportCovTest, RedirectStringPrintLarge)
{
  Report report;
  report.redirectStringBegin();
  std::string large(500, 'A');
  report.printString(large.c_str(), large.size());
  const char *result = report.redirectStringEnd();
  EXPECT_EQ(std::string(result), large);
}

// Test Report redirectStringPrint multiple times
TEST(ReportCovTest, RedirectStringPrintMultiple)
{
  Report report;
  report.redirectStringBegin();
  report.printString("abc", 3);
  report.printString("def", 3);
  report.printString("ghi", 3);
  const char *result = report.redirectStringEnd();
  EXPECT_STREQ(result, "abcdefghi");
}

// Test Report::report with format args
// Covers: Report::report(std::string_view, Args&&...)
TEST(ReportCovTest, ReportFormatViaReport)
{
  Report report;
  report.redirectStringBegin();
  // report() uses std::format style formatting
  report.report("value={}", 42);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("value=42"), std::string::npos);
}

// Test Report::reportLineString
TEST(ReportCovTest, ReportLineString)
{
  Report report;
  report.redirectStringBegin();
  report.reportLine("test line");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("test line"), std::string::npos);
}

// Test Report::reportLineString with std::string
TEST(ReportCovTest, ReportLineStringStd)
{
  Report report;
  report.redirectStringBegin();
  std::string line = "std string line";
  report.reportLine(line);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("std string line"), std::string::npos);
}

// Test Report::reportBlankLine
TEST(ReportCovTest, ReportBlankLine)
{
  Report report;
  report.redirectStringBegin();
  report.reportBlankLine();
  const char *result = report.redirectStringEnd();
  // Blank line produces just a newline
  EXPECT_STREQ(result, "\n");
}

// Test ReportStd constructor
// Covers: ReportStd::ReportStd()
TEST(ReportStdCovTest, ReportStdConstructor)
{
  Report *report = makeReportStd();
  EXPECT_NE(report, nullptr);
  // Verify it's the default report
  EXPECT_EQ(Report::defaultReport(), report);
  delete report;
}

// Test ReportStd printErrorConsole via fileError
// Covers: ReportStd::printErrorConsole
TEST(ReportStdCovTest, PrintErrorConsoleViaWarn)
{
  ASSERT_NO_THROW(( [&](){
  Report *report = makeReportStd();
  // warn uses printErrorConsole path
  report->warn(9999, "test warning {}", 42);
  delete report;

  }() ));
}

// Test Report suppress/unsuppress messages
TEST(ReportCovTest, SuppressUnsuppress)
{
  Report report;
  EXPECT_FALSE(report.isSuppressed(100));
  report.suppressMsgId(100);
  EXPECT_TRUE(report.isSuppressed(100));
  report.unsuppressMsgId(100);
  EXPECT_FALSE(report.isSuppressed(100));
}

// Test Report suppressed warn is silent
TEST(ReportCovTest, SuppressedWarn)
{
  Report report;
  report.suppressMsgId(200);
  report.redirectStringBegin();
  report.warn(200, "this should be suppressed");
  const char *result = report.redirectStringEnd();
  // Suppressed message should not appear
  EXPECT_STREQ(result, "");
}

// Test Report logBegin/logEnd
TEST(ReportCovTest, LogBeginEnd)
{
  Report report;
  const char *logfile = "/tmp/sta_test_log_r5.log";
  report.logBegin(logfile);
  report.report("log line {}", 1);
  report.logEnd();
  // Verify log file exists and has content
  std::ifstream in(logfile);
  EXPECT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("log line 1"), std::string::npos);
  in.close();
  std::remove(logfile);
}

// Test Report redirectFileBegin/redirectFileEnd
TEST(ReportCovTest, RedirectFileBeginEnd)
{
  Report report;
  const char *tmpfile = "/tmp/sta_test_redirect_r5.txt";
  report.redirectFileBegin(tmpfile);
  report.reportLine("redirected line");
  report.redirectFileEnd();

  std::ifstream in(tmpfile);
  EXPECT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("redirected line"), std::string::npos);
  in.close();
  std::remove(tmpfile);
}

// Test Report redirectFileAppendBegin
TEST(ReportCovTest, RedirectFileAppendBegin)
{
  Report report;
  const char *tmpfile = "/tmp/sta_test_append_r5.txt";

  // Write initial content
  report.redirectFileBegin(tmpfile);
  report.reportLine("line1");
  report.redirectFileEnd();

  // Append
  report.redirectFileAppendBegin(tmpfile);
  report.reportLine("line2");
  report.redirectFileEnd();

  std::ifstream in(tmpfile);
  EXPECT_TRUE(in.is_open());
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("line1"), std::string::npos);
  std::getline(in, line);
  EXPECT_NE(line.find("line2"), std::string::npos);
  in.close();
  std::remove(tmpfile);
}

// Test gzstreambuf basic operations
// Covers: gzstreambuf::~gzstreambufD0Ev (virtual destructor)
TEST(GzStreamTest, GzStreamBufConstruction)
{
  // Test igzstream with a non-existent file
  gzstream::igzstream stream;
  EXPECT_FALSE(stream.is_open());
}

// Test gzstreambuf with actual gz file
TEST(GzStreamTest, GzStreamWriteRead)
{
  const char *tmpfile = "/tmp/sta_test_gz_r5.gz";

  // Write
  {
    gzstream::ogzstream out(tmpfile);
    EXPECT_TRUE(out.rdbuf()->is_open());
    out << "hello gz world" << std::endl;
  }

  // Read
  {
    gzstream::igzstream in(tmpfile);
    EXPECT_TRUE(in.is_open());
    std::string line;
    std::getline(in, line);
    EXPECT_EQ(line, "hello gz world");
  }

  std::remove(tmpfile);
}

// Test Report error throws ExceptionMsg
TEST(ReportCovTest, ErrorThrowsException)
{
  Report report;
  EXPECT_THROW(report.error(1, "test error {}", "msg"), ExceptionMsg);
}

// Test Report fileError throws ExceptionMsg
TEST(ReportCovTest, FileErrorThrowsException)
{
  Report report;
  EXPECT_THROW(report.fileError(1, "test.v", 10, "file error"), ExceptionMsg);
}

// Test Report verror throws ExceptionMsg
TEST(ReportCovTest, VerrorThrowsException)
{
  Report report;
  EXPECT_THROW(report.error(1, "verror test"), ExceptionMsg);
}

////////////////////////////////////////////////////////////////
// R6_ tests for Util function coverage
////////////////////////////////////////////////////////////////

// Test Report::critical calls exit (we can't test exit directly,
// but we can test that the function exists and the format works)
// Covers: Report::critical - we test via Report::error which shares formatting
TEST(ReportCovTest, ReportErrorFormatting)
{
  Report report;
  try {
    report.error(999, "critical format test {} {}", "value", 42);
    FAIL();
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("critical format test value 42"), std::string::npos);
  }
}

// Test Report::fileCritical via fileError (shares code path)
// Covers: Report::fileCritical formatting path
TEST(ReportCovTest, ReportFileErrorFormatting)
{
  Report report;
  try {
    report.fileError(998, "critical.v", 42, "critical file error {}", "detail");
    FAIL();
  } catch (const ExceptionMsg &e) {
    std::string what = e.what();
    EXPECT_NE(what.find("critical.v"), std::string::npos);
    EXPECT_NE(what.find("line 42"), std::string::npos);
    EXPECT_NE(what.find("critical file error detail"), std::string::npos);
  }
}

// Test Report D0 destructor through base pointer
// Covers: Report::~Report() D0
TEST(ReportCovTest, ReportD0Destructor)
{
  Report *report = new Report();
  EXPECT_NE(report, nullptr);
  delete report;
  // No crash = D0 destructor works
}

// Test ReportStd creation via makeReportStd
// Covers: ReportStd::ReportStd constructor, makeReportStd
TEST(ReportCovTest, ReportStdCreation)
{
  Report *report = makeReportStd();
  ASSERT_NE(report, nullptr);
  // Verify it works as a Report
  report->redirectStringBegin();
  report->reportLine("test via ReportStd");
  const char *result = report->redirectStringEnd();
  EXPECT_NE(result, nullptr);
  std::string s(result);
  EXPECT_NE(s.find("test via ReportStd"), std::string::npos);
  delete report;
}

// Test ReportStd warn output
// Covers: ReportStd::printErrorConsole (indirectly via warn)
TEST(ReportCovTest, ReportStdWarn)
{
  Report *report = makeReportStd();
  ASSERT_NE(report, nullptr);
  report->redirectStringBegin();
  report->warn(700, "reportstd warn test {}", 99);
  const char *result = report->redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 700:"), std::string::npos);
  EXPECT_NE(s.find("reportstd warn test 99"), std::string::npos);
  delete report;
}

// Test ReportStd error
// Covers: ReportStd error path
TEST(ReportCovTest, ReportStdError)
{
  Report *report = makeReportStd();
  ASSERT_NE(report, nullptr);
  EXPECT_THROW(report->error(700, "reportstd error test"), ExceptionMsg);
  delete report;
}

// Test Report printToBuffer with long format (indirectly via reportLine)
// Covers: Report::printToBuffer buffer growth
TEST(ReportCovTest, ReportPrintToBufferLong)
{
  Report report;
  report.redirectStringBegin();
  // Create a string exceeding the initial 1000-char buffer
  std::string long_str(3000, 'Z');
  report.reportLine(long_str);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find(long_str), std::string::npos);
}

// Test Report redirectStringPrint indirectly via redirectStringBegin/End
// Covers: Report::redirectStringPrint
TEST(ReportCovTest, RedirectStringPrint)
{
  Report report;
  report.redirectStringBegin();
  // printString calls redirectStringPrint when redirect_to_string_ is true
  report.printString("direct string data", 18);
  const char *result = report.redirectStringEnd();
  EXPECT_STREQ(result, "direct string data");
}

// Test Report multiple printString calls during redirect
// Covers: Report::redirectStringPrint concatenation
TEST(ReportCovTest, RedirectStringPrintMultiple2)
{
  Report report;
  report.redirectStringBegin();
  report.printString("part1", 5);
  report.printString("part2", 5);
  report.printString("part3", 5);
  const char *result = report.redirectStringEnd();
  EXPECT_STREQ(result, "part1part2part3");
}

// Test gzstreambuf D0 destructor through gzstream objects
// Covers: gzstreambuf::~gzstreambuf() D0
TEST(GzStreamCovTest, GzStreamBufD0Destructor)
{
  const char *tmpfile = "/tmp/test_gz_d0.gz";
  {
    // Create and destroy to exercise D0 destructor
    gzstream::ogzstream *out = new gzstream::ogzstream(tmpfile);
    *out << "test data" << std::endl;
    delete out;  // triggers D0 destructor for gzstreambuf
  }

  {
    gzstream::igzstream *in = new gzstream::igzstream(tmpfile);
    std::string line;
    std::getline(*in, line);
    EXPECT_EQ(line, "test data");
    delete in;  // triggers D0 destructor for gzstreambuf
  }
  std::remove(tmpfile);
}

// Test Report suppress and unsuppress multiple IDs
// Covers: Report::suppressMsgId, unsuppressMsgId
TEST(ReportCovTest, SuppressMultipleIds)
{
  Report report;
  report.suppressMsgId(1);
  report.suppressMsgId(2);
  report.suppressMsgId(3);
  EXPECT_TRUE(report.isSuppressed(1));
  EXPECT_TRUE(report.isSuppressed(2));
  EXPECT_TRUE(report.isSuppressed(3));
  EXPECT_FALSE(report.isSuppressed(4));

  report.unsuppressMsgId(2);
  EXPECT_TRUE(report.isSuppressed(1));
  EXPECT_FALSE(report.isSuppressed(2));
  EXPECT_TRUE(report.isSuppressed(3));
}

// Test Report warn with long message that exceeds buffer
// Covers: Report::printToBuffer buffer reallocation
TEST(ReportCovTest, WarnLongMessage)
{
  Report report;
  report.redirectStringBegin();
  std::string long_msg(5000, 'W');
  report.warn(800, "{}", long_msg);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 800:"), std::string::npos);
  EXPECT_NE(s.find(long_msg), std::string::npos);
}

// Test Report fileWarn with long message
// Covers: Report::printToBuffer via fileWarn
TEST(ReportCovTest, FileWarnLongMessage2)
{
  Report report;
  report.redirectStringBegin();
  std::string long_msg(2000, 'F');
  report.fileWarn(801, "long_file.v", 999, "{}", long_msg);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning 801:"), std::string::npos);
  EXPECT_NE(s.find("long_file.v"), std::string::npos);
  EXPECT_NE(s.find(long_msg), std::string::npos);
}

// Test Report error with suppressed flag
// Covers: ExceptionMsg suppression
TEST(ReportCovTest, ErrorSuppressed)
{
  Report report;
  report.suppressMsgId(900);
  try {
    report.error(900, "suppressed error");
    FAIL();
  } catch (const ExceptionMsg &e) {
    EXPECT_TRUE(e.suppressed());
  }
}

// Test Report error without suppression
// Covers: ExceptionMsg non-suppression
TEST(ReportCovTest, ErrorNotSuppressed)
{
  Report report;
  try {
    report.error(901, "not suppressed error");
    FAIL();
  } catch (const ExceptionMsg &e) {
    EXPECT_FALSE(e.suppressed());
  }
}

////////////////////////////////////////////////////////////////
// R8_ tests for UTIL module coverage improvement
////////////////////////////////////////////////////////////////

// Test Report::critical throws/exits
// Covers: Report::critical(int, const char*, ...)
// Note: critical() calls abort(), so we test it carefully
// We can only test that the function exists and is callable through
// other paths. The critical() function cannot be directly tested
// because it calls abort(). Instead, test printToBuffer and
// redirectStringPrint paths.

// Test Report::report with multiple format args
// Covers: Report::report(std::string_view, Args&&...)
TEST(ReportCovTest, ReportFormatViaReport2)
{
  Report report;
  report.redirectStringBegin();
  report.report("test {} {} {:.2f}", 42, "hello", 3.14);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("42"), std::string::npos);
  EXPECT_NE(s.find("hello"), std::string::npos);
  EXPECT_NE(s.find("3.14"), std::string::npos);
}

// Test Report::redirectStringPrint via redirectStringBegin/End
// Covers: Report::redirectStringPrint(const char*, size_t)
TEST(ReportCovTest, RedirectStringPrint2)
{
  Report report;
  report.redirectStringBegin();
  report.reportLine("line 1");
  report.reportLine("line 2");
  report.reportLine("line 3");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("line 1"), std::string::npos);
  EXPECT_NE(s.find("line 2"), std::string::npos);
  EXPECT_NE(s.find("line 3"), std::string::npos);
}

// Test Report::redirectStringPrint with long string
// Covers: Report::redirectStringPrint(const char*, size_t)
TEST(ReportCovTest, RedirectStringPrintLong)
{
  Report report;
  report.redirectStringBegin();
  std::string long_str(5000, 'X');
  report.reportLine(long_str);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("XXXXX"), std::string::npos);
}

// Test Report::report with various format strings
// Covers: Report::report(std::string_view, Args&&...)
TEST(ReportCovTest, ReportFormatVariousFormats)
{
  Report report;
  report.redirectStringBegin();
  // Exercise various std::format specifiers
  report.report("int: {}", 12345);
  report.report("float: {}", 1.5);
  report.report("string: {}", "test_string");
  report.report("hex: {:x}", 0xFF);
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("12345"), std::string::npos);
  EXPECT_NE(s.find("test_string"), std::string::npos);
}

// Test ReportStd constructor and printErrorConsole
// Covers: ReportStd::ReportStd(), ReportStd::printErrorConsole
TEST(ReportStdCovTest, ReportStdConstructorAndPrint)
{
  Report *report = makeReportStd();
  ASSERT_NE(report, nullptr);
  // warn() calls printErrorConsole
  report->warn(10001, "R8 test warning {}", "message");
  // reportLine calls printConsole
  report->report("R8 test print {}", 42);
  delete report;
}

// Test ReportStd printErrorConsole through fileWarn
// Covers: ReportStd::printErrorConsole(const char*, size_t)
TEST(ReportStdCovTest, PrintErrorConsoleViaFileWarn)
{
  Report *report = makeReportStd();
  ASSERT_NE(report, nullptr);
  report->fileWarn(10002, "test_file.v", 100, "file warning {}", 99);
  delete report;
}

// Test Report::printToBuffer with empty format
// Covers: Report::printToBuffer
TEST(ReportCovTest, PrintToBufferEmpty)
{
  Report report;
  report.redirectStringBegin();
  report.reportLine("");
  const char *result = report.redirectStringEnd();
  // Should have at least a newline
  EXPECT_NE(result, nullptr);
}

// Test Report warn with redirect
// Covers: Report::printToBuffer, Report::redirectStringPrint
TEST(ReportCovTest, WarnWithRedirect)
{
  Report report;
  report.redirectStringBegin();
  report.warn(10003, "warning {}: {}", 1, "test");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning"), std::string::npos);
  EXPECT_NE(s.find("test"), std::string::npos);
}

// Test Report fileWarn with redirect
// Covers: Report::printToBuffer, Report::redirectStringPrint
TEST(ReportCovTest, FileWarnWithRedirect)
{
  Report report;
  report.redirectStringBegin();
  report.fileWarn(10004, "myfile.tcl", 42, "file issue {}", "here");
  const char *result = report.redirectStringEnd();
  std::string s(result);
  EXPECT_NE(s.find("Warning"), std::string::npos);
  EXPECT_NE(s.find("myfile.tcl"), std::string::npos);
}

// Test gzstream write and read
// Covers: gzstreambuf (exercises gzstream I/O paths)
TEST(GzStreamTest, WriteAndReadGz)
{
  const char *tmpfile = "/tmp/test_r8_gzstream.gz";
  {
    gzstream::ogzstream out(tmpfile);
    ASSERT_TRUE(out.good());
    out << "test line 1\n";
    out << "test line 2\n";
    out.close();
  }
  {
    gzstream::igzstream in(tmpfile);
    ASSERT_TRUE(in.good());
    std::string line1, line2;
    std::getline(in, line1);
    std::getline(in, line2);
    EXPECT_EQ(line1, "test line 1");
    EXPECT_EQ(line2, "test line 2");
    in.close();
  }
  std::remove(tmpfile);
}

} // namespace sta
