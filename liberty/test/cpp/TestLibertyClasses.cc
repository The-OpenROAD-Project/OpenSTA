#include <gtest/gtest.h>
#include <string>
#include <cmath>
#include <atomic>
#include <unistd.h>
#include "Units.hh"
#include "TimingRole.hh"
#include "MinMax.hh"
#include "Wireload.hh"
#include "FuncExpr.hh"
#include "TableModel.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "InternalPower.hh"
#include "LinearModel.hh"
#include "Transition.hh"
#include "RiseFallValues.hh"
#include "PortDirection.hh"
#include "StringUtil.hh"
#include "liberty/LibertyParser.hh"
#include "liberty/LibertyBuilder.hh"
#include "ReportStd.hh"
#include "liberty/LibertyReaderPvt.hh"

namespace sta {


// Test Unit class
class UnitTest : public ::testing::Test {
protected:
  void SetUp() override {}
};

TEST_F(UnitTest, DefaultConstructor) {
  Unit unit("s");
  // Default scale is 1.0
  EXPECT_FLOAT_EQ(unit.scale(), 1.0f);
  EXPECT_EQ(unit.suffix(), "s");
}

TEST_F(UnitTest, ParameterizedConstructor) {
  Unit unit(1e-9f, "s", 3);
  EXPECT_FLOAT_EQ(unit.scale(), 1e-9f);
  EXPECT_EQ(unit.suffix(), "s");
  EXPECT_EQ(unit.digits(), 3);
}

TEST_F(UnitTest, StaToUser) {
  // 1ns scale: internal 1e-9 -> user 1.0
  Unit unit(1e-9f, "s", 3);
  double result = unit.staToUser(1e-9);
  EXPECT_NEAR(result, 1.0, 1e-6);
}

TEST_F(UnitTest, UserToSta) {
  Unit unit(1e-9f, "s", 3);
  double result = unit.userToSta(1.0);
  EXPECT_NEAR(result, 1e-9, 1e-12);
}

TEST_F(UnitTest, AsString) {
  Unit unit(1e-9f, "s", 3);
  const char *str = unit.asString(1e-9f);
  EXPECT_NE(str, nullptr);
  // Should produce something like "1.000"
}

TEST_F(UnitTest, SetScale) {
  Unit unit("s");
  unit.setScale(1e-12f);
  EXPECT_FLOAT_EQ(unit.scale(), 1e-12f);
}

TEST_F(UnitTest, SetDigits) {
  Unit unit(1.0f, "V", 2);
  unit.setDigits(4);
  EXPECT_EQ(unit.digits(), 4);
}

// Test Units class
class UnitsTest : public ::testing::Test {
protected:
  Units units;
};

TEST_F(UnitsTest, TimeUnit) {
  Unit *time = units.timeUnit();
  EXPECT_NE(time, nullptr);
  EXPECT_EQ(time->suffix(), "s");
}

TEST_F(UnitsTest, CapacitanceUnit) {
  Unit *cap = units.capacitanceUnit();
  EXPECT_NE(cap, nullptr);
}

TEST_F(UnitsTest, FindTime) {
  Unit *found = units.find("time");
  EXPECT_NE(found, nullptr);
}

TEST_F(UnitsTest, FindCapacitance) {
  Unit *found = units.find("capacitance");
  EXPECT_NE(found, nullptr);
}

TEST_F(UnitsTest, FindVoltage) {
  Unit *found = units.find("voltage");
  EXPECT_NE(found, nullptr);
}

TEST_F(UnitsTest, FindResistance) {
  Unit *found = units.find("resistance");
  EXPECT_NE(found, nullptr);
}

TEST_F(UnitsTest, FindInvalid) {
  Unit *found = units.find("invalid_unit");
  EXPECT_EQ(found, nullptr);
}

// Test TimingRole singletons
class TimingRoleTest : public ::testing::Test {};

TEST_F(TimingRoleTest, WireSingleton) {
  const TimingRole *wire = TimingRole::wire();
  EXPECT_NE(wire, nullptr);
  EXPECT_EQ(wire->to_string(), "wire");
}

TEST_F(TimingRoleTest, SetupSingleton) {
  const TimingRole *setup = TimingRole::setup();
  EXPECT_NE(setup, nullptr);
  EXPECT_TRUE(setup->isTimingCheck());
}

TEST_F(TimingRoleTest, HoldSingleton) {
  const TimingRole *hold = TimingRole::hold();
  EXPECT_NE(hold, nullptr);
  EXPECT_TRUE(hold->isTimingCheck());
}

TEST_F(TimingRoleTest, CombinationalSingleton) {
  const TimingRole *comb = TimingRole::combinational();
  EXPECT_NE(comb, nullptr);
  EXPECT_FALSE(comb->isTimingCheck());
}

TEST_F(TimingRoleTest, FindByName) {
  const TimingRole *setup = TimingRole::find("setup");
  EXPECT_NE(setup, nullptr);
  EXPECT_EQ(setup, TimingRole::setup());
}

TEST_F(TimingRoleTest, FindInvalid) {
  const TimingRole *invalid = TimingRole::find("nonexistent");
  EXPECT_EQ(invalid, nullptr);
}

TEST_F(TimingRoleTest, RegClkToQ) {
  const TimingRole *role = TimingRole::regClkToQ();
  EXPECT_NE(role, nullptr);
  EXPECT_FALSE(role->isTimingCheck());
}

TEST_F(TimingRoleTest, IsWire) {
  EXPECT_TRUE(TimingRole::wire()->isWire());
  EXPECT_FALSE(TimingRole::setup()->isWire());
}

////////////////////////////////////////////////////////////////
// Wireload string conversion tests - covers wireloadTreeString,
// stringWireloadTree, wireloadModeString, stringWireloadMode
////////////////////////////////////////////////////////////////

TEST(WireloadStringTest, WireloadTreeToString) {
  EXPECT_STREQ(wireloadTreeString(WireloadTree::worst_case), "worst_case_tree");
  EXPECT_STREQ(wireloadTreeString(WireloadTree::best_case), "best_case_tree");
  EXPECT_STREQ(wireloadTreeString(WireloadTree::balanced), "balanced_tree");
  EXPECT_STREQ(wireloadTreeString(WireloadTree::unknown), "unknown");
}

TEST(WireloadStringTest, StringToWireloadTree) {
  EXPECT_EQ(stringWireloadTree("worst_case_tree"), WireloadTree::worst_case);
  EXPECT_EQ(stringWireloadTree("best_case_tree"), WireloadTree::best_case);
  EXPECT_EQ(stringWireloadTree("balanced_tree"), WireloadTree::balanced);
  EXPECT_EQ(stringWireloadTree("something_else"), WireloadTree::unknown);
}

TEST(WireloadStringTest, WireloadModeToString) {
  EXPECT_STREQ(wireloadModeString(WireloadMode::top), "top");
  EXPECT_STREQ(wireloadModeString(WireloadMode::enclosed), "enclosed");
  EXPECT_STREQ(wireloadModeString(WireloadMode::segmented), "segmented");
  EXPECT_STREQ(wireloadModeString(WireloadMode::unknown), "unknown");
}

TEST(WireloadStringTest, StringToWireloadMode) {
  EXPECT_EQ(stringWireloadMode("top"), WireloadMode::top);
  EXPECT_EQ(stringWireloadMode("enclosed"), WireloadMode::enclosed);
  EXPECT_EQ(stringWireloadMode("segmented"), WireloadMode::segmented);
  EXPECT_EQ(stringWireloadMode("something_else"), WireloadMode::unknown);
}

////////////////////////////////////////////////////////////////
// FuncExpr tests - covers constructors, operators, to_string,
// equiv, less, hasPort, copy, deleteSubexprs
////////////////////////////////////////////////////////////////

TEST(FuncExprTest, MakeZero) {
  FuncExpr *zero = FuncExpr::makeZero();
  EXPECT_NE(zero, nullptr);
  EXPECT_EQ(zero->op(), FuncExpr::Op::zero);
  EXPECT_EQ(zero->left(), nullptr);
  EXPECT_EQ(zero->right(), nullptr);
  EXPECT_EQ(zero->port(), nullptr);
  EXPECT_EQ(zero->to_string(), "0");
  delete zero;
}

TEST(FuncExprTest, MakeOne) {
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_NE(one, nullptr);
  EXPECT_EQ(one->op(), FuncExpr::Op::one);
  EXPECT_EQ(one->to_string(), "1");
  delete one;
}

TEST(FuncExprTest, MakeNot) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  EXPECT_NE(not_one, nullptr);
  EXPECT_EQ(not_one->op(), FuncExpr::Op::not_);
  EXPECT_EQ(not_one->left(), one);
  EXPECT_EQ(not_one->right(), nullptr);
  EXPECT_EQ(not_one->to_string(), "!1");
  delete not_one;
}

TEST(FuncExprTest, MakeAnd) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *and_expr = FuncExpr::makeAnd(zero, one);
  EXPECT_NE(and_expr, nullptr);
  EXPECT_EQ(and_expr->op(), FuncExpr::Op::and_);
  EXPECT_EQ(and_expr->left(), zero);
  EXPECT_EQ(and_expr->right(), one);
  std::string str = and_expr->to_string();
  EXPECT_EQ(str, "0*1");
  delete and_expr;
}

TEST(FuncExprTest, MakeOr) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *or_expr = FuncExpr::makeOr(zero, one);
  EXPECT_NE(or_expr, nullptr);
  EXPECT_EQ(or_expr->op(), FuncExpr::Op::or_);
  std::string str = or_expr->to_string();
  EXPECT_EQ(str, "0+1");
  delete or_expr;
}

TEST(FuncExprTest, MakeXor) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *xor_expr = FuncExpr::makeXor(zero, one);
  EXPECT_NE(xor_expr, nullptr);
  EXPECT_EQ(xor_expr->op(), FuncExpr::Op::xor_);
  std::string str = xor_expr->to_string();
  EXPECT_EQ(str, "0^1");
  delete xor_expr;
}

TEST(FuncExprTest, Copy) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  FuncExpr *copy = not_one->copy();
  EXPECT_NE(copy, nullptr);
  EXPECT_EQ(copy->op(), FuncExpr::Op::not_);
  EXPECT_NE(copy, not_one);
  EXPECT_NE(copy->left(), one);  // should be deep copy
  EXPECT_EQ(copy->left()->op(), FuncExpr::Op::one);
  delete not_one;
  delete copy;
}

TEST(FuncExprTest, EquivBothNull) {
  EXPECT_TRUE(FuncExpr::equiv(nullptr, nullptr));
}

TEST(FuncExprTest, EquivOneNull) {
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_FALSE(FuncExpr::equiv(one, nullptr));
  EXPECT_FALSE(FuncExpr::equiv(nullptr, one));
  delete one;
}

TEST(FuncExprTest, EquivSameOp) {
  FuncExpr *one1 = FuncExpr::makeOne();
  FuncExpr *one2 = FuncExpr::makeOne();
  // Both op_one, same structure - equiv checks sub-expressions
  // For op_one, they are equivalent since no sub-expressions exist.
  // Actually op_one falls in "default" which checks left/right
  EXPECT_TRUE(FuncExpr::equiv(one1, one2));
  delete one1;
  delete one2;
}

TEST(FuncExprTest, EquivDifferentOp) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  EXPECT_FALSE(FuncExpr::equiv(one, zero));
  delete one;
  delete zero;
}

TEST(FuncExprTest, EquivNotExprs) {
  FuncExpr *one1 = FuncExpr::makeOne();
  FuncExpr *not1 = FuncExpr::makeNot(one1);
  FuncExpr *one2 = FuncExpr::makeOne();
  FuncExpr *not2 = FuncExpr::makeNot(one2);
  EXPECT_TRUE(FuncExpr::equiv(not1, not2));
  delete not1;
  delete not2;
}

TEST(FuncExprTest, LessBothNull) {
  EXPECT_FALSE(FuncExpr::less(nullptr, nullptr));
}

TEST(FuncExprTest, LessOneNull) {
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_TRUE(FuncExpr::less(nullptr, one));
  EXPECT_FALSE(FuncExpr::less(one, nullptr));
  delete one;
}

TEST(FuncExprTest, LessDifferentOps) {
  // op_not(1) < op_or is based on enum ordering
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  FuncExpr *zero1 = FuncExpr::makeZero();
  FuncExpr *zero2 = FuncExpr::makeZero();
  FuncExpr *or_expr = FuncExpr::makeOr(zero1, zero2);
  // op_not=1, op_or=2, so not_one < or_expr
  EXPECT_TRUE(FuncExpr::less(not_one, or_expr));
  EXPECT_FALSE(FuncExpr::less(or_expr, not_one));
  delete not_one;
  delete or_expr;
}

TEST(FuncExprTest, HasPortNoPort) {
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_FALSE(one->hasPort(nullptr));
  delete one;
}

TEST(FuncExprTest, HasPortZero) {
  FuncExpr *zero = FuncExpr::makeZero();
  EXPECT_FALSE(zero->hasPort(nullptr));
  delete zero;
}

TEST(FuncExprTest, HasPortNot) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  EXPECT_FALSE(not_one->hasPort(nullptr));
  delete not_one;
}

TEST(FuncExprTest, HasPortAndOrXor) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *and_expr = FuncExpr::makeAnd(one, zero);
  EXPECT_FALSE(and_expr->hasPort(nullptr));
  delete and_expr;
}

TEST(FuncExprTest, InvertDoubleNegation) {
  // invert() on a NOT expression should unwrap it
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  FuncExpr *result = not_one->invert();
  // Should return 'one' directly and delete the not wrapper
  EXPECT_EQ(result->op(), FuncExpr::Op::one);
  delete result;
}

TEST(FuncExprTest, InvertNonNot) {
  // invert() on non-NOT expression should create NOT wrapper
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *result = one->invert();
  EXPECT_EQ(result->op(), FuncExpr::Op::not_);
  delete result;
}

TEST(FuncExprTest, PortTimingSenseOne) {
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_EQ(one->portTimingSense(nullptr), TimingSense::none);
  delete one;
}

TEST(FuncExprTest, PortTimingSenseZero) {
  FuncExpr *zero = FuncExpr::makeZero();
  EXPECT_EQ(zero->portTimingSense(nullptr), TimingSense::none);
  delete zero;
}

TEST(FuncExprTest, PortTimingSenseNotOfOne) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  // not of constant -> none sense
  EXPECT_EQ(not_one->portTimingSense(nullptr), TimingSense::none);
  delete not_one;
}

TEST(FuncExprTest, PortTimingSenseAndBothNone) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *and_expr = FuncExpr::makeAnd(one, zero);
  // Both have none sense for nullptr port -> returns none
  EXPECT_EQ(and_expr->portTimingSense(nullptr), TimingSense::none);
  delete and_expr;
}

TEST(FuncExprTest, PortTimingSenseXorNone) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *xor_expr = FuncExpr::makeXor(one, zero);
  // XOR with none senses should return unknown
  TimingSense sense = xor_expr->portTimingSense(nullptr);
  // Both children return none -> falls to else -> unknown
  EXPECT_EQ(sense, TimingSense::unknown);
  delete xor_expr;
}

TEST(FuncExprTest, CheckSizeOne) {
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_FALSE(one->checkSize(1));
  EXPECT_FALSE(one->checkSize(4));
  delete one;
}

TEST(FuncExprTest, CheckSizeZero) {
  FuncExpr *zero = FuncExpr::makeZero();
  EXPECT_FALSE(zero->checkSize(1));
  delete zero;
}

TEST(FuncExprTest, CheckSizeNot) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  EXPECT_FALSE(not_one->checkSize(1));
  delete not_one;
}

TEST(FuncExprTest, CheckSizeAndOrXor) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *and_expr = FuncExpr::makeAnd(one, zero);
  EXPECT_FALSE(and_expr->checkSize(1));
  delete and_expr;
}

TEST(FuncExprTest, BitSubExprOne) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *sub = one->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::Op::one);  // op_one returns a new makeOne()
  delete sub;
  delete one;
}

TEST(FuncExprTest, BitSubExprZero) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *sub = zero->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::Op::zero);  // op_zero returns a new makeZero()
  delete sub;
  delete zero;
}

TEST(FuncExprTest, BitSubExprNot) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  FuncExpr *sub = not_one->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::Op::not_);
  // Clean up: sub wraps the original one, so delete sub
  delete sub;
  // not_one's left was consumed by bitSubExpr
  delete not_one;
}

TEST(FuncExprTest, BitSubExprOr) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *or_expr = FuncExpr::makeOr(one, zero);
  FuncExpr *sub = or_expr->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::Op::or_);
  delete sub;
  delete or_expr;
}

TEST(FuncExprTest, BitSubExprAnd) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *and_expr = FuncExpr::makeAnd(one, zero);
  FuncExpr *sub = and_expr->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::Op::and_);
  delete sub;
  delete and_expr;
}

TEST(FuncExprTest, BitSubExprXor) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *xor_expr = FuncExpr::makeXor(one, zero);
  FuncExpr *sub = xor_expr->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::Op::xor_);
  delete sub;
  delete xor_expr;
}

