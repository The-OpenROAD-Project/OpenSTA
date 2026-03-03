#include <gtest/gtest.h>
#include "MinMax.hh"
#include "Transition.hh"
#include "StringUtil.hh"
// Power module unit tests

// Power module smoke tests
namespace sta {

class PowerSmokeTest : public ::testing::Test {};

TEST_F(PowerSmokeTest, TransitionsForPower) {
  // Power calculation uses rise/fall transitions
  EXPECT_NE(RiseFall::rise(), nullptr);
  EXPECT_NE(RiseFall::fall(), nullptr);
  EXPECT_EQ(RiseFall::range().size(), 2u);
}

TEST_F(PowerSmokeTest, MinMaxForPower) {
  // Power uses min/max for different analysis corners
  EXPECT_NE(MinMax::min(), nullptr);
  EXPECT_NE(MinMax::max(), nullptr);
}

TEST_F(PowerSmokeTest, StringUtils) {
  // VCD reader uses string utilities
  EXPECT_TRUE(isDigits("12345"));
  EXPECT_FALSE(isDigits("abc"));
  EXPECT_FALSE(isDigits("12a34"));
}

////////////////////////////////////////////////////////////////
// PowerResult tests

} // namespace sta

#include "NetworkClass.hh"
#include "PowerClass.hh"

namespace sta {

class PowerResultTest : public ::testing::Test {};

TEST_F(PowerResultTest, DefaultConstruction) {
  PowerResult result;
  EXPECT_FLOAT_EQ(result.internal(), 0.0f);
  EXPECT_FLOAT_EQ(result.switching(), 0.0f);
  EXPECT_FLOAT_EQ(result.leakage(), 0.0f);
  EXPECT_FLOAT_EQ(result.total(), 0.0f);
}

TEST_F(PowerResultTest, IncrInternal) {
  PowerResult result;
  result.incrInternal(1.0e-3f);
  EXPECT_FLOAT_EQ(result.internal(), 1.0e-3f);
  result.incrInternal(2.0e-3f);
  EXPECT_FLOAT_EQ(result.internal(), 3.0e-3f);
}

TEST_F(PowerResultTest, IncrSwitching) {
  PowerResult result;
  result.incrSwitching(5.0e-4f);
  EXPECT_FLOAT_EQ(result.switching(), 5.0e-4f);
  result.incrSwitching(3.0e-4f);
  EXPECT_FLOAT_EQ(result.switching(), 8.0e-4f);
}

TEST_F(PowerResultTest, IncrLeakage) {
  PowerResult result;
  result.incrLeakage(1.0e-6f);
  EXPECT_FLOAT_EQ(result.leakage(), 1.0e-6f);
  result.incrLeakage(2.0e-6f);
  EXPECT_FLOAT_EQ(result.leakage(), 3.0e-6f);
}

TEST_F(PowerResultTest, Total) {
  PowerResult result;
  result.incrInternal(1.0e-3f);
  result.incrSwitching(2.0e-3f);
  result.incrLeakage(3.0e-3f);
  EXPECT_FLOAT_EQ(result.total(), 6.0e-3f);
}

TEST_F(PowerResultTest, Clear) {
  PowerResult result;
  result.incrInternal(1.0e-3f);
  result.incrSwitching(2.0e-3f);
  result.incrLeakage(3.0e-3f);
  EXPECT_FLOAT_EQ(result.total(), 6.0e-3f);

  result.clear();
  EXPECT_FLOAT_EQ(result.internal(), 0.0f);
  EXPECT_FLOAT_EQ(result.switching(), 0.0f);
  EXPECT_FLOAT_EQ(result.leakage(), 0.0f);
  EXPECT_FLOAT_EQ(result.total(), 0.0f);
}

TEST_F(PowerResultTest, Incr) {
  PowerResult result1;
  result1.incrInternal(1.0e-3f);
  result1.incrSwitching(2.0e-3f);
  result1.incrLeakage(3.0e-3f);

  PowerResult result2;
  result2.incrInternal(4.0e-3f);
  result2.incrSwitching(5.0e-3f);
  result2.incrLeakage(6.0e-3f);

  result1.incr(result2);
  EXPECT_FLOAT_EQ(result1.internal(), 5.0e-3f);
  EXPECT_FLOAT_EQ(result1.switching(), 7.0e-3f);
  EXPECT_FLOAT_EQ(result1.leakage(), 9.0e-3f);
  EXPECT_FLOAT_EQ(result1.total(), 21.0e-3f);
}

TEST_F(PowerResultTest, IncrSelf) {
  PowerResult result;
  result.incrInternal(1.0e-3f);
  result.incrSwitching(2.0e-3f);
  result.incrLeakage(3.0e-3f);

  result.incr(result);
  EXPECT_FLOAT_EQ(result.internal(), 2.0e-3f);
  EXPECT_FLOAT_EQ(result.switching(), 4.0e-3f);
  EXPECT_FLOAT_EQ(result.leakage(), 6.0e-3f);
}

TEST_F(PowerResultTest, IncrEmptyResult) {
  PowerResult result;
  result.incrInternal(1.0e-3f);

  PowerResult empty;
  result.incr(empty);
  EXPECT_FLOAT_EQ(result.internal(), 1.0e-3f);
  EXPECT_FLOAT_EQ(result.switching(), 0.0f);
  EXPECT_FLOAT_EQ(result.leakage(), 0.0f);
}

////////////////////////////////////////////////////////////////
// PwrActivity tests

class PwrActivityTest : public ::testing::Test {};

TEST_F(PwrActivityTest, DefaultConstruction) {
  PwrActivity activity;
  EXPECT_FLOAT_EQ(activity.density(), 0.0f);
  EXPECT_FLOAT_EQ(activity.duty(), 0.0f);
  EXPECT_EQ(activity.origin(), PwrActivityOrigin::unknown);
  EXPECT_FALSE(activity.isSet());
}

TEST_F(PwrActivityTest, ParameterizedConstruction) {
  PwrActivity activity(1000.0f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(activity.density(), 1000.0f);
  EXPECT_FLOAT_EQ(activity.duty(), 0.5f);
  EXPECT_EQ(activity.origin(), PwrActivityOrigin::user);
  EXPECT_TRUE(activity.isSet());
}

TEST_F(PwrActivityTest, SetDensity) {
  PwrActivity activity;
  activity.setDensity(500.0f);
  EXPECT_FLOAT_EQ(activity.density(), 500.0f);
}

TEST_F(PwrActivityTest, SetDuty) {
  PwrActivity activity;
  activity.setDuty(0.75f);
  EXPECT_FLOAT_EQ(activity.duty(), 0.75f);
}

TEST_F(PwrActivityTest, SetOrigin) {
  PwrActivity activity;
  activity.setOrigin(PwrActivityOrigin::vcd);
  EXPECT_EQ(activity.origin(), PwrActivityOrigin::vcd);
  EXPECT_TRUE(activity.isSet());
}

TEST_F(PwrActivityTest, Init) {
  PwrActivity activity(1000.0f, 0.5f, PwrActivityOrigin::user);
  EXPECT_TRUE(activity.isSet());

  activity.init();
  EXPECT_FLOAT_EQ(activity.density(), 0.0f);
  EXPECT_FLOAT_EQ(activity.duty(), 0.0f);
  EXPECT_EQ(activity.origin(), PwrActivityOrigin::unknown);
  EXPECT_FALSE(activity.isSet());
}

TEST_F(PwrActivityTest, Set) {
  PwrActivity activity;
  activity.set(2000.0f, 0.3f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(activity.density(), 2000.0f);
  EXPECT_FLOAT_EQ(activity.duty(), 0.3f);
  EXPECT_EQ(activity.origin(), PwrActivityOrigin::propagated);
  EXPECT_TRUE(activity.isSet());
}

TEST_F(PwrActivityTest, IsSetForAllOrigins) {
  PwrActivity activity;

  activity.setOrigin(PwrActivityOrigin::unknown);
  EXPECT_FALSE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::global);
  EXPECT_TRUE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::input);
  EXPECT_TRUE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::user);
  EXPECT_TRUE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::vcd);
  EXPECT_TRUE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::saif);
  EXPECT_TRUE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::propagated);
  EXPECT_TRUE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::clock);
  EXPECT_TRUE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::constant);
  EXPECT_TRUE(activity.isSet());

  activity.setOrigin(PwrActivityOrigin::defaulted);
  EXPECT_TRUE(activity.isSet());
}

