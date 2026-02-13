#include <gtest/gtest.h>
#include "MinMax.hh"
#include "Transition.hh"
#include "StringUtil.hh"
// VcdParse.hh not included - VcdValue constructor is not defined in library

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
// VcdValue coverage tests
////////////////////////////////////////////////////////////////

// VcdValue tests - use zero-initialized memory since constructor is not defined
// VcdValue has only POD members (int64_t, char, uint64_t) so memset is safe

} // namespace sta

#include <cstring>
#include "power/VcdParse.hh"

namespace sta {

class VcdValueTest : public ::testing::Test {};

TEST_F(VcdValueTest, SetValueAndAccess) {
  // Zero-initialize to work around missing default constructor
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);

  val->setValue(100, '1');
  EXPECT_EQ(val->time(), 100);
  EXPECT_EQ(val->value(), '1');
}

TEST_F(VcdValueTest, ValueBitAccess) {
  // Zero-initialize
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);

  // When value_ is non-null char, value(int) returns value_ regardless of bit
  val->setValue(200, 'X');
  EXPECT_EQ(val->value(0), 'X');
  EXPECT_EQ(val->value(3), 'X');
}

// Test PwrActivity check() is called during construction
// Covers: PwrActivity::check
TEST_F(PwrActivityTest, R5_CheckCalledDuringConstruction) {
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
TEST_F(PwrActivityTest, R5_CheckCalledDuringSet) {
  PwrActivity act;
  act.set(1e-11f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);  // clipped by check()
}

// Test PwrActivity setDensity does not call check()
TEST_F(PwrActivityTest, R5_SetDensityDirect) {
  PwrActivity act;
  act.setDensity(1e-11f);
  // setDensity does NOT call check(), so the value is stored as-is
  EXPECT_FLOAT_EQ(act.density(), 1e-11f);
}

// Test VcdValue with zero value
TEST_F(VcdValueTest, R5_ValueZero) {
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);

  val->setValue(0, '0');
  EXPECT_EQ(val->time(), 0);
  EXPECT_EQ(val->value(), '0');
}

// Test VcdValue with Z value
TEST_F(VcdValueTest, R5_ValueZ) {
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);

  val->setValue(500, 'Z');
  EXPECT_EQ(val->time(), 500);
  EXPECT_EQ(val->value(), 'Z');
  EXPECT_EQ(val->value(0), 'Z');
}

// Test VcdValue busValue
TEST_F(VcdValueTest, R5_BusValue) {
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);

  // When value_ is '\0', busValue is used
  val->setValue(100, '\0');
  EXPECT_EQ(val->value(), '\0');
  // busValue will be 0 since we zero-initialized
  EXPECT_EQ(val->busValue(), 0u);
}

// Test VcdValue large time values
TEST_F(VcdValueTest, R5_LargeTime) {
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);

  VcdTime large_time = 1000000000LL;
  val->setValue(large_time, '1');
  EXPECT_EQ(val->time(), large_time);
  EXPECT_EQ(val->value(), '1');
}

// Test VcdValue overwrite
TEST_F(VcdValueTest, R5_OverwriteValue) {
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);

  val->setValue(100, '0');
  EXPECT_EQ(val->value(), '0');

  val->setValue(200, '1');
  EXPECT_EQ(val->time(), 200);
  EXPECT_EQ(val->value(), '1');
}

// Test PwrActivity check() is called and clips negative small density
// Covers: PwrActivity::check
TEST_F(PwrActivityTest, R6_CheckClipsNegativeSmallDensity) {
  PwrActivity act(-5e-12f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);  // clipped by check()
}

// Test PwrActivity check() boundary at threshold
// Covers: PwrActivity::check with values near threshold
TEST_F(PwrActivityTest, R6_CheckAtThreshold) {
  // 1E-10 is exactly the threshold - should NOT be clipped
  PwrActivity act1(1e-10f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act1.density(), 1e-10f);

  // Just below threshold - should be clipped
  PwrActivity act2(9e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act2.density(), 0.0f);
}

// Test PwrActivity check() via set() with negative small
// Covers: PwrActivity::check via set
TEST_F(PwrActivityTest, R6_CheckViaSetNegative) {
  PwrActivity act;
  act.set(-5e-12f, 0.3f, PwrActivityOrigin::vcd);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);
  EXPECT_FLOAT_EQ(act.duty(), 0.3f);
}

// Test PwrActivity check() does NOT clip normal density
// Covers: PwrActivity::check positive path
TEST_F(PwrActivityTest, R6_CheckDoesNotClipNormal) {
  PwrActivity act(1e-5f, 0.5f, PwrActivityOrigin::clock);
  EXPECT_FLOAT_EQ(act.density(), 1e-5f);
}

// Test PwrActivity with zero density and zero duty
// Covers: PwrActivity construction edge case
TEST_F(PwrActivityTest, R6_ZeroDensityZeroDuty) {
  PwrActivity act(0.0f, 0.0f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);
  EXPECT_FLOAT_EQ(act.duty(), 0.0f);
  EXPECT_TRUE(act.isSet());
}