TEST(FuncExprTest, LessNotExprs) {
  FuncExpr *one1 = FuncExpr::makeOne();
  FuncExpr *not1 = FuncExpr::makeNot(one1);
  FuncExpr *one2 = FuncExpr::makeOne();
  FuncExpr *not2 = FuncExpr::makeNot(one2);
  // Same structure -> not less
  EXPECT_FALSE(FuncExpr::less(not1, not2));
  EXPECT_FALSE(FuncExpr::less(not2, not1));
  delete not1;
  delete not2;
}

TEST(FuncExprTest, LessDefaultBranch) {
  // Test default branch: and/or/xor with equal left
  FuncExpr *one1 = FuncExpr::makeOne();
  FuncExpr *zero1 = FuncExpr::makeZero();
  FuncExpr *and1 = FuncExpr::makeAnd(one1, zero1);

  FuncExpr *one2 = FuncExpr::makeOne();
  FuncExpr *one3 = FuncExpr::makeOne();
  FuncExpr *and2 = FuncExpr::makeAnd(one2, one3);

  // and1 left=one, and2 left=one -> equal left, compare right
  // and1 right=zero(op_zero=6), and2 right=one(op_one=5), zero > one
  EXPECT_FALSE(FuncExpr::less(and1, and2));
  EXPECT_TRUE(FuncExpr::less(and2, and1));

  delete and1;
  delete and2;
}

////////////////////////////////////////////////////////////////
// TableAxis tests - covers axis construction, findAxisIndex,
// findAxisClosestIndex, inBounds, min, max, variableString
////////////////////////////////////////////////////////////////

class TableAxisTest : public ::testing::Test {
protected:
  TableAxisPtr makeAxis(TableAxisVariable var,
                        std::initializer_list<float> vals) {
    FloatSeq values;
    for (float v : vals)
      values.push_back(std::move(v));
    return std::make_shared<TableAxis>(var, std::move(values));
  }
};

TEST_F(TableAxisTest, BasicProperties) {
  auto axis = makeAxis(TableAxisVariable::total_output_net_capacitance,
                        {1.0f, 2.0f, 3.0f, 4.0f});
  EXPECT_EQ(axis->size(), 4u);
  EXPECT_EQ(axis->variable(), TableAxisVariable::total_output_net_capacitance);
  EXPECT_FLOAT_EQ(axis->axisValue(0), 1.0f);
  EXPECT_FLOAT_EQ(axis->axisValue(3), 4.0f);
}

TEST_F(TableAxisTest, MinMax) {
  auto axis = makeAxis(TableAxisVariable::input_net_transition,
                        {0.5f, 1.0f, 2.0f, 5.0f});
  EXPECT_FLOAT_EQ(axis->min(), 0.5f);
  EXPECT_FLOAT_EQ(axis->max(), 5.0f);
}

TEST_F(TableAxisTest, MinMaxEmpty) {
  auto axis = makeAxis(TableAxisVariable::input_net_transition, {});
  EXPECT_FLOAT_EQ(axis->min(), 0.0f);
  EXPECT_FLOAT_EQ(axis->max(), 0.0f);
}

TEST_F(TableAxisTest, InBounds) {
  auto axis = makeAxis(TableAxisVariable::input_net_transition,
                        {1.0f, 2.0f, 3.0f});
  EXPECT_TRUE(axis->inBounds(1.5f));
  EXPECT_TRUE(axis->inBounds(1.0f));
  EXPECT_TRUE(axis->inBounds(3.0f));
  EXPECT_FALSE(axis->inBounds(0.5f));
  EXPECT_FALSE(axis->inBounds(3.5f));
}

TEST_F(TableAxisTest, InBoundsSingleElement) {
  auto axis = makeAxis(TableAxisVariable::input_net_transition, {1.0f});
  // Single element -> size <= 1 -> false
  EXPECT_FALSE(axis->inBounds(1.0f));
}

TEST_F(TableAxisTest, FindAxisIndex) {
  auto axis = makeAxis(TableAxisVariable::total_output_net_capacitance,
                        {1.0f, 2.0f, 4.0f, 8.0f});
  // value below min -> 0
  EXPECT_EQ(axis->findAxisIndex(0.5f), 0u);
  // value at min -> 0
  EXPECT_EQ(axis->findAxisIndex(1.0f), 0u);
  // value between 1.0 and 2.0 -> 0
  EXPECT_EQ(axis->findAxisIndex(1.5f), 0u);
  // value at second point -> 1
  EXPECT_EQ(axis->findAxisIndex(2.0f), 1u);
  // value between 2.0 and 4.0 -> 1
  EXPECT_EQ(axis->findAxisIndex(3.0f), 1u);
  // value between 4.0 and 8.0 -> 2
  EXPECT_EQ(axis->findAxisIndex(6.0f), 2u);
  // value above max -> size-2 = 2
  EXPECT_EQ(axis->findAxisIndex(10.0f), 2u);
}

TEST_F(TableAxisTest, FindAxisIndexSingleElement) {
  auto axis = makeAxis(TableAxisVariable::total_output_net_capacitance,
                        {5.0f});
  // Single element -> returns 0
  EXPECT_EQ(axis->findAxisIndex(5.0f), 0u);
  EXPECT_EQ(axis->findAxisIndex(1.0f), 0u);
  EXPECT_EQ(axis->findAxisIndex(10.0f), 0u);
}

TEST_F(TableAxisTest, FindAxisClosestIndex) {
  auto axis = makeAxis(TableAxisVariable::total_output_net_capacitance,
                        {1.0f, 3.0f, 5.0f, 7.0f});
  // Below min -> 0
  EXPECT_EQ(axis->findAxisClosestIndex(0.0f), 0u);
  // Above max -> size-1
  EXPECT_EQ(axis->findAxisClosestIndex(10.0f), 3u);
  // Close to 1.0 -> 0
  EXPECT_EQ(axis->findAxisClosestIndex(1.5f), 0u);
  // Close to 3.0 -> 1
  EXPECT_EQ(axis->findAxisClosestIndex(2.8f), 1u);
  // Midpoint: 4.0 between 3.0 and 5.0 -> closer to upper (5.0)
  EXPECT_EQ(axis->findAxisClosestIndex(4.0f), 2u);
  // Exact match
  EXPECT_EQ(axis->findAxisClosestIndex(5.0f), 2u);
}

TEST_F(TableAxisTest, FindAxisIndexExact) {
  auto axis = makeAxis(TableAxisVariable::total_output_net_capacitance,
                        {1.0f, 2.0f, 4.0f, 8.0f});
  size_t index;
  bool exists;

  axis->findAxisIndex(2.0f, index, exists);
  EXPECT_TRUE(exists);
  EXPECT_EQ(index, 1u);

  axis->findAxisIndex(4.0f, index, exists);
  EXPECT_TRUE(exists);
  EXPECT_EQ(index, 2u);

  axis->findAxisIndex(3.0f, index, exists);
  EXPECT_FALSE(exists);

  // Out of range
  axis->findAxisIndex(0.5f, index, exists);
  EXPECT_FALSE(exists);

  axis->findAxisIndex(10.0f, index, exists);
  EXPECT_FALSE(exists);
}

TEST_F(TableAxisTest, VariableString) {
  auto axis = makeAxis(TableAxisVariable::total_output_net_capacitance,
                        {1.0f});
  const char *str = axis->variableString();
  EXPECT_NE(str, nullptr);
  EXPECT_STREQ(str, "total_output_net_capacitance");
}

TEST_F(TableAxisTest, UnitLookup) {
  Units units;
  auto axis = makeAxis(TableAxisVariable::total_output_net_capacitance,
                        {1.0f});
  const Unit *unit = axis->unit(&units);
  EXPECT_NE(unit, nullptr);
}

////////////////////////////////////////////////////////////////
// Table variable string conversion tests
////////////////////////////////////////////////////////////////

TEST(TableVariableTest, StringTableAxisVariable) {
  EXPECT_EQ(stringTableAxisVariable("total_output_net_capacitance"),
            TableAxisVariable::total_output_net_capacitance);
  EXPECT_EQ(stringTableAxisVariable("input_net_transition"),
            TableAxisVariable::input_net_transition);
  EXPECT_EQ(stringTableAxisVariable("input_transition_time"),
            TableAxisVariable::input_transition_time);
  EXPECT_EQ(stringTableAxisVariable("related_pin_transition"),
            TableAxisVariable::related_pin_transition);
  EXPECT_EQ(stringTableAxisVariable("constrained_pin_transition"),
            TableAxisVariable::constrained_pin_transition);
  EXPECT_EQ(stringTableAxisVariable("output_pin_transition"),
            TableAxisVariable::output_pin_transition);
  EXPECT_EQ(stringTableAxisVariable("connect_delay"),
            TableAxisVariable::connect_delay);
  EXPECT_EQ(stringTableAxisVariable("related_out_total_output_net_capacitance"),
            TableAxisVariable::related_out_total_output_net_capacitance);
  EXPECT_EQ(stringTableAxisVariable("time"),
            TableAxisVariable::time);
  EXPECT_EQ(stringTableAxisVariable("iv_output_voltage"),
            TableAxisVariable::iv_output_voltage);
  EXPECT_EQ(stringTableAxisVariable("input_noise_width"),
            TableAxisVariable::input_noise_width);
  EXPECT_EQ(stringTableAxisVariable("input_noise_height"),
            TableAxisVariable::input_noise_height);
  EXPECT_EQ(stringTableAxisVariable("input_voltage"),
            TableAxisVariable::input_voltage);
  EXPECT_EQ(stringTableAxisVariable("output_voltage"),
            TableAxisVariable::output_voltage);
  EXPECT_EQ(stringTableAxisVariable("path_depth"),
            TableAxisVariable::path_depth);
  EXPECT_EQ(stringTableAxisVariable("path_distance"),
            TableAxisVariable::path_distance);
  EXPECT_EQ(stringTableAxisVariable("normalized_voltage"),
            TableAxisVariable::normalized_voltage);
  EXPECT_EQ(stringTableAxisVariable("nonexistent"),
            TableAxisVariable::unknown);
}

TEST(TableVariableTest, TableVariableString) {
  EXPECT_STREQ(tableVariableString(TableAxisVariable::total_output_net_capacitance),
               "total_output_net_capacitance");
  EXPECT_STREQ(tableVariableString(TableAxisVariable::input_net_transition),
               "input_net_transition");
  EXPECT_STREQ(tableVariableString(TableAxisVariable::time),
               "time");
}

TEST(TableVariableTest, TableVariableUnit) {
  Units units;
  // Capacitance variables
  const Unit *u = tableVariableUnit(TableAxisVariable::total_output_net_capacitance,
                                     &units);
  EXPECT_EQ(u, units.capacitanceUnit());

  u = tableVariableUnit(TableAxisVariable::related_out_total_output_net_capacitance,
                        &units);
  EXPECT_EQ(u, units.capacitanceUnit());

  u = tableVariableUnit(TableAxisVariable::equal_or_opposite_output_net_capacitance,
                        &units);
  EXPECT_EQ(u, units.capacitanceUnit());

  // Time variables
  u = tableVariableUnit(TableAxisVariable::input_net_transition, &units);
  EXPECT_EQ(u, units.timeUnit());

  u = tableVariableUnit(TableAxisVariable::input_transition_time, &units);
  EXPECT_EQ(u, units.timeUnit());

  u = tableVariableUnit(TableAxisVariable::related_pin_transition, &units);
  EXPECT_EQ(u, units.timeUnit());

  u = tableVariableUnit(TableAxisVariable::constrained_pin_transition, &units);
  EXPECT_EQ(u, units.timeUnit());

  u = tableVariableUnit(TableAxisVariable::output_pin_transition, &units);
  EXPECT_EQ(u, units.timeUnit());

  u = tableVariableUnit(TableAxisVariable::connect_delay, &units);
  EXPECT_EQ(u, units.timeUnit());

  u = tableVariableUnit(TableAxisVariable::time, &units);
  EXPECT_EQ(u, units.timeUnit());

  u = tableVariableUnit(TableAxisVariable::input_noise_height, &units);
  EXPECT_EQ(u, units.timeUnit());

  // Voltage variables
  u = tableVariableUnit(TableAxisVariable::input_voltage, &units);
  EXPECT_EQ(u, units.voltageUnit());

  u = tableVariableUnit(TableAxisVariable::output_voltage, &units);
  EXPECT_EQ(u, units.voltageUnit());

  u = tableVariableUnit(TableAxisVariable::iv_output_voltage, &units);
  EXPECT_EQ(u, units.voltageUnit());

  u = tableVariableUnit(TableAxisVariable::input_noise_width, &units);
  EXPECT_EQ(u, units.voltageUnit());

  // Distance
  u = tableVariableUnit(TableAxisVariable::path_distance, &units);
  EXPECT_EQ(u, units.distanceUnit());

  // Scalar
  u = tableVariableUnit(TableAxisVariable::path_depth, &units);
  EXPECT_EQ(u, units.scalarUnit());

  u = tableVariableUnit(TableAxisVariable::normalized_voltage, &units);
  EXPECT_EQ(u, units.scalarUnit());

  u = tableVariableUnit(TableAxisVariable::unknown, &units);
  EXPECT_EQ(u, units.scalarUnit());
}

////////////////////////////////////////////////////////////////
// Table0 tests (scalar table)
////////////////////////////////////////////////////////////////

TEST(Table0Test, BasicValue) {
  Table table(42.0f);
  EXPECT_EQ(table.order(), 0);
  EXPECT_FLOAT_EQ(table.value(0, 0, 0), 42.0f);
  EXPECT_FLOAT_EQ(table.findValue(0.0f, 0.0f, 0.0f), 42.0f);
  EXPECT_FLOAT_EQ(table.findValue(1.0f, 2.0f, 3.0f), 42.0f);
  EXPECT_EQ(table.axis1(), nullptr);
  EXPECT_EQ(table.axis2(), nullptr);
  EXPECT_EQ(table.axis3(), nullptr);
}

////////////////////////////////////////////////////////////////
// Table1 tests (1D table)
////////////////////////////////////////////////////////////////

class Table1Test : public ::testing::Test {
protected:
  TableAxisPtr makeAxis(std::initializer_list<float> vals) {
    FloatSeq values;
    for (float v : vals)
      values.push_back(std::move(v));
    return std::make_shared<TableAxis>(
      TableAxisVariable::total_output_net_capacitance, std::move(values));
  }
};

TEST_F(Table1Test, DefaultConstructor) {
  Table table;
  // Unified Table default constructor creates order 0
  EXPECT_EQ(table.order(), 0);
}

TEST_F(Table1Test, ValueLookup) {
  auto axis = makeAxis({1.0f, 2.0f, 4.0f});
  FloatSeq vals;
  vals.push_back(10.0f);
  vals.push_back(20.0f);
  vals.push_back(40.0f);
  Table table(std::move(vals), axis);
  EXPECT_EQ(table.order(), 1);
  EXPECT_FLOAT_EQ(table.value(0), 10.0f);
  EXPECT_FLOAT_EQ(table.value(1), 20.0f);
  EXPECT_FLOAT_EQ(table.value(2), 40.0f);
  EXPECT_NE(table.axis1(), nullptr);
}

TEST_F(Table1Test, FindValueInterpolation) {
  auto axis = makeAxis({0.0f, 1.0f});
  FloatSeq vals;
  vals.push_back(0.0f);
  vals.push_back(10.0f);
  Table table(std::move(vals), axis);
  // Exact match at lower bound
  EXPECT_FLOAT_EQ(table.findValue(0.0f), 0.0f);
  // Midpoint
  EXPECT_NEAR(table.findValue(0.5f), 5.0f, 0.01f);
  // Extrapolation beyond upper bound
  float val = table.findValue(2.0f);
  // Linear extrapolation: 20.0
  EXPECT_NEAR(val, 20.0f, 0.01f);
}

TEST_F(Table1Test, FindValueClip) {
  auto axis = makeAxis({1.0f, 3.0f});
  FloatSeq vals;
  vals.push_back(10.0f);
  vals.push_back(30.0f);
  Table table(std::move(vals), axis);
  // Below range -> clip to 0
  EXPECT_FLOAT_EQ(table.findValueClip(0.0f), 0.0f);
  // In range
  EXPECT_NEAR(table.findValueClip(2.0f), 20.0f, 0.01f);
  // Above range -> clip to last value
  EXPECT_FLOAT_EQ(table.findValueClip(4.0f), 30.0f);
}

TEST_F(Table1Test, FindValueSingleElement) {
  auto axis = makeAxis({5.0f});
  FloatSeq vals;
  vals.push_back(42.0f);
  Table table(std::move(vals), axis);
  // Single element: findValue(float) -> value(size_t(float))
  // Only index 0 is valid, so pass 0.0f which converts to index 0.
  EXPECT_FLOAT_EQ(table.findValue(0.0f), 42.0f);
  // Also test findValueClip for single element
  EXPECT_FLOAT_EQ(table.findValueClip(0.0f), 42.0f);
}

TEST_F(Table1Test, CopyConstructor) {
  auto axis = makeAxis({1.0f, 2.0f});
  FloatSeq vals;
  vals.push_back(10.0f);
  vals.push_back(20.0f);
  Table table(std::move(vals), axis);
  Table copy(table);
  EXPECT_FLOAT_EQ(copy.value(0), 10.0f);
  EXPECT_FLOAT_EQ(copy.value(1), 20.0f);
}