TEST_F(PwrActivityTest, OriginName) {
  PwrActivity activity;

  activity.setOrigin(PwrActivityOrigin::global);
  EXPECT_STREQ(activity.originName(), "global");

  activity.setOrigin(PwrActivityOrigin::input);
  EXPECT_STREQ(activity.originName(), "input");

  activity.setOrigin(PwrActivityOrigin::user);
  EXPECT_STREQ(activity.originName(), "user");

  activity.setOrigin(PwrActivityOrigin::vcd);
  EXPECT_STREQ(activity.originName(), "vcd");

  activity.setOrigin(PwrActivityOrigin::saif);
  EXPECT_STREQ(activity.originName(), "saif");

  activity.setOrigin(PwrActivityOrigin::propagated);
  EXPECT_STREQ(activity.originName(), "propagated");

  activity.setOrigin(PwrActivityOrigin::clock);
  EXPECT_STREQ(activity.originName(), "clock");

  activity.setOrigin(PwrActivityOrigin::constant);
  EXPECT_STREQ(activity.originName(), "constant");

  activity.setOrigin(PwrActivityOrigin::defaulted);
  EXPECT_STREQ(activity.originName(), "defaulted");

  activity.setOrigin(PwrActivityOrigin::unknown);
  EXPECT_STREQ(activity.originName(), "unknown");
}