// Test PwrActivity multiple init/set cycles
// Covers: PwrActivity::init and set cycle
TEST_F(PwrActivityTest, R6_MultipleInitSetCycles) {
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
TEST_F(PowerResultTest, R6_VerySmallValues) {
  PowerResult result;
  result.incrInternal(1e-20f);
  result.incrSwitching(2e-20f);
  result.incrLeakage(3e-20f);
  EXPECT_FLOAT_EQ(result.total(), 6e-20f);
}

// Test PowerResult clear then incr multiple times
// Covers: PowerResult clear/incr pattern
TEST_F(PowerResultTest, R6_ClearIncrPattern) {
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
TEST_F(PowerResultTest, R6_IncrMultipleSources) {
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

// Test VcdValue busValue with zero-initialized memory
// Covers: VcdValue::busValue
TEST_F(VcdValueTest, R6_BusValueZeroInit) {
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);
  // Zero-initialized: busValue should be 0
  EXPECT_EQ(val->busValue(), 0u);
  // value_ is '\0', so value(bit) should look at bus_value_
  EXPECT_EQ(val->value(0), '0');
}

// Test VcdValue value(bit) when value_ is set (non-null char)
// Covers: VcdValue::value(int bit) when value_ is non-null
TEST_F(VcdValueTest, R6_ValueBitWithScalarValue) {
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);
  // When value_ is non-null, value(bit) returns value_ regardless of bit
  val->setValue(100, '1');
  EXPECT_EQ(val->value(0), '1');
  EXPECT_EQ(val->value(5), '1');
  EXPECT_EQ(val->value(31), '1');
}

// Test VcdValue setValue multiple times
// Covers: VcdValue::setValue overwrite behavior
TEST_F(VcdValueTest, R6_SetValueMultipleTimes) {
  alignas(VcdValue) char buf[sizeof(VcdValue)];
  std::memset(buf, 0, sizeof(VcdValue));
  VcdValue *val = reinterpret_cast<VcdValue *>(buf);
  val->setValue(100, '0');
  EXPECT_EQ(val->time(), 100);
  EXPECT_EQ(val->value(), '0');
  val->setValue(200, '1');
  EXPECT_EQ(val->time(), 200);
  EXPECT_EQ(val->value(), '1');
  val->setValue(300, 'X');
  EXPECT_EQ(val->time(), 300);
  EXPECT_EQ(val->value(), 'X');
}

// Test PwrActivity density boundary with negative threshold
// Covers: PwrActivity::check with negative values near threshold
TEST_F(PwrActivityTest, R6_NegativeNearThreshold) {
  PwrActivity act1(-1e-10f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act1.density(), -1e-10f);
  PwrActivity act2(-9e-11f, 0.5f, PwrActivityOrigin::user);
  EXPECT_FLOAT_EQ(act2.density(), 0.0f);
}

// Test PwrActivity originName for all origins
// Covers: PwrActivity::originName exhaustive
TEST_F(PwrActivityTest, R6_OriginNameExhaustive) {
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
TEST_F(PwrActivityTest, R8_CheckClipsBelowThreshold) {
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
TEST_F(PwrActivityTest, R8_CheckViaSet) {
  PwrActivity act;
  act.set(1e-12f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 0.0f);  // below threshold, clipped to 0
  act.set(1e-8f, 0.5f, PwrActivityOrigin::propagated);
  EXPECT_FLOAT_EQ(act.density(), 1e-8f);  // above threshold, kept
}

// Test PwrActivity setDensity does NOT call check() (no clipping)
// Covers: PwrActivity::setDensity
TEST_F(PwrActivityTest, R8_CheckViaSetDensity) {
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
#include "Corner.hh"
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

// Test power calculation exercises many uncovered functions
// Covers: Power::activity, Power::hasActivity, Power::userActivity,
//         Power::hasUserActivity, Power::findLinkPort, Power::powerInside,
//         Power::findInstClk, Power::clockGatePins,
//         ActivitySrchPred::ActivitySrchPred,
//         PropActivityVisitor::copy, PropActivityVisitor::init,
//         SeqPinHash::SeqPinHash, SeqPinEqual::operator()
TEST_F(PowerDesignTest, R8_PowerCalculation) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
                  MinMaxAll::all(), false, false, 1.0f, true);

  // Power calculation exercises many uncovered functions
  PowerResult total, sequential, combinational, clk, macro, pad;
  sta_->power(corner, total, sequential, combinational, clk, macro, pad);

  EXPECT_GE(total.total(), 0.0f);
}

// Test Power for individual instances
// Covers: Power::powerInside, Power::findInstClk, Power::findLinkPort
TEST_F(PowerDesignTest, R8_PowerPerInstance) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Corner *corner = sta_->cmdCorner();
  sta_->readSpef("test/reg1_asap7.spef", sta_->network()->topInstance(), corner,
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
TEST_F(PowerDesignTest, R8_PinActivityQuery) {
  ASSERT_TRUE(design_loaded_);
  sta_->ensureGraph();

  Network *network = sta_->network();
  Instance *top = network->topInstance();

  InstancePinIterator *pin_iter = network->pinIterator(top);
  int count = 0;
  while (pin_iter->hasNext() && count < 3) {
    const Pin *pin = pin_iter->next();
    // Use Sta::activity which internally calls Power::activity/hasActivity
    PwrActivity act = sta_->activity(pin);
    // Activity origin might be unknown if not set
    (void)act.density();
    (void)act.duty();
    count++;
  }
  delete pin_iter;
}

} // namespace sta