TEST_F(Table1Test, MoveConstructor) {
  auto axis = makeAxis({1.0f, 2.0f});
  FloatSeq vals;
  vals.push_back(10.0f);
  vals.push_back(20.0f);
  Table table(std::move(vals), axis);
  Table moved(std::move(table));
  EXPECT_FLOAT_EQ(moved.value(0), 10.0f);
  EXPECT_FLOAT_EQ(moved.value(1), 20.0f);
}

TEST_F(Table1Test, MoveAssignment) {
  auto axis1 = makeAxis({1.0f, 2.0f});
  FloatSeq vals1;
  vals1.push_back(10.0f);
  vals1.push_back(20.0f);
  Table table1(std::move(vals1), axis1);

  auto axis2 = makeAxis({3.0f, 4.0f});
  FloatSeq vals2;
  vals2.push_back(30.0f);
  vals2.push_back(40.0f);
  Table table2(std::move(vals2), axis2);

  table2 = std::move(table1);
  EXPECT_FLOAT_EQ(table2.value(0), 10.0f);
  EXPECT_FLOAT_EQ(table2.value(1), 20.0f);
}

TEST_F(Table1Test, ValueViaThreeArgs) {
  auto axis = makeAxis({1.0f, 3.0f});
  FloatSeq vals;
  vals.push_back(10.0f);
  vals.push_back(30.0f);
  Table table(std::move(vals), axis);

  // The three-arg findValue just uses the first arg
  EXPECT_NEAR(table.findValue(2.0f, 0.0f, 0.0f), 20.0f, 0.01f);
  EXPECT_NEAR(table.findValue(1.0f, 0.0f, 0.0f), 10.0f, 0.01f);

  // value(idx, idx, idx) also just uses first
  EXPECT_FLOAT_EQ(table.value(0, 0, 0), 10.0f);
  EXPECT_FLOAT_EQ(table.value(1, 0, 0), 30.0f);
}

////////////////////////////////////////////////////////////////
// Table2 tests (2D table)
////////////////////////////////////////////////////////////////

TEST(Table2Test, BilinearInterpolation) {
  FloatSeq axis1_vals;
  axis1_vals.push_back(0.0f);
  axis1_vals.push_back(2.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(axis1_vals));

  FloatSeq axis2_vals;
  axis2_vals.push_back(0.0f);
  axis2_vals.push_back(4.0f);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(axis2_vals));

  FloatTable values;
  FloatSeq row0;
  row0.push_back(0.0f);
  row0.push_back(4.0f);
  values.push_back(std::move(row0));
  FloatSeq row1;
  row1.push_back(2.0f);
  row1.push_back(6.0f);
  values.push_back(std::move(row1));

  Table table(std::move(values), axis1, axis2);
  EXPECT_EQ(table.order(), 2);

  // Corner values
  EXPECT_FLOAT_EQ(table.value(0, 0), 0.0f);
  EXPECT_FLOAT_EQ(table.value(0, 1), 4.0f);
  EXPECT_FLOAT_EQ(table.value(1, 0), 2.0f);
  EXPECT_FLOAT_EQ(table.value(1, 1), 6.0f);

  // Center (bilinear interpolation)
  EXPECT_NEAR(table.findValue(1.0f, 2.0f, 0.0f), 3.0f, 0.01f);
}

TEST(Table2Test, SingleRowInterpolation) {
  FloatSeq axis1_vals;
  axis1_vals.push_back(0.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(axis1_vals));

  FloatSeq axis2_vals;
  axis2_vals.push_back(0.0f);
  axis2_vals.push_back(4.0f);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(axis2_vals));

  FloatTable values;
  FloatSeq row0;
  row0.push_back(10.0f);
  row0.push_back(30.0f);
  values.push_back(std::move(row0));

  Table table(std::move(values), axis1, axis2);
  // Size1==1, so use axis2 only interpolation
  EXPECT_NEAR(table.findValue(0.0f, 2.0f, 0.0f), 20.0f, 0.01f);
}

TEST(Table2Test, SingleColumnInterpolation) {
  FloatSeq axis1_vals;
  axis1_vals.push_back(0.0f);
  axis1_vals.push_back(4.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(axis1_vals));

  FloatSeq axis2_vals;
  axis2_vals.push_back(0.0f);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(axis2_vals));

  FloatTable values;
  FloatSeq row0;
  row0.push_back(10.0f);
  values.push_back(std::move(row0));
  FloatSeq row1;
  row1.push_back(30.0f);
  values.push_back(std::move(row1));

  Table table(std::move(values), axis1, axis2);
  // Size2==1, so use axis1 only interpolation
  EXPECT_NEAR(table.findValue(2.0f, 0.0f, 0.0f), 20.0f, 0.01f);
}

TEST(Table2Test, SingleCellValue) {
  FloatSeq axis1_vals;
  axis1_vals.push_back(0.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(axis1_vals));

  FloatSeq axis2_vals;
  axis2_vals.push_back(0.0f);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(axis2_vals));

  FloatTable values;
  FloatSeq row0;
  row0.push_back(42.0f);
  values.push_back(std::move(row0));

  Table table(std::move(values), axis1, axis2);
  EXPECT_FLOAT_EQ(table.findValue(0.0f, 0.0f, 0.0f), 42.0f);
}

////////////////////////////////////////////////////////////////
// TimingType/TimingSense string conversions
////////////////////////////////////////////////////////////////

TEST(TimingTypeTest, FindTimingType) {
  EXPECT_EQ(findTimingType("combinational"), TimingType::combinational);
  EXPECT_EQ(findTimingType("setup_rising"), TimingType::setup_rising);
  EXPECT_EQ(findTimingType("setup_falling"), TimingType::setup_falling);
  EXPECT_EQ(findTimingType("hold_rising"), TimingType::hold_rising);
  EXPECT_EQ(findTimingType("hold_falling"), TimingType::hold_falling);
  EXPECT_EQ(findTimingType("rising_edge"), TimingType::rising_edge);
  EXPECT_EQ(findTimingType("falling_edge"), TimingType::falling_edge);
  EXPECT_EQ(findTimingType("clear"), TimingType::clear);
  EXPECT_EQ(findTimingType("preset"), TimingType::preset);
  EXPECT_EQ(findTimingType("three_state_enable"), TimingType::three_state_enable);
  EXPECT_EQ(findTimingType("three_state_disable"), TimingType::three_state_disable);
  EXPECT_EQ(findTimingType("recovery_rising"), TimingType::recovery_rising);
  EXPECT_EQ(findTimingType("removal_falling"), TimingType::removal_falling);
  EXPECT_EQ(findTimingType("min_pulse_width"), TimingType::min_pulse_width);
  EXPECT_EQ(findTimingType("minimum_period"), TimingType::minimum_period);
  EXPECT_EQ(findTimingType("nonexistent"), TimingType::unknown);
}

TEST(TimingTypeTest, TimingTypeIsCheck) {
  EXPECT_TRUE(timingTypeIsCheck(TimingType::setup_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::setup_falling));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::hold_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::hold_falling));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::recovery_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::recovery_falling));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::removal_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::removal_falling));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::min_pulse_width));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::minimum_period));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::skew_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::skew_falling));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::nochange_high_high));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::nochange_high_low));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::nochange_low_high));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::nochange_low_low));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::non_seq_setup_falling));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::non_seq_setup_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::non_seq_hold_falling));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::non_seq_hold_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::retaining_time));

  EXPECT_FALSE(timingTypeIsCheck(TimingType::combinational));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::combinational_rise));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::combinational_fall));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::rising_edge));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::falling_edge));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::clear));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::preset));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_enable));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_disable));
}

TEST(TimingTypeTest, TimingTypeScaleFactorType) {
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::setup_rising),
            ScaleFactorType::setup);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::setup_falling),
            ScaleFactorType::setup);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::hold_rising),
            ScaleFactorType::hold);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::hold_falling),
            ScaleFactorType::hold);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::recovery_rising),
            ScaleFactorType::recovery);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::removal_falling),
            ScaleFactorType::removal);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::skew_rising),
            ScaleFactorType::skew);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::minimum_period),
            ScaleFactorType::min_period);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::nochange_high_high),
            ScaleFactorType::nochange);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::min_pulse_width),
            ScaleFactorType::min_pulse_width);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::combinational),
            ScaleFactorType::cell);
}

TEST(TimingSenseTest, ToString) {
  EXPECT_STREQ(to_string(TimingSense::positive_unate), "positive_unate");
  EXPECT_STREQ(to_string(TimingSense::negative_unate), "negative_unate");
  EXPECT_STREQ(to_string(TimingSense::non_unate), "non_unate");
  EXPECT_STREQ(to_string(TimingSense::none), "none");
  EXPECT_STREQ(to_string(TimingSense::unknown), "unknown");
}

TEST(TimingSenseTest, Opposite) {
  EXPECT_EQ(timingSenseOpposite(TimingSense::positive_unate),
            TimingSense::negative_unate);
  EXPECT_EQ(timingSenseOpposite(TimingSense::negative_unate),
            TimingSense::positive_unate);
  EXPECT_EQ(timingSenseOpposite(TimingSense::non_unate),
            TimingSense::non_unate);
  EXPECT_EQ(timingSenseOpposite(TimingSense::unknown),
            TimingSense::unknown);
  EXPECT_EQ(timingSenseOpposite(TimingSense::none),
            TimingSense::none);
}

////////////////////////////////////////////////////////////////
// RiseFallValues tests
////////////////////////////////////////////////////////////////

TEST(RiseFallValuesTest, DefaultConstructor) {
  RiseFallValues rfv;
  EXPECT_FALSE(rfv.hasValue(RiseFall::rise()));
  EXPECT_FALSE(rfv.hasValue(RiseFall::fall()));
}

TEST(RiseFallValuesTest, InitValueConstructor) {
  RiseFallValues rfv(3.14f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_TRUE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 3.14f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 3.14f);
}

TEST(RiseFallValuesTest, SetValueRiseFall) {
  RiseFallValues rfv;
  rfv.setValue(RiseFall::rise(), 1.0f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_FALSE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 1.0f);
}

TEST(RiseFallValuesTest, SetValueBoth) {
  RiseFallValues rfv;
  rfv.setValue(2.5f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_TRUE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 2.5f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 2.5f);
}

TEST(RiseFallValuesTest, SetValueRiseFallBoth) {
  RiseFallValues rfv;
  rfv.setValue(RiseFallBoth::riseFall(), 5.0f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_TRUE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 5.0f);
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::fall()), 5.0f);
}

TEST(RiseFallValuesTest, SetValueRiseOnly) {
  RiseFallValues rfv;
  rfv.setValue(RiseFallBoth::rise(), 1.0f);
  EXPECT_TRUE(rfv.hasValue(RiseFall::rise()));
  EXPECT_FALSE(rfv.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(rfv.value(RiseFall::rise()), 1.0f);
}

TEST(RiseFallValuesTest, ValueWithExists) {
  RiseFallValues rfv;
  float val;
  bool exists;
  rfv.value(RiseFall::rise(), val, exists);
  EXPECT_FALSE(exists);

  rfv.setValue(RiseFall::rise(), 7.0f);
  rfv.value(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 7.0f);
}

TEST(RiseFallValuesTest, SetValues) {
  RiseFallValues src(10.0f);
  RiseFallValues dst;
  dst.setValues(&src);
  EXPECT_TRUE(dst.hasValue(RiseFall::rise()));
  EXPECT_TRUE(dst.hasValue(RiseFall::fall()));
  EXPECT_FLOAT_EQ(dst.value(RiseFall::rise()), 10.0f);
  EXPECT_FLOAT_EQ(dst.value(RiseFall::fall()), 10.0f);
}

TEST(RiseFallValuesTest, Clear) {
  RiseFallValues rfv(5.0f);
  rfv.clear();
  EXPECT_FALSE(rfv.hasValue(RiseFall::rise()));
  EXPECT_FALSE(rfv.hasValue(RiseFall::fall()));
}

////////////////////////////////////////////////////////////////
// InternalPower tests (InternalPowerAttrs removed in MCMM update)
////////////////////////////////////////////////////////////////

TEST(InternalPowerTest, DirectConstruction) {
  // InternalPower is now constructed directly
  InternalPowerModels models{};
  std::shared_ptr<FuncExpr> when_expr(FuncExpr::makeOne());
  InternalPower pwr(nullptr, nullptr, nullptr, when_expr, models);
  EXPECT_EQ(pwr.when(), when_expr.get());
  EXPECT_EQ(pwr.relatedPgPin(), nullptr);
  EXPECT_EQ(pwr.model(RiseFall::rise()), nullptr);
  EXPECT_EQ(pwr.model(RiseFall::fall()), nullptr);
}

////////////////////////////////////////////////////////////////
// TimingArcAttrs tests
////////////////////////////////////////////////////////////////

TEST(TimingArcAttrsTest, DefaultConstructor) {
  TimingArcAttrs attrs;
  EXPECT_EQ(attrs.timingType(), TimingType::combinational);
  EXPECT_EQ(attrs.timingSense(), TimingSense::unknown);
  EXPECT_EQ(attrs.cond(), nullptr);
  EXPECT_TRUE(attrs.sdfCond().empty());
  EXPECT_TRUE(attrs.sdfCondStart().empty());
  EXPECT_TRUE(attrs.sdfCondEnd().empty());
  EXPECT_TRUE(attrs.modeName().empty());
  EXPECT_TRUE(attrs.modeValue().empty());
  EXPECT_FLOAT_EQ(attrs.ocvArcDepth(), 0.0f);
  EXPECT_EQ(attrs.model(RiseFall::rise()), nullptr);
  EXPECT_EQ(attrs.model(RiseFall::fall()), nullptr);
}

TEST(TimingArcAttrsTest, SenseConstructor) {
  TimingArcAttrs attrs(TimingSense::positive_unate);
  EXPECT_EQ(attrs.timingSense(), TimingSense::positive_unate);
  EXPECT_EQ(attrs.timingType(), TimingType::combinational);
}

TEST(TimingArcAttrsTest, SetTimingType) {
  TimingArcAttrs attrs;
  attrs.setTimingType(TimingType::setup_rising);
  EXPECT_EQ(attrs.timingType(), TimingType::setup_rising);
}

TEST(TimingArcAttrsTest, SetTimingSense) {
  TimingArcAttrs attrs;
  attrs.setTimingSense(TimingSense::negative_unate);
  EXPECT_EQ(attrs.timingSense(), TimingSense::negative_unate);
}

TEST(TimingArcAttrsTest, SetOcvArcDepth) {
  TimingArcAttrs attrs;
  attrs.setOcvArcDepth(2.5f);
  EXPECT_FLOAT_EQ(attrs.ocvArcDepth(), 2.5f);
}

TEST(TimingArcAttrsTest, SetModeName) {
  TimingArcAttrs attrs;
  attrs.setModeName("test_mode");
  EXPECT_EQ(attrs.modeName(), "test_mode");
  attrs.setModeName("another_mode");
  EXPECT_EQ(attrs.modeName(), "another_mode");
}

TEST(TimingArcAttrsTest, SetModeValue) {
  TimingArcAttrs attrs;
  attrs.setModeValue("mode_val");
  EXPECT_EQ(attrs.modeValue(), "mode_val");
}

TEST(TimingArcAttrsTest, SetSdfCond) {
  TimingArcAttrs attrs;
  attrs.setSdfCond("A==1");
  EXPECT_EQ(attrs.sdfCond(), "A==1");
  // After setSdfCond, sdfCondStart and sdfCondEnd point to same string
  EXPECT_EQ(attrs.sdfCondStart(), "A==1");
  EXPECT_EQ(attrs.sdfCondEnd(), "A==1");
}

TEST(TimingArcAttrsTest, SetSdfCondStartEnd) {
  TimingArcAttrs attrs;
  attrs.setSdfCondStart("start_cond");
  EXPECT_EQ(attrs.sdfCondStart(), "start_cond");
  attrs.setSdfCondEnd("end_cond");
  EXPECT_EQ(attrs.sdfCondEnd(), "end_cond");
}

////////////////////////////////////////////////////////////////
// Transition/RiseFall tests
////////////////////////////////////////////////////////////////

TEST(RiseFallTest, BasicProperties) {
  EXPECT_EQ(RiseFall::rise()->index(), 0);
  EXPECT_EQ(RiseFall::fall()->index(), 1);
  EXPECT_STREQ(RiseFall::rise()->name(), "rise");
  EXPECT_STREQ(RiseFall::fall()->name(), "fall");
  EXPECT_EQ(RiseFall::rise()->opposite(), RiseFall::fall());
  EXPECT_EQ(RiseFall::fall()->opposite(), RiseFall::rise());
}

TEST(RiseFallTest, Find) {
  EXPECT_EQ(RiseFall::find("rise"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("fall"), RiseFall::fall());
  EXPECT_EQ(RiseFall::find(0), RiseFall::rise());
  EXPECT_EQ(RiseFall::find(1), RiseFall::fall());
}

TEST(RiseFallTest, Range) {
  auto &range = RiseFall::range();
  EXPECT_EQ(range.size(), 2u);
  EXPECT_EQ(range[0], RiseFall::rise());
  EXPECT_EQ(range[1], RiseFall::fall());
}

TEST(TransitionTest, BasicProperties) {
  EXPECT_EQ(Transition::rise()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::fall()->asRiseFall(), RiseFall::fall());
}

TEST(TransitionTest, Find) {
  // Transition names are "^" and "v", not "rise" and "fall"
  EXPECT_EQ(Transition::find("^"), Transition::rise());
  EXPECT_EQ(Transition::find("v"), Transition::fall());
  // Also findable by init_final strings
  EXPECT_EQ(Transition::find("01"), Transition::rise());
  EXPECT_EQ(Transition::find("10"), Transition::fall());
}

TEST(RiseFallBothTest, Matches) {
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::rise()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::fall()));
  EXPECT_TRUE(RiseFallBoth::rise()->matches(RiseFall::rise()));
  EXPECT_FALSE(RiseFallBoth::rise()->matches(RiseFall::fall()));
  EXPECT_FALSE(RiseFallBoth::fall()->matches(RiseFall::rise()));
  EXPECT_TRUE(RiseFallBoth::fall()->matches(RiseFall::fall()));
}