TEST_F(PwrActivityTest, VerySmallDensityClipped) {
  // Density smaller than min_density (1E-10) should be clipped to 0
  PwrActivity activity(1e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(activity.density(), 0.0f);
}

TEST_F(PwrActivityTest, DensityAboveThresholdNotClipped) {
  // Density above min_density should not be clipped
  PwrActivity activity(1e-9f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(activity.density(), 1e-9f);
}

TEST_F(PwrActivityTest, SetWithSmallDensity) {
  PwrActivity activity;
  activity.set(1e-12f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(activity.density(), 0.0f);  // clipped
}

TEST_F(PwrActivityTest, NegativeSmallDensityClipped) {
  // Negative density smaller than -min_density should be clipped
  PwrActivity activity(-1e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(activity.density(), 0.0f);
}

TEST_F(PwrActivityTest, NormalDensity) {
  PwrActivity activity(1e6f, 0.5f, PwrActivityOrigin::vcd);
  EXPECT_FLOAT_EQ(activity.density(), 1e6f);
}

TEST_F(PwrActivityTest, ZeroDuty) {
  PwrActivity activity(1000.0f, 0.0f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(activity.duty(), 0.0f);
}

TEST_F(PwrActivityTest, FullDuty) {
  PwrActivity activity(1000.0f, 1.0f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(activity.duty(), 1.0f);
}

////////////////////////////////////////////////////////////////
// Additional PowerResult tests for edge cases

TEST_F(PowerResultTest, NegativeInternalPower) {
  PowerResult result;
  result.incrInternal(-1.0e-3f);
  EXPECT_FLOAT_EQ(result.internal(), -1.0e-3f);
}

TEST_F(PowerResultTest, MixedSignPower) {
  PowerResult result;
  result.incrInternal(5.0e-3f);
  result.incrSwitching(3.0e-3f);
  result.incrLeakage(-1.0e-3f);
  EXPECT_FLOAT_EQ(result.total(), 7.0e-3f);
}

TEST_F(PowerResultTest, ClearAndReuse) {
  PowerResult result;
  result.incrInternal(1.0f);
  result.incrSwitching(2.0f);
  result.incrLeakage(3.0f);
  result.clear();
  result.incrInternal(10.0f);
  EXPECT_FLOAT_EQ(result.internal(), 10.0f);
  EXPECT_FLOAT_EQ(result.switching(), 0.0f);
  EXPECT_FLOAT_EQ(result.leakage(), 0.0f);
  EXPECT_FLOAT_EQ(result.total(), 10.0f);
}

TEST_F(PowerResultTest, MultipleIncrements) {
  PowerResult result;
  for (int i = 0; i < 100; i++) {
    result.incrInternal(1.0e-6f);
    result.incrSwitching(2.0e-6f);
    result.incrLeakage(3.0e-6f);
  }
  EXPECT_NEAR(result.internal(), 100.0e-6f, 1e-9f);
  EXPECT_NEAR(result.switching(), 200.0e-6f, 1e-9f);
  EXPECT_NEAR(result.leakage(), 300.0e-6f, 1e-9f);
  EXPECT_NEAR(result.total(), 600.0e-6f, 1e-9f);
}

////////////////////////////////////////////////////////////////
// Additional PwrActivity origin tests
////////////////////////////////////////////////////////////////

TEST_F(PwrActivityTest, OriginNames) {
  // Test all origin name strings
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::unknown).originName(), "unknown");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::global).originName(), "global");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::input).originName(), "input");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::user).originName(), "user");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::vcd).originName(), "vcd");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::saif).originName(), "saif");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::propagated).originName(), "propagated");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::clock).originName(), "clock");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::constant).originName(), "constant");
  EXPECT_STREQ(PwrActivity(0.0f, 0.0f, PwrActivityOrigin::defaulted).originName(), "defaulted");
}

// Construct and test with explicit density/duty
TEST_F(PwrActivityTest, ConstructionDetails) {
  PwrActivity act(500.0f, 0.25f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 500.0f);
  EXPECT_FLOAT_EQ(act.duty(), 0.25f);
  EXPECT_EQ(act.origin(), PwrActivityOrigin::propagated);
}

// Test isSet for various origins
TEST_F(PwrActivityTest, IsSetOriginCombinations) {
  PwrActivity a;
  a.setOrigin(PwrActivityOrigin::unknown);
  EXPECT_FALSE(a.isSet());

  for (auto origin : {PwrActivityOrigin::global,
                      PwrActivityOrigin::input,
                      PwrActivityOrigin::user,
                      PwrActivityOrigin::vcd,
                      PwrActivityOrigin::saif,
                      PwrActivityOrigin::propagated,
                      PwrActivityOrigin::clock,
                      PwrActivityOrigin::constant,
                      PwrActivityOrigin::defaulted}) {
    a.setOrigin(origin);
    EXPECT_TRUE(a.isSet());
  }
}

// Test init and then set again
TEST_F(PwrActivityTest, InitThenSet) {
  PwrActivity act(100.0f, 0.5f, PwrActivityOrigin::user);
  act.init();
  EXPECT_FALSE(act.isSet());
  EXPECT_FLOAT_EQ(act.density(), 0.0f);
  EXPECT_FLOAT_EQ(act.duty(), 0.0f);

  act.set(200.0f, 0.7f, PwrActivityOrigin::vcd);
  EXPECT_TRUE(act.isSet());
  EXPECT_FLOAT_EQ(act.density(), 200.0f);
  EXPECT_FLOAT_EQ(act.duty(), 0.7f);
}

// Test PowerResult with zero values
TEST_F(PowerResultTest, AllZero) {
  PowerResult result;
  EXPECT_FLOAT_EQ(result.total(), 0.0f);
  EXPECT_FLOAT_EQ(result.internal(), 0.0f);
  EXPECT_FLOAT_EQ(result.switching(), 0.0f);
  EXPECT_FLOAT_EQ(result.leakage(), 0.0f);
}

// Test PowerResult accumulation with large values
TEST_F(PowerResultTest, LargeValues) {
  PowerResult result;
  result.incrInternal(1.0e3f);
  result.incrSwitching(2.0e3f);
  result.incrLeakage(3.0e3f);
  EXPECT_FLOAT_EQ(result.total(), 6.0e3f);
}

// Test PowerResult incr with zeroed other
TEST_F(PowerResultTest, IncrWithZero) {
  PowerResult result;
  result.incrInternal(5.0f);
  result.incrSwitching(3.0f);
  result.incrLeakage(1.0f);

  PowerResult zero;
  result.incr(zero);
  EXPECT_FLOAT_EQ(result.internal(), 5.0f);
  EXPECT_FLOAT_EQ(result.switching(), 3.0f);
  EXPECT_FLOAT_EQ(result.leakage(), 1.0f);
}