////////////////////////////////////////////////////////////////
// WireloadSelection tests
////////////////////////////////////////////////////////////////

TEST(WireloadSelectionTest, FindWireloadBasic) {
  // Create a mock library to use with Wireload
  LibertyLibrary lib("test_lib", "test.lib");

  Wireload wl_small("small", &lib, 0.0f, 1.0f, 1.0f, 0.0f);
  Wireload wl_medium("medium", &lib, 0.0f, 2.0f, 2.0f, 0.0f);
  Wireload wl_large("large", &lib, 0.0f, 3.0f, 3.0f, 0.0f);

  WireloadSelection sel("test_sel");
  sel.addWireloadFromArea(0.0f, 100.0f, &wl_small);
  sel.addWireloadFromArea(100.0f, 500.0f, &wl_medium);
  sel.addWireloadFromArea(500.0f, 1000.0f, &wl_large);

  // Below minimum -> first
  EXPECT_EQ(sel.findWireload(-1.0f), &wl_small);
  // At minimum
  EXPECT_EQ(sel.findWireload(0.0f), &wl_small);
  // In second range
  EXPECT_EQ(sel.findWireload(200.0f), &wl_medium);
  // At max
  EXPECT_EQ(sel.findWireload(500.0f), &wl_large);
  // Above max
  EXPECT_EQ(sel.findWireload(2000.0f), &wl_large);
}

////////////////////////////////////////////////////////////////
// LinearModel tests - covers GateLinearModel and CheckLinearModel
////////////////////////////////////////////////////////////////

class LinearModelTest : public ::testing::Test {
protected:
  void SetUp() override {
    lib_ = new LibertyLibrary("test_lib", "test.lib");
    cell_ = new LibertyCell(lib_, "INV", "inv.lib");
  }
  void TearDown() override {
    delete cell_;
    delete lib_;
  }
  LibertyLibrary *lib_;
  LibertyCell *cell_;
};

TEST_F(LinearModelTest, GateLinearModelConstruct) {
  GateLinearModel model(cell_, 1.5f, 0.5f);
  EXPECT_FLOAT_EQ(model.driveResistance(nullptr), 0.5f);
}

TEST_F(LinearModelTest, GateLinearModelGateDelay) {
  GateLinearModel model(cell_, 1.0f, 2.0f);
  ArcDelay gate_delay;
  Slew drvr_slew;
  // delay = intrinsic + resistance * load_cap = 1.0 + 2.0 * 3.0 = 7.0
  model.gateDelay(nullptr, 0.0f, 3.0f, false, gate_delay, drvr_slew);
  EXPECT_FLOAT_EQ(delayAsFloat(gate_delay), 7.0f);
  EXPECT_FLOAT_EQ(delayAsFloat(drvr_slew), 0.0f);
}

TEST_F(LinearModelTest, GateLinearModelZeroLoad) {
  GateLinearModel model(cell_, 2.5f, 1.0f);
  ArcDelay gate_delay;
  Slew drvr_slew;
  // delay = 2.5 + 1.0 * 0.0 = 2.5
  model.gateDelay(nullptr, 0.0f, 0.0f, false, gate_delay, drvr_slew);
  EXPECT_FLOAT_EQ(delayAsFloat(gate_delay), 2.5f);
}

TEST_F(LinearModelTest, GateLinearModelReportGateDelay) {
  GateLinearModel model(cell_, 1.0f, 2.0f);
  std::string report = model.reportGateDelay(nullptr, 0.0f, 0.5f, false, 3);
  EXPECT_FALSE(report.empty());
  // Report should contain "Delay ="
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, CheckLinearModelConstruct) {
  CheckLinearModel model(cell_, 3.0f);
  ArcDelay delay = model.checkDelay(nullptr, 0.0f, 0.0f, 0.0f, false);
  EXPECT_FLOAT_EQ(delayAsFloat(delay), 3.0f);
}

TEST_F(LinearModelTest, CheckLinearModelCheckDelay) {
  CheckLinearModel model(cell_, 5.5f);
  // checkDelay always returns intrinsic_ regardless of other params
  ArcDelay delay1 = model.checkDelay(nullptr, 1.0f, 2.0f, 3.0f, true);
  EXPECT_FLOAT_EQ(delayAsFloat(delay1), 5.5f);
  ArcDelay delay2 = model.checkDelay(nullptr, 0.0f, 0.0f, 0.0f, false);
  EXPECT_FLOAT_EQ(delayAsFloat(delay2), 5.5f);
}

TEST_F(LinearModelTest, CheckLinearModelReportCheckDelay) {
  CheckLinearModel model(cell_, 2.0f);
  std::string report = model.reportCheckDelay(nullptr, 0.0f, nullptr,
                                               0.0f, 0.0f, false, 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Check"), std::string::npos);
}

////////////////////////////////////////////////////////////////
// InternalPower additional coverage
////////////////////////////////////////////////////////////////

TEST(InternalPowerTest, ModelAccess) {
  InternalPowerModels models{};
  InternalPower pwr(nullptr, nullptr, nullptr, nullptr, models);
  // Initially models should be nullptr
  EXPECT_EQ(pwr.model(RiseFall::rise()), nullptr);
  EXPECT_EQ(pwr.model(RiseFall::fall()), nullptr);
}

TEST(InternalPowerTest, WithModel) {
  // Create a minimal model: Table -> TableModel -> InternalPowerModel
  TablePtr tbl = std::make_shared<Table>(1.0f);
  TableModel *table_model = new TableModel(tbl, nullptr,
                                           ScaleFactorType::internal_power,
                                           RiseFall::rise());
  auto power_model = std::make_shared<InternalPowerModel>(table_model);

  InternalPowerModels models{};
  models[RiseFall::riseIndex()] = power_model;
  InternalPower pwr(nullptr, nullptr, nullptr, nullptr, models);
  EXPECT_EQ(pwr.model(RiseFall::rise()), power_model.get());
  EXPECT_EQ(pwr.model(RiseFall::fall()), nullptr);
}

////////////////////////////////////////////////////////////////
// TimingArcAttrs additional coverage
////////////////////////////////////////////////////////////////

TEST(TimingArcAttrsTest, SetCond) {
  TimingArcAttrs attrs;
  FuncExpr *cond = FuncExpr::makeOne();
  attrs.setCond(cond);
  EXPECT_EQ(attrs.cond(), cond);
  // Destructor cleans up cond
}

TEST(TimingArcAttrsTest, SetModel) {
  TimingArcAttrs attrs;
  // Models are initially null
  EXPECT_EQ(attrs.model(RiseFall::rise()), nullptr);
  EXPECT_EQ(attrs.model(RiseFall::fall()), nullptr);
}

TEST(TimingArcAttrsTest, DestructorCleanup) {
  // Create attrs on heap and verify destructor cleans up properly
  TimingArcAttrs *attrs = new TimingArcAttrs();
  FuncExpr *cond = FuncExpr::makeZero();
  attrs->setCond(cond);
  attrs->setSdfCond("A==1");
  attrs->setSdfCondStart("start");
  attrs->setSdfCondEnd("end");
  attrs->setModeName("mode1");
  attrs->setModeValue("val1");
  EXPECT_EQ(attrs->cond(), cond);
  EXPECT_FALSE(attrs->sdfCond().empty());
  EXPECT_FALSE(attrs->sdfCondStart().empty());
  EXPECT_FALSE(attrs->sdfCondEnd().empty());
  EXPECT_EQ(attrs->modeName(), "mode1");
  EXPECT_EQ(attrs->modeValue(), "val1");
  // Destructor should clean up cond, sdf strings, mode strings
  delete attrs;
  // If we get here without crash, cleanup succeeded
}

////////////////////////////////////////////////////////////////
// Table3 test - basic construction and value lookup
////////////////////////////////////////////////////////////////

TEST(Table3Test, BasicConstruction) {
  // Table3 extends Table2: values_ is FloatTable* (Vector<FloatSeq*>)
  // Layout: values_[axis1_idx * axis2_size + axis2_idx][axis3_idx]
  // For a 2x2x2 table: 4 rows of 2 elements each
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.1f); ax1_vals.push_back(0.5f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(1.0f); ax2_vals.push_back(2.0f);
  FloatSeq ax3_vals;
  ax3_vals.push_back(10.0f); ax3_vals.push_back(20.0f);
  auto axis1 = std::make_shared<TableAxis>(TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  auto axis3 = std::make_shared<TableAxis>(TableAxisVariable::related_pin_transition, std::move(ax3_vals));

  // 2x2x2: values_[axis1*axis2_size + axis2][axis3]
  // row0 = (0,0) -> {1,2}, row1 = (0,1) -> {3,4}, row2 = (1,0) -> {5,6}, row3 = (1,1) -> {7,8}
  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(2.0f);
  FloatSeq row1; row1.push_back(3.0f); row1.push_back(4.0f);
  FloatSeq row2; row2.push_back(5.0f); row2.push_back(6.0f);
  FloatSeq row3; row3.push_back(7.0f); row3.push_back(8.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  values.push_back(std::move(row2)); values.push_back(std::move(row3));

  Table tbl(std::move(values), axis1, axis2, axis3);

  EXPECT_EQ(tbl.order(), 3);
  EXPECT_NE(tbl.axis1(), nullptr);
  EXPECT_NE(tbl.axis2(), nullptr);
  EXPECT_NE(tbl.axis3(), nullptr);

  // Check corner values
  EXPECT_FLOAT_EQ(tbl.value(0, 0, 0), 1.0f);
  EXPECT_FLOAT_EQ(tbl.value(1, 1, 1), 8.0f);
}

TEST(Table3Test, FindValue) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.1f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.1f); ax2_vals.push_back(1.0f);
  FloatSeq ax3_vals;
  ax3_vals.push_back(0.1f); ax3_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  auto axis3 = std::make_shared<TableAxis>(TableAxisVariable::related_pin_transition, std::move(ax3_vals));

  // All values 1.0 in a 2x2x2 table (4 rows of 2)
  FloatTable values;
  for (int i = 0; i < 4; i++) {
    FloatSeq row;
    row.push_back(1.0f); row.push_back(1.0f);
    values.push_back(std::move(row));
  }

  Table tbl(std::move(values), axis1, axis2, axis3);

  // All values are 1.0, so any lookup should return ~1.0
  float result = tbl.findValue(0.5f, 0.5f, 0.5f);
  EXPECT_FLOAT_EQ(result, 1.0f);
}

////////////////////////////////////////////////////////////////
// TableModel wrapper tests
////////////////////////////////////////////////////////////////

TEST(TableModelTest, Order0) {
  TablePtr tbl = std::make_shared<Table>(42.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  EXPECT_EQ(model.order(), 0);
}

TEST(TableModelTest, Order1) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(TableAxisVariable::input_transition_time, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f); values.push_back(2.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  EXPECT_EQ(model.order(), 1);
  EXPECT_NE(model.axis1(), nullptr);
  EXPECT_EQ(model.axis2(), nullptr);
  EXPECT_EQ(model.axis3(), nullptr);
}

TEST(TableModelTest, Order2) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.1f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.1f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(2.0f);
  FloatSeq row1; row1.push_back(3.0f); row1.push_back(4.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis1, axis2);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  EXPECT_EQ(model.order(), 2);
  EXPECT_NE(model.axis1(), nullptr);
  EXPECT_NE(model.axis2(), nullptr);
  EXPECT_EQ(model.axis3(), nullptr);
}

////////////////////////////////////////////////////////////////
// Wireload additional tests
////////////////////////////////////////////////////////////////

TEST(WireloadTest, BasicConstruction) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload wl("test_wl", &lib, 0.0f, 1.0f, 2.0f, 3.0f);
  EXPECT_STREQ(wl.name(), "test_wl");
}

TEST(WireloadTest, SimpleConstructor) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload wl("test_wl", &lib);
  EXPECT_STREQ(wl.name(), "test_wl");
  // Set individual properties
  wl.setArea(10.0f);
  wl.setResistance(1.5f);
  wl.setCapacitance(2.5f);
  wl.setSlope(0.5f);
}

TEST(WireloadTest, AddFanoutLength) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload wl("test_wl", &lib, 0.0f, 1.0f, 1.0f, 0.5f);
  wl.addFanoutLength(1, 10.0f);
  wl.addFanoutLength(2, 20.0f);
  wl.addFanoutLength(4, 40.0f);

  float cap, res;
  // Exact fanout match (first entry)
  wl.findWireload(1.0f, nullptr, cap, res);
  EXPECT_GT(cap, 0.0f);
  EXPECT_GT(res, 0.0f);

  // Between entries (interpolation)
  wl.findWireload(3.0f, nullptr, cap, res);
  EXPECT_GT(cap, 0.0f);

  // Beyond max fanout (extrapolation)
  wl.findWireload(5.0f, nullptr, cap, res);
  EXPECT_GT(cap, 0.0f);

  // Below min fanout (extrapolation)
  wl.findWireload(0.5f, nullptr, cap, res);
  // Result may be non-negative
}

TEST(WireloadTest, EmptyFanoutLengths) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload wl("test_wl", &lib, 0.0f, 1.0f, 1.0f, 0.0f);
  // No fanout lengths added
  float cap, res;
  wl.findWireload(1.0f, nullptr, cap, res);
  // With no fanout lengths, length=0 so cap and res should be 0
  EXPECT_FLOAT_EQ(cap, 0.0f);
  EXPECT_FLOAT_EQ(res, 0.0f);
}

TEST(WireloadTest, UnsortedFanoutLengths) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload wl("test_wl", &lib, 0.0f, 1.0f, 1.0f, 0.0f);
  // Add in reverse order to exercise sorting
  wl.addFanoutLength(4, 40.0f);
  wl.addFanoutLength(2, 20.0f);
  wl.addFanoutLength(1, 10.0f);

  float cap, res;
  wl.findWireload(1.0f, nullptr, cap, res);
  EXPECT_GT(cap, 0.0f);
}

////////////////////////////////////////////////////////////////
// FuncExpr additional coverage
////////////////////////////////////////////////////////////////

TEST(FuncExprTest, PortTimingSensePositiveUnate) {
  // Create port expression
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *a = cell->makePort("A");
  LibertyPort *port = reinterpret_cast<LibertyPort*>(a);
  FuncExpr *port_expr = FuncExpr::makePort(port);

  // A port expression itself should be positive_unate for the same port
  TimingSense sense = port_expr->portTimingSense(port);
  EXPECT_EQ(sense, TimingSense::positive_unate);

  delete port_expr;
}

TEST(FuncExprTest, NotTimingSenseNegativeUnate) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *a = cell->makePort("A");
  LibertyPort *port = reinterpret_cast<LibertyPort*>(a);
  FuncExpr *port_expr = FuncExpr::makePort(port);
  FuncExpr *not_expr = FuncExpr::makeNot(port_expr);

  // NOT(A) should be negative_unate for A
  TimingSense sense = not_expr->portTimingSense(port);
  EXPECT_EQ(sense, TimingSense::negative_unate);

  delete not_expr;
}

////////////////////////////////////////////////////////////////
// LibertyLibrary property tests
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, NominalValues) {
  LibertyLibrary lib("test_lib", "test.lib");
  lib.setNominalProcess(1.0f);
  lib.setNominalVoltage(1.2f);
  lib.setNominalTemperature(25.0f);
  EXPECT_FLOAT_EQ(lib.nominalProcess(), 1.0f);
  EXPECT_FLOAT_EQ(lib.nominalVoltage(), 1.2f);
  EXPECT_FLOAT_EQ(lib.nominalTemperature(), 25.0f);
}

TEST(LibertyLibraryTest, DelayModelType) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.delayModelType(), DelayModelType::table);
  lib.setDelayModelType(DelayModelType::cmos_linear);
  EXPECT_EQ(lib.delayModelType(), DelayModelType::cmos_linear);
}

TEST(LibertyLibraryTest, DefaultPinCaps) {
  LibertyLibrary lib("test_lib", "test.lib");
  lib.setDefaultInputPinCap(0.01f);
  lib.setDefaultOutputPinCap(0.02f);
  lib.setDefaultBidirectPinCap(0.015f);
  EXPECT_FLOAT_EQ(lib.defaultInputPinCap(), 0.01f);
  EXPECT_FLOAT_EQ(lib.defaultOutputPinCap(), 0.02f);
  EXPECT_FLOAT_EQ(lib.defaultBidirectPinCap(), 0.015f);
}