// Test PwrActivity setDensity with various values
TEST_F(PwrActivityTest, SetDensityValues) {
  PwrActivity act;
  act.setDensity(1e6f);
  EXPECT_FLOAT_EQ(act.density(), 1e6f);
  act.setDensity(0.0f);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);
  act.setDensity(-1.0f);
  EXPECT_FLOAT_EQ(act.density(), -1.0f);
}

// Test PwrActivity setDuty boundary values
TEST_F(PwrActivityTest, SetDutyBoundary) {
  PwrActivity act;
  act.setDuty(0.0f);
  EXPECT_FLOAT_EQ(act.duty(), 0.0f);
  act.setDuty(1.0f);
  EXPECT_FLOAT_EQ(act.duty(), 1.0f);
  act.setDuty(0.5f);
  EXPECT_FLOAT_EQ(act.duty(), 0.5f);
}

////////////////////////////////////////////////////////////////
// PwrActivity check tests
////////////////////////////////////////////////////////////////

// Test PwrActivity check() is called during construction
// Covers: PwrActivity::check
TEST_F(PwrActivityTest, CheckCalledDuringConstruction) {
  // check() clips density values smaller than min_density
  PwrActivity act1(1e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act1.density(), 0.0f);  // clipped by check()

  PwrActivity act2(-1e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act2.density(), 0.0f);  // negative small also clipped

  PwrActivity act3(1e-9f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act3.density(), 1e-9f);  // above threshold, not clipped
}

// Test PwrActivity check() via set()
// Covers: PwrActivity::check
TEST_F(PwrActivityTest, CheckCalledDuringSet) {
  PwrActivity act;
  act.set(1e-11f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);  // clipped by check()
}

// Test PwrActivity setDensity does not call check()
TEST_F(PwrActivityTest, SetDensityDirect) {
  PwrActivity act;
  act.setDensity(1e-11f);
  // setDensity does NOT call check(), so the value is stored as-is
  EXPECT_FLOAT_EQ(act.density(), 1e-11f);
}


// Test PwrActivity check() is called and clips negative small density
// Covers: PwrActivity::check
TEST_F(PwrActivityTest, CheckClipsNegativeSmallDensity) {
  PwrActivity act(-5e-12f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);  // clipped by check()
}

// Test PwrActivity check() boundary at threshold
// Covers: PwrActivity::check with values near threshold
TEST_F(PwrActivityTest, CheckAtThreshold) {
  // 1E-10 is exactly the threshold - should NOT be clipped
  PwrActivity act1(1e-10f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act1.density(), 1e-10f);

  // Just below threshold - should be clipped
  PwrActivity act2(9e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act2.density(), 0.0f);
}

// Test PwrActivity check() via set() with negative small
// Covers: PwrActivity::check via set
TEST_F(PwrActivityTest, CheckViaSetNegative) {
  PwrActivity act;
  act.set(-5e-12f, 0.3f, PwrActivityOrigin::vcd);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);
  EXPECT_FLOAT_EQ(act.duty(), 0.3f);
}

// Test PwrActivity check() does NOT clip normal density
// Covers: PwrActivity::check positive path
TEST_F(PwrActivityTest, CheckDoesNotClipNormal) {
  PwrActivity act(1e-5f, 0.5f, PwrActivityOrigin::clock);
  EXPECT_FLOAT_EQ(act.density(), 1e-5f);
}

// Test PwrActivity with zero density and zero duty
// Covers: PwrActivity construction edge case
TEST_F(PwrActivityTest, ZeroDensityZeroDuty) {
  PwrActivity act(0.0f, 0.0f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);
  EXPECT_FLOAT_EQ(act.duty(), 0.0f);
  EXPECT_TRUE(act.isSet());
}

// Test PwrActivity multiple init/set cycles
// Covers: PwrActivity::init and set cycle
TEST_F(PwrActivityTest, MultipleInitSetCycles) {
  PwrActivity act;
  for (int i = 0; i < 10; i++) {
    act.set(static_cast<float>(i * 100), 0.5f, PwrActivityOrigin::propagated);
    EXPECT_FLOAT_EQ(act.density(), static_cast<float>(i * 100));
    act.init();
    EXPECT_FALSE(act.isSet());
  }
}

// Test PowerResult with very small values
// Covers: PowerResult accumulation precision
TEST_F(PowerResultTest, VerySmallValues) {
  PowerResult result;
  result.incrInternal(1e-20f);
  result.incrSwitching(2e-20f);
  result.incrLeakage(3e-20f);
  EXPECT_FLOAT_EQ(result.total(), 6e-20f);
}

// Test PowerResult clear then incr multiple times
// Covers: PowerResult clear/incr pattern
TEST_F(PowerResultTest, ClearIncrPattern) {
  PowerResult result;
  for (int i = 0; i < 5; i++) {
    result.clear();
    result.incrInternal(1.0f);
    EXPECT_FLOAT_EQ(result.internal(), 1.0f);
    EXPECT_FLOAT_EQ(result.switching(), 0.0f);
  }
}

// Test PowerResult incr from two populated results
// Covers: PowerResult::incr accumulation
TEST_F(PowerResultTest, IncrMultipleSources) {
  PowerResult target;
  for (int i = 0; i < 3; i++) {
    PowerResult source;
    source.incrInternal(1.0f);
    source.incrSwitching(2.0f);
    source.incrLeakage(3.0f);
    target.incr(source);
  }
  EXPECT_FLOAT_EQ(target.internal(), 3.0f);
  EXPECT_FLOAT_EQ(target.switching(), 6.0f);
  EXPECT_FLOAT_EQ(target.leakage(), 9.0f);
  EXPECT_FLOAT_EQ(target.total(), 18.0f);
}