TEST(LibertyLibraryTest, DefaultMaxCapacitance) {
  LibertyLibrary lib("test_lib", "test.lib");
  float cap;
  bool exists;
  lib.defaultMaxCapacitance(cap, exists);
  EXPECT_FALSE(exists);

  lib.setDefaultMaxCapacitance(5.0f);
  lib.defaultMaxCapacitance(cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 5.0f);
}

TEST(LibertyLibraryTest, DefaultFanoutLoad) {
  LibertyLibrary lib("test_lib", "test.lib");
  float load;
  bool exists;
  lib.defaultFanoutLoad(load, exists);
  EXPECT_FALSE(exists);

  lib.setDefaultFanoutLoad(1.5f);
  lib.defaultFanoutLoad(load, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(load, 1.5f);
}

TEST(LibertyLibraryTest, DefaultIntrinsic) {
  LibertyLibrary lib("test_lib", "test.lib");
  float intrinsic;
  bool exists;
  lib.defaultIntrinsic(RiseFall::rise(), intrinsic, exists);
  EXPECT_FALSE(exists);

  lib.setDefaultIntrinsic(RiseFall::rise(), 0.5f);
  lib.defaultIntrinsic(RiseFall::rise(), intrinsic, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(intrinsic, 0.5f);
}

TEST(LibertyLibraryTest, WireSlewDegradationTable) {
  LibertyLibrary lib("test_lib", "test.lib");
  // Initially no wire slew degradation table
  EXPECT_EQ(lib.wireSlewDegradationTable(RiseFall::rise()), nullptr);
  EXPECT_EQ(lib.wireSlewDegradationTable(RiseFall::fall()), nullptr);

  // Set a simple order-0 table (scalar)
  TablePtr tbl = std::make_shared<Table>(0.1f);
  TableModel *model = new TableModel(tbl, nullptr,
                                     ScaleFactorType::transition,
                                     RiseFall::rise());
  lib.setWireSlewDegradationTable(model, RiseFall::rise());
  EXPECT_NE(lib.wireSlewDegradationTable(RiseFall::rise()), nullptr);

  // degradeWireSlew with order-0 table returns the constant
  float result = lib.degradeWireSlew(RiseFall::rise(), 0.5f, 0.1f);
  EXPECT_FLOAT_EQ(result, 0.1f);

  // Fall should still return input slew (no table)
  float result_fall = lib.degradeWireSlew(RiseFall::fall(), 0.5f, 0.1f);
  EXPECT_FLOAT_EQ(result_fall, 0.5f);
}

TEST(LibertyLibraryTest, WireSlewDegradationOrder1) {
  LibertyLibrary lib("test_lib", "test.lib");
  // Create order-1 table with output_pin_transition axis
  FloatSeq axis_values;
  axis_values.push_back(0.1f);
  axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::output_pin_transition, std::move(axis_values));
  FloatSeq values;
  values.push_back(0.1f);
  values.push_back(1.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  TableModel *model = new TableModel(tbl, nullptr,
                                     ScaleFactorType::transition,
                                     RiseFall::rise());
  lib.setWireSlewDegradationTable(model, RiseFall::rise());

  float result = lib.degradeWireSlew(RiseFall::rise(), 0.5f, 0.1f);
  // Should interpolate between 0.1 and 1.0 at slew=0.5
  EXPECT_GT(result, 0.0f);
  EXPECT_LT(result, 2.0f);
}

TEST(LibertyLibraryTest, Units) {
  LibertyLibrary lib("test_lib", "test.lib");
  const Units *units = lib.units();
  EXPECT_NE(units, nullptr);
  EXPECT_NE(units->timeUnit(), nullptr);
  EXPECT_NE(units->capacitanceUnit(), nullptr);
  EXPECT_NE(units->resistanceUnit(), nullptr);
}

////////////////////////////////////////////////////////////////
// Table report and additional tests
////////////////////////////////////////////////////////////////

TEST_F(LinearModelTest, Table0ReportValue) {
  Table tbl(42.0f);
  const Units *units = lib_->units();
  std::string report = tbl.reportValue("Delay", cell_, nullptr,
                                        0.0f, nullptr, 0.0f, 0.0f,
                                        units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, Table1ReportValue) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f);
  axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f);
  values.push_back(2.0f);
  Table tbl(std::move(values), axis);

  const Units *units = lib_->units();
  std::string report = tbl.reportValue("Delay", cell_, nullptr,
                                        0.5f, nullptr, 0.0f, 0.0f,
                                        units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, Table2ReportValue) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.1f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.1f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(2.0f);
  FloatSeq row1; row1.push_back(3.0f); row1.push_back(4.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  Table tbl(std::move(values), axis1, axis2);

  const Units *units = lib_->units();
  std::string report = tbl.reportValue("Delay", cell_, nullptr,
                                        0.5f, nullptr, 0.5f, 0.0f,
                                        units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, Table3ReportValue) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.1f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.1f); ax2_vals.push_back(1.0f);
  FloatSeq ax3_vals;
  ax3_vals.push_back(0.1f); ax3_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  auto axis3 = std::make_shared<TableAxis>(
    TableAxisVariable::related_pin_transition, std::move(ax3_vals));

  FloatTable values;
  for (int i = 0; i < 4; i++) {
    FloatSeq row;
    row.push_back(1.0f + i); row.push_back(2.0f + i);
    values.push_back(std::move(row));
  }
  Table tbl(std::move(values), axis1, axis2, axis3);

  const Units *units = lib_->units();
  std::string report = tbl.reportValue("Delay", cell_, nullptr,
                                        0.5f, nullptr, 0.5f, 0.5f,
                                        units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, TableModelReport) {
  TablePtr tbl = std::make_shared<Table>(42.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  const Units *units = lib_->units();
  Report *report_obj = nullptr;
  // report needs Report*; test order/axes instead
  EXPECT_EQ(model.order(), 0);
  EXPECT_EQ(model.axis1(), nullptr);
  EXPECT_EQ(model.axis2(), nullptr);
  EXPECT_EQ(model.axis3(), nullptr);
}

TEST_F(LinearModelTest, TableModelFindValue) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f);
  axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(axis_values));
  FloatSeq values;
  values.push_back(10.0f);
  values.push_back(20.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());

  float result = model.findValue(0.5f, 0.0f, 0.0f);
  EXPECT_GT(result, 10.0f);
  EXPECT_LT(result, 20.0f);
}

TEST_F(LinearModelTest, TableModelReportValue) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f);
  axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(axis_values));
  FloatSeq values;
  values.push_back(10.0f);
  values.push_back(20.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());

  const Units *units = lib_->units();
  std::string report = model.reportValue("Delay", cell_, nullptr,
                                          0.5f, nullptr, 0.0f, 0.0f,
                                          units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

////////////////////////////////////////////////////////////////
// FuncExpr additional operators
////////////////////////////////////////////////////////////////

TEST(FuncExprTest, AndTimingSense) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("AND2", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *b = cell->makePort("B");
  LibertyPort *port_a = reinterpret_cast<LibertyPort*>(a);
  LibertyPort *port_b = reinterpret_cast<LibertyPort*>(b);
  FuncExpr *expr_a = FuncExpr::makePort(port_a);
  FuncExpr *expr_b = FuncExpr::makePort(port_b);
  FuncExpr *and_expr = FuncExpr::makeAnd(expr_a, expr_b);

  // A AND B should be positive_unate for A
  TimingSense sense = and_expr->portTimingSense(port_a);
  EXPECT_EQ(sense, TimingSense::positive_unate);

  delete and_expr;
}

TEST(FuncExprTest, OrTimingSense) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("OR2", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *b = cell->makePort("B");
  LibertyPort *port_a = reinterpret_cast<LibertyPort*>(a);
  LibertyPort *port_b = reinterpret_cast<LibertyPort*>(b);
  FuncExpr *expr_a = FuncExpr::makePort(port_a);
  FuncExpr *expr_b = FuncExpr::makePort(port_b);
  FuncExpr *or_expr = FuncExpr::makeOr(expr_a, expr_b);

  TimingSense sense = or_expr->portTimingSense(port_a);
  EXPECT_EQ(sense, TimingSense::positive_unate);

  delete or_expr;
}

TEST(FuncExprTest, XorTimingSense) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("XOR2", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *b = cell->makePort("B");
  LibertyPort *port_a = reinterpret_cast<LibertyPort*>(a);
  LibertyPort *port_b = reinterpret_cast<LibertyPort*>(b);
  FuncExpr *expr_a = FuncExpr::makePort(port_a);
  FuncExpr *expr_b = FuncExpr::makePort(port_b);
  FuncExpr *xor_expr = FuncExpr::makeXor(expr_a, expr_b);

  // XOR should be non_unate
  TimingSense sense = xor_expr->portTimingSense(port_a);
  EXPECT_EQ(sense, TimingSense::non_unate);

  delete xor_expr;
}

TEST(FuncExprTest, ZeroOneExpressions) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_NE(zero, nullptr);
  EXPECT_NE(one, nullptr);
  delete zero;
  delete one;
}


////////////////////////////////////////////////////////////////
// Sequential tests
////////////////////////////////////////////////////////////////

TEST(SequentialTest, BasicConstruction) {
  // Sequential class is constructed and used during liberty parsing
  // We can test the StringTableAxisVariable utility
  const char *var_str = tableVariableString(TableAxisVariable::input_transition_time);
  EXPECT_STREQ(var_str, "input_transition_time");

  var_str = tableVariableString(TableAxisVariable::total_output_net_capacitance);
  EXPECT_STREQ(var_str, "total_output_net_capacitance");
}

TEST(TableAxisVariableTest, StringToVariable) {
  TableAxisVariable var = stringTableAxisVariable("input_transition_time");
  EXPECT_EQ(var, TableAxisVariable::input_transition_time);

  var = stringTableAxisVariable("total_output_net_capacitance");
  EXPECT_EQ(var, TableAxisVariable::total_output_net_capacitance);

  var = stringTableAxisVariable("related_pin_transition");
  EXPECT_EQ(var, TableAxisVariable::related_pin_transition);
}

////////////////////////////////////////////////////////////////
// WireloadSelection tests
////////////////////////////////////////////////////////////////

TEST(WireloadSelectionTest, BasicConstruction) {
  WireloadSelection sel("test_sel");
  EXPECT_STREQ(sel.name(), "test_sel");
}

TEST(WireloadSelectionTest, FindWireload) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload wl1("small", &lib, 0.0f, 1.0f, 1.0f, 0.5f);
  Wireload wl2("large", &lib, 0.0f, 2.0f, 2.0f, 1.0f);

  WireloadSelection sel("test_sel");
  sel.addWireloadFromArea(0.0f, 100.0f, &wl1);
  sel.addWireloadFromArea(100.0f, 1000.0f, &wl2);

  const Wireload *found = sel.findWireload(50.0f);
  EXPECT_EQ(found, &wl1);

  found = sel.findWireload(500.0f);
  EXPECT_EQ(found, &wl2);
}

////////////////////////////////////////////////////////////////
// Table utility functions
////////////////////////////////////////////////////////////////

TEST(TableUtilTest, WireloadTreeString) {
  EXPECT_STREQ(wireloadTreeString(WireloadTree::worst_case), "worst_case_tree");
  EXPECT_STREQ(wireloadTreeString(WireloadTree::best_case), "best_case_tree");
  EXPECT_STREQ(wireloadTreeString(WireloadTree::balanced), "balanced_tree");
}

TEST(TableUtilTest, StringWireloadTree) {
  EXPECT_EQ(stringWireloadTree("worst_case_tree"), WireloadTree::worst_case);
  EXPECT_EQ(stringWireloadTree("best_case_tree"), WireloadTree::best_case);
  EXPECT_EQ(stringWireloadTree("balanced_tree"), WireloadTree::balanced);
  EXPECT_EQ(stringWireloadTree("invalid"), WireloadTree::unknown);
}

TEST(TableUtilTest, WireloadModeString) {
  EXPECT_STREQ(wireloadModeString(WireloadMode::top), "top");
  EXPECT_STREQ(wireloadModeString(WireloadMode::enclosed), "enclosed");
  EXPECT_STREQ(wireloadModeString(WireloadMode::segmented), "segmented");
}

TEST(TableUtilTest, StringWireloadMode) {
  EXPECT_EQ(stringWireloadMode("top"), WireloadMode::top);
  EXPECT_EQ(stringWireloadMode("enclosed"), WireloadMode::enclosed);
  EXPECT_EQ(stringWireloadMode("segmented"), WireloadMode::segmented);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary wireload & operating conditions tests
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, MakeAndFindWireload) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload *wl = lib.makeWireload("test_wl");
  EXPECT_NE(wl, nullptr);
  const Wireload *found = lib.findWireload("test_wl");
  EXPECT_EQ(found, wl);
  EXPECT_EQ(lib.findWireload("nonexistent"), nullptr);
}

TEST(LibertyLibraryTest, DefaultWireload) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.defaultWireload(), nullptr);
  Wireload *wl = lib.makeWireload("default_wl");
  lib.setDefaultWireload(wl);
  EXPECT_EQ(lib.defaultWireload(), wl);
}

TEST(LibertyLibraryTest, WireloadSelection) {
  LibertyLibrary lib("test_lib", "test.lib");
  WireloadSelection *sel = lib.makeWireloadSelection("test_sel");
  EXPECT_NE(sel, nullptr);
  EXPECT_EQ(lib.findWireloadSelection("test_sel"), sel);
  EXPECT_EQ(lib.findWireloadSelection("nonexistent"), nullptr);
}

TEST(LibertyLibraryTest, DefaultWireloadSelection) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.defaultWireloadSelection(), nullptr);
  WireloadSelection *sel = lib.makeWireloadSelection("test_sel");
  lib.setDefaultWireloadSelection(sel);
  EXPECT_EQ(lib.defaultWireloadSelection(), sel);
}

TEST(LibertyLibraryTest, DefaultWireloadMode) {
  LibertyLibrary lib("test_lib", "test.lib");
  lib.setDefaultWireloadMode(WireloadMode::top);
  EXPECT_EQ(lib.defaultWireloadMode(), WireloadMode::top);
  lib.setDefaultWireloadMode(WireloadMode::enclosed);
  EXPECT_EQ(lib.defaultWireloadMode(), WireloadMode::enclosed);
}

TEST(LibertyLibraryTest, Thresholds) {
  LibertyLibrary lib("test_lib", "test.lib");
  lib.setInputThreshold(RiseFall::rise(), 0.5f);
  lib.setInputThreshold(RiseFall::fall(), 0.5f);
  EXPECT_FLOAT_EQ(lib.inputThreshold(RiseFall::rise()), 0.5f);
  EXPECT_FLOAT_EQ(lib.inputThreshold(RiseFall::fall()), 0.5f);

  lib.setOutputThreshold(RiseFall::rise(), 0.5f);
  lib.setOutputThreshold(RiseFall::fall(), 0.5f);
  EXPECT_FLOAT_EQ(lib.outputThreshold(RiseFall::rise()), 0.5f);
  EXPECT_FLOAT_EQ(lib.outputThreshold(RiseFall::fall()), 0.5f);

  lib.setSlewLowerThreshold(RiseFall::rise(), 0.2f);
  lib.setSlewUpperThreshold(RiseFall::rise(), 0.8f);
  lib.setSlewLowerThreshold(RiseFall::fall(), 0.2f);
  lib.setSlewUpperThreshold(RiseFall::fall(), 0.8f);
  EXPECT_FLOAT_EQ(lib.slewLowerThreshold(RiseFall::rise()), 0.2f);
  EXPECT_FLOAT_EQ(lib.slewUpperThreshold(RiseFall::rise()), 0.8f);
  EXPECT_FLOAT_EQ(lib.slewLowerThreshold(RiseFall::fall()), 0.2f);
  EXPECT_FLOAT_EQ(lib.slewUpperThreshold(RiseFall::fall()), 0.8f);
}

TEST(LibertyLibraryTest, SlewDerateFromLibrary) {
  LibertyLibrary lib("test_lib", "test.lib");
  // Default derate is 1.0
  EXPECT_FLOAT_EQ(lib.slewDerateFromLibrary(), 1.0f);
  // Set custom derate
  lib.setSlewDerateFromLibrary(1.667f);
  EXPECT_FLOAT_EQ(lib.slewDerateFromLibrary(), 1.667f);
}