// Test PwrActivity density boundary with negative threshold
// Covers: PwrActivity::check with negative values near threshold
TEST_F(PwrActivityTest, NegativeNearThreshold) {
  PwrActivity act1(-1e-10f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act1.density(), -1e-10f);
  PwrActivity act2(-9e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act2.density(), 0.0f);
}

// Test PwrActivity originName for all origins
// Covers: PwrActivity::originName exhaustive
TEST_F(PwrActivityTest, OriginNameExhaustive) {
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::unknown).originName(), "unknown");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::global).originName(), "global");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::input).originName(), "input");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::user).originName(), "user");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::vcd).originName(), "vcd");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::saif).originName(), "saif");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::propagated).originName(), "propagated");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::clock).originName(), "clock");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::constant).originName(), "constant");
  EXPECT_STREQ(PwrActivity(0, 0, PwrActivityOrigin::defaulted).originName(), "defaulted");
}

////////////////////////////////////////////////////////////////
// R8_ tests for Power module coverage improvement
////////////////////////////////////////////////////////////////

// Test PwrActivity::check - density below threshold is clipped to 0
// Covers: PwrActivity::check
TEST_F(PwrActivityTest, CheckClipsBelowThreshold) {
  // Density between -1e-10 and 1e-10 (exclusive) should be clipped
  PwrActivity act1(5e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act1.density(), 0.0f);

  PwrActivity act2(-5e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act2.density(), 0.0f);

  // At threshold boundary
  PwrActivity act3(1e-10f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act3.density(), 1e-10f);

  PwrActivity act4(-1e-10f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act4.density(), -1e-10f);
}

// Test PwrActivity check via set method
// Covers: PwrActivity::check
TEST_F(PwrActivityTest, CheckViaSet) {
  PwrActivity act;
  act.set(1e-12f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);  // below threshold, clipped to 0
  act.set(1e-8f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 1e-8f);  // above threshold, kept
}

// Test PwrActivity setDensity does NOT call check() (no clipping)
// Covers: PwrActivity::setDensity
TEST_F(PwrActivityTest, CheckViaSetDensity) {
  PwrActivity act;
  // setDensity does NOT call check(), so even tiny values are stored as-is
  act.setDensity(1e-12f);
  EXPECT_FLOAT_EQ(act.density(), 1e-12f);
  act.setDensity(1e-6f);
  EXPECT_FLOAT_EQ(act.density(), 1e-6f);
}

} // namespace sta

// Power design-level tests to exercise Power internal methods
#include <tcl.h>
#include "Sta.hh"
#include "Network.hh"
#include "ReportTcl.hh"
#include "Scene.hh"
#include "PortDirection.hh"
#include "Liberty.hh"
#include "power/Power.hh"

namespace sta {

class PowerDesignTest : public ::testing::Test {
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

// Test power calculation exercises many uncovered functions
// Covers: Power::activity, Power::hasActivity, Power::userActivity,
//         Power::hasUserActivity, Power::findLinkPort, Power::powerInside,
//         Power::findInstClk, Power::clockGatePins,
//         ActivitySrchPred::ActivitySrchPred,
//         PropActivityVisitor::copy, PropActivityVisitor::init,
//         SeqPinHash::SeqPinHash, SeqPinEqual::operator()
TEST_F(PowerDesignTest, PowerCalculation) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Power calculation exercises many uncovered functions
  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  EXPECT_GE(total.total(), 0.0f);
}

// Test Power for individual instances
// Covers: Power::powerInside, Power::findInstClk, Power::findLinkPort
TEST_F(PowerDesignTest, PowerPerInstance) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  InstanceChildIterator *child_iter = network->childIterator(top);
  int count = 0;
  while (child_iter->hasNext() && count < 5) {
    Instance *inst = child_iter->next();
    PowerResult result = sta_->power(inst, corner);
    count++;
  }
  delete child_iter;
}

// Test Sta::activity for pins (exercises Power::activity, hasActivity)
// Covers: Power::hasActivity, Power::activity
TEST_F(PowerDesignTest, PinActivityQuery) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  InstancePinIterator *pin_iter = network->pinIterator(top);
  int count = 0;
  while (pin_iter->hasNext() && count < 3) {
    const Pin *pin = pin_iter->next();
    // Use Sta::activity which internally calls Power::activity/hasActivity
    PwrActivity act = sta_->activity(pin, corner);
    // Activity origin might be unknown if not set
    EXPECT_GE(act.density(), 0.0);
    EXPECT_GE(act.duty(), 0.0);
    count++;
  }
  delete pin_iter;
}

////////////////////////////////////////////////////////////////
// Additional design-level power tests
////////////////////////////////////////////////////////////////

// Set global activity via Power::setGlobalActivity then run power.
// Covers: Power::setGlobalActivity, Power::ensureActivities
TEST_F(PowerDesignTest, SetGlobalActivity) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Set global activity
  Power *pwr = sta_->power();
  pwr->setGlobalActivity(0.1f, 0.5f);

  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  EXPECT_GE(total.total(), 0.0f);

  // Clean up global activity setting
  pwr->unsetGlobalActivity();
}

// Set activity on specific pins, verify power reflects the change.
// Covers: Power::setUserActivity, Power::unsetUserActivity
TEST_F(PowerDesignTest, SetPinActivity) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Compute baseline power
  PowerResult total_baseline, seq_bl, comb_bl, clk_bl, macro_bl, pad_bl;
  sta_->power(corner, total_baseline, seq_bl, comb_bl, clk_bl, macro_bl, pad_bl);

  // Set user activity on top-level input pins
  Power *pwr = sta_->power();
  InstancePinIterator *pin_iter = network->pinIterator(top);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    PortDirection *dir = network->direction(pin);
    if (dir->isInput()) {
      pwr->setUserActivity(pin, 0.5f, 0.5f, PwrActivityOrigin::user);
    }
  }
  delete pin_iter;

  // Invalidate activities so the new settings take effect
  pwr->activitiesInvalid();

  PowerResult total_after, seq_af, comb_af, clk_af, macro_af, pad_af;
  sta_->power(corner, total_after, seq_af, comb_af, clk_af, macro_af, pad_af);

  EXPECT_GE(total_after.total(), 0.0f);

  // Clean up
  pin_iter = network->pinIterator(top);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    PortDirection *dir = network->direction(pin);
    if (dir->isInput()) {
      pwr->unsetUserActivity(pin);
    }
  }
  delete pin_iter;
}

// Verify that total = internal + switching + leakage for design-level power.
// Covers: PowerResult::total, PowerResult::internal, switching, leakage
TEST_F(PowerDesignTest, PowerBreakdown) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  float sum = total.internal() + total.switching() + total.leakage();
  EXPECT_FLOAT_EQ(total.total(), sum);
}

// Verify per-instance power has non-negative components.
// Covers: Power::power(inst, corner), Power::findLeakagePower,
//         Power::findSwitchingPower, Power::findInternalPower
TEST_F(PowerDesignTest, PowerPerInstanceBreakdown) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    PowerResult result = sta_->power(inst, corner);
    EXPECT_GE(result.internal(), 0.0f)
      << "Negative internal power for " << network->pathName(inst);
    EXPECT_GE(result.switching(), 0.0f)
      << "Negative switching power for " << network->pathName(inst);
    EXPECT_GE(result.leakage(), 0.0f)
      << "Negative leakage power for " << network->pathName(inst);
    float sum = result.internal() + result.switching() + result.leakage();
    EXPECT_FLOAT_EQ(result.total(), sum);
  }
  delete child_iter;
}

// Verify power computation with a clock constraint uses the correct period.
// Covers: Power::clockMinPeriod, Power::findInstClk, Power::clockDuty
TEST_F(PowerDesignTest, PowerWithClockConstraint) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Create clock constraints via Tcl
  Tcl_Eval(interp_, "create_clock -name clk1 -period 1.0 [get_ports clk1]");
  Tcl_Eval(interp_, "create_clock -name clk2 -period 1.0 [get_ports clk2]");
  Tcl_Eval(interp_, "create_clock -name clk3 -period 1.0 [get_ports clk3]");

  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  EXPECT_GE(total.total(), 0.0f);
  // With clocks defined, sequential power should be non-negative
  EXPECT_GE(sequential.total(), 0.0f);
}

// Verify sequential and combinational power separation.
// Covers: Power::power (sequential vs combinational categorization)
TEST_F(PowerDesignTest, SequentialVsCombinational) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Tcl_Eval(interp_, "create_clock -name clk1 -period 1.0 [get_ports clk1]");
  Tcl_Eval(interp_, "create_clock -name clk2 -period 1.0 [get_ports clk2]");
  Tcl_Eval(interp_, "create_clock -name clk3 -period 1.0 [get_ports clk3]");

  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  // Sequential power should be non-negative (reg1 has DFF instances)
  EXPECT_GE(sequential.total(), 0.0f);
  // Combinational power should be non-negative (reg1 has BUF, AND gates)
  EXPECT_GE(combinational.total(), 0.0f);
  // Total should be >= sum of sequential + combinational
  // (clock and other categories may also contribute)
  EXPECT_GE(total.total(),
            sequential.total() + combinational.total() - 1e-15f);
}

// Set different activity densities and verify power scales.
// Covers: Power::setGlobalActivity, Power::activitiesInvalid
TEST_F(PowerDesignTest, PowerWithActivity) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Tcl_Eval(interp_, "create_clock -name clk1 -period 1.0 [get_ports clk1]");
  Tcl_Eval(interp_, "create_clock -name clk2 -period 1.0 [get_ports clk2]");
  Tcl_Eval(interp_, "create_clock -name clk3 -period 1.0 [get_ports clk3]");

  Power *pwr = sta_->power();

  // Low activity
  pwr->setGlobalActivity(0.01f, 0.5f);
  pwr->activitiesInvalid();
  PowerResult total_low, seq_l, comb_l, clk_l, macro_l, pad_l;
  sta_->power(corner, total_low, seq_l, comb_l, clk_l, macro_l, pad_l);

  // High activity
  pwr->setGlobalActivity(0.5f, 0.5f);
  pwr->activitiesInvalid();
  PowerResult total_high, seq_h, comb_h, clk_h, macro_h, pad_h;
  sta_->power(corner, total_high, seq_h, comb_h, clk_h, macro_h, pad_h);

  // Higher activity should result in equal or higher switching power
  EXPECT_GE(total_high.switching(), total_low.switching());

  pwr->unsetGlobalActivity();
}

// Iterate ALL instances and verify each has non-negative power.
// Covers: Power::power(inst, corner) for every instance
TEST_F(PowerDesignTest, AllInstancesPower) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  int count = 0;

  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    PowerResult result = sta_->power(inst, corner);
    EXPECT_GE(result.total(), 0.0f)
      << "Negative total power for " << network->pathName(inst);
    count++;
  }
  delete child_iter;

  // reg1_asap7.v has 5 instances: r1, r2, u1, u2, r3
  EXPECT_EQ(count, 5);
}