TEST(LibertyLibraryTest, DefaultPinResistance) {
  LibertyLibrary lib("test_lib", "test.lib");
  float res;
  bool exists;
  lib.defaultOutputPinRes(RiseFall::rise(), res, exists);
  EXPECT_FALSE(exists);

  lib.setDefaultOutputPinRes(RiseFall::rise(), 10.0f);
  lib.defaultOutputPinRes(RiseFall::rise(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 10.0f);

  lib.setDefaultBidirectPinRes(RiseFall::rise(), 15.0f);
  lib.defaultBidirectPinRes(RiseFall::rise(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 15.0f);
}

TEST(LibertyLibraryTest, ScaleFactor) {
  LibertyLibrary lib("test_lib", "test.lib");
  // With no scale factors set, should return 1.0
  float sf = lib.scaleFactor(ScaleFactorType::cell, nullptr);
  EXPECT_FLOAT_EQ(sf, 1.0f);
}

TEST(LibertyLibraryTest, DefaultMaxSlew) {
  LibertyLibrary lib("test_lib", "test.lib");
  float slew;
  bool exists;
  lib.defaultMaxSlew(slew, exists);
  EXPECT_FALSE(exists);

  lib.setDefaultMaxSlew(5.0f);
  lib.defaultMaxSlew(slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 5.0f);
}

TEST(LibertyLibraryTest, DefaultMaxFanout) {
  LibertyLibrary lib("test_lib", "test.lib");
  float fanout;
  bool exists;
  lib.defaultMaxFanout(fanout, exists);
  EXPECT_FALSE(exists);

  lib.setDefaultMaxFanout(10.0f);
  lib.defaultMaxFanout(fanout, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(fanout, 10.0f);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary table template and bus type management
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, MakeAndFindTableTemplate) {
  LibertyLibrary lib("test_lib", "test.lib");
  TableTemplate *tmpl = lib.makeTableTemplate("delay_template",
                                              TableTemplateType::delay);
  EXPECT_NE(tmpl, nullptr);
  TableTemplate *found = lib.findTableTemplate("delay_template",
                                               TableTemplateType::delay);
  EXPECT_EQ(found, tmpl);
  EXPECT_EQ(lib.findTableTemplate("nonexistent", TableTemplateType::delay),
            nullptr);
}

TEST(LibertyLibraryTest, MakeAndFindBusDcl) {
  LibertyLibrary lib("test_lib", "test.lib");
  BusDcl *bus = lib.makeBusDcl("data_bus", 7, 0);
  EXPECT_NE(bus, nullptr);
  BusDcl *found = lib.findBusDcl("data_bus");
  EXPECT_EQ(found, bus);
  EXPECT_EQ(lib.findBusDcl("nonexistent"), nullptr);
}

////////////////////////////////////////////////////////////////
// Table2 findValue test
////////////////////////////////////////////////////////////////

TEST(Table2Test, FindValueInterpolation) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.0f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.0f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));

  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(3.0f);
  FloatSeq row1; row1.push_back(5.0f); row1.push_back(7.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  Table tbl(std::move(values), axis1, axis2);

  // Center should be average of all corners: (1+3+5+7)/4 = 4
  float center = tbl.findValue(0.5f, 0.5f, 0.0f);
  EXPECT_NEAR(center, 4.0f, 0.01f);

  // Corner values
  EXPECT_FLOAT_EQ(tbl.findValue(0.0f, 0.0f, 0.0f), 1.0f);
  EXPECT_FLOAT_EQ(tbl.findValue(1.0f, 1.0f, 0.0f), 7.0f);
}

////////////////////////////////////////////////////////////////
// GateTableModel static method (checkAxes)
////////////////////////////////////////////////////////////////

TEST(GateTableModelTest, CheckAxesOrder0) {
  TablePtr tbl = std::make_shared<Table>(1.0f);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

TEST(GateTableModelTest, CheckAxesOrder1) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f); values.push_back(2.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

TEST(GateTableModelTest, CheckAxesOrder2) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.1f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.1f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(2.0f);
  FloatSeq row1; row1.push_back(3.0f); row1.push_back(4.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis1, axis2);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// CheckSlewDegradationAxes
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, CheckSlewDegradationAxesOrder0) {
  TablePtr tbl = std::make_shared<Table>(1.0f);
  EXPECT_TRUE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

TEST(LibertyLibraryTest, CheckSlewDegradationAxesOrder1) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::output_pin_transition, std::move(axis_values));
  FloatSeq values;
  values.push_back(0.1f); values.push_back(1.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  EXPECT_TRUE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

////////////////////////////////////////////////////////////////
// InternalPower additional
////////////////////////////////////////////////////////////////

TEST(InternalPowerTest, RelatedPgPinViaConstruction) {
  InternalPowerModels models{};
  // relatedPgPin is now set via constructor
  InternalPower pwr(nullptr, nullptr, nullptr, nullptr, models);
  EXPECT_EQ(pwr.relatedPgPin(), nullptr);
}

////////////////////////////////////////////////////////////////
// TimingArc set/model tests
////////////////////////////////////////////////////////////////

TEST(TimingArcAttrsTest, SdfCondStrings) {
  TimingArcAttrs attrs;
  attrs.setSdfCond("A==1'b1");
  EXPECT_EQ(attrs.sdfCond(), "A==1'b1");
  attrs.setSdfCondStart("start_val");
  EXPECT_EQ(attrs.sdfCondStart(), "start_val");
  attrs.setSdfCondEnd("end_val");
  EXPECT_EQ(attrs.sdfCondEnd(), "end_val");
}

TEST(TimingArcAttrsTest, ModeNameValue) {
  TimingArcAttrs attrs;
  attrs.setModeName("test_mode");
  EXPECT_EQ(attrs.modeName(), "test_mode");
  attrs.setModeValue("mode_val");
  EXPECT_EQ(attrs.modeValue(), "mode_val");
}

////////////////////////////////////////////////////////////////
// Table0 value access
////////////////////////////////////////////////////////////////

TEST(Table0Test, ValueAccess) {
  Table tbl(42.5f);
  EXPECT_FLOAT_EQ(tbl.value(0, 0, 0), 42.5f);
  EXPECT_FLOAT_EQ(tbl.value(1, 2, 3), 42.5f);
  EXPECT_FLOAT_EQ(tbl.findValue(0.0f, 0.0f, 0.0f), 42.5f);
  EXPECT_FLOAT_EQ(tbl.findValue(1.0f, 2.0f, 3.0f), 42.5f);
  EXPECT_EQ(tbl.order(), 0);
}

////////////////////////////////////////////////////////////////
// TableModel with Table2 findValue
////////////////////////////////////////////////////////////////

TEST(TableModelTest, FindValueOrder2) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.0f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.0f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(3.0f);
  FloatSeq row1; row1.push_back(5.0f); row1.push_back(7.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis1, axis2);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());

  float center = model.findValue(0.5f, 0.5f, 0.0f);
  EXPECT_NEAR(center, 4.0f, 0.01f);
}

////////////////////////////////////////////////////////////////
// ScaleFactors tests
////////////////////////////////////////////////////////////////

TEST(ScaleFactorsTest, BasicConstruction) {
  ScaleFactors sf("test_scales");
  EXPECT_STREQ(sf.name(), "test_scales");
}

TEST(ScaleFactorsTest, SetAndGetWithRiseFall) {
  ScaleFactors sf("sf1");
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::process,
              RiseFall::rise(), 1.5f);
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::process,
              RiseFall::fall(), 2.0f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::cell, ScaleFactorPvt::process,
                            RiseFall::rise()), 1.5f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::cell, ScaleFactorPvt::process,
                            RiseFall::fall()), 2.0f);
}

TEST(ScaleFactorsTest, SetAndGetWithIndex) {
  ScaleFactors sf("sf2");
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::volt,
              RiseFall::rise(), 3.0f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::cell, ScaleFactorPvt::volt,
                            RiseFall::riseIndex()), 3.0f);
}

TEST(ScaleFactorsTest, SetAndGetWithoutRiseFall) {
  ScaleFactors sf("sf3");
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::temp, 4.0f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::cell, ScaleFactorPvt::temp), 4.0f);
}

////////////////////////////////////////////////////////////////
// OcvDerate tests
////////////////////////////////////////////////////////////////

TEST(OcvDerateTest, BasicConstruction) {
  OcvDerate derate(stringCopy("test_ocv"));
  EXPECT_EQ(derate.name(), "test_ocv");
}

TEST(OcvDerateTest, SetAndGetDerateTable) {
  OcvDerate derate(stringCopy("ocv1"));
  TablePtr tbl = std::make_shared<Table>(0.95f);
  derate.setDerateTable(RiseFall::rise(), EarlyLate::early(),
                        PathType::data, tbl);
  const Table *found = derate.derateTable(RiseFall::rise(), EarlyLate::early(),
                                          PathType::data);
  EXPECT_NE(found, nullptr);
}

TEST(OcvDerateTest, NullByDefault) {
  OcvDerate derate(stringCopy("ocv2"));
  const Table *found = derate.derateTable(RiseFall::fall(), EarlyLate::late(),
                                          PathType::clk);
  EXPECT_EQ(found, nullptr);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary OCV and supply voltage tests
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, OcvArcDepth) {
  LibertyLibrary lib("test_lib", "test.lib");
  lib.setOcvArcDepth(5.0f);
  EXPECT_FLOAT_EQ(lib.ocvArcDepth(), 5.0f);
}

TEST(LibertyLibraryTest, DefaultOcvDerate) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.defaultOcvDerate(), nullptr);
  OcvDerate *derate = new OcvDerate(stringCopy("default_ocv"));
  lib.setDefaultOcvDerate(derate);
  EXPECT_EQ(lib.defaultOcvDerate(), derate);
}

TEST(LibertyLibraryTest, MakeAndFindOcvDerate) {
  LibertyLibrary lib("test_lib", "test.lib");
  OcvDerate *derate = lib.makeOcvDerate("cell_ocv");
  EXPECT_NE(derate, nullptr);
  OcvDerate *found = lib.findOcvDerate("cell_ocv");
  EXPECT_EQ(found, derate);
  EXPECT_EQ(lib.findOcvDerate("nonexistent"), nullptr);
}

TEST(LibertyLibraryTest, SupplyVoltage) {
  LibertyLibrary lib("test_lib", "test.lib");
  float voltage;
  bool exists;
  lib.supplyVoltage("VDD", voltage, exists);
  EXPECT_FALSE(exists);

  lib.addSupplyVoltage("VDD", 1.1f);
  lib.supplyVoltage("VDD", voltage, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(voltage, 1.1f);
  EXPECT_TRUE(lib.supplyExists("VDD"));
  EXPECT_FALSE(lib.supplyExists("VSS"));
}

TEST(LibertyLibraryTest, MakeAndFindScaleFactors) {
  LibertyLibrary lib("test_lib", "test.lib");
  ScaleFactors *sf = lib.makeScaleFactors("k_process");
  EXPECT_NE(sf, nullptr);
  ScaleFactors *found = lib.findScaleFactors("k_process");
  EXPECT_EQ(found, sf);
}

TEST(LibertyLibraryTest, DefaultScaleFactors) {
  ASSERT_NO_THROW(( [&](){
  LibertyLibrary lib("test_lib", "test.lib");
  ScaleFactors *sf = new ScaleFactors("default_sf");
  lib.setScaleFactors(sf);
  // Just verifying it doesn't crash - scale factors are used internally

  }() ));
}

TEST(LibertyLibraryTest, MakeScaledCell) {
  LibertyLibrary lib("test_lib", "test.lib");
  LibertyCell *cell = lib.makeScaledCell("scaled_inv", "test.lib");
  EXPECT_NE(cell, nullptr);
  EXPECT_STREQ(cell->name(), "scaled_inv");
  delete cell;
}

TEST(LibertyLibraryTest, DefaultPinResistanceWithDirection) {
  PortDirection::init();
  LibertyLibrary lib("test_lib", "test.lib");
  float res;
  bool exists;

  // Test with output direction
  lib.setDefaultOutputPinRes(RiseFall::rise(), 100.0f);
  lib.defaultPinResistance(RiseFall::rise(), PortDirection::output(),
                           res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 100.0f);

  // Test with tristate direction
  lib.setDefaultBidirectPinRes(RiseFall::rise(), 200.0f);
  lib.defaultPinResistance(RiseFall::rise(), PortDirection::tristate(),
                           res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 200.0f);
}

TEST(LibertyLibraryTest, TableTemplates) {
  LibertyLibrary lib("test_lib", "test.lib");
  lib.makeTableTemplate("tmpl1", TableTemplateType::delay);
  lib.makeTableTemplate("tmpl2", TableTemplateType::power);
  auto tbl_tmpls = lib.tableTemplates();
  EXPECT_GE(tbl_tmpls.size(), 2u);
}

////////////////////////////////////////////////////////////////
// TestCell (LibertyCell) tests
////////////////////////////////////////////////////////////////

TEST(TestCellTest, BasicConstruction) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV_X1", "test.lib");
  EXPECT_STREQ(cell.name(), "INV_X1");
  EXPECT_EQ(cell.libertyLibrary(), &lib);
}

TEST(TestCellTest, SetArea) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "BUF_X1", "test.lib");
  cell.setArea(2.5f);
  EXPECT_FLOAT_EQ(cell.area(), 2.5f);
}

TEST(TestCellTest, SetDontUse) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "BUF_X1", "test.lib");
  EXPECT_FALSE(cell.dontUse());
  cell.setDontUse(true);
  EXPECT_TRUE(cell.dontUse());
}

TEST(TestCellTest, SetIsMacro) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "SRAM", "test.lib");
  cell.setIsMacro(true);
  EXPECT_TRUE(cell.isMacro());
}

TEST(TestCellTest, SetIsPad) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "PAD1", "test.lib");
  cell.setIsPad(true);
  EXPECT_TRUE(cell.isPad());
}

TEST(TestCellTest, SetIsClockCell) {
  ASSERT_NO_THROW(( [&](){
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CLKBUF", "test.lib");
  cell.setIsClockCell(true);
  // isClockCell is not directly queryable, but this covers the setter

  }() ));
}

TEST(TestCellTest, SetIsLevelShifter) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "LS1", "test.lib");
  cell.setIsLevelShifter(true);
  EXPECT_TRUE(cell.isLevelShifter());
}

TEST(TestCellTest, SetLevelShifterType) {
  ASSERT_NO_THROW(( [&](){
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "LS2", "test.lib");
  cell.setLevelShifterType(LevelShifterType::HL);

  }() ));
}

TEST(TestCellTest, SetIsIsolationCell) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "ISO1", "test.lib");
  cell.setIsIsolationCell(true);
  EXPECT_TRUE(cell.isIsolationCell());
}

TEST(TestCellTest, SetSwitchCellType) {
  ASSERT_NO_THROW(( [&](){
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "SW1", "test.lib");
  cell.setSwitchCellType(SwitchCellType::coarse_grain);

  }() ));
}

TEST(TestCellTest, SetInterfaceTiming) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  cell.setInterfaceTiming(true);
  EXPECT_TRUE(cell.interfaceTiming());
}

TEST(TestCellTest, ClockGateTypes) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "ICG1", "test.lib");

  EXPECT_FALSE(cell.isClockGate());
  EXPECT_FALSE(cell.isClockGateLatchPosedge());
  EXPECT_FALSE(cell.isClockGateLatchNegedge());
  EXPECT_FALSE(cell.isClockGateOther());

  cell.setClockGateType(ClockGateType::latch_posedge);
  EXPECT_TRUE(cell.isClockGate());
  EXPECT_TRUE(cell.isClockGateLatchPosedge());
  EXPECT_FALSE(cell.isClockGateLatchNegedge());

  cell.setClockGateType(ClockGateType::latch_negedge);
  EXPECT_TRUE(cell.isClockGateLatchNegedge());

  cell.setClockGateType(ClockGateType::other);
  EXPECT_TRUE(cell.isClockGateOther());
}

TEST(TestCellTest, ModeDef) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  ModeDef *mode = cell.makeModeDef("test_mode");
  EXPECT_NE(mode, nullptr);
  EXPECT_EQ(mode->name(), "test_mode");
  const ModeDef *found = cell.findModeDef("test_mode");
  EXPECT_EQ(found, mode);
  EXPECT_EQ(cell.findModeDef("nonexistent"), nullptr);
}

TEST(TestCellTest, CellScaleFactors) {
  ASSERT_NO_THROW(( [&](){
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  ScaleFactors *sf = new ScaleFactors("cell_sf");
  cell.setScaleFactors(sf);
  // Scale factors are used internally during delay calculation

  }() ));
}

TEST(TestCellTest, CellBusDcl) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  BusDcl *bus = cell.makeBusDcl("data", 7, 0);
  BusDcl *found = cell.findBusDcl("data");
  EXPECT_EQ(found, bus);
  EXPECT_EQ(cell.findBusDcl("nonexistent"), nullptr);
}

TEST(TestCellTest, HasInternalPorts) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_FALSE(cell.hasInternalPorts());
}

TEST(TestCellTest, SetAlwaysOn) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "AON1", "test.lib");
  cell.setAlwaysOn(true);
  EXPECT_TRUE(cell.alwaysOn());
}

TEST(TestCellTest, SetIsMemory) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "MEM1", "test.lib");
  cell.setIsMemory(true);
  EXPECT_TRUE(cell.isMemory());
}

TEST(TestCellTest, CellOcvArcDepth) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  cell.setOcvArcDepth(3.0f);
  EXPECT_FLOAT_EQ(cell.ocvArcDepth(), 3.0f);
}

TEST(TestCellTest, CellOcvDerate) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");

  // Without cell-level derate, returns library default
  EXPECT_EQ(cell.ocvDerate(), nullptr);

  OcvDerate *derate = new OcvDerate(stringCopy("cell_ocv"));
  cell.setOcvDerate(derate);
  EXPECT_EQ(cell.ocvDerate(), derate);
}

TEST(TestCellTest, CellAddFindOcvDerate) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  OcvDerate *derate = cell.makeOcvDerate("named_ocv");
  OcvDerate *found = cell.findOcvDerate("named_ocv");
  EXPECT_EQ(found, derate);
  EXPECT_EQ(cell.findOcvDerate("nonexistent"), nullptr);
}

TEST(TestCellTest, LeakagePower) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  float leakage;
  bool exists;
  cell.leakagePower(leakage, exists);
  EXPECT_FALSE(exists);

  cell.setLeakagePower(0.001f);
  cell.leakagePower(leakage, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(leakage, 0.001f);
}

TEST(TestCellTest, TimingArcSetCount) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_EQ(cell.timingArcSetCount(), 0u);
}

////////////////////////////////////////////////////////////////
// ScanSignalType tests
////////////////////////////////////////////////////////////////

TEST(ScanSignalTypeTest, Names) {
  EXPECT_NE(scanSignalTypeName(ScanSignalType::enable), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::enable_inverted), nullptr);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary cell iteration tests
////////////////////////////////////////////////////////////////

TEST(LibertyCellIteratorTest, EmptyLibrary) {
  LibertyLibrary lib("test_lib", "test.lib");
  LibertyCellIterator iter(&lib);
  EXPECT_FALSE(iter.hasNext());
}

////////////////////////////////////////////////////////////////
// checkSlewDegradationAxes Order2 test
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, CheckSlewDegradationAxesOrder2) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.0f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.0f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::output_pin_transition, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::connect_delay, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(0.1f); row0.push_back(0.2f);
  FloatSeq row1; row1.push_back(0.3f); row1.push_back(0.4f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis1, axis2);
  EXPECT_TRUE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

TEST(LibertyLibraryTest, CheckSlewDegradationAxesOrder2Reversed) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.0f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.0f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::connect_delay, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::output_pin_transition, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(0.1f); row0.push_back(0.2f);
  FloatSeq row1; row1.push_back(0.3f); row1.push_back(0.4f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis1, axis2);
  EXPECT_TRUE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

////////////////////////////////////////////////////////////////
// TableTemplate axis tests
////////////////////////////////////////////////////////////////

TEST(TableTemplateTest, BasicConstruction) {
  TableTemplate tmpl("delay_tmpl");
  EXPECT_EQ(tmpl.name(), "delay_tmpl");
  EXPECT_EQ(tmpl.axis1(), nullptr);
  EXPECT_EQ(tmpl.axis2(), nullptr);
  EXPECT_EQ(tmpl.axis3(), nullptr);
}

TEST(TableTemplateTest, ConstructionWithAxes) {
  FloatSeq vals1;
  vals1.push_back(0.1f); vals1.push_back(1.0f);
  FloatSeq vals2;
  vals2.push_back(0.01f); vals2.push_back(0.1f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(vals1));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(vals2));
  TableTemplate tmpl("delay_2d", axis1, axis2, nullptr);
  EXPECT_EQ(tmpl.name(), "delay_2d");
  EXPECT_NE(tmpl.axis1(), nullptr);
  EXPECT_NE(tmpl.axis2(), nullptr);
  EXPECT_EQ(tmpl.axis3(), nullptr);
}

TEST(TableTemplateTest, SetAxes) {
  TableTemplate tmpl("tmpl_set");
  FloatSeq vals;
  vals.push_back(0.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(vals));
  tmpl.setAxis1(axis);
  EXPECT_NE(tmpl.axis1(), nullptr);
  tmpl.setAxis2(axis);
  EXPECT_NE(tmpl.axis2(), nullptr);
  tmpl.setAxis3(axis);
  EXPECT_NE(tmpl.axis3(), nullptr);
}

////////////////////////////////////////////////////////////////
// portLibertyToSta and pwrGndType tests
////////////////////////////////////////////////////////////////

TEST(LibertyUtilTest, PortLibertyToSta) {
  std::string result = portLibertyToSta("simple_port");
  EXPECT_EQ(result, "simple_port");
}

TEST(LibertyUtilTest, PwrGndTypeName) {
  const char *name = pwrGndTypeName(PwrGndType::primary_power);
  EXPECT_NE(name, nullptr);
}

TEST(LibertyUtilTest, FindPwrGndType) {
  PwrGndType type = findPwrGndType("primary_power");
  EXPECT_EQ(type, PwrGndType::primary_power);
}

////////////////////////////////////////////////////////////////
// ScaleFactorPvt name/find tests
////////////////////////////////////////////////////////////////

TEST(ScaleFactorPvtTest, FindByName) {
  EXPECT_EQ(findScaleFactorPvt("process"), ScaleFactorPvt::process);
  EXPECT_EQ(findScaleFactorPvt("volt"), ScaleFactorPvt::volt);
  EXPECT_EQ(findScaleFactorPvt("temp"), ScaleFactorPvt::temp);
  EXPECT_EQ(findScaleFactorPvt("nonexistent"), ScaleFactorPvt::unknown);
}

TEST(ScaleFactorPvtTest, PvtToName) {
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::process), "process");
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::volt), "volt");
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::temp), "temp");
}

////////////////////////////////////////////////////////////////
// ScaleFactorType name/find/suffix tests
////////////////////////////////////////////////////////////////

TEST(ScaleFactorTypeTest, FindByName) {
  EXPECT_EQ(findScaleFactorType("pin_cap"), ScaleFactorType::pin_cap);
  // Note: in the source map, "wire_res" string is mapped to ScaleFactorType::wire_cap
  // and there is no "wire_cap" string entry
  EXPECT_EQ(findScaleFactorType("wire_res"), ScaleFactorType::wire_cap);
  EXPECT_EQ(findScaleFactorType("wire_cap"), ScaleFactorType::unknown);
  EXPECT_EQ(findScaleFactorType("min_period"), ScaleFactorType::min_period);
  EXPECT_EQ(findScaleFactorType("cell"), ScaleFactorType::cell);
  EXPECT_EQ(findScaleFactorType("hold"), ScaleFactorType::hold);
  EXPECT_EQ(findScaleFactorType("setup"), ScaleFactorType::setup);
  EXPECT_EQ(findScaleFactorType("recovery"), ScaleFactorType::recovery);
  EXPECT_EQ(findScaleFactorType("removal"), ScaleFactorType::removal);
  EXPECT_EQ(findScaleFactorType("nochange"), ScaleFactorType::nochange);
  EXPECT_EQ(findScaleFactorType("skew"), ScaleFactorType::skew);
  EXPECT_EQ(findScaleFactorType("leakage_power"), ScaleFactorType::leakage_power);
  EXPECT_EQ(findScaleFactorType("internal_power"), ScaleFactorType::internal_power);
  EXPECT_EQ(findScaleFactorType("transition"), ScaleFactorType::transition);
  EXPECT_EQ(findScaleFactorType("min_pulse_width"), ScaleFactorType::min_pulse_width);
  EXPECT_EQ(findScaleFactorType("nonexistent"), ScaleFactorType::unknown);
}

TEST(ScaleFactorTypeTest, TypeToName) {
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::pin_cap), "pin_cap");
  // Note: wire_cap maps to "wire_res" string in source (implementation quirk)
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::wire_cap), "wire_res");
  // wire_res is not in the map - returns nullptr
  EXPECT_EQ(scaleFactorTypeName(ScaleFactorType::wire_res), nullptr);
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::cell), "cell");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::hold), "hold");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::setup), "setup");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::recovery), "recovery");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::removal), "removal");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::transition), "transition");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::min_pulse_width), "min_pulse_width");
}

TEST(ScaleFactorTypeTest, RiseFallSuffix) {
  EXPECT_TRUE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::cell));
  EXPECT_TRUE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::hold));
  EXPECT_TRUE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::setup));
  EXPECT_TRUE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::recovery));
  EXPECT_TRUE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::removal));
  EXPECT_TRUE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::nochange));
  EXPECT_TRUE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::skew));
  EXPECT_FALSE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::pin_cap));
  EXPECT_FALSE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::wire_cap));
  EXPECT_FALSE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::transition));
  EXPECT_FALSE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::min_pulse_width));
}

TEST(ScaleFactorTypeTest, RiseFallPrefix) {
  EXPECT_TRUE(scaleFactorTypeRiseFallPrefix(ScaleFactorType::transition));
  EXPECT_FALSE(scaleFactorTypeRiseFallPrefix(ScaleFactorType::cell));
  EXPECT_FALSE(scaleFactorTypeRiseFallPrefix(ScaleFactorType::hold));
  EXPECT_FALSE(scaleFactorTypeRiseFallPrefix(ScaleFactorType::pin_cap));
  EXPECT_FALSE(scaleFactorTypeRiseFallPrefix(ScaleFactorType::min_pulse_width));
}

TEST(ScaleFactorTypeTest, LowHighSuffix) {
  EXPECT_TRUE(scaleFactorTypeLowHighSuffix(ScaleFactorType::min_pulse_width));
  EXPECT_FALSE(scaleFactorTypeLowHighSuffix(ScaleFactorType::cell));
  EXPECT_FALSE(scaleFactorTypeLowHighSuffix(ScaleFactorType::transition));
  EXPECT_FALSE(scaleFactorTypeLowHighSuffix(ScaleFactorType::pin_cap));
}

////////////////////////////////////////////////////////////////
// Pvt class tests
////////////////////////////////////////////////////////////////

TEST(PvtTest, Constructor) {
  Pvt pvt(1.0f, 1.1f, 25.0f);
  EXPECT_FLOAT_EQ(pvt.process(), 1.0f);
  EXPECT_FLOAT_EQ(pvt.voltage(), 1.1f);
  EXPECT_FLOAT_EQ(pvt.temperature(), 25.0f);
}

TEST(PvtTest, Setters) {
  Pvt pvt(1.0f, 1.0f, 25.0f);
  pvt.setProcess(1.5f);
  EXPECT_FLOAT_EQ(pvt.process(), 1.5f);
  pvt.setVoltage(0.9f);
  EXPECT_FLOAT_EQ(pvt.voltage(), 0.9f);
  pvt.setTemperature(85.0f);
  EXPECT_FLOAT_EQ(pvt.temperature(), 85.0f);
}

////////////////////////////////////////////////////////////////
// OperatingConditions class tests
////////////////////////////////////////////////////////////////

TEST(OperatingConditionsTest, NameOnlyConstructor) {
  OperatingConditions opcond("typical");
  EXPECT_STREQ(opcond.name(), "typical");
}

TEST(OperatingConditionsTest, FullConstructor) {
  OperatingConditions opcond("worst", 1.0f, 0.9f, 125.0f,
                             WireloadTree::worst_case);
  EXPECT_STREQ(opcond.name(), "worst");
  EXPECT_FLOAT_EQ(opcond.process(), 1.0f);
  EXPECT_FLOAT_EQ(opcond.voltage(), 0.9f);
  EXPECT_FLOAT_EQ(opcond.temperature(), 125.0f);
  EXPECT_EQ(opcond.wireloadTree(), WireloadTree::worst_case);
}

TEST(OperatingConditionsTest, SetWireloadTree) {
  OperatingConditions opcond("typ");
  opcond.setWireloadTree(WireloadTree::balanced);
  EXPECT_EQ(opcond.wireloadTree(), WireloadTree::balanced);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary OperatingConditions tests
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, MakeAndFindOperatingConditions) {
  LibertyLibrary lib("test_lib", "test.lib");
  OperatingConditions *opcond = lib.makeOperatingConditions("typical");
  EXPECT_NE(opcond, nullptr);
  OperatingConditions *found = lib.findOperatingConditions("typical");
  EXPECT_EQ(found, opcond);
  EXPECT_EQ(lib.findOperatingConditions("nonexistent"), nullptr);
}

TEST(LibertyLibraryTest, DefaultOperatingConditions) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.defaultOperatingConditions(), nullptr);
  OperatingConditions *opcond = lib.makeOperatingConditions("typical");
  lib.setDefaultOperatingConditions(opcond);
  EXPECT_EQ(lib.defaultOperatingConditions(), opcond);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary scale factor with cell and pvt
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, ScaleFactorWithCell) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  float sf = lib.scaleFactor(ScaleFactorType::cell, &cell, nullptr);
  EXPECT_FLOAT_EQ(sf, 1.0f);
}

TEST(LibertyLibraryTest, ScaleFactorWithCellAndRf) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  float sf = lib.scaleFactor(ScaleFactorType::cell, RiseFall::riseIndex(),
                             &cell, nullptr);
  EXPECT_FLOAT_EQ(sf, 1.0f);
}

TEST(LibertyLibraryTest, BuffersAndInverters) {
  LibertyLibrary lib("test_lib", "test.lib");
  LibertyCellSeq *bufs = lib.buffers();
  EXPECT_NE(bufs, nullptr);
  // Empty library should have no buffers
  EXPECT_EQ(bufs->size(), 0u);
  LibertyCellSeq *invs = lib.inverters();
  EXPECT_NE(invs, nullptr);
  EXPECT_EQ(invs->size(), 0u);
}

TEST(LibertyLibraryTest, FindLibertyCell) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.findLibertyCell("nonexistent"), nullptr);
}

TEST(LibertyLibraryTest, BusDcls) {
  LibertyLibrary lib("test_lib", "test.lib");
  lib.makeBusDcl("d_bus", 7, 0);
  auto dcls = lib.busDcls();
  EXPECT_GE(dcls.size(), 1u);
}

////////////////////////////////////////////////////////////////
// BusDcl tests
////////////////////////////////////////////////////////////////

TEST(BusDclTest, Properties) {
  BusDcl dcl("data_bus", 15, 0);
  EXPECT_EQ(dcl.name(), "data_bus");
  EXPECT_EQ(dcl.from(), 15);
  EXPECT_EQ(dcl.to(), 0);
}

////////////////////////////////////////////////////////////////
// ModeValueDef tests (via ModeDef)
////////////////////////////////////////////////////////////////

TEST(ModeDefTest, DefineAndFindValue) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  ModeDef *mode = cell.makeModeDef("scan_mode");
  EXPECT_NE(mode, nullptr);

  FuncExpr *cond = FuncExpr::makeOne();
  ModeValueDef *valdef = mode->defineValue("test_value", cond, "A==1");
  EXPECT_NE(valdef, nullptr);
  EXPECT_EQ(valdef->value(), "test_value");
  EXPECT_EQ(valdef->cond(), cond);
  EXPECT_EQ(valdef->sdfCond(), "A==1");

  const ModeValueDef *found = mode->findValueDef("test_value");
  EXPECT_EQ(found, valdef);
  EXPECT_EQ(mode->findValueDef("nonexistent"), nullptr);

  const ModeValueMap *vals = mode->values();
  EXPECT_NE(vals, nullptr);
}

////////////////////////////////////////////////////////////////
// LibertyCell additional getters
////////////////////////////////////////////////////////////////

// isDisabledConstraint / setIsDisabledConstraint removed in MCMM update

TEST(TestCellTest, HasInferedRegTimingArcs) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_FALSE(cell.hasInferedRegTimingArcs());
  cell.setHasInferedRegTimingArcs(true);
  EXPECT_TRUE(cell.hasInferedRegTimingArcs());
}

TEST(TestCellTest, HasSequentials) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_FALSE(cell.hasSequentials());
}

TEST(TestCellTest, SequentialsEmpty) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  auto &seqs = cell.sequentials();
  EXPECT_EQ(seqs.size(), 0u);
}

TEST(TestCellTest, TestCellPtr) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_EQ(cell.testCell(), nullptr);
}

TEST(TestCellTest, LeakagePowerExists) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_FALSE(cell.leakagePowerExists());
  cell.setLeakagePower(0.005f);
  EXPECT_TRUE(cell.leakagePowerExists());
}

TEST(TestCellTest, InternalPowersEmpty) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  auto &powers = cell.internalPowers();
  EXPECT_EQ(powers.size(), 0u);
}

TEST(TestCellTest, LeakagePowersEmpty) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  const auto &leak_powers = cell.leakagePowers();
  EXPECT_EQ(leak_powers.size(), 0u);
}

TEST(TestCellTest, StatetableNull) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_EQ(cell.statetable(), nullptr);
}

TEST(TestCellTest, TimingArcSetsEmpty) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  auto &arcsets = cell.timingArcSets();
  EXPECT_EQ(arcsets.size(), 0u);
}

TEST(TestCellTest, FootprintDefault) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  const char *fp = cell.footprint();
  // Empty string or nullptr for default
  if (fp)
    EXPECT_EQ(fp, "");
}

TEST(TestCellTest, SetFootprint) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  cell.setFootprint("INV_FP");
  EXPECT_STREQ(cell.footprint(), "INV_FP");
}

TEST(TestCellTest, UserFunctionClassDefault) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  const char *ufc = cell.userFunctionClass();
  if (ufc)
    EXPECT_EQ(ufc, "");
}

TEST(TestCellTest, SetUserFunctionClass) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  cell.setUserFunctionClass("inverter");
  EXPECT_STREQ(cell.userFunctionClass(), "inverter");
}