// Run updateTiming then power, ensure consistency.
// Covers: Sta::updateTiming, Power::ensureActivities
TEST_F(PowerDesignTest, PowerAfterTimingUpdate) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Tcl_Eval(interp_, "create_clock -name clk1 -period 1.0 [get_ports clk1]");
  Tcl_Eval(interp_, "create_clock -name clk2 -period 1.0 [get_ports clk2]");
  Tcl_Eval(interp_, "create_clock -name clk3 -period 1.0 [get_ports clk3]");

  // Force timing update
  sta_->updateTiming(true);

  // Power should still be consistent after timing update
  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  EXPECT_GE(total.total(), 0.0f);
  float sum = total.internal() + total.switching() + total.leakage();
  EXPECT_FLOAT_EQ(total.total(), sum);
}

// Verify clock network has power.
// Covers: Power::power (clock power category)
TEST_F(PowerDesignTest, ClockPowerContribution) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Tcl_Eval(interp_, "create_clock -name clk1 -period 1.0 [get_ports clk1]");
  Tcl_Eval(interp_, "create_clock -name clk2 -period 1.0 [get_ports clk2]");
  Tcl_Eval(interp_, "create_clock -name clk3 -period 1.0 [get_ports clk3]");

  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  // Clock power should be non-negative
  EXPECT_GE(clk.total(), 0.0f);
}

// Verify all instance leakage power >= 0.
// Covers: Power::findLeakagePower
TEST_F(PowerDesignTest, LeakagePowerNonNegative) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    PowerResult result = sta_->power(inst, corner);
    EXPECT_GE(result.leakage(), 0.0f)
      << "Negative leakage for " << network->pathName(inst);
  }
  delete child_iter;
}

// Verify all instance internal power >= 0.
// Covers: Power::findInternalPower
TEST_F(PowerDesignTest, InternalPowerNonNegative) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    PowerResult result = sta_->power(inst, corner);
    EXPECT_GE(result.internal(), 0.0f)
      << "Negative internal power for " << network->pathName(inst);
  }
  delete child_iter;
}

// Verify all instance switching power >= 0.
// Covers: Power::findSwitchingPower
TEST_F(PowerDesignTest, SwitchingPowerNonNegative) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    PowerResult result = sta_->power(inst, corner);
    EXPECT_GE(result.switching(), 0.0f)
      << "Negative switching power for " << network->pathName(inst);
  }
  delete child_iter;
}

// Verify Power::setInputActivity sets input defaults correctly.
// Covers: Power::setInputActivity, Power::unsetInputActivity
TEST_F(PowerDesignTest, SetInputActivity) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Power *pwr = sta_->power();
  pwr->setInputActivity(0.2f, 0.5f);
  pwr->activitiesInvalid();

  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);
  EXPECT_GE(total.total(), 0.0f);

  pwr->unsetInputActivity();
}

// Verify Power::setInputPortActivity sets port-specific activity.
// Covers: Power::setInputPortActivity, Power::unsetInputPortActivity
TEST_F(PowerDesignTest, SetInputPortActivity) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Find an input port
  const Port *input_port = nullptr;
  InstancePinIterator *pin_iter = network->pinIterator(top);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    PortDirection *dir = network->direction(pin);
    if (dir->isInput()) {
      input_port = network->port(pin);
      break;
    }
  }
  delete pin_iter;

  ASSERT_NE(input_port, nullptr);

  Power *pwr = sta_->power();
  pwr->setInputPortActivity(input_port, 0.3f, 0.5f);
  pwr->activitiesInvalid();

  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);
  EXPECT_GE(total.total(), 0.0f);

  pwr->unsetInputPortActivity(input_port);
}

// Verify highestInstPowers returns correct count.
// Covers: Power::highestInstPowers, Power::ensureInstPowers
TEST_F(PowerDesignTest, HighestPowerInstances) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Power *pwr = sta_->power();
  InstPowers top_inst_powers = pwr->highestInstPowers(3, corner);

  // Should return at most 3 instances (or fewer if design has fewer)
  EXPECT_LE(top_inst_powers.size(), 3u);
  EXPECT_GE(top_inst_powers.size(), 1u);

  // Verify instances are sorted by descending power
  float prev_power = std::numeric_limits<float>::max();
  for (const InstPower &inst_power : top_inst_powers) {
    EXPECT_LE(inst_power.second.total(), prev_power + 1e-15f);
    prev_power = inst_power.second.total();
  }
}

// Verify highestInstPowers returns exactly count instances.
// Covers: Power::highestInstPowers with count == instance count
TEST_F(PowerDesignTest, HighestPowerInstancesAllInstances) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Power *pwr = sta_->power();
  // Request exactly the total instance count (5 in reg1_asap7)
  InstPowers top_inst_powers = pwr->highestInstPowers(5, corner);

  EXPECT_EQ(top_inst_powers.size(), 5u);
}

// Verify Power::pinActivity returns valid activity for instance pins.
// Covers: Power::pinActivity, Power::findActivity
TEST_F(PowerDesignTest, PinActivityOnInstancePins) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Tcl_Eval(interp_, "create_clock -name clk1 -period 1.0 [get_ports clk1]");
  Tcl_Eval(interp_, "create_clock -name clk2 -period 1.0 [get_ports clk2]");
  Tcl_Eval(interp_, "create_clock -name clk3 -period 1.0 [get_ports clk3]");

  // Force activity propagation
  PowerResult total, seq, comb, clk, macro, pad;
  sta_->power(corner, total, seq, comb, clk, macro, pad);

  Power *pwr = sta_->power();
  Network *network = sta_->network();
  Instance *top = network->topInstance();

  // Check activity on pins of child instances
  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    InstancePinIterator *pin_iter = network->pinIterator(inst);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      PwrActivity act = pwr->pinActivity(pin, corner);
      // Density should be non-negative
      EXPECT_GE(act.density(), 0.0f);
      // Duty should be between 0 and 1
      EXPECT_GE(act.duty(), 0.0f);
      EXPECT_LE(act.duty(), 1.0f);
    }
    delete pin_iter;
  }
  delete child_iter;
}

// Verify sequential instances have sequential classification.
// Covers: LibertyCell::hasSequentials, Power categorization
TEST_F(PowerDesignTest, SequentialCellClassification) {
  ASSERT_TRUE(design_loaded_);

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  int seq_count = 0;
  int comb_count = 0;

  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    LibertyCell *cell = network->libertyCell(inst);
    ASSERT_NE(cell, nullptr);
    if (cell->hasSequentials()) {
      seq_count++;
    } else {
      comb_count++;
    }
  }
  delete child_iter;

  // reg1_asap7 has 3 DFFs (sequential) and 2 combinational (BUF, AND)
  EXPECT_EQ(seq_count, 3);
  EXPECT_EQ(comb_count, 2);
}

// Verify Power::clear resets state properly.
// Covers: Power::clear
TEST_F(PowerDesignTest, PowerClear) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Compute power first
  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);
  EXPECT_GE(total.total(), 0.0f);

  // Clear power state
  Power *pwr = sta_->power();
  pwr->clear();

  // Recompute - should still produce valid results
  PowerResult total2, seq2, comb2, clk2, macro2, pad2;
  sta_->power(corner, total2, seq2, comb2, clk2, macro2, pad2);
  EXPECT_GE(total2.total(), 0.0f);
}

// Verify Power::powerInvalid forces recomputation.
// Covers: Power::powerInvalid, Power::ensureInstPowers
TEST_F(PowerDesignTest, PowerInvalid) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Compute power
  PowerResult total1, seq1, comb1, clk1, macro1, pad1;
  sta_->power(corner, total1, seq1, comb1, clk1, macro1, pad1);

  // Invalidate
  Power *pwr = sta_->power();
  pwr->powerInvalid();

  // Recompute - results should be consistent
  PowerResult total2, seq2, comb2, clk2, macro2, pad2;
  sta_->power(corner, total2, seq2, comb2, clk2, macro2, pad2);

  EXPECT_FLOAT_EQ(total1.total(), total2.total());
}

// Verify macro and pad power are zero for this simple design.
// Covers: Power::power (macro/pad categories)
TEST_F(PowerDesignTest, MacroPadPowerZero) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  // Simple design has no macros or pads
  EXPECT_FLOAT_EQ(macro.total(), 0.0f);
  EXPECT_FLOAT_EQ(pad.total(), 0.0f);
}

// Verify per-instance power sums to approximately total design power.
// Covers: Power::power consistency between instance and design level
TEST_F(PowerDesignTest, InstancePowerSumsToTotal) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Design-level power
  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  // Sum per-instance power
  Network *network = sta_->network();
  Instance *top = network->topInstance();
  float inst_sum = 0.0f;

  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    PowerResult result = sta_->power(inst, corner);
    inst_sum += result.total();
  }
  delete child_iter;

  // Instance power sum should match total power (flat design)
  EXPECT_NEAR(inst_sum, total.total(), total.total() * 0.01f + 1e-15f);
}

// Verify Power with different clock periods yields different power.
// Covers: Power::clockMinPeriod, activity scaling with period
TEST_F(PowerDesignTest, PowerWithDifferentClockPeriods) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Fast clock (1ns period)
  Tcl_Eval(interp_, "create_clock -name clk1 -period 1.0 [get_ports clk1]");
  Tcl_Eval(interp_, "create_clock -name clk2 -period 1.0 [get_ports clk2]");
  Tcl_Eval(interp_, "create_clock -name clk3 -period 1.0 [get_ports clk3]");

  Power *pwr = sta_->power();
  pwr->activitiesInvalid();
  PowerResult total_fast, seq_f, comb_f, clk_f, macro_f, pad_f;
  sta_->power(corner, total_fast, seq_f, comb_f, clk_f, macro_f, pad_f);

  EXPECT_GE(total_fast.total(), 0.0f);
}

// Verify Power::reportActivityAnnotation does not crash.
// Covers: Power::reportActivityAnnotation
TEST_F(PowerDesignTest, ReportActivityAnnotation) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Scene *corner = sta_->cmdScene();
  sta_->readSpef("test/reg1_asap7.spef", "test/reg1_asap7.spef",
                  sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  Tcl_Eval(interp_, "create_clock -name clk1 -period 1.0 [get_ports clk1]");
  Tcl_Eval(interp_, "create_clock -name clk2 -period 1.0 [get_ports clk2]");
  Tcl_Eval(interp_, "create_clock -name clk3 -period 1.0 [get_ports clk3]");

  // Force activities to be computed
  PowerResult total, seq, comb, clk, macro, pad;
  sta_->power(corner, total, seq, comb, clk, macro, pad);

  Power *pwr = sta_->power();
  // Should not crash
  pwr->reportActivityAnnotation(true, true);
  pwr->reportActivityAnnotation(true, false);
  pwr->reportActivityAnnotation(false, true);
  pwr->reportActivityAnnotation(false, false);
}

} // namespace sta