TEST(TestCellTest, SwitchCellTypeGetter) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  cell.setSwitchCellType(SwitchCellType::fine_grain);
  EXPECT_EQ(cell.switchCellType(), SwitchCellType::fine_grain);
}

TEST(TestCellTest, LevelShifterTypeGetter) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  cell.setLevelShifterType(LevelShifterType::LH);
  EXPECT_EQ(cell.levelShifterType(), LevelShifterType::LH);
  cell.setLevelShifterType(LevelShifterType::HL_LH);
  EXPECT_EQ(cell.levelShifterType(), LevelShifterType::HL_LH);
}

TEST(TestCellTest, IsClockCellGetter) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_FALSE(cell.isClockCell());
  cell.setIsClockCell(true);
  EXPECT_TRUE(cell.isClockCell());
}

// Note: timingTypeString is defined in TimingArc.cc but not declared
// in a public header, so we cannot test it directly here.

////////////////////////////////////////////////////////////////
// FindTimingType additional values
////////////////////////////////////////////////////////////////

TEST(TimingTypeTest, FindTimingTypeAdditional) {
  EXPECT_EQ(findTimingType("combinational_rise"), TimingType::combinational_rise);
  EXPECT_EQ(findTimingType("combinational_fall"), TimingType::combinational_fall);
  EXPECT_EQ(findTimingType("recovery_falling"), TimingType::recovery_falling);
  EXPECT_EQ(findTimingType("removal_rising"), TimingType::removal_rising);
  EXPECT_EQ(findTimingType("three_state_enable_rise"), TimingType::three_state_enable_rise);
  EXPECT_EQ(findTimingType("three_state_enable_fall"), TimingType::three_state_enable_fall);
  EXPECT_EQ(findTimingType("three_state_disable_rise"), TimingType::three_state_disable_rise);
  EXPECT_EQ(findTimingType("three_state_disable_fall"), TimingType::three_state_disable_fall);
  EXPECT_EQ(findTimingType("skew_rising"), TimingType::skew_rising);
  EXPECT_EQ(findTimingType("skew_falling"), TimingType::skew_falling);
  EXPECT_EQ(findTimingType("nochange_high_high"), TimingType::nochange_high_high);
  EXPECT_EQ(findTimingType("nochange_high_low"), TimingType::nochange_high_low);
  EXPECT_EQ(findTimingType("nochange_low_high"), TimingType::nochange_low_high);
  EXPECT_EQ(findTimingType("nochange_low_low"), TimingType::nochange_low_low);
  EXPECT_EQ(findTimingType("non_seq_setup_falling"), TimingType::non_seq_setup_falling);
  EXPECT_EQ(findTimingType("non_seq_setup_rising"), TimingType::non_seq_setup_rising);
  EXPECT_EQ(findTimingType("non_seq_hold_falling"), TimingType::non_seq_hold_falling);
  EXPECT_EQ(findTimingType("non_seq_hold_rising"), TimingType::non_seq_hold_rising);
  EXPECT_EQ(findTimingType("retaining_time"), TimingType::retaining_time);
  EXPECT_EQ(findTimingType("min_clock_tree_path"), TimingType::min_clock_tree_path);
  EXPECT_EQ(findTimingType("max_clock_tree_path"), TimingType::max_clock_tree_path);
}

////////////////////////////////////////////////////////////////
// TimingTypeScaleFactorType additional coverage
////////////////////////////////////////////////////////////////

TEST(TimingTypeTest, ScaleFactorTypeAdditional) {
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::recovery_falling),
            ScaleFactorType::recovery);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::removal_rising),
            ScaleFactorType::removal);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::skew_falling),
            ScaleFactorType::skew);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::nochange_high_low),
            ScaleFactorType::nochange);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::nochange_low_high),
            ScaleFactorType::nochange);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::nochange_low_low),
            ScaleFactorType::nochange);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::non_seq_setup_falling),
            ScaleFactorType::setup);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::non_seq_setup_rising),
            ScaleFactorType::setup);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::non_seq_hold_falling),
            ScaleFactorType::hold);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::non_seq_hold_rising),
            ScaleFactorType::hold);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::retaining_time),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::rising_edge),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::falling_edge),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::clear),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::preset),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::three_state_enable),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::three_state_disable),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::three_state_enable_rise),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::three_state_enable_fall),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::three_state_disable_rise),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::three_state_disable_fall),
            ScaleFactorType::cell);
}

////////////////////////////////////////////////////////////////
// ScanSignalType full coverage
////////////////////////////////////////////////////////////////

TEST(ScanSignalTypeTest, AllNames) {
  EXPECT_NE(scanSignalTypeName(ScanSignalType::enable), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::enable_inverted), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::clock), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::clock_a), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::clock_b), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::input), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::input_inverted), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::output), nullptr);
  EXPECT_NE(scanSignalTypeName(ScanSignalType::output_inverted), nullptr);
}

////////////////////////////////////////////////////////////////
// PwrGndType full coverage
////////////////////////////////////////////////////////////////

TEST(LibertyUtilTest, PwrGndTypeAllNames) {
  EXPECT_NE(pwrGndTypeName(PwrGndType::primary_power), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::primary_ground), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::backup_power), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::backup_ground), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::internal_power), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::internal_ground), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::nwell), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::pwell), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::deepnwell), nullptr);
  EXPECT_NE(pwrGndTypeName(PwrGndType::deeppwell), nullptr);
}

TEST(LibertyUtilTest, FindPwrGndTypeAll) {
  EXPECT_EQ(findPwrGndType("primary_ground"), PwrGndType::primary_ground);
  EXPECT_EQ(findPwrGndType("backup_power"), PwrGndType::backup_power);
  EXPECT_EQ(findPwrGndType("backup_ground"), PwrGndType::backup_ground);
  EXPECT_EQ(findPwrGndType("internal_power"), PwrGndType::internal_power);
  EXPECT_EQ(findPwrGndType("internal_ground"), PwrGndType::internal_ground);
  EXPECT_EQ(findPwrGndType("nwell"), PwrGndType::nwell);
  EXPECT_EQ(findPwrGndType("pwell"), PwrGndType::pwell);
  EXPECT_EQ(findPwrGndType("deepnwell"), PwrGndType::deepnwell);
  EXPECT_EQ(findPwrGndType("deeppwell"), PwrGndType::deeppwell);
  EXPECT_EQ(findPwrGndType("nonexistent"), PwrGndType::none);
}

TEST(LibertyUtilTest, PortLibertyToStaWithBrackets) {
  std::string result = portLibertyToSta("bus[0]");
  // Should convert liberty port name to Sta format
  EXPECT_FALSE(result.empty());
}

////////////////////////////////////////////////////////////////
// InternalPowerModel tests
////////////////////////////////////////////////////////////////

TEST(InternalPowerModelTest, PowerLookupOrder0) {
  TablePtr tbl = std::make_shared<Table>(5.0f);
  TableModel *table_model = new TableModel(tbl, nullptr,
                                           ScaleFactorType::internal_power,
                                           RiseFall::rise());
  InternalPowerModel model(table_model);
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV", "test.lib");
  float pwr = model.power(&cell, nullptr, 0.5f, 1.0f);
  EXPECT_FLOAT_EQ(pwr, 5.0f);
}

TEST(InternalPowerModelTest, ReportPowerOrder0) {
  TablePtr tbl = std::make_shared<Table>(3.0f);
  TableModel *table_model = new TableModel(tbl, nullptr,
                                           ScaleFactorType::internal_power,
                                           RiseFall::rise());
  InternalPowerModel model(table_model);
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV", "test.lib");
  std::string report = model.reportPower(&cell, nullptr, 0.5f, 1.0f, 3);
  EXPECT_FALSE(report.empty());
}

TEST(InternalPowerModelTest, PowerLookupOrder1) {
  FloatSeq axis_values;
  axis_values.push_back(0.0f);
  axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f);
  values.push_back(3.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  TableModel *table_model = new TableModel(tbl, nullptr,
                                           ScaleFactorType::internal_power,
                                           RiseFall::rise());
  InternalPowerModel model(table_model);
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV", "test.lib");
  float pwr = model.power(&cell, nullptr, 0.5f, 0.0f);
  EXPECT_GT(pwr, 0.0f);
}

TEST(InternalPowerModelTest, PowerLookupOrder2) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.0f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.0f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(2.0f);
  FloatSeq row1; row1.push_back(3.0f); row1.push_back(4.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis1, axis2);
  TableModel *table_model = new TableModel(tbl, nullptr,
                                           ScaleFactorType::internal_power,
                                           RiseFall::rise());
  InternalPowerModel model(table_model);
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV", "test.lib");
  float pwr = model.power(&cell, nullptr, 0.5f, 0.5f);
  EXPECT_GT(pwr, 0.0f);
}

////////////////////////////////////////////////////////////////
// GateTableModel additional tests
////////////////////////////////////////////////////////////////

TEST(GateTableModelTest, CheckAxesOrder1BadAxis) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  // path_depth is not a valid gate model axis
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f); values.push_back(2.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  EXPECT_FALSE(GateTableModel::checkAxes(tbl));
}

TEST(GateTableModelTest, CheckAxesOrder2BadAxis) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.1f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.1f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(2.0f);
  FloatSeq row1; row1.push_back(3.0f); row1.push_back(4.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis1, axis2);
  EXPECT_FALSE(GateTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// CheckTableModel tests
////////////////////////////////////////////////////////////////

TEST(CheckTableModelTest, CheckAxesOrder0) {
  TablePtr tbl = std::make_shared<Table>(1.0f);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(CheckTableModelTest, CheckAxesOrder1) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::related_pin_transition, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f); values.push_back(2.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(CheckTableModelTest, CheckAxesOrder1BadAxis) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f); values.push_back(2.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  EXPECT_FALSE(CheckTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// ReceiverModel checkAxes
////////////////////////////////////////////////////////////////

TEST(ReceiverModelTest, CheckAxesOrder0False) {
  // Table0 has no axes, ReceiverModel requires input_net_transition axis
  TablePtr tbl = std::make_shared<Table>(1.0f);
  EXPECT_FALSE(ReceiverModel::checkAxes(tbl));
}

TEST(ReceiverModelTest, CheckAxesOrder1Valid) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f); values.push_back(2.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  EXPECT_TRUE(ReceiverModel::checkAxes(tbl));
}

TEST(ReceiverModelTest, CheckAxesOrder1BadAxis) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, std::move(axis_values));
  FloatSeq values;
  values.push_back(1.0f); values.push_back(2.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  EXPECT_FALSE(ReceiverModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// LibertyLibrary checkSlewDegradationAxes bad axis
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, CheckSlewDegradationAxesBadAxis) {
  FloatSeq axis_values;
  axis_values.push_back(0.1f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, std::move(axis_values));
  FloatSeq values;
  values.push_back(0.1f); values.push_back(1.0f);
  TablePtr tbl = std::make_shared<Table>(std::move(values), axis);
  EXPECT_FALSE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

////////////////////////////////////////////////////////////////
// Table report methods (Table0, Table1 report via Report*)
// Covers Table::report virtual functions
////////////////////////////////////////////////////////////////

TEST(Table0Test, ReportValue) {
  Table tbl(42.0f);
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV", "test.lib");
  const Units *units = lib.units();
  std::string report = tbl.reportValue("Power", &cell, nullptr,
                                        0.0f, nullptr, 0.0f, 0.0f,
                                        units->powerUnit(), 3);
  EXPECT_FALSE(report.empty());
}

////////////////////////////////////////////////////////////////
// TableModel with Pvt scaling
////////////////////////////////////////////////////////////////

TEST(TableModelTest, FindValueWithPvtScaling) {
  TablePtr tbl = std::make_shared<Table>(10.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV", "test.lib");
  // Without pvt, scale factor should be 1.0
  float result = model.findValue(&cell, nullptr, 0.0f, 0.0f, 0.0f);
  EXPECT_FLOAT_EQ(result, 10.0f);
}

TEST(TableModelTest, SetScaleFactorType) {
  TablePtr tbl = std::make_shared<Table>(10.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  model.setScaleFactorType(ScaleFactorType::hold);
  // Just verify it doesn't crash
  EXPECT_EQ(model.order(), 0);
}

TEST(TableModelTest, SetIsScaled) {
  TablePtr tbl = std::make_shared<Table>(10.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  model.setIsScaled(true);
  // Verify it doesn't crash
  EXPECT_EQ(model.order(), 0);
}

////////////////////////////////////////////////////////////////
// Table1 findValue with extrapolation info
////////////////////////////////////////////////////////////////

TEST(Table1ExtraTest, FindValueWithExtrapolation) {
  FloatSeq axis_values;
  axis_values.push_back(0.0f);
  axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(axis_values));
  FloatSeq values;
  values.push_back(10.0f);
  values.push_back(20.0f);
  Table tbl(std::move(values), axis);

  // In bounds - single arg findValue
  float result_in = tbl.findValue(0.5f);
  EXPECT_NEAR(result_in, 15.0f, 0.01f);

  // Out of bounds (above) - extrapolation
  float result_above = tbl.findValue(2.0f);
  EXPECT_NEAR(result_above, 30.0f, 0.01f);

  // Out of bounds (below) - extrapolation
  float result_below = tbl.findValue(-1.0f);
  EXPECT_NEAR(result_below, 0.0f, 1.0f);

  // findValueClip - clips to bounds
  float clip_above = tbl.findValueClip(2.0f);
  EXPECT_FLOAT_EQ(clip_above, 20.0f);

  float clip_below = tbl.findValueClip(-1.0f);
  EXPECT_FLOAT_EQ(clip_below, 0.0f);
}

TEST(Table1ExtraTest, ValuesPointer) {
  FloatSeq axis_values;
  axis_values.push_back(0.0f); axis_values.push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(axis_values));
  FloatSeq vals;
  vals.push_back(10.0f); vals.push_back(20.0f);
  Table tbl(std::move(vals), axis);
  FloatSeq *v = tbl.values();
  EXPECT_NE(v, nullptr);
  EXPECT_EQ(v->size(), 2u);
}

TEST(Table1ExtraTest, Axis1ptr) {
  FloatSeq axis_values;
  axis_values.push_back(0.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(axis_values));
  FloatSeq vals;
  vals.push_back(10.0f);
  Table tbl(std::move(vals), axis);
  auto aptr = tbl.axis1ptr();
  EXPECT_NE(aptr, nullptr);
}

////////////////////////////////////////////////////////////////
// Table2 values3 and specific value access
////////////////////////////////////////////////////////////////

TEST(Table2Test, Values3Pointer) {
  FloatSeq ax1_vals;
  ax1_vals.push_back(0.0f); ax1_vals.push_back(1.0f);
  FloatSeq ax2_vals;
  ax2_vals.push_back(0.0f); ax2_vals.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, std::move(ax1_vals));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(ax2_vals));
  FloatTable values;
  FloatSeq row0; row0.push_back(1.0f); row0.push_back(2.0f);
  FloatSeq row1; row1.push_back(3.0f); row1.push_back(4.0f);
  values.push_back(std::move(row0)); values.push_back(std::move(row1));
  Table tbl(std::move(values), axis1, axis2);
  FloatTable *v3 = tbl.values3();
  EXPECT_NE(v3, nullptr);
  EXPECT_EQ(v3->size(), 2u);
}

////////////////////////////////////////////////////////////////
// TableAxis values() pointer test
////////////////////////////////////////////////////////////////

TEST(TableAxisExtraTest, ValuesReference) {
  FloatSeq vals;
  vals.push_back(1.0f); vals.push_back(2.0f);
  TableAxis axis(TableAxisVariable::input_net_transition, std::move(vals));
  const FloatSeq &v = axis.values();
  EXPECT_EQ(v.size(), 2u);
}

////////////////////////////////////////////////////////////////
// TableTemplate name setter test
////////////////////////////////////////////////////////////////

TEST(TableTemplateTest, SetName) {
  TableTemplate tmpl("original_name");
  EXPECT_EQ(tmpl.name(), "original_name");
  tmpl.setName("new_name");
  EXPECT_EQ(tmpl.name(), "new_name");
}

TEST(TableTemplateTest, AxisPtrs) {
  FloatSeq vals1;
  vals1.push_back(0.1f); vals1.push_back(1.0f);
  FloatSeq vals2;
  vals2.push_back(0.01f); vals2.push_back(0.1f);
  FloatSeq vals3;
  vals3.push_back(0.0f); vals3.push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, std::move(vals1));
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, std::move(vals2));
  auto axis3 = std::make_shared<TableAxis>(
    TableAxisVariable::related_pin_transition, std::move(vals3));
  TableTemplate tmpl("tmpl_3d", axis1, axis2, axis3);
  EXPECT_NE(tmpl.axis1ptr(), nullptr);
  EXPECT_NE(tmpl.axis2ptr(), nullptr);
  EXPECT_NE(tmpl.axis3ptr(), nullptr);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary DriverWaveform
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, DriverWaveformDefault) {
  LibertyLibrary lib("test_lib", "test.lib");
  // No driver waveforms added -> default is nullptr
  EXPECT_EQ(lib.driverWaveformDefault(), nullptr);
  EXPECT_EQ(lib.findDriverWaveform("nonexistent"), nullptr);
}

} // namespace sta
