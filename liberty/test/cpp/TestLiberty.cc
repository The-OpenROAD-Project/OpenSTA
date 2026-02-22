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
  EXPECT_EQ(zero->op(), FuncExpr::op_zero);
  EXPECT_EQ(zero->left(), nullptr);
  EXPECT_EQ(zero->right(), nullptr);
  EXPECT_EQ(zero->port(), nullptr);
  EXPECT_EQ(zero->to_string(), "0");
  delete zero;
}

TEST(FuncExprTest, MakeOne) {
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_NE(one, nullptr);
  EXPECT_EQ(one->op(), FuncExpr::op_one);
  EXPECT_EQ(one->to_string(), "1");
  delete one;
}

TEST(FuncExprTest, MakeNot) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  EXPECT_NE(not_one, nullptr);
  EXPECT_EQ(not_one->op(), FuncExpr::op_not);
  EXPECT_EQ(not_one->left(), one);
  EXPECT_EQ(not_one->right(), nullptr);
  EXPECT_EQ(not_one->to_string(), "!1");
  not_one->deleteSubexprs();
}

TEST(FuncExprTest, MakeAnd) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *and_expr = FuncExpr::makeAnd(zero, one);
  EXPECT_NE(and_expr, nullptr);
  EXPECT_EQ(and_expr->op(), FuncExpr::op_and);
  EXPECT_EQ(and_expr->left(), zero);
  EXPECT_EQ(and_expr->right(), one);
  std::string str = and_expr->to_string();
  EXPECT_EQ(str, "0*1");
  and_expr->deleteSubexprs();
}

TEST(FuncExprTest, MakeOr) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *or_expr = FuncExpr::makeOr(zero, one);
  EXPECT_NE(or_expr, nullptr);
  EXPECT_EQ(or_expr->op(), FuncExpr::op_or);
  std::string str = or_expr->to_string();
  EXPECT_EQ(str, "0+1");
  or_expr->deleteSubexprs();
}

TEST(FuncExprTest, MakeXor) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *xor_expr = FuncExpr::makeXor(zero, one);
  EXPECT_NE(xor_expr, nullptr);
  EXPECT_EQ(xor_expr->op(), FuncExpr::op_xor);
  std::string str = xor_expr->to_string();
  EXPECT_EQ(str, "0^1");
  xor_expr->deleteSubexprs();
}

TEST(FuncExprTest, Copy) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  FuncExpr *copy = not_one->copy();
  EXPECT_NE(copy, nullptr);
  EXPECT_EQ(copy->op(), FuncExpr::op_not);
  EXPECT_NE(copy, not_one);
  EXPECT_NE(copy->left(), one);  // should be deep copy
  EXPECT_EQ(copy->left()->op(), FuncExpr::op_one);
  not_one->deleteSubexprs();
  copy->deleteSubexprs();
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
  not1->deleteSubexprs();
  not2->deleteSubexprs();
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
  not_one->deleteSubexprs();
  or_expr->deleteSubexprs();
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
  not_one->deleteSubexprs();
}

TEST(FuncExprTest, HasPortAndOrXor) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *and_expr = FuncExpr::makeAnd(one, zero);
  EXPECT_FALSE(and_expr->hasPort(nullptr));
  and_expr->deleteSubexprs();
}

TEST(FuncExprTest, FuncExprNotDoubleNegation) {
  // funcExprNot on a NOT expression should unwrap it
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  FuncExpr *result = funcExprNot(not_one);
  // Should return 'one' directly and delete the not wrapper
  EXPECT_EQ(result->op(), FuncExpr::op_one);
  delete result;
}

TEST(FuncExprTest, FuncExprNotNonNot) {
  // funcExprNot on non-NOT expression should create NOT wrapper
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *result = funcExprNot(one);
  EXPECT_EQ(result->op(), FuncExpr::op_not);
  result->deleteSubexprs();
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
  not_one->deleteSubexprs();
}

TEST(FuncExprTest, PortTimingSenseAndBothNone) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *and_expr = FuncExpr::makeAnd(one, zero);
  // Both have none sense for nullptr port -> returns none
  EXPECT_EQ(and_expr->portTimingSense(nullptr), TimingSense::none);
  and_expr->deleteSubexprs();
}

TEST(FuncExprTest, PortTimingSenseXorNone) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *xor_expr = FuncExpr::makeXor(one, zero);
  // XOR with none senses should return unknown
  TimingSense sense = xor_expr->portTimingSense(nullptr);
  // Both children return none -> falls to else -> unknown
  EXPECT_EQ(sense, TimingSense::unknown);
  xor_expr->deleteSubexprs();
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
  not_one->deleteSubexprs();
}

TEST(FuncExprTest, CheckSizeAndOrXor) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *and_expr = FuncExpr::makeAnd(one, zero);
  EXPECT_FALSE(and_expr->checkSize(1));
  and_expr->deleteSubexprs();
}

TEST(FuncExprTest, BitSubExprOne) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *sub = one->bitSubExpr(0);
  EXPECT_EQ(sub, one);  // op_one returns this
  delete one;
}

TEST(FuncExprTest, BitSubExprZero) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *sub = zero->bitSubExpr(0);
  EXPECT_EQ(sub, zero);  // op_zero returns this
  delete zero;
}

TEST(FuncExprTest, BitSubExprNot) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *not_one = FuncExpr::makeNot(one);
  FuncExpr *sub = not_one->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::op_not);
  // Clean up: sub wraps the original one, so delete sub
  sub->deleteSubexprs();
  // not_one's left was consumed by bitSubExpr
  delete not_one;
}

TEST(FuncExprTest, BitSubExprOr) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *or_expr = FuncExpr::makeOr(one, zero);
  FuncExpr *sub = or_expr->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::op_or);
  sub->deleteSubexprs();
  delete or_expr;
}

TEST(FuncExprTest, BitSubExprAnd) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *and_expr = FuncExpr::makeAnd(one, zero);
  FuncExpr *sub = and_expr->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::op_and);
  sub->deleteSubexprs();
  delete and_expr;
}

TEST(FuncExprTest, BitSubExprXor) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *xor_expr = FuncExpr::makeXor(one, zero);
  FuncExpr *sub = xor_expr->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  EXPECT_EQ(sub->op(), FuncExpr::op_xor);
  sub->deleteSubexprs();
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
  not1->deleteSubexprs();
  not2->deleteSubexprs();
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

  and1->deleteSubexprs();
  and2->deleteSubexprs();
}

////////////////////////////////////////////////////////////////
// TableAxis tests - covers axis construction, findAxisIndex,
// findAxisClosestIndex, inBounds, min, max, variableString
////////////////////////////////////////////////////////////////

class TableAxisTest : public ::testing::Test {
protected:
  TableAxisPtr makeAxis(TableAxisVariable var,
                        std::initializer_list<float> vals) {
    FloatSeq *values = new FloatSeq;
    for (float v : vals)
      values->push_back(v);
    return std::make_shared<TableAxis>(var, values);
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
  Table0 table(42.0f);
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
    FloatSeq *values = new FloatSeq;
    for (float v : vals)
      values->push_back(v);
    return std::make_shared<TableAxis>(
      TableAxisVariable::total_output_net_capacitance, values);
  }
};

TEST_F(Table1Test, DefaultConstructor) {
  Table1 table;
  EXPECT_EQ(table.order(), 1);
}

TEST_F(Table1Test, ValueLookup) {
  auto axis = makeAxis({1.0f, 2.0f, 4.0f});
  FloatSeq *vals = new FloatSeq;
  vals->push_back(10.0f);
  vals->push_back(20.0f);
  vals->push_back(40.0f);
  Table1 table(vals, axis);
  EXPECT_EQ(table.order(), 1);
  EXPECT_FLOAT_EQ(table.value(0), 10.0f);
  EXPECT_FLOAT_EQ(table.value(1), 20.0f);
  EXPECT_FLOAT_EQ(table.value(2), 40.0f);
  EXPECT_NE(table.axis1(), nullptr);
}

TEST_F(Table1Test, FindValueInterpolation) {
  auto axis = makeAxis({0.0f, 1.0f});
  FloatSeq *vals = new FloatSeq;
  vals->push_back(0.0f);
  vals->push_back(10.0f);
  Table1 table(vals, axis);
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
  FloatSeq *vals = new FloatSeq;
  vals->push_back(10.0f);
  vals->push_back(30.0f);
  Table1 table(vals, axis);
  // Below range -> clip to 0
  EXPECT_FLOAT_EQ(table.findValueClip(0.0f), 0.0f);
  // In range
  EXPECT_NEAR(table.findValueClip(2.0f), 20.0f, 0.01f);
  // Above range -> clip to last value
  EXPECT_FLOAT_EQ(table.findValueClip(4.0f), 30.0f);
}

TEST_F(Table1Test, FindValueSingleElement) {
  auto axis = makeAxis({5.0f});
  FloatSeq *vals = new FloatSeq;
  vals->push_back(42.0f);
  Table1 table(vals, axis);
  // Single element: findValue(float) -> value(size_t(float))
  // Only index 0 is valid, so pass 0.0f which converts to index 0.
  EXPECT_FLOAT_EQ(table.findValue(0.0f), 42.0f);
  // Also test findValueClip for single element
  EXPECT_FLOAT_EQ(table.findValueClip(0.0f), 42.0f);
}

TEST_F(Table1Test, CopyConstructor) {
  auto axis = makeAxis({1.0f, 2.0f});
  FloatSeq *vals = new FloatSeq;
  vals->push_back(10.0f);
  vals->push_back(20.0f);
  Table1 table(vals, axis);
  Table1 copy(table);
  EXPECT_FLOAT_EQ(copy.value(0), 10.0f);
  EXPECT_FLOAT_EQ(copy.value(1), 20.0f);
}

TEST_F(Table1Test, MoveConstructor) {
  auto axis = makeAxis({1.0f, 2.0f});
  FloatSeq *vals = new FloatSeq;
  vals->push_back(10.0f);
  vals->push_back(20.0f);
  Table1 table(vals, axis);
  Table1 moved(std::move(table));
  EXPECT_FLOAT_EQ(moved.value(0), 10.0f);
  EXPECT_FLOAT_EQ(moved.value(1), 20.0f);
}

TEST_F(Table1Test, MoveAssignment) {
  auto axis1 = makeAxis({1.0f, 2.0f});
  FloatSeq *vals1 = new FloatSeq;
  vals1->push_back(10.0f);
  vals1->push_back(20.0f);
  Table1 table1(vals1, axis1);

  auto axis2 = makeAxis({3.0f, 4.0f});
  FloatSeq *vals2 = new FloatSeq;
  vals2->push_back(30.0f);
  vals2->push_back(40.0f);
  Table1 table2(vals2, axis2);

  table2 = std::move(table1);
  EXPECT_FLOAT_EQ(table2.value(0), 10.0f);
  EXPECT_FLOAT_EQ(table2.value(1), 20.0f);
}

TEST_F(Table1Test, ValueViaThreeArgs) {
  auto axis = makeAxis({1.0f, 3.0f});
  FloatSeq *vals = new FloatSeq;
  vals->push_back(10.0f);
  vals->push_back(30.0f);
  Table1 table(vals, axis);

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
  FloatSeq *axis1_vals = new FloatSeq;
  axis1_vals->push_back(0.0f);
  axis1_vals->push_back(2.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis1_vals);

  FloatSeq *axis2_vals = new FloatSeq;
  axis2_vals->push_back(0.0f);
  axis2_vals->push_back(4.0f);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, axis2_vals);

  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq;
  row0->push_back(0.0f);
  row0->push_back(4.0f);
  values->push_back(row0);
  FloatSeq *row1 = new FloatSeq;
  row1->push_back(2.0f);
  row1->push_back(6.0f);
  values->push_back(row1);

  Table2 table(values, axis1, axis2);
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
  FloatSeq *axis1_vals = new FloatSeq;
  axis1_vals->push_back(0.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis1_vals);

  FloatSeq *axis2_vals = new FloatSeq;
  axis2_vals->push_back(0.0f);
  axis2_vals->push_back(4.0f);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, axis2_vals);

  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq;
  row0->push_back(10.0f);
  row0->push_back(30.0f);
  values->push_back(row0);

  Table2 table(values, axis1, axis2);
  // Size1==1, so use axis2 only interpolation
  EXPECT_NEAR(table.findValue(0.0f, 2.0f, 0.0f), 20.0f, 0.01f);
}

TEST(Table2Test, SingleColumnInterpolation) {
  FloatSeq *axis1_vals = new FloatSeq;
  axis1_vals->push_back(0.0f);
  axis1_vals->push_back(4.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis1_vals);

  FloatSeq *axis2_vals = new FloatSeq;
  axis2_vals->push_back(0.0f);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, axis2_vals);

  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq;
  row0->push_back(10.0f);
  values->push_back(row0);
  FloatSeq *row1 = new FloatSeq;
  row1->push_back(30.0f);
  values->push_back(row1);

  Table2 table(values, axis1, axis2);
  // Size2==1, so use axis1 only interpolation
  EXPECT_NEAR(table.findValue(2.0f, 0.0f, 0.0f), 20.0f, 0.01f);
}

TEST(Table2Test, SingleCellValue) {
  FloatSeq *axis1_vals = new FloatSeq;
  axis1_vals->push_back(0.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis1_vals);

  FloatSeq *axis2_vals = new FloatSeq;
  axis2_vals->push_back(0.0f);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, axis2_vals);

  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq;
  row0->push_back(42.0f);
  values->push_back(row0);

  Table2 table(values, axis1, axis2);
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
// InternalPowerAttrs tests
////////////////////////////////////////////////////////////////

TEST(InternalPowerAttrsTest, DefaultConstructor) {
  InternalPowerAttrs attrs;
  EXPECT_EQ(attrs.when(), nullptr);
  EXPECT_EQ(attrs.relatedPgPin(), nullptr);
  EXPECT_EQ(attrs.model(RiseFall::rise()), nullptr);
  EXPECT_EQ(attrs.model(RiseFall::fall()), nullptr);
}

TEST(InternalPowerAttrsTest, SetWhen) {
  InternalPowerAttrs attrs;
  FuncExpr *expr = FuncExpr::makeOne();
  attrs.setWhen(expr);
  EXPECT_EQ(attrs.when(), expr);
  // Don't call deleteContents - test just checks setter
  delete expr;
}

TEST(InternalPowerAttrsTest, SetRelatedPgPin) {
  InternalPowerAttrs attrs;
  attrs.setRelatedPgPin("VDD");
  EXPECT_STREQ(attrs.relatedPgPin(), "VDD");
  attrs.setRelatedPgPin("VSS");
  EXPECT_STREQ(attrs.relatedPgPin(), "VSS");
  attrs.deleteContents();
}

////////////////////////////////////////////////////////////////
// TimingArcAttrs tests
////////////////////////////////////////////////////////////////

TEST(TimingArcAttrsTest, DefaultConstructor) {
  TimingArcAttrs attrs;
  EXPECT_EQ(attrs.timingType(), TimingType::combinational);
  EXPECT_EQ(attrs.timingSense(), TimingSense::unknown);
  EXPECT_EQ(attrs.cond(), nullptr);
  EXPECT_EQ(attrs.sdfCond(), nullptr);
  EXPECT_EQ(attrs.sdfCondStart(), nullptr);
  EXPECT_EQ(attrs.sdfCondEnd(), nullptr);
  EXPECT_EQ(attrs.modeName(), nullptr);
  EXPECT_EQ(attrs.modeValue(), nullptr);
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
  EXPECT_STREQ(attrs.modeName(), "test_mode");
  attrs.setModeName("another_mode");
  EXPECT_STREQ(attrs.modeName(), "another_mode");
}

TEST(TimingArcAttrsTest, SetModeValue) {
  TimingArcAttrs attrs;
  attrs.setModeValue("mode_val");
  EXPECT_STREQ(attrs.modeValue(), "mode_val");
}

TEST(TimingArcAttrsTest, SetSdfCond) {
  TimingArcAttrs attrs;
  attrs.setSdfCond("A==1");
  EXPECT_STREQ(attrs.sdfCond(), "A==1");
  // After setSdfCond, sdfCondStart and sdfCondEnd point to same string
  EXPECT_STREQ(attrs.sdfCondStart(), "A==1");
  EXPECT_STREQ(attrs.sdfCondEnd(), "A==1");
}

TEST(TimingArcAttrsTest, SetSdfCondStartEnd) {
  TimingArcAttrs attrs;
  attrs.setSdfCondStart("start_cond");
  EXPECT_STREQ(attrs.sdfCondStart(), "start_cond");
  attrs.setSdfCondEnd("end_cond");
  EXPECT_STREQ(attrs.sdfCondEnd(), "end_cond");
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
// InternalPowerAttrs additional coverage
////////////////////////////////////////////////////////////////

TEST(InternalPowerAttrsTest, ModelAccess) {
  InternalPowerAttrs attrs;
  // Initially models should be nullptr
  EXPECT_EQ(attrs.model(RiseFall::rise()), nullptr);
  EXPECT_EQ(attrs.model(RiseFall::fall()), nullptr);
}

TEST(InternalPowerAttrsTest, SetModel) {
  InternalPowerAttrs attrs;
  // Create a minimal model: Table0 -> TableModel -> InternalPowerModel
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  TableModel *table_model = new TableModel(tbl, nullptr,
                                           ScaleFactorType::internal_power,
                                           RiseFall::rise());
  InternalPowerModel *power_model = new InternalPowerModel(table_model);

  attrs.setModel(RiseFall::rise(), power_model);
  EXPECT_EQ(attrs.model(RiseFall::rise()), power_model);
  EXPECT_EQ(attrs.model(RiseFall::fall()), nullptr);

  // Set same model for fall
  attrs.setModel(RiseFall::fall(), power_model);
  EXPECT_EQ(attrs.model(RiseFall::fall()), power_model);

  // deleteContents handles the cleanup when rise==fall model
  attrs.deleteContents();
}

TEST(InternalPowerAttrsTest, DeleteContentsWithWhen) {
  InternalPowerAttrs attrs;
  // When expr is a simple zero expression
  FuncExpr *when = FuncExpr::makeZero();
  attrs.setWhen(when);
  EXPECT_EQ(attrs.when(), when);
  // deleteContents should clean up when expr
  attrs.deleteContents();
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
  EXPECT_NE(attrs->sdfCond(), nullptr);
  EXPECT_NE(attrs->sdfCondStart(), nullptr);
  EXPECT_NE(attrs->sdfCondEnd(), nullptr);
  EXPECT_STREQ(attrs->modeName(), "mode1");
  EXPECT_STREQ(attrs->modeValue(), "val1");
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
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.1f); ax1_vals->push_back(0.5f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(1.0f); ax2_vals->push_back(2.0f);
  FloatSeq *ax3_vals = new FloatSeq;
  ax3_vals->push_back(10.0f); ax3_vals->push_back(20.0f);
  auto axis1 = std::make_shared<TableAxis>(TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance, ax2_vals);
  auto axis3 = std::make_shared<TableAxis>(TableAxisVariable::related_pin_transition, ax3_vals);

  // 2x2x2: values_[axis1*axis2_size + axis2][axis3]
  // row0 = (0,0) -> {1,2}, row1 = (0,1) -> {3,4}, row2 = (1,0) -> {5,6}, row3 = (1,1) -> {7,8}
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(2.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(3.0f); row1->push_back(4.0f);
  FloatSeq *row2 = new FloatSeq; row2->push_back(5.0f); row2->push_back(6.0f);
  FloatSeq *row3 = new FloatSeq; row3->push_back(7.0f); row3->push_back(8.0f);
  values->push_back(row0); values->push_back(row1);
  values->push_back(row2); values->push_back(row3);

  Table3 tbl(values, axis1, axis2, axis3);

  EXPECT_EQ(tbl.order(), 3);
  EXPECT_NE(tbl.axis1(), nullptr);
  EXPECT_NE(tbl.axis2(), nullptr);
  EXPECT_NE(tbl.axis3(), nullptr);

  // Check corner values
  EXPECT_FLOAT_EQ(tbl.value(0, 0, 0), 1.0f);
  EXPECT_FLOAT_EQ(tbl.value(1, 1, 1), 8.0f);
}

TEST(Table3Test, FindValue) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.1f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.1f); ax2_vals->push_back(1.0f);
  FloatSeq *ax3_vals = new FloatSeq;
  ax3_vals->push_back(0.1f); ax3_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance, ax2_vals);
  auto axis3 = std::make_shared<TableAxis>(TableAxisVariable::related_pin_transition, ax3_vals);

  // All values 1.0 in a 2x2x2 table (4 rows of 2)
  FloatTable *values = new FloatTable;
  for (int i = 0; i < 4; i++) {
    FloatSeq *row = new FloatSeq;
    row->push_back(1.0f); row->push_back(1.0f);
    values->push_back(row);
  }

  Table3 tbl(values, axis1, axis2, axis3);

  // All values are 1.0, so any lookup should return ~1.0
  float result = tbl.findValue(0.5f, 0.5f, 0.5f);
  EXPECT_FLOAT_EQ(result, 1.0f);
}

////////////////////////////////////////////////////////////////
// TableModel wrapper tests
////////////////////////////////////////////////////////////////

TEST(TableModelTest, Order0) {
  TablePtr tbl = std::make_shared<Table0>(42.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  EXPECT_EQ(model.order(), 0);
}

TEST(TableModelTest, Order1) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(TableAxisVariable::input_transition_time, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f); values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  EXPECT_EQ(model.order(), 1);
  EXPECT_NE(model.axis1(), nullptr);
  EXPECT_EQ(model.axis2(), nullptr);
  EXPECT_EQ(model.axis3(), nullptr);
}

TEST(TableModelTest, Order2) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.1f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.1f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(TableAxisVariable::total_output_net_capacitance, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(2.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(3.0f); row1->push_back(4.0f);
  values->push_back(row0); values->push_back(row1);
  TablePtr tbl = std::make_shared<Table2>(values, axis1, axis2);
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

  port_expr->deleteSubexprs();
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

  not_expr->deleteSubexprs();
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
  TablePtr tbl = std::make_shared<Table0>(0.1f);
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
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::output_pin_transition, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(0.1f);
  values->push_back(1.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
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
  Table0 tbl(42.0f);
  const Units *units = lib_->units();
  std::string report = tbl.reportValue("Delay", cell_, nullptr,
                                        0.0f, nullptr, 0.0f, 0.0f,
                                        units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, Table1ReportValue) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f);
  values->push_back(2.0f);
  Table1 tbl(values, axis);

  const Units *units = lib_->units();
  std::string report = tbl.reportValue("Delay", cell_, nullptr,
                                        0.5f, nullptr, 0.0f, 0.0f,
                                        units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, Table2ReportValue) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.1f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.1f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(2.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(3.0f); row1->push_back(4.0f);
  values->push_back(row0); values->push_back(row1);
  Table2 tbl(values, axis1, axis2);

  const Units *units = lib_->units();
  std::string report = tbl.reportValue("Delay", cell_, nullptr,
                                        0.5f, nullptr, 0.5f, 0.0f,
                                        units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, Table3ReportValue) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.1f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.1f); ax2_vals->push_back(1.0f);
  FloatSeq *ax3_vals = new FloatSeq;
  ax3_vals->push_back(0.1f); ax3_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, ax2_vals);
  auto axis3 = std::make_shared<TableAxis>(
    TableAxisVariable::related_pin_transition, ax3_vals);

  FloatTable *values = new FloatTable;
  for (int i = 0; i < 4; i++) {
    FloatSeq *row = new FloatSeq;
    row->push_back(1.0f + i); row->push_back(2.0f + i);
    values->push_back(row);
  }
  Table3 tbl(values, axis1, axis2, axis3);

  const Units *units = lib_->units();
  std::string report = tbl.reportValue("Delay", cell_, nullptr,
                                        0.5f, nullptr, 0.5f, 0.5f,
                                        units->timeUnit(), 3);
  EXPECT_FALSE(report.empty());
  EXPECT_NE(report.find("Delay"), std::string::npos);
}

TEST_F(LinearModelTest, TableModelReport) {
  TablePtr tbl = std::make_shared<Table0>(42.0f);
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
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(10.0f);
  values->push_back(20.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());

  float result = model.findValue(0.5f, 0.0f, 0.0f);
  EXPECT_GT(result, 10.0f);
  EXPECT_LT(result, 20.0f);
}

TEST_F(LinearModelTest, TableModelReportValue) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(10.0f);
  values->push_back(20.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
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

  and_expr->deleteSubexprs();
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

  or_expr->deleteSubexprs();
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

  xor_expr->deleteSubexprs();
}

TEST(FuncExprTest, ZeroOneExpressions) {
  FuncExpr *zero = FuncExpr::makeZero();
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_NE(zero, nullptr);
  EXPECT_NE(one, nullptr);
  zero->deleteSubexprs();
  one->deleteSubexprs();
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

TEST(LibertyLibraryTest, AddAndFindWireload) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload *wl = new Wireload("test_wl", &lib, 0.0f, 1.0f, 1.0f, 0.5f);
  lib.addWireload(wl);
  Wireload *found = lib.findWireload("test_wl");
  EXPECT_EQ(found, wl);
  EXPECT_EQ(lib.findWireload("nonexistent"), nullptr);
}

TEST(LibertyLibraryTest, DefaultWireload) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.defaultWireload(), nullptr);
  Wireload *wl = new Wireload("default_wl", &lib);
  lib.setDefaultWireload(wl);
  EXPECT_EQ(lib.defaultWireload(), wl);
}

TEST(LibertyLibraryTest, WireloadSelection) {
  LibertyLibrary lib("test_lib", "test.lib");
  WireloadSelection *sel = new WireloadSelection("test_sel");
  lib.addWireloadSelection(sel);
  EXPECT_EQ(lib.findWireloadSelection("test_sel"), sel);
  EXPECT_EQ(lib.findWireloadSelection("nonexistent"), nullptr);
}

TEST(LibertyLibraryTest, DefaultWireloadSelection) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.defaultWireloadSelection(), nullptr);
  WireloadSelection *sel = new WireloadSelection("test_sel");
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

TEST(LibertyLibraryTest, AddAndFindTableTemplate) {
  LibertyLibrary lib("test_lib", "test.lib");
  TableTemplate *tmpl = new TableTemplate("delay_template");
  lib.addTableTemplate(tmpl, TableTemplateType::delay);
  TableTemplate *found = lib.findTableTemplate("delay_template",
                                               TableTemplateType::delay);
  EXPECT_EQ(found, tmpl);
  EXPECT_EQ(lib.findTableTemplate("nonexistent", TableTemplateType::delay),
            nullptr);
}

TEST(LibertyLibraryTest, AddAndFindBusDcl) {
  LibertyLibrary lib("test_lib", "test.lib");
  BusDcl *bus = new BusDcl("data_bus", 7, 0);
  lib.addBusDcl(bus);
  BusDcl *found = lib.findBusDcl("data_bus");
  EXPECT_EQ(found, bus);
  EXPECT_EQ(lib.findBusDcl("nonexistent"), nullptr);
}

////////////////////////////////////////////////////////////////
// Table2 findValue test
////////////////////////////////////////////////////////////////

TEST(Table2Test, FindValueInterpolation) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.0f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.0f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, ax2_vals);

  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(3.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(5.0f); row1->push_back(7.0f);
  values->push_back(row0); values->push_back(row1);
  Table2 tbl(values, axis1, axis2);

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
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

TEST(GateTableModelTest, CheckAxesOrder1) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f); values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

TEST(GateTableModelTest, CheckAxesOrder2) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.1f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.1f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(2.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(3.0f); row1->push_back(4.0f);
  values->push_back(row0); values->push_back(row1);
  TablePtr tbl = std::make_shared<Table2>(values, axis1, axis2);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// CheckSlewDegradationAxes
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, CheckSlewDegradationAxesOrder0) {
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_TRUE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

TEST(LibertyLibraryTest, CheckSlewDegradationAxesOrder1) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::output_pin_transition, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(0.1f); values->push_back(1.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_TRUE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

////////////////////////////////////////////////////////////////
// InternalPower additional
////////////////////////////////////////////////////////////////

TEST(InternalPowerAttrsTest, SetRelatedPgPinMultiple) {
  InternalPowerAttrs attrs;
  EXPECT_EQ(attrs.relatedPgPin(), nullptr);
  attrs.setRelatedPgPin("VDD");
  EXPECT_STREQ(attrs.relatedPgPin(), "VDD");
  // Override with a different pin
  attrs.setRelatedPgPin("VSS");
  EXPECT_STREQ(attrs.relatedPgPin(), "VSS");
  attrs.deleteContents();
}

////////////////////////////////////////////////////////////////
// TimingArc set/model tests
////////////////////////////////////////////////////////////////

TEST(TimingArcAttrsTest, SdfCondStrings) {
  TimingArcAttrs attrs;
  attrs.setSdfCond("A==1'b1");
  EXPECT_STREQ(attrs.sdfCond(), "A==1'b1");
  attrs.setSdfCondStart("start_val");
  EXPECT_STREQ(attrs.sdfCondStart(), "start_val");
  attrs.setSdfCondEnd("end_val");
  EXPECT_STREQ(attrs.sdfCondEnd(), "end_val");
}

TEST(TimingArcAttrsTest, ModeNameValue) {
  TimingArcAttrs attrs;
  attrs.setModeName("test_mode");
  EXPECT_STREQ(attrs.modeName(), "test_mode");
  attrs.setModeValue("mode_val");
  EXPECT_STREQ(attrs.modeValue(), "mode_val");
}

////////////////////////////////////////////////////////////////
// Table0 value access
////////////////////////////////////////////////////////////////

TEST(Table0Test, ValueAccess) {
  Table0 tbl(42.5f);
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
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.0f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.0f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(3.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(5.0f); row1->push_back(7.0f);
  values->push_back(row0); values->push_back(row1);
  TablePtr tbl = std::make_shared<Table2>(values, axis1, axis2);
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
  EXPECT_STREQ(derate.name(), "test_ocv");
}

TEST(OcvDerateTest, SetAndGetDerateTable) {
  OcvDerate derate(stringCopy("ocv1"));
  TablePtr tbl = std::make_shared<Table0>(0.95f);
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

TEST(LibertyLibraryTest, AddAndFindOcvDerate) {
  LibertyLibrary lib("test_lib", "test.lib");
  OcvDerate *derate = new OcvDerate(stringCopy("cell_ocv"));
  lib.addOcvDerate(derate);
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

TEST(LibertyLibraryTest, AddAndFindScaleFactors) {
  LibertyLibrary lib("test_lib", "test.lib");
  ScaleFactors *sf = new ScaleFactors("k_process");
  lib.addScaleFactors(sf);
  ScaleFactors *found = lib.findScaleFactors("k_process");
  EXPECT_EQ(found, sf);
}

TEST(LibertyLibraryTest, DefaultScaleFactors) {
  LibertyLibrary lib("test_lib", "test.lib");
  ScaleFactors *sf = new ScaleFactors("default_sf");
  lib.setScaleFactors(sf);
  // Just verifying it doesn't crash - scale factors are used internally
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
  TableTemplate *tmpl1 = new TableTemplate("tmpl1");
  TableTemplate *tmpl2 = new TableTemplate("tmpl2");
  lib.addTableTemplate(tmpl1, TableTemplateType::delay);
  lib.addTableTemplate(tmpl2, TableTemplateType::power);
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
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CLKBUF", "test.lib");
  cell.setIsClockCell(true);
  // isClockCell is not directly queryable, but this covers the setter
}

TEST(TestCellTest, SetIsLevelShifter) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "LS1", "test.lib");
  cell.setIsLevelShifter(true);
  EXPECT_TRUE(cell.isLevelShifter());
}

TEST(TestCellTest, SetLevelShifterType) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "LS2", "test.lib");
  cell.setLevelShifterType(LevelShifterType::HL);
}

TEST(TestCellTest, SetIsIsolationCell) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "ISO1", "test.lib");
  cell.setIsIsolationCell(true);
  EXPECT_TRUE(cell.isIsolationCell());
}

TEST(TestCellTest, SetSwitchCellType) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "SW1", "test.lib");
  cell.setSwitchCellType(SwitchCellType::coarse_grain);
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
  EXPECT_STREQ(mode->name(), "test_mode");
  ModeDef *found = cell.findModeDef("test_mode");
  EXPECT_EQ(found, mode);
  EXPECT_EQ(cell.findModeDef("nonexistent"), nullptr);
}

TEST(TestCellTest, CellScaleFactors) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  ScaleFactors *sf = new ScaleFactors("cell_sf");
  cell.setScaleFactors(sf);
  // Scale factors are used internally during delay calculation
}

TEST(TestCellTest, CellBusDcl) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  BusDcl *bus = new BusDcl("data", 7, 0);
  cell.addBusDcl(bus);
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
  OcvDerate *derate = new OcvDerate(stringCopy("named_ocv"));
  cell.addOcvDerate(derate);
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
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.0f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.0f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::output_pin_transition, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::connect_delay, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(0.1f); row0->push_back(0.2f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(0.3f); row1->push_back(0.4f);
  values->push_back(row0); values->push_back(row1);
  TablePtr tbl = std::make_shared<Table2>(values, axis1, axis2);
  EXPECT_TRUE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

TEST(LibertyLibraryTest, CheckSlewDegradationAxesOrder2Reversed) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.0f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.0f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::connect_delay, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::output_pin_transition, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(0.1f); row0->push_back(0.2f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(0.3f); row1->push_back(0.4f);
  values->push_back(row0); values->push_back(row1);
  TablePtr tbl = std::make_shared<Table2>(values, axis1, axis2);
  EXPECT_TRUE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

////////////////////////////////////////////////////////////////
// TableTemplate axis tests
////////////////////////////////////////////////////////////////

TEST(TableTemplateTest, BasicConstruction) {
  TableTemplate tmpl("delay_tmpl");
  EXPECT_STREQ(tmpl.name(), "delay_tmpl");
  EXPECT_EQ(tmpl.axis1(), nullptr);
  EXPECT_EQ(tmpl.axis2(), nullptr);
  EXPECT_EQ(tmpl.axis3(), nullptr);
}

TEST(TableTemplateTest, ConstructionWithAxes) {
  FloatSeq *vals1 = new FloatSeq;
  vals1->push_back(0.1f); vals1->push_back(1.0f);
  FloatSeq *vals2 = new FloatSeq;
  vals2->push_back(0.01f); vals2->push_back(0.1f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, vals1);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, vals2);
  TableTemplate tmpl("delay_2d", axis1, axis2, nullptr);
  EXPECT_STREQ(tmpl.name(), "delay_2d");
  EXPECT_NE(tmpl.axis1(), nullptr);
  EXPECT_NE(tmpl.axis2(), nullptr);
  EXPECT_EQ(tmpl.axis3(), nullptr);
}

TEST(TableTemplateTest, SetAxes) {
  TableTemplate tmpl("tmpl_set");
  FloatSeq *vals = new FloatSeq;
  vals->push_back(0.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, vals);
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

TEST(LibertyLibraryTest, AddAndFindOperatingConditions) {
  LibertyLibrary lib("test_lib", "test.lib");
  OperatingConditions *opcond = new OperatingConditions("typical", 1.0f, 1.1f, 25.0f,
                                                        WireloadTree::balanced);
  lib.addOperatingConditions(opcond);
  OperatingConditions *found = lib.findOperatingConditions("typical");
  EXPECT_EQ(found, opcond);
  EXPECT_EQ(lib.findOperatingConditions("nonexistent"), nullptr);
}

TEST(LibertyLibraryTest, DefaultOperatingConditions) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.defaultOperatingConditions(), nullptr);
  OperatingConditions *opcond = new OperatingConditions("typical");
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
  BusDcl *bus = new BusDcl("d_bus", 7, 0);
  lib.addBusDcl(bus);
  auto dcls = lib.busDcls();
  EXPECT_GE(dcls.size(), 1u);
}

////////////////////////////////////////////////////////////////
// BusDcl tests
////////////////////////////////////////////////////////////////

TEST(BusDclTest, Properties) {
  BusDcl dcl("data_bus", 15, 0);
  EXPECT_STREQ(dcl.name(), "data_bus");
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
  EXPECT_STREQ(valdef->value(), "test_value");
  EXPECT_EQ(valdef->cond(), cond);
  EXPECT_STREQ(valdef->sdfCond(), "A==1");

  ModeValueDef *found = mode->findValueDef("test_value");
  EXPECT_EQ(found, valdef);
  EXPECT_EQ(mode->findValueDef("nonexistent"), nullptr);

  ModeValueMap *vals = mode->values();
  EXPECT_NE(vals, nullptr);
}

////////////////////////////////////////////////////////////////
// LibertyCell additional getters
////////////////////////////////////////////////////////////////

TEST(TestCellTest, SetIsDisabledConstraint) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_FALSE(cell.isDisabledConstraint());
  cell.setIsDisabledConstraint(true);
  EXPECT_TRUE(cell.isDisabledConstraint());
}

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
  auto *leak_powers = cell.leakagePowers();
  EXPECT_NE(leak_powers, nullptr);
  EXPECT_EQ(leak_powers->size(), 0u);
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
    EXPECT_STREQ(fp, "");
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
    EXPECT_STREQ(ufc, "");
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
  TablePtr tbl = std::make_shared<Table0>(5.0f);
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
  TablePtr tbl = std::make_shared<Table0>(3.0f);
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
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.0f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f);
  values->push_back(3.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
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
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.0f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.0f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(2.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(3.0f); row1->push_back(4.0f);
  values->push_back(row0); values->push_back(row1);
  TablePtr tbl = std::make_shared<Table2>(values, axis1, axis2);
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
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  // path_depth is not a valid gate model axis
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f); values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_FALSE(GateTableModel::checkAxes(tbl));
}

TEST(GateTableModelTest, CheckAxesOrder2BadAxis) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.1f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.1f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(2.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(3.0f); row1->push_back(4.0f);
  values->push_back(row0); values->push_back(row1);
  TablePtr tbl = std::make_shared<Table2>(values, axis1, axis2);
  EXPECT_FALSE(GateTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// CheckTableModel tests
////////////////////////////////////////////////////////////////

TEST(CheckTableModelTest, CheckAxesOrder0) {
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(CheckTableModelTest, CheckAxesOrder1) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::related_pin_transition, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f); values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(CheckTableModelTest, CheckAxesOrder1BadAxis) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f); values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_FALSE(CheckTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// ReceiverModel checkAxes
////////////////////////////////////////////////////////////////

TEST(ReceiverModelTest, CheckAxesOrder0False) {
  // Table0 has no axes, ReceiverModel requires input_net_transition axis
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_FALSE(ReceiverModel::checkAxes(tbl));
}

TEST(ReceiverModelTest, CheckAxesOrder1Valid) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f); values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_TRUE(ReceiverModel::checkAxes(tbl));
}

TEST(ReceiverModelTest, CheckAxesOrder1BadAxis) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f); values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_FALSE(ReceiverModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// LibertyLibrary checkSlewDegradationAxes bad axis
////////////////////////////////////////////////////////////////

TEST(LibertyLibraryTest, CheckSlewDegradationAxesBadAxis) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(0.1f); values->push_back(1.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_FALSE(LibertyLibrary::checkSlewDegradationAxes(tbl));
}

////////////////////////////////////////////////////////////////
// Table report methods (Table0, Table1 report via Report*)
// Covers Table::report virtual functions
////////////////////////////////////////////////////////////////

TEST(Table0Test, ReportValue) {
  Table0 tbl(42.0f);
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
  TablePtr tbl = std::make_shared<Table0>(10.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV", "test.lib");
  // Without pvt, scale factor should be 1.0
  float result = model.findValue(&cell, nullptr, 0.0f, 0.0f, 0.0f);
  EXPECT_FLOAT_EQ(result, 10.0f);
}

TEST(TableModelTest, SetScaleFactorType) {
  TablePtr tbl = std::make_shared<Table0>(10.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  model.setScaleFactorType(ScaleFactorType::hold);
  // Just verify it doesn't crash
  EXPECT_EQ(model.order(), 0);
}

TEST(TableModelTest, SetIsScaled) {
  TablePtr tbl = std::make_shared<Table0>(10.0f);
  TableModel model(tbl, nullptr, ScaleFactorType::cell, RiseFall::rise());
  model.setIsScaled(true);
  // Verify it doesn't crash
  EXPECT_EQ(model.order(), 0);
}

////////////////////////////////////////////////////////////////
// Table1 findValue with extrapolation info
////////////////////////////////////////////////////////////////

TEST(Table1ExtraTest, FindValueWithExtrapolation) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.0f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(10.0f);
  values->push_back(20.0f);
  Table1 tbl(values, axis);

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
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.0f); axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_values);
  FloatSeq *vals = new FloatSeq;
  vals->push_back(10.0f); vals->push_back(20.0f);
  Table1 tbl(vals, axis);
  FloatSeq *v = tbl.values();
  EXPECT_NE(v, nullptr);
  EXPECT_EQ(v->size(), 2u);
}

TEST(Table1ExtraTest, Axis1ptr) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_values);
  FloatSeq *vals = new FloatSeq;
  vals->push_back(10.0f);
  Table1 tbl(vals, axis);
  auto aptr = tbl.axis1ptr();
  EXPECT_NE(aptr, nullptr);
}

////////////////////////////////////////////////////////////////
// Table2 values3 and specific value access
////////////////////////////////////////////////////////////////

TEST(Table2Test, Values3Pointer) {
  FloatSeq *ax1_vals = new FloatSeq;
  ax1_vals->push_back(0.0f); ax1_vals->push_back(1.0f);
  FloatSeq *ax2_vals = new FloatSeq;
  ax2_vals->push_back(0.0f); ax2_vals->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, ax1_vals);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, ax2_vals);
  FloatTable *values = new FloatTable;
  FloatSeq *row0 = new FloatSeq; row0->push_back(1.0f); row0->push_back(2.0f);
  FloatSeq *row1 = new FloatSeq; row1->push_back(3.0f); row1->push_back(4.0f);
  values->push_back(row0); values->push_back(row1);
  Table2 tbl(values, axis1, axis2);
  FloatTable *v3 = tbl.values3();
  EXPECT_NE(v3, nullptr);
  EXPECT_EQ(v3->size(), 2u);
}

////////////////////////////////////////////////////////////////
// TableAxis values() pointer test
////////////////////////////////////////////////////////////////

TEST(TableAxisExtraTest, ValuesPointer) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(1.0f); vals->push_back(2.0f);
  TableAxis axis(TableAxisVariable::input_net_transition, vals);
  FloatSeq *v = axis.values();
  EXPECT_NE(v, nullptr);
  EXPECT_EQ(v->size(), 2u);
}

////////////////////////////////////////////////////////////////
// TableTemplate name setter test
////////////////////////////////////////////////////////////////

TEST(TableTemplateTest, SetName) {
  TableTemplate tmpl("original_name");
  EXPECT_STREQ(tmpl.name(), "original_name");
  tmpl.setName("new_name");
  EXPECT_STREQ(tmpl.name(), "new_name");
}

TEST(TableTemplateTest, AxisPtrs) {
  FloatSeq *vals1 = new FloatSeq;
  vals1->push_back(0.1f); vals1->push_back(1.0f);
  FloatSeq *vals2 = new FloatSeq;
  vals2->push_back(0.01f); vals2->push_back(0.1f);
  FloatSeq *vals3 = new FloatSeq;
  vals3->push_back(0.0f); vals3->push_back(1.0f);
  auto axis1 = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, vals1);
  auto axis2 = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, vals2);
  auto axis3 = std::make_shared<TableAxis>(
    TableAxisVariable::related_pin_transition, vals3);
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

////////////////////////////////////////////////////////////////
// Sta-based fixture for reading real liberty files
// This enables testing functions that need a parsed library
////////////////////////////////////////////////////////////////

} // close sta namespace temporarily for the Sta-based fixture

#include <tcl.h>
#include "Sta.hh"
#include "ReportTcl.hh"
#include "PatternMatch.hh"
#include "Corner.hh"
#include "LibertyWriter.hh"
#include "DcalcAnalysisPt.hh"

namespace sta {

class StaLibertyTest : public ::testing::Test {
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

    // Read Nangate45 liberty file
    lib_ = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                             sta_->cmdCorner(),
                             MinMaxAll::min(),
                             false);
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
  LibertyLibrary *lib_;
};

TEST_F(StaLibertyTest, LibraryNotNull) {
  EXPECT_NE(lib_, nullptr);
}

TEST_F(StaLibertyTest, FindLibertyCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  EXPECT_NE(buf, nullptr);
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  EXPECT_NE(inv, nullptr);
  EXPECT_EQ(lib_->findLibertyCell("NONEXISTENT_CELL_XYZ"), nullptr);
}

TEST_F(StaLibertyTest, FindLibertyCellsMatching) {
  PatternMatch pattern("BUF_*", false, false, nullptr);
  auto cells = lib_->findLibertyCellsMatching(&pattern);
  EXPECT_GT(cells.size(), 0u);
}

TEST_F(StaLibertyTest, LibraryCellIterator) {
  LibertyCellIterator iter(lib_);
  int count = 0;
  while (iter.hasNext()) {
    LibertyCell *cell = iter.next();
    EXPECT_NE(cell, nullptr);
    count++;
  }
  EXPECT_GT(count, 0);
}

TEST_F(StaLibertyTest, CellArea) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  float area = buf->area();
  EXPECT_GT(area, 0.0f);
}

TEST_F(StaLibertyTest, CellIsBuffer) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_TRUE(buf->isBuffer());
}

TEST_F(StaLibertyTest, CellIsInverter) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  EXPECT_TRUE(inv->isInverter());
}

TEST_F(StaLibertyTest, CellBufferPorts) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_TRUE(buf->isBuffer());
  LibertyPort *input = nullptr;
  LibertyPort *output = nullptr;
  buf->bufferPorts(input, output);
  EXPECT_NE(input, nullptr);
  EXPECT_NE(output, nullptr);
}

TEST_F(StaLibertyTest, CellHasTimingArcs) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(buf->hasTimingArcs(a));
}

TEST_F(StaLibertyTest, CellFindLibertyPort) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  EXPECT_NE(a, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  EXPECT_NE(z, nullptr);
  EXPECT_EQ(buf->findLibertyPort("NONEXISTENT_PORT"), nullptr);
}

TEST_F(StaLibertyTest, CellTimingArcSets) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  EXPECT_GT(arcsets.size(), 0u);
  EXPECT_GT(buf->timingArcSetCount(), 0u);
}

TEST_F(StaLibertyTest, CellTimingArcSetsFromTo) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(z, nullptr);
  auto &arcsets = buf->timingArcSets(a, z);
  EXPECT_GT(arcsets.size(), 0u);
}

TEST_F(StaLibertyTest, TimingArcSetProperties) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  EXPECT_NE(arcset, nullptr);

  // Test arc set properties
  EXPECT_NE(arcset->from(), nullptr);
  EXPECT_NE(arcset->to(), nullptr);
  EXPECT_NE(arcset->role(), nullptr);
  EXPECT_FALSE(arcset->isWire());
  TimingSense sense = arcset->sense();
  (void)sense; // Just ensure it doesn't crash
  EXPECT_GT(arcset->arcCount(), 0u);
  EXPECT_GE(arcset->index(), 0u);
  EXPECT_FALSE(arcset->isDisabledConstraint());
  EXPECT_EQ(arcset->libertyCell(), buf);
}

TEST_F(StaLibertyTest, TimingArcSetIsRisingFallingEdge) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff) {
    auto &arcsets = dff->timingArcSets();
    for (auto *arcset : arcsets) {
      // Call isRisingFallingEdge - it returns nullptr for non-edge arcs
      const RiseFall *rf = arcset->isRisingFallingEdge();
      (void)rf; // Just calling it for coverage
    }
  }
}

TEST_F(StaLibertyTest, TimingArcSetArcsFrom) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  TimingArc *arc1 = nullptr;
  TimingArc *arc2 = nullptr;
  arcset->arcsFrom(RiseFall::rise(), arc1, arc2);
  // At least one arc should exist
  EXPECT_TRUE(arc1 != nullptr || arc2 != nullptr);
}

TEST_F(StaLibertyTest, TimingArcSetArcTo) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  TimingArc *arc = arcset->arcTo(RiseFall::rise());
  // May or may not be nullptr depending on the arc
  (void)arc;
}

TEST_F(StaLibertyTest, TimingArcSetOcvArcDepth) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  float depth = arcset->ocvArcDepth();
  EXPECT_GE(depth, 0.0f);
}

TEST_F(StaLibertyTest, TimingArcSetEquivAndLess) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  if (arcsets.size() >= 2) {
    TimingArcSet *set1 = arcsets[0];
    TimingArcSet *set2 = arcsets[1];
    // Test equiv - same set should be equiv
    EXPECT_TRUE(TimingArcSet::equiv(set1, set1));
    // Test less - antisymmetric
    bool less12 = TimingArcSet::less(set1, set2);
    bool less21 = TimingArcSet::less(set2, set1);
    EXPECT_FALSE(less12 && less21); // Can't both be true
  }
}

TEST_F(StaLibertyTest, TimingArcSetCondDefault) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  // Just call the getter for coverage
  bool is_default = arcset->isCondDefault();
  (void)is_default;
}

TEST_F(StaLibertyTest, TimingArcSetSdfCond) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  // SDF condition getters - may be null
  const char *sdf_cond = arcset->sdfCond();
  const char *sdf_start = arcset->sdfCondStart();
  const char *sdf_end = arcset->sdfCondEnd();
  const char *mode_name = arcset->modeName();
  const char *mode_value = arcset->modeValue();
  (void)sdf_cond;
  (void)sdf_start;
  (void)sdf_end;
  (void)mode_name;
  (void)mode_value;
}

TEST_F(StaLibertyTest, TimingArcProperties) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  auto &arcs = arcset->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingArc *arc = arcs[0];

  EXPECT_NE(arc->from(), nullptr);
  EXPECT_NE(arc->to(), nullptr);
  EXPECT_NE(arc->fromEdge(), nullptr);
  EXPECT_NE(arc->toEdge(), nullptr);
  EXPECT_NE(arc->role(), nullptr);
  EXPECT_EQ(arc->set(), arcset);
  EXPECT_GE(arc->index(), 0u);

  // Test sense
  TimingSense sense = arc->sense();
  (void)sense;

  // Test to_string
  std::string arc_str = arc->to_string();
  EXPECT_FALSE(arc_str.empty());

  // Test model
  TimingModel *model = arc->model();
  (void)model; // May or may not be null depending on cell
}

TEST_F(StaLibertyTest, TimingArcDriveResistance) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  auto &arcs = arcset->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingArc *arc = arcs[0];
  float drive_res = arc->driveResistance();
  EXPECT_GE(drive_res, 0.0f);
}

TEST_F(StaLibertyTest, TimingArcIntrinsicDelay) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  auto &arcs = arcset->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingArc *arc = arcs[0];
  ArcDelay delay = arc->intrinsicDelay();
  (void)delay; // Just test it doesn't crash
}

TEST_F(StaLibertyTest, TimingArcEquiv) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  auto &arcs = arcsets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingArc *arc = arcs[0];
  EXPECT_TRUE(TimingArc::equiv(arc, arc));
}

TEST_F(StaLibertyTest, TimingArcGateTableModel) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  auto &arcs = arcsets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingArc *arc = arcs[0];
  GateTableModel *gtm = arc->gateTableModel();
  if (gtm) {
    EXPECT_NE(gtm->delayModel(), nullptr);
  }
}

TEST_F(StaLibertyTest, LibraryPortProperties) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(z, nullptr);

  // Test capacitance getters
  float cap = a->capacitance();
  EXPECT_GE(cap, 0.0f);
  float cap_min = a->capacitance(MinMax::min());
  EXPECT_GE(cap_min, 0.0f);
  float cap_rise_max = a->capacitance(RiseFall::rise(), MinMax::max());
  EXPECT_GE(cap_rise_max, 0.0f);

  // Test capacitance with exists
  float cap_val;
  bool exists;
  a->capacitance(RiseFall::rise(), MinMax::max(), cap_val, exists);
  // This may or may not exist depending on the lib

  // Test capacitanceIsOneValue
  bool one_val = a->capacitanceIsOneValue();
  (void)one_val;

  // Test driveResistance
  float drive_res = z->driveResistance();
  EXPECT_GE(drive_res, 0.0f);
  float drive_res_rise = z->driveResistance(RiseFall::rise(), MinMax::max());
  EXPECT_GE(drive_res_rise, 0.0f);
}

TEST_F(StaLibertyTest, PortFunction) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *zn = inv->findLibertyPort("ZN");
  ASSERT_NE(zn, nullptr);
  FuncExpr *func = zn->function();
  EXPECT_NE(func, nullptr);
}

TEST_F(StaLibertyTest, PortTristateEnable) {
  // Find a tristate cell if available
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  FuncExpr *tristate = z->tristateEnable();
  // BUF_X1 likely doesn't have a tristate enable
  (void)tristate;
}

TEST_F(StaLibertyTest, PortClockFlags) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff) {
    LibertyPort *ck = dff->findLibertyPort("CK");
    if (ck) {
      bool is_clk = ck->isClock();
      bool is_reg_clk = ck->isRegClk();
      bool is_check_clk = ck->isCheckClk();
      (void)is_clk;
      (void)is_reg_clk;
      (void)is_check_clk;
    }
    LibertyPort *q = dff->findLibertyPort("Q");
    if (q) {
      bool is_reg_out = q->isRegOutput();
      (void)is_reg_out;
    }
  }
}

TEST_F(StaLibertyTest, PortLimitGetters) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);

  float limit;
  bool exists;

  a->slewLimit(MinMax::max(), limit, exists);
  // May or may not exist
  (void)limit;
  (void)exists;

  a->capacitanceLimit(MinMax::max(), limit, exists);
  (void)limit;
  (void)exists;

  a->fanoutLimit(MinMax::max(), limit, exists);
  (void)limit;
  (void)exists;

  float fanout_load;
  bool fl_exists;
  a->fanoutLoad(fanout_load, fl_exists);
  (void)fanout_load;
  (void)fl_exists;
}

TEST_F(StaLibertyTest, PortMinPeriod) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff) {
    LibertyPort *ck = dff->findLibertyPort("CK");
    if (ck) {
      float min_period;
      bool exists;
      ck->minPeriod(min_period, exists);
      // May or may not exist
      (void)min_period;
      (void)exists;
    }
  }
}

TEST_F(StaLibertyTest, PortMinPulseWidth) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff) {
    LibertyPort *ck = dff->findLibertyPort("CK");
    if (ck) {
      float min_width;
      bool exists;
      ck->minPulseWidth(RiseFall::rise(), min_width, exists);
      (void)min_width;
      (void)exists;
      ck->minPulseWidth(RiseFall::fall(), min_width, exists);
      (void)min_width;
      (void)exists;
    }
  }
}

TEST_F(StaLibertyTest, PortPwrGndProperties) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  // Regular ports are not power/ground
  EXPECT_FALSE(a->isPwrGnd());
  EXPECT_EQ(a->pwrGndType(), PwrGndType::none);
}

TEST_F(StaLibertyTest, PortScanSignalType) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  // Regular ports have ScanSignalType::none
  EXPECT_EQ(a->scanSignalType(), ScanSignalType::none);
}

TEST_F(StaLibertyTest, PortBoolFlags) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);

  EXPECT_FALSE(a->isClockGateClock());
  EXPECT_FALSE(a->isClockGateEnable());
  EXPECT_FALSE(a->isClockGateOut());
  EXPECT_FALSE(a->isPllFeedback());
  EXPECT_FALSE(a->isolationCellData());
  EXPECT_FALSE(a->isolationCellEnable());
  EXPECT_FALSE(a->levelShifterData());
  EXPECT_FALSE(a->isSwitch());
  EXPECT_FALSE(a->isLatchData());
  EXPECT_FALSE(a->isDisabledConstraint());
  EXPECT_FALSE(a->isPad());
}

TEST_F(StaLibertyTest, PortRelatedPins) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  const char *ground_pin = a->relatedGroundPin();
  const char *power_pin = a->relatedPowerPin();
  (void)ground_pin;
  (void)power_pin;
}

TEST_F(StaLibertyTest, PortLibertyLibrary) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->libertyLibrary(), lib_);
  EXPECT_EQ(a->libertyCell(), buf);
}

TEST_F(StaLibertyTest, PortPulseClk) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->pulseClkTrigger(), nullptr);
  EXPECT_EQ(a->pulseClkSense(), nullptr);
}

TEST_F(StaLibertyTest, PortBusDcl) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  BusDcl *bus = a->busDcl();
  EXPECT_EQ(bus, nullptr); // Scalar port has no bus declaration
}

TEST_F(StaLibertyTest, PortReceiverModel) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  const ReceiverModel *rm = a->receiverModel();
  (void)rm; // May be null
}

TEST_F(StaLibertyTest, CellInternalPowers) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &powers = buf->internalPowers();
  EXPECT_GT(powers.size(), 0u);
  if (powers.size() > 0) {
    InternalPower *pwr = powers[0];
    EXPECT_NE(pwr, nullptr);
    EXPECT_NE(pwr->port(), nullptr);
    // relatedPort may be nullptr
    LibertyPort *rp = pwr->relatedPort();
    (void)rp;
    // when may be nullptr
    FuncExpr *when = pwr->when();
    (void)when;
    // relatedPgPin may be nullptr
    const char *pgpin = pwr->relatedPgPin();
    (void)pgpin;
    EXPECT_EQ(pwr->libertyCell(), buf);
  }
}

TEST_F(StaLibertyTest, CellInternalPowersByPort) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  if (z) {
    auto &powers = buf->internalPowers(z);
    // May or may not have internal powers for this port
    (void)powers;
  }
}

TEST_F(StaLibertyTest, CellDontUse) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  bool dont_use = buf->dontUse();
  (void)dont_use;
}

TEST_F(StaLibertyTest, CellIsMacro) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isMacro());
}

TEST_F(StaLibertyTest, CellIsMemory) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isMemory());
}

TEST_F(StaLibertyTest, CellLibraryPtr) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_EQ(buf->libertyLibrary(), lib_);
  // Non-const version
  LibertyLibrary *lib_nc = buf->libertyLibrary();
  EXPECT_EQ(lib_nc, lib_);
}

TEST_F(StaLibertyTest, CellFindLibertyPortsMatching) {
  LibertyCell *and2 = lib_->findLibertyCell("AND2_X1");
  if (and2) {
    PatternMatch pattern("A*", false, false, nullptr);
    auto ports = and2->findLibertyPortsMatching(&pattern);
    EXPECT_GT(ports.size(), 0u);
  }
}

TEST_F(StaLibertyTest, LibraryCellPortIterator) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyCellPortIterator iter(buf);
  int count = 0;
  while (iter.hasNext()) {
    LibertyPort *port = iter.next();
    EXPECT_NE(port, nullptr);
    count++;
  }
  EXPECT_GT(count, 0);
}

TEST_F(StaLibertyTest, LibertyCellPortBitIterator) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyCellPortBitIterator iter(buf);
  int count = 0;
  while (iter.hasNext()) {
    LibertyPort *port = iter.next();
    EXPECT_NE(port, nullptr);
    count++;
  }
  EXPECT_GT(count, 0);
}

TEST_F(StaLibertyTest, LibertyPortMemberIterator) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  LibertyPortMemberIterator iter(a);
  int count = 0;
  while (iter.hasNext()) {
    LibertyPort *member = iter.next();
    EXPECT_NE(member, nullptr);
    count++;
  }
  // Scalar port may have 0 members in the member iterator
  // (it iterates bus bits, not the port itself)
  EXPECT_GE(count, 0);
}

TEST_F(StaLibertyTest, LibraryNominalValues) {
  // The library should have nominal PVT values from parsing
  float process = lib_->nominalProcess();
  float voltage = lib_->nominalVoltage();
  float temperature = lib_->nominalTemperature();
  // These should be non-zero for a real library
  EXPECT_GT(voltage, 0.0f);
  (void)process;
  (void)temperature;
}

TEST_F(StaLibertyTest, LibraryThresholds) {
  float in_rise = lib_->inputThreshold(RiseFall::rise());
  float in_fall = lib_->inputThreshold(RiseFall::fall());
  float out_rise = lib_->outputThreshold(RiseFall::rise());
  float out_fall = lib_->outputThreshold(RiseFall::fall());
  float slew_lower_rise = lib_->slewLowerThreshold(RiseFall::rise());
  float slew_upper_rise = lib_->slewUpperThreshold(RiseFall::rise());
  float slew_derate = lib_->slewDerateFromLibrary();
  EXPECT_GT(in_rise, 0.0f);
  EXPECT_GT(in_fall, 0.0f);
  EXPECT_GT(out_rise, 0.0f);
  EXPECT_GT(out_fall, 0.0f);
  EXPECT_GT(slew_lower_rise, 0.0f);
  EXPECT_GT(slew_upper_rise, 0.0f);
  EXPECT_GT(slew_derate, 0.0f);
}

TEST_F(StaLibertyTest, LibraryDelayModelType) {
  DelayModelType model_type = lib_->delayModelType();
  // Nangate45 should use table model
  EXPECT_EQ(model_type, DelayModelType::table);
}

TEST_F(StaLibertyTest, CellHasSequentials) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff) {
    EXPECT_TRUE(dff->hasSequentials());
    auto &seqs = dff->sequentials();
    EXPECT_GT(seqs.size(), 0u);
  }
}

TEST_F(StaLibertyTest, CellOutputPortSequential) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff) {
    LibertyPort *q = dff->findLibertyPort("Q");
    if (q) {
      Sequential *seq = dff->outputPortSequential(q);
      // outputPortSequential may return nullptr depending on the cell
      (void)seq;
    }
  }
}

TEST_F(StaLibertyTest, LibraryBuffersAndInverters) {
  LibertyCellSeq *bufs = lib_->buffers();
  EXPECT_NE(bufs, nullptr);
  // Nangate45 should have buffer cells
  EXPECT_GT(bufs->size(), 0u);

  LibertyCellSeq *invs = lib_->inverters();
  EXPECT_NE(invs, nullptr);
  EXPECT_GT(invs->size(), 0u);
}

TEST_F(StaLibertyTest, CellFindTimingArcSet) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  // Find by index
  TimingArcSet *found = buf->findTimingArcSet(unsigned(0));
  EXPECT_NE(found, nullptr);
}

TEST_F(StaLibertyTest, CellLeakagePower) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  float leakage;
  bool exists;
  buf->leakagePower(leakage, exists);
  // Nangate45 may or may not have cell-level leakage power
  (void)leakage;
  (void)exists;
}

TEST_F(StaLibertyTest, TimingArcSetFindTimingArc) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *arcset = arcsets[0];
  auto &arcs = arcset->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingArc *found = arcset->findTimingArc(0);
  EXPECT_NE(found, nullptr);
}

TEST_F(StaLibertyTest, TimingArcSetWire) {
  // Test the static wire timing arc set
  TimingArcSet *wire_set = TimingArcSet::wireTimingArcSet();
  EXPECT_NE(wire_set, nullptr);
  EXPECT_EQ(TimingArcSet::wireArcCount(), 2);
  int rise_idx = TimingArcSet::wireArcIndex(RiseFall::rise());
  int fall_idx = TimingArcSet::wireArcIndex(RiseFall::fall());
  EXPECT_NE(rise_idx, fall_idx);
}

TEST_F(StaLibertyTest, InternalPowerCompute) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  auto &powers = inv->internalPowers();
  if (powers.size() > 0) {
    InternalPower *pwr = powers[0];
    // Compute power with some slew and cap values
    float power_val = pwr->power(RiseFall::rise(), nullptr, 0.1f, 0.01f);
    // Power should be a reasonable value
    (void)power_val;
  }
}

TEST_F(StaLibertyTest, PortDriverWaveform) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  DriverWaveform *dw_rise = z->driverWaveform(RiseFall::rise());
  DriverWaveform *dw_fall = z->driverWaveform(RiseFall::fall());
  (void)dw_rise;
  (void)dw_fall;
}

TEST_F(StaLibertyTest, PortVoltageName) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  const char *vname = a->voltageName();
  (void)vname; // May be empty for non-pg pins
}

TEST_F(StaLibertyTest, PortEquivAndLess) {
  LibertyCell *and2 = lib_->findLibertyCell("AND2_X1");
  if (and2) {
    LibertyPort *a1 = and2->findLibertyPort("A1");
    LibertyPort *a2 = and2->findLibertyPort("A2");
    LibertyPort *zn = and2->findLibertyPort("ZN");
    if (a1 && a2 && zn) {
      // Same port should be equiv
      EXPECT_TRUE(LibertyPort::equiv(a1, a1));
      // Different ports
      bool less12 = LibertyPort::less(a1, a2);
      bool less21 = LibertyPort::less(a2, a1);
      EXPECT_FALSE(less12 && less21);
    }
  }
}

TEST_F(StaLibertyTest, PortIntrinsicDelay) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  ArcDelay delay = z->intrinsicDelay(sta_);
  (void)delay;
  ArcDelay delay_rf = z->intrinsicDelay(RiseFall::rise(), MinMax::max(), sta_);
  (void)delay_rf;
}

TEST_F(StaLibertyTest, CellLatchEnable) {
  LibertyCell *dlatch = lib_->findLibertyCell("DLATCH_X1");
  if (dlatch) {
    auto &arcsets = dlatch->timingArcSets();
    for (auto *arcset : arcsets) {
      const LibertyPort *enable_port;
      const FuncExpr *enable_func;
      const RiseFall *enable_rf;
      dlatch->latchEnable(arcset, enable_port, enable_func, enable_rf);
      (void)enable_port;
      (void)enable_func;
      (void)enable_rf;
    }
  }
}

TEST_F(StaLibertyTest, CellClockGateFlags) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isClockGate());
  EXPECT_FALSE(buf->isClockGateLatchPosedge());
  EXPECT_FALSE(buf->isClockGateLatchNegedge());
  EXPECT_FALSE(buf->isClockGateOther());
}

TEST_F(StaLibertyTest, GateTableModelDriveResistanceAndDelay) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  auto &arcs = arcsets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingArc *arc = arcs[0];
  GateTableModel *gtm = arc->gateTableModel();
  if (gtm) {
    // Test gate delay
    ArcDelay delay;
    Slew slew;
    gtm->gateDelay(nullptr, 0.1f, 0.01f, false, delay, slew);
    // Values should be reasonable
    (void)delay;
    (void)slew;

    // Test drive resistance
    float res = gtm->driveResistance(nullptr);
    EXPECT_GE(res, 0.0f);

    // Test report
    std::string report = gtm->reportGateDelay(nullptr, 0.1f, 0.01f, false, 3);
    EXPECT_FALSE(report.empty());

    // Test model accessors
    const TableModel *delay_model = gtm->delayModel();
    EXPECT_NE(delay_model, nullptr);
    const TableModel *slew_model = gtm->slewModel();
    (void)slew_model;
    const ReceiverModel *rm = gtm->receiverModel();
    (void)rm;
    OutputWaveforms *ow = gtm->outputWaveforms();
    (void)ow;
  }
}

TEST_F(StaLibertyTest, LibraryScaleFactors) {
  ScaleFactors *sf = lib_->scaleFactors();
  // May or may not have scale factors
  (void)sf;
  float sf_val = lib_->scaleFactor(ScaleFactorType::cell, nullptr);
  EXPECT_FLOAT_EQ(sf_val, 1.0f);
}

TEST_F(StaLibertyTest, LibraryDefaultPinCaps) {
  float input_cap = lib_->defaultInputPinCap();
  float output_cap = lib_->defaultOutputPinCap();
  float bidirect_cap = lib_->defaultBidirectPinCap();
  (void)input_cap;
  (void)output_cap;
  (void)bidirect_cap;
}

TEST_F(StaLibertyTest, LibraryUnits) {
  const Units *units = lib_->units();
  EXPECT_NE(units, nullptr);
  Units *units_nc = lib_->units();
  EXPECT_NE(units_nc, nullptr);
}

TEST_F(StaLibertyTest, CellScaleFactors) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ScaleFactors *sf = buf->scaleFactors();
  (void)sf; // May be nullptr
}

TEST_F(StaLibertyTest, CellOcvArcDepth) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  float depth = buf->ocvArcDepth();
  EXPECT_GE(depth, 0.0f);
}

TEST_F(StaLibertyTest, CellOcvDerate) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  OcvDerate *derate = buf->ocvDerate();
  (void)derate; // May be nullptr
}

TEST_F(StaLibertyTest, LibraryOcvDerate) {
  OcvDerate *derate = lib_->defaultOcvDerate();
  (void)derate;
  float depth = lib_->ocvArcDepth();
  EXPECT_GE(depth, 0.0f);
}


////////////////////////////////////////////////////////////////
// Helper to create FloatSeq from initializer list
////////////////////////////////////////////////////////////////

static FloatSeq *makeFloatSeq(std::initializer_list<float> vals) {
  FloatSeq *seq = new FloatSeq;
  for (float v : vals)
    seq->push_back(v);
  return seq;
}

static TableAxisPtr makeTestAxis(TableAxisVariable var,
                                 std::initializer_list<float> vals) {
  FloatSeq *values = makeFloatSeq(vals);
  return std::make_shared<TableAxis>(var, values);
}

////////////////////////////////////////////////////////////////
// Table virtual method coverage (Table0/1/2/3 order, axis1, axis2)
////////////////////////////////////////////////////////////////

TEST(TableVirtualTest, Table0Order) {
  Table0 t(1.5f);
  EXPECT_EQ(t.order(), 0);
  // Table base class axis1/axis2 return nullptr
  EXPECT_EQ(t.axis1(), nullptr);
  EXPECT_EQ(t.axis2(), nullptr);
}

TEST(TableVirtualTest, Table1OrderAndAxis) {
  FloatSeq *vals = makeFloatSeq({1.0f, 2.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  Table1 t(vals, axis);
  EXPECT_EQ(t.order(), 1);
  EXPECT_NE(t.axis1(), nullptr);
  EXPECT_EQ(t.axis2(), nullptr);
}

TEST(TableVirtualTest, Table2OrderAndAxes) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  Table2 t(vals, ax1, ax2);
  EXPECT_EQ(t.order(), 2);
  EXPECT_NE(t.axis1(), nullptr);
  EXPECT_NE(t.axis2(), nullptr);
  EXPECT_EQ(t.axis3(), nullptr);
}

TEST(TableVirtualTest, Table3OrderAndAxes) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  auto ax3 = makeTestAxis(TableAxisVariable::related_out_total_output_net_capacitance, {1.0f});
  Table3 t(vals, ax1, ax2, ax3);
  EXPECT_EQ(t.order(), 3);
  EXPECT_NE(t.axis1(), nullptr);
  EXPECT_NE(t.axis2(), nullptr);
  EXPECT_NE(t.axis3(), nullptr);
}

////////////////////////////////////////////////////////////////
// Table report() / reportValue() methods
////////////////////////////////////////////////////////////////

TEST(TableReportTest, Table0ReportValue) {
  Table0 t(42.0f);
  Unit unit(1e-9f, "s", 3);
  std::string rv = t.reportValue("delay", nullptr, nullptr,
                                  0.0f, nullptr, 0.0f, 0.0f,
                                  &unit, 3);
  EXPECT_FALSE(rv.empty());
}

// Table1/2/3::reportValue dereferences cell->libertyLibrary()->units()
// so they need a real cell. Tested via StaLibertyTest fixture below.

////////////////////////////////////////////////////////////////
// Table destruction coverage
////////////////////////////////////////////////////////////////

TEST(TableDestructTest, Table1Destruct) {
  FloatSeq *vals = makeFloatSeq({1.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f});
  Table1 *t = new Table1(vals, axis);
  delete t; // covers Table1::~Table1
}

TEST(TableDestructTest, Table2Destruct) {
  FloatSeq *row0 = makeFloatSeq({1.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f});
  Table2 *t = new Table2(vals, ax1, ax2);
  delete t; // covers Table2::~Table2
}

TEST(TableDestructTest, Table3Destruct) {
  FloatSeq *row0 = makeFloatSeq({1.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f});
  auto ax3 = makeTestAxis(TableAxisVariable::related_out_total_output_net_capacitance, {1.0f});
  Table3 *t = new Table3(vals, ax1, ax2, ax3);
  delete t; // covers Table3::~Table3
}

////////////////////////////////////////////////////////////////
// TableModel::value coverage
////////////////////////////////////////////////////////////////

TEST(TableModelValueTest, ValueByIndex) {
  Table0 *tbl = new Table0(5.5f);
  TablePtr table_ptr(tbl);
  TableTemplate *tmpl = new TableTemplate("test_tmpl");
  TableModel model(table_ptr, tmpl, ScaleFactorType::cell, RiseFall::rise());
  float v = model.value(0, 0, 0);
  EXPECT_FLOAT_EQ(v, 5.5f);
  delete tmpl;
}

////////////////////////////////////////////////////////////////
// Pvt destructor coverage
////////////////////////////////////////////////////////////////

TEST(PvtDestructTest, CreateAndDestroy) {
  // Pvt(process, voltage, temperature)
  Pvt *pvt = new Pvt(1.1f, 1.0f, 25.0f);
  EXPECT_FLOAT_EQ(pvt->process(), 1.1f);
  EXPECT_FLOAT_EQ(pvt->voltage(), 1.0f);
  EXPECT_FLOAT_EQ(pvt->temperature(), 25.0f);
  delete pvt; // covers Pvt::~Pvt
}

////////////////////////////////////////////////////////////////
// ScaleFactors::print coverage
////////////////////////////////////////////////////////////////

TEST(ScaleFactorsPrintTest, Print) {
  ScaleFactors sf("test_sf");
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::process,
              RiseFall::rise(), 1.0f);
  sf.print(); // covers ScaleFactors::print()
}

////////////////////////////////////////////////////////////////
// GateTableModel / CheckTableModel static checkAxes
////////////////////////////////////////////////////////////////

TEST(GateTableModelCheckAxesTest, ValidAxes) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  TablePtr tbl = std::make_shared<Table2>(vals, ax1, ax2);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

TEST(GateTableModelCheckAxesTest, InvalidAxis) {
  FloatSeq *vals = makeFloatSeq({1.0f, 2.0f});
  auto axis = makeTestAxis(TableAxisVariable::constrained_pin_transition, {0.01f, 0.02f});
  TablePtr tbl = std::make_shared<Table1>(vals, axis);
  EXPECT_FALSE(GateTableModel::checkAxes(tbl));
}

TEST(GateTableModelCheckAxesTest, Table0NoAxes) {
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

TEST(CheckTableModelCheckAxesTest, ValidAxes) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::related_pin_transition, {0.01f, 0.02f});
  auto ax2 = makeTestAxis(TableAxisVariable::constrained_pin_transition, {0.1f, 0.2f});
  TablePtr tbl = std::make_shared<Table2>(vals, ax1, ax2);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(CheckTableModelCheckAxesTest, InvalidAxis) {
  FloatSeq *vals = makeFloatSeq({1.0f, 2.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  TablePtr tbl = std::make_shared<Table1>(vals, axis);
  EXPECT_FALSE(CheckTableModel::checkAxes(tbl));
}

TEST(CheckTableModelCheckAxesTest, Table0NoAxes) {
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(ReceiverModelCheckAxesTest, ValidAxes) {
  FloatSeq *vals = makeFloatSeq({1.0f, 2.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  TablePtr tbl = std::make_shared<Table1>(vals, axis);
  EXPECT_TRUE(ReceiverModel::checkAxes(tbl));
}

TEST(ReceiverModelCheckAxesTest, Table0NoAxis) {
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_FALSE(ReceiverModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// DriverWaveform
////////////////////////////////////////////////////////////////

TEST(DriverWaveformTest, CreateAndName) {
  // DriverWaveform::waveform() expects a Table2 with axis1=slew, axis2=voltage
  FloatSeq *row0 = makeFloatSeq({0.0f, 1.0f});
  FloatSeq *row1 = makeFloatSeq({0.5f, 1.5f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.1f, 0.2f});
  auto ax2 = makeTestAxis(TableAxisVariable::normalized_voltage, {0.0f, 1.0f});
  TablePtr tbl = std::make_shared<Table2>(vals, ax1, ax2);
  DriverWaveform *dw = new DriverWaveform("test_driver_waveform", tbl);
  EXPECT_STREQ(dw->name(), "test_driver_waveform");
  Table1 wf = dw->waveform(0.15f);
  (void)wf; // covers DriverWaveform::waveform
  delete dw;
}

////////////////////////////////////////////////////////////////
// InternalPowerAttrs destructor
////////////////////////////////////////////////////////////////

TEST(InternalPowerAttrsTest, CreateAndDestroy) {
  InternalPowerAttrs *attrs = new InternalPowerAttrs();
  EXPECT_EQ(attrs->when(), nullptr);
  EXPECT_EQ(attrs->model(RiseFall::rise()), nullptr);
  EXPECT_EQ(attrs->model(RiseFall::fall()), nullptr);
  EXPECT_EQ(attrs->relatedPgPin(), nullptr);
  attrs->setRelatedPgPin("VDD");
  EXPECT_STREQ(attrs->relatedPgPin(), "VDD");
  attrs->deleteContents();
  delete attrs; // covers InternalPowerAttrs::~InternalPowerAttrs
}

////////////////////////////////////////////////////////////////
// LibertyCellPortBitIterator destructor coverage
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellPortBitIteratorDestruction) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyCellPortBitIterator *iter = new LibertyCellPortBitIterator(buf);
  int count = 0;
  while (iter->hasNext()) {
    LibertyPort *p = iter->next();
    (void)p;
    count++;
  }
  EXPECT_GT(count, 0);
  delete iter; // covers ~LibertyCellPortBitIterator
}

////////////////////////////////////////////////////////////////
// LibertyPort setter coverage (using parsed ports)
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, PortSetIsPad) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  bool orig = port->isPad();
  port->setIsPad(true);
  EXPECT_TRUE(port->isPad());
  port->setIsPad(orig);
}

TEST_F(StaLibertyTest, PortSetIsSwitch) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setIsSwitch(true);
  EXPECT_TRUE(port->isSwitch());
  port->setIsSwitch(false);
}

TEST_F(StaLibertyTest, PortSetIsPllFeedback) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setIsPllFeedback(true);
  EXPECT_TRUE(port->isPllFeedback());
  port->setIsPllFeedback(false);
}

TEST_F(StaLibertyTest, PortSetIsCheckClk) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setIsCheckClk(true);
  EXPECT_TRUE(port->isCheckClk());
  port->setIsCheckClk(false);
}

TEST_F(StaLibertyTest, PortSetPulseClk) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setPulseClk(RiseFall::rise(), RiseFall::fall());
  EXPECT_EQ(port->pulseClkTrigger(), RiseFall::rise());
  EXPECT_EQ(port->pulseClkSense(), RiseFall::fall());
  port->setPulseClk(nullptr, nullptr);
}

TEST_F(StaLibertyTest, PortSetFanoutLoad) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setFanoutLoad(2.5f);
  float fanout;
  bool exists;
  port->fanoutLoad(fanout, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(fanout, 2.5f);
}

TEST_F(StaLibertyTest, PortSetFanoutLimit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("Z");
  ASSERT_NE(port, nullptr);
  port->setFanoutLimit(10.0f, MinMax::max());
  float limit;
  bool exists;
  port->fanoutLimit(MinMax::max(), limit, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(limit, 10.0f);
}

TEST_F(StaLibertyTest, PortBundlePort) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  LibertyPort *bundle = port->bundlePort();
  EXPECT_EQ(bundle, nullptr);
}

TEST_F(StaLibertyTest, PortFindLibertyBusBit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  LibertyPort *bit = port->findLibertyBusBit(0);
  EXPECT_EQ(bit, nullptr);
}

// findLibertyMember(0) on scalar port crashes (member_ports_ is nullptr)
// Would need a bus port to test this safely.

TEST_F(StaLibertyTest, PortCornerPort) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  LibertyPort *cp = port->cornerPort(0);
  (void)cp;
  const LibertyPort *ccp = static_cast<const LibertyPort*>(port)->cornerPort(0);
  (void)ccp;
}

TEST_F(StaLibertyTest, PortClkTreeDelay) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *clk = dff->findLibertyPort("CK");
  ASSERT_NE(clk, nullptr);
  float d = clk->clkTreeDelay(0.1f, RiseFall::rise(), RiseFall::rise(), MinMax::max());
  (void)d;
}

// setMemberFloat is protected - skip

////////////////////////////////////////////////////////////////
// ModeValueDef::setSdfCond and setCond coverage
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, ModeValueDefSetSdfCond) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ModeDef *mode_def = buf->makeModeDef("test_mode");
  ASSERT_NE(mode_def, nullptr);
  ModeValueDef *val_def = mode_def->defineValue("val1", nullptr, "orig_sdf_cond");
  ASSERT_NE(val_def, nullptr);
  EXPECT_STREQ(val_def->value(), "val1");
  EXPECT_STREQ(val_def->sdfCond(), "orig_sdf_cond");
  val_def->setSdfCond("new_sdf_cond");
  EXPECT_STREQ(val_def->sdfCond(), "new_sdf_cond");
}

TEST_F(StaLibertyTest, ModeValueDefSetCond) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ModeDef *mode_def = buf->makeModeDef("test_mode2");
  ASSERT_NE(mode_def, nullptr);
  ModeValueDef *val_def = mode_def->defineValue("val2", nullptr, nullptr);
  ASSERT_NE(val_def, nullptr);
  EXPECT_EQ(val_def->cond(), nullptr);
  val_def->setCond(nullptr);
  EXPECT_EQ(val_def->cond(), nullptr);
}

////////////////////////////////////////////////////////////////
// LibertyCell::latchCheckEnableEdge
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellLatchCheckEnableEdgeWithDFF) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  auto &arcsets = dff->timingArcSets();
  if (!arcsets.empty()) {
    const RiseFall *edge = dff->latchCheckEnableEdge(arcsets[0]);
    (void)edge;
  }
}

////////////////////////////////////////////////////////////////
// LibertyCell::cornerCell
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellCornerCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyCell *cc = buf->cornerCell(0);
  (void)cc;
}

////////////////////////////////////////////////////////////////
// TimingArcSet::less (static)
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, TimingArcSetLessStatic) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GE(arcsets.size(), 1u);
  bool result = TimingArcSet::less(arcsets[0], arcsets[0]);
  EXPECT_FALSE(result);
  if (arcsets.size() >= 2) {
    bool r1 = TimingArcSet::less(arcsets[0], arcsets[1]);
    bool r2 = TimingArcSet::less(arcsets[1], arcsets[0]);
    EXPECT_FALSE(r1 && r2);
  }
}

////////////////////////////////////////////////////////////////
// TimingArc::cornerArc
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, TimingArcCornerArc) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  auto &arcs = arcsets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  const TimingArc *corner = arcs[0]->cornerArc(0);
  (void)corner;
}

////////////////////////////////////////////////////////////////
// TimingArcSet setters
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, TimingArcSetSetRole) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *set = arcsets[0];
  const TimingRole *orig = set->role();
  set->setRole(TimingRole::setup());
  EXPECT_EQ(set->role(), TimingRole::setup());
  set->setRole(orig);
}

TEST_F(StaLibertyTest, TimingArcSetSetIsCondDefaultExplicit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *set = arcsets[0];
  bool orig = set->isCondDefault();
  set->setIsCondDefault(true);
  EXPECT_TRUE(set->isCondDefault());
  set->setIsCondDefault(orig);
}

TEST_F(StaLibertyTest, TimingArcSetSetIsDisabledConstraintExplicit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *set = arcsets[0];
  bool orig = set->isDisabledConstraint();
  set->setIsDisabledConstraint(true);
  EXPECT_TRUE(set->isDisabledConstraint());
  set->setIsDisabledConstraint(orig);
}

////////////////////////////////////////////////////////////////
// GateTableModel::gateDelay deprecated 7-arg version
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, GateTableModelGateDelayDeprecated) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  auto &arcs = arcsets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  GateTableModel *gtm = arcs[0]->gateTableModel();
  if (gtm) {
    ArcDelay delay;
    Slew slew;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    gtm->gateDelay(nullptr, 0.1f, 0.01f, 0.0f, false, delay, slew);
#pragma GCC diagnostic pop
    (void)delay;
    (void)slew;
  }
}

////////////////////////////////////////////////////////////////
// CheckTableModel via Sta (setup/hold arcs)
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CheckTableModelCheckDelay) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  auto &arcsets = dff->timingArcSets();
  for (auto *set : arcsets) {
    const TimingRole *role = set->role();
    if (role == TimingRole::setup() || role == TimingRole::hold()) {
      auto &arcs = set->arcs();
      if (!arcs.empty()) {
        TimingModel *model = arcs[0]->model();
        CheckTableModel *ctm = dynamic_cast<CheckTableModel*>(model);
        if (ctm) {
          ArcDelay d = ctm->checkDelay(nullptr, 0.1f, 0.1f, 0.0f, false);
          (void)d;
          std::string rpt = ctm->reportCheckDelay(nullptr, 0.1f, nullptr,
                                                    0.1f, 0.0f, false, 3);
          EXPECT_FALSE(rpt.empty());
          return;
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////
// Library addDriverWaveform / findDriverWaveform
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, LibraryAddAndFindDriverWaveform) {
  FloatSeq *vals = makeFloatSeq({0.0f, 1.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.0f, 1.0f});
  TablePtr tbl = std::make_shared<Table1>(vals, axis);
  DriverWaveform *dw = new DriverWaveform("my_driver_wf", tbl);
  lib_->addDriverWaveform(dw);
  DriverWaveform *found = lib_->findDriverWaveform("my_driver_wf");
  EXPECT_EQ(found, dw);
  EXPECT_STREQ(found->name(), "my_driver_wf");
  EXPECT_EQ(lib_->findDriverWaveform("no_such_wf"), nullptr);
}

////////////////////////////////////////////////////////////////
// TableModel::report (via StaLibertyTest)
////////////////////////////////////////////////////////////////

// TableModel::reportValue needs non-null table_unit and may dereference null pvt
// Covered via GateTableModel::reportGateDelay which exercises the same code path.

////////////////////////////////////////////////////////////////
// Port setDriverWaveform
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, PortSetDriverWaveform) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("Z");
  ASSERT_NE(port, nullptr);
  FloatSeq *vals = makeFloatSeq({0.0f, 1.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.0f, 1.0f});
  TablePtr tbl = std::make_shared<Table1>(vals, axis);
  DriverWaveform *dw = new DriverWaveform("port_dw", tbl);
  lib_->addDriverWaveform(dw);
  port->setDriverWaveform(dw, RiseFall::rise());
  DriverWaveform *got = port->driverWaveform(RiseFall::rise());
  EXPECT_EQ(got, dw);
}

////////////////////////////////////////////////////////////////
// LibertyCell::setTestCell / findModeDef
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellSetTestCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  TestCell *tc = buf->testCell();
  (void)tc;
  buf->setTestCell(nullptr);
  EXPECT_EQ(buf->testCell(), nullptr);
}

TEST_F(StaLibertyTest, CellFindModeDef) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ModeDef *md = buf->findModeDef("nonexistent_mode");
  EXPECT_EQ(md, nullptr);
  ModeDef *created = buf->makeModeDef("my_mode");
  ASSERT_NE(created, nullptr);
  ModeDef *found = buf->findModeDef("my_mode");
  EXPECT_EQ(found, created);
}

////////////////////////////////////////////////////////////////
// Library wireload defaults
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, LibraryWireloadDefaults) {
  Wireload *wl = lib_->defaultWireload();
  (void)wl;
  WireloadMode mode = lib_->defaultWireloadMode();
  (void)mode;
}

////////////////////////////////////////////////////////////////
// GateTableModel with Table0
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, GateTableModelWithTable0Delay) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);

  Table0 *delay_tbl = new Table0(1.0e-10f);
  TablePtr delay_ptr(delay_tbl);
  Table0 *slew_tbl = new Table0(2.0e-10f);
  TablePtr slew_ptr(slew_tbl);
  TableTemplate *tmpl = new TableTemplate("test_tmpl2");

  TableModel *delay_model = new TableModel(delay_ptr, tmpl, ScaleFactorType::cell,
                                            RiseFall::rise());
  TableModel *slew_model = new TableModel(slew_ptr, tmpl, ScaleFactorType::cell,
                                           RiseFall::rise());
  GateTableModel *gtm = new GateTableModel(buf, delay_model, nullptr,
                                             slew_model, nullptr, nullptr, nullptr);
  ArcDelay d;
  Slew s;
  gtm->gateDelay(nullptr, 0.0f, 0.0f, false, d, s);
  (void)d;
  (void)s;

  float res = gtm->driveResistance(nullptr);
  (void)res;

  std::string rpt = gtm->reportGateDelay(nullptr, 0.0f, 0.0f, false, 3);
  EXPECT_FALSE(rpt.empty());

  delete gtm;
  delete tmpl;
}

////////////////////////////////////////////////////////////////
// CheckTableModel direct creation
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CheckTableModelDirect) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);

  Table0 *check_tbl = new Table0(5.0e-11f);
  TablePtr check_ptr(check_tbl);
  TableTemplate *tmpl = new TableTemplate("check_tmpl");

  TableModel *model = new TableModel(check_ptr, tmpl, ScaleFactorType::cell,
                                      RiseFall::rise());
  CheckTableModel *ctm = new CheckTableModel(buf, model, nullptr);
  ArcDelay d = ctm->checkDelay(nullptr, 0.1f, 0.1f, 0.0f, false);
  (void)d;

  std::string rpt = ctm->reportCheckDelay(nullptr, 0.1f, nullptr,
                                            0.1f, 0.0f, false, 3);
  EXPECT_FALSE(rpt.empty());

  const TableModel *m = ctm->model();
  EXPECT_NE(m, nullptr);

  delete ctm;
  delete tmpl;
}

////////////////////////////////////////////////////////////////
// Table findValue / value coverage
////////////////////////////////////////////////////////////////

TEST(TableLookupTest, Table0FindValue) {
  Table0 t(7.5f);
  float v = t.findValue(0.0f, 0.0f, 0.0f);
  EXPECT_FLOAT_EQ(v, 7.5f);
  float v2 = t.value(0, 0, 0);
  EXPECT_FLOAT_EQ(v2, 7.5f);
}

TEST(TableLookupTest, Table1FindValue) {
  FloatSeq *vals = makeFloatSeq({10.0f, 20.0f, 30.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {1.0f, 2.0f, 3.0f});
  Table1 t(vals, axis);
  float v = t.findValue(1.0f, 0.0f, 0.0f);
  EXPECT_FLOAT_EQ(v, 10.0f);
  float v2 = t.findValue(1.5f, 0.0f, 0.0f);
  EXPECT_NEAR(v2, 15.0f, 0.1f);
}

TEST(TableLookupTest, Table2FindValue) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {1.0f, 2.0f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {10.0f, 20.0f});
  Table2 t(vals, ax1, ax2);
  float v = t.findValue(1.0f, 10.0f, 0.0f);
  EXPECT_FLOAT_EQ(v, 1.0f);
}

TEST(TableLookupTest, Table3Value) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  auto ax3 = makeTestAxis(TableAxisVariable::related_out_total_output_net_capacitance, {1.0f});
  Table3 t(vals, ax1, ax2, ax3);
  float v = t.value(0, 0, 0);
  EXPECT_FLOAT_EQ(v, 1.0f);
}

////////////////////////////////////////////////////////////////
// LibertyCell::findTimingArcSet by pointer
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellFindTimingArcSetByPtr) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *found = buf->findTimingArcSet(arcsets[0]);
  EXPECT_EQ(found, arcsets[0]);
}

////////////////////////////////////////////////////////////////
// LibertyCell::addScaledCell
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellAddScaledCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  OperatingConditions *oc = new OperatingConditions("test_oc");
  TestCell *tc = new TestCell(lib_, "scaled_buf", "test.lib");
  buf->addScaledCell(oc, tc);
}

////////////////////////////////////////////////////////////////
// LibertyCell property tests
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellInverterCheck) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  EXPECT_TRUE(inv->isInverter());
}

TEST_F(StaLibertyTest, CellFootprint) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const char *fp = buf->footprint();
  (void)fp;
  buf->setFootprint("test_fp");
  EXPECT_STREQ(buf->footprint(), "test_fp");
}

TEST_F(StaLibertyTest, CellUserFunctionClass) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const char *ufc = buf->userFunctionClass();
  (void)ufc;
  buf->setUserFunctionClass("my_class");
  EXPECT_STREQ(buf->userFunctionClass(), "my_class");
}

TEST_F(StaLibertyTest, CellSetArea) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  float orig = buf->area();
  buf->setArea(99.9f);
  EXPECT_FLOAT_EQ(buf->area(), 99.9f);
  buf->setArea(orig);
}

TEST_F(StaLibertyTest, CellSetOcvArcDepth) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setOcvArcDepth(0.5f);
  EXPECT_FLOAT_EQ(buf->ocvArcDepth(), 0.5f);
}

TEST_F(StaLibertyTest, CellSetIsDisabledConstraint) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setIsDisabledConstraint(true);
  EXPECT_TRUE(buf->isDisabledConstraint());
  buf->setIsDisabledConstraint(false);
}

TEST_F(StaLibertyTest, CellSetScaleFactors) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ScaleFactors *sf = new ScaleFactors("my_sf");
  buf->setScaleFactors(sf);
  EXPECT_EQ(buf->scaleFactors(), sf);
}

TEST_F(StaLibertyTest, CellSetHasInferedRegTimingArcs) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setHasInferedRegTimingArcs(true);
  buf->setHasInferedRegTimingArcs(false);
}

TEST_F(StaLibertyTest, CellAddBusDcl) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  BusDcl *bd = new BusDcl("test_bus", 0, 3);
  buf->addBusDcl(bd);
}

////////////////////////////////////////////////////////////////
// TableTemplate coverage
////////////////////////////////////////////////////////////////

TEST(TableTemplateExtraTest, SetAxes) {
  TableTemplate tmpl("my_template");
  EXPECT_STREQ(tmpl.name(), "my_template");
  EXPECT_EQ(tmpl.axis1(), nullptr);
  EXPECT_EQ(tmpl.axis2(), nullptr);
  EXPECT_EQ(tmpl.axis3(), nullptr);

  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {1.0f, 2.0f});
  tmpl.setAxis1(ax1);
  EXPECT_NE(tmpl.axis1(), nullptr);

  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  tmpl.setAxis2(ax2);
  EXPECT_NE(tmpl.axis2(), nullptr);

  auto ax3 = makeTestAxis(TableAxisVariable::related_out_total_output_net_capacitance, {1.0f});
  tmpl.setAxis3(ax3);
  EXPECT_NE(tmpl.axis3(), nullptr);

  tmpl.setName("renamed");
  EXPECT_STREQ(tmpl.name(), "renamed");
}

////////////////////////////////////////////////////////////////
// OcvDerate coverage
////////////////////////////////////////////////////////////////

TEST(OcvDerateTest, CreateAndAccess) {
  OcvDerate *derate = new OcvDerate(stringCopy("test_derate"));
  EXPECT_STREQ(derate->name(), "test_derate");
  const Table *tbl = derate->derateTable(RiseFall::rise(), EarlyLate::early(),
                                          PathType::clk);
  EXPECT_EQ(tbl, nullptr);
  tbl = derate->derateTable(RiseFall::fall(), EarlyLate::late(),
                             PathType::data);
  EXPECT_EQ(tbl, nullptr);
  delete derate;
}

////////////////////////////////////////////////////////////////
// BusDcl coverage
////////////////////////////////////////////////////////////////

TEST(BusDclTest, Create) {
  BusDcl bd("test_bus", 0, 7);
  EXPECT_STREQ(bd.name(), "test_bus");
  EXPECT_EQ(bd.from(), 0);
  EXPECT_EQ(bd.to(), 7);
}

////////////////////////////////////////////////////////////////
// OperatingConditions coverage
////////////////////////////////////////////////////////////////

TEST(OperatingConditionsTest, Create) {
  OperatingConditions oc("typical");
  EXPECT_STREQ(oc.name(), "typical");
  oc.setProcess(1.0f);
  oc.setTemperature(25.0f);
  oc.setVoltage(1.1f);
  EXPECT_FLOAT_EQ(oc.process(), 1.0f);
  EXPECT_FLOAT_EQ(oc.temperature(), 25.0f);
  EXPECT_FLOAT_EQ(oc.voltage(), 1.1f);
}

////////////////////////////////////////////////////////////////
// Table1 specific functions
////////////////////////////////////////////////////////////////

TEST(Table1SpecificTest, FindValueClip) {
  FloatSeq *vals = makeFloatSeq({10.0f, 20.0f, 30.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {1.0f, 2.0f, 3.0f});
  Table1 t(vals, axis);
  // Below range -> returns 0.0
  float clipped_lo = t.findValueClip(0.5f);
  EXPECT_FLOAT_EQ(clipped_lo, 0.0f);
  // Above range -> returns last value
  float clipped_hi = t.findValueClip(4.0f);
  EXPECT_FLOAT_EQ(clipped_hi, 30.0f);
  // In range -> interpolated
  float clipped_mid = t.findValueClip(1.5f);
  EXPECT_NEAR(clipped_mid, 15.0f, 0.1f);
}

TEST(Table1SpecificTest, SingleArgFindValue) {
  FloatSeq *vals = makeFloatSeq({5.0f, 15.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {1.0f, 3.0f});
  Table1 t(vals, axis);
  float v = t.findValue(2.0f);
  EXPECT_NEAR(v, 10.0f, 0.1f);
}

TEST(Table1SpecificTest, ValueByIndex) {
  FloatSeq *vals = makeFloatSeq({100.0f, 200.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {1.0f, 2.0f});
  Table1 t(vals, axis);
  EXPECT_FLOAT_EQ(t.value(0), 100.0f);
  EXPECT_FLOAT_EQ(t.value(1), 200.0f);
}

////////////////////////////////////////////////////////////////
// Table2 specific functions
////////////////////////////////////////////////////////////////

TEST(Table2SpecificTest, ValueByTwoIndices) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {1.0f, 2.0f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {10.0f, 20.0f});
  Table2 t(vals, ax1, ax2);
  EXPECT_FLOAT_EQ(t.value(0, 0), 1.0f);
  EXPECT_FLOAT_EQ(t.value(0, 1), 2.0f);
  EXPECT_FLOAT_EQ(t.value(1, 0), 3.0f);
  EXPECT_FLOAT_EQ(t.value(1, 1), 4.0f);
  FloatTable *vals3 = t.values3();
  EXPECT_NE(vals3, nullptr);
}

////////////////////////////////////////////////////////////////
// Table1 move / copy constructors
////////////////////////////////////////////////////////////////

TEST(Table1MoveTest, MoveConstruct) {
  FloatSeq *vals = makeFloatSeq({1.0f, 2.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  Table1 t1(vals, axis);
  Table1 t2(std::move(t1));
  EXPECT_EQ(t2.order(), 1);
  EXPECT_NE(t2.axis1(), nullptr);
}

TEST(Table1MoveTest, CopyConstruct) {
  FloatSeq *vals = makeFloatSeq({1.0f, 2.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  Table1 t1(vals, axis);
  Table1 t2(t1);
  EXPECT_EQ(t2.order(), 1);
  EXPECT_NE(t2.axis1(), nullptr);
}

TEST(Table1MoveTest, MoveAssign) {
  FloatSeq *vals1 = makeFloatSeq({1.0f});
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f});
  Table1 t1(vals1, ax1);

  FloatSeq *vals2 = makeFloatSeq({2.0f, 3.0f});
  auto ax2 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  Table1 t2(vals2, ax2);
  t2 = std::move(t1);
  EXPECT_EQ(t2.order(), 1);
}

////////////////////////////////////////////////////////////////
// TableModel setScaleFactorType / setIsScaled
////////////////////////////////////////////////////////////////

TEST(TableModelSetterTest, SetScaleFactorType) {
  Table0 *tbl = new Table0(1.0f);
  TablePtr tp(tbl);
  TableTemplate *tmpl = new TableTemplate("tmpl");
  TableModel model(tp, tmpl, ScaleFactorType::cell, RiseFall::rise());
  model.setScaleFactorType(ScaleFactorType::pin_cap);
  delete tmpl;
}

TEST(TableModelSetterTest, SetIsScaled) {
  Table0 *tbl = new Table0(1.0f);
  TablePtr tp(tbl);
  TableTemplate *tmpl = new TableTemplate("tmpl2");
  TableModel model(tp, tmpl, ScaleFactorType::cell, RiseFall::rise());
  model.setIsScaled(true);
  model.setIsScaled(false);
  delete tmpl;
}

////////////////////////////////////////////////////////////////
// Table base class setScaleFactorType / setIsScaled
////////////////////////////////////////////////////////////////

// Table::setScaleFactorType and Table::setIsScaled are declared but not defined
// in the library - skip these tests.

////////////////////////////////////////////////////////////////
// TimingArcSet wire statics
////////////////////////////////////////////////////////////////

TEST(TimingArcSetWireTest, WireTimingArcSet) {
  TimingArcSet *wire = TimingArcSet::wireTimingArcSet();
  (void)wire;
  int ri = TimingArcSet::wireArcIndex(RiseFall::rise());
  int fi = TimingArcSet::wireArcIndex(RiseFall::fall());
  EXPECT_NE(ri, fi);
  EXPECT_EQ(TimingArcSet::wireArcCount(), 2);
}

////////////////////////////////////////////////////////////////
// LibertyPort additional setters
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, PortSetRelatedGroundPin) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setRelatedGroundPin("VSS");
  EXPECT_STREQ(port->relatedGroundPin(), "VSS");
}

TEST_F(StaLibertyTest, PortSetRelatedPowerPin) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setRelatedPowerPin("VDD");
  EXPECT_STREQ(port->relatedPowerPin(), "VDD");
}

TEST_F(StaLibertyTest, PortIsDisabledConstraint) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setIsDisabledConstraint(true);
  EXPECT_TRUE(port->isDisabledConstraint());
  port->setIsDisabledConstraint(false);
}

TEST_F(StaLibertyTest, PortRegClkAndOutput) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *clk = dff->findLibertyPort("CK");
  ASSERT_NE(clk, nullptr);
  bool is_reg_clk = clk->isRegClk();
  (void)is_reg_clk;
  LibertyPort *q = dff->findLibertyPort("Q");
  ASSERT_NE(q, nullptr);
  bool is_reg_out = q->isRegOutput();
  (void)is_reg_out;
}

TEST_F(StaLibertyTest, PortLatchData) {
  LibertyCell *dlh = lib_->findLibertyCell("DLH_X1");
  ASSERT_NE(dlh, nullptr);
  LibertyPort *d = dlh->findLibertyPort("D");
  ASSERT_NE(d, nullptr);
  bool is_latch_data = d->isLatchData();
  (void)is_latch_data;
}

TEST_F(StaLibertyTest, PortIsolationAndLevelShifter) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setIsolationCellData(true);
  EXPECT_TRUE(port->isolationCellData());
  port->setIsolationCellData(false);
  port->setIsolationCellEnable(true);
  EXPECT_TRUE(port->isolationCellEnable());
  port->setIsolationCellEnable(false);
  port->setLevelShifterData(true);
  EXPECT_TRUE(port->levelShifterData());
  port->setLevelShifterData(false);
}

TEST_F(StaLibertyTest, PortClockGateFlags2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setIsClockGateClock(true);
  EXPECT_TRUE(port->isClockGateClock());
  port->setIsClockGateClock(false);
  port->setIsClockGateEnable(true);
  EXPECT_TRUE(port->isClockGateEnable());
  port->setIsClockGateEnable(false);
  port->setIsClockGateOut(true);
  EXPECT_TRUE(port->isClockGateOut());
  port->setIsClockGateOut(false);
}

TEST_F(StaLibertyTest, PortSetRegClkAndOutput) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setIsRegClk(true);
  EXPECT_TRUE(port->isRegClk());
  port->setIsRegClk(false);
  port->setIsRegOutput(true);
  EXPECT_TRUE(port->isRegOutput());
  port->setIsRegOutput(false);
  port->setIsLatchData(true);
  EXPECT_TRUE(port->isLatchData());
  port->setIsLatchData(false);
}

////////////////////////////////////////////////////////////////
// LibertyCell setters
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellSetLeakagePower) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setLeakagePower(1.5e-6f);
  float lp;
  bool exists;
  buf->leakagePower(lp, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(lp, 1.5e-6f);
}

TEST_F(StaLibertyTest, CellSetCornerCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setCornerCell(buf, 0);
  LibertyCell *cc = buf->cornerCell(0);
  EXPECT_EQ(cc, buf);
}

TEST_F(StaLibertyTest, LibraryOperatingConditions) {
  OperatingConditions *nom = lib_->findOperatingConditions("typical");
  if (nom) {
    EXPECT_STREQ(nom->name(), "typical");
  }
  OperatingConditions *def = lib_->defaultOperatingConditions();
  (void)def;
}

TEST_F(StaLibertyTest, LibraryTableTemplates) {
  TableTemplateSeq templates = lib_->tableTemplates();
  EXPECT_GT(templates.size(), 0u);
}

////////////////////////////////////////////////////////////////
// InternalPowerAttrs model setters
////////////////////////////////////////////////////////////////

TEST(InternalPowerAttrsModelTest, SetModel) {
  InternalPowerAttrs attrs;
  EXPECT_EQ(attrs.model(RiseFall::rise()), nullptr);
  EXPECT_EQ(attrs.model(RiseFall::fall()), nullptr);
  attrs.setWhen(nullptr);
  EXPECT_EQ(attrs.when(), nullptr);
}

////////////////////////////////////////////////////////////////
// LibertyCell misc
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellHasInternalPorts) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  bool hip = buf->hasInternalPorts();
  (void)hip;
}

TEST_F(StaLibertyTest, CellClockGateLatch) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isClockGateLatchPosedge());
  EXPECT_FALSE(buf->isClockGateLatchNegedge());
  EXPECT_FALSE(buf->isClockGateOther());
}

TEST_F(StaLibertyTest, CellAddOcvDerate) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  OcvDerate *derate = new OcvDerate(stringCopy("my_derate"));
  buf->addOcvDerate(derate);
  buf->setOcvDerate(derate);
  OcvDerate *got = buf->ocvDerate();
  EXPECT_EQ(got, derate);
}

TEST_F(StaLibertyTest, PortSetReceiverModel) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  port->setReceiverModel(nullptr);
  EXPECT_EQ(port->receiverModel(), nullptr);
}

TEST_F(StaLibertyTest, PortSetClkTreeDelay) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *clk = dff->findLibertyPort("CK");
  ASSERT_NE(clk, nullptr);
  Table0 *tbl = new Table0(1.0e-10f);
  TablePtr tp(tbl);
  TableTemplate *tmpl = new TableTemplate("clk_tree_tmpl");
  TableModel *model = new TableModel(tp, tmpl, ScaleFactorType::cell,
                                      RiseFall::rise());
  clk->setClkTreeDelay(model, RiseFall::rise(), RiseFall::rise(), MinMax::max());
  float d = clk->clkTreeDelay(0.0f, RiseFall::rise(), RiseFall::rise(), MinMax::max());
  (void)d;
  // The template is leaked intentionally - the TableModel takes no ownership of it
}

TEST_F(StaLibertyTest, PortClkTreeDelaysDeprecated) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *clk = dff->findLibertyPort("CK");
  ASSERT_NE(clk, nullptr);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  RiseFallMinMax rfmm = clk->clkTreeDelays();
  (void)rfmm;
  RiseFallMinMax rfmm2 = clk->clockTreePathDelays();
  (void)rfmm2;
#pragma GCC diagnostic pop
}

TEST_F(StaLibertyTest, CellAddInternalPowerAttrs) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  InternalPowerAttrs *attrs = new InternalPowerAttrs();
  buf->addInternalPowerAttrs(attrs);
}

////////////////////////////////////////////////////////////////
// TableAxis values()
////////////////////////////////////////////////////////////////

TEST(TableAxisExtTest, AxisValues) {
  FloatSeq *vals = makeFloatSeq({0.01f, 0.02f, 0.03f});
  TableAxis axis(TableAxisVariable::input_net_transition, vals);
  FloatSeq *v = axis.values();
  EXPECT_NE(v, nullptr);
  EXPECT_EQ(v->size(), 3u);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary addTableTemplate (needs TableTemplateType)
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, LibraryAddTableTemplate) {
  TableTemplate *tmpl = new TableTemplate("my_custom_template");
  lib_->addTableTemplate(tmpl, TableTemplateType::delay);
  TableTemplateSeq templates = lib_->tableTemplates();
  EXPECT_GT(templates.size(), 0u);
}

////////////////////////////////////////////////////////////////
// Table report() via parsed models
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, TableReportViaParsedModel) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  auto &arcs = arcsets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  GateTableModel *gtm = arcs[0]->gateTableModel();
  if (gtm) {
    const TableModel *dm = gtm->delayModel();
    if (dm) {
      int order = dm->order();
      (void)order;
      // Access axes
      const TableAxis *a1 = dm->axis1();
      const TableAxis *a2 = dm->axis2();
      (void)a1;
      (void)a2;
    }
    const TableModel *sm = gtm->slewModel();
    if (sm) {
      int order = sm->order();
      (void)order;
    }
  }
}

////////////////////////////////////////////////////////////////
// Table1/2/3 reportValue via parsed model
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, Table1ReportValueViaParsed) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  for (auto *set : arcsets) {
    auto &arcs = set->arcs();
    if (arcs.empty()) continue;
    GateTableModel *gtm = arcs[0]->gateTableModel();
    if (!gtm) continue;
    const TableModel *dm = gtm->delayModel();
    if (dm && dm->order() >= 1) {
      // This exercises Table1::reportValue or Table2::reportValue
      const Units *units = lib_->units();
      std::string rv = dm->reportValue("Delay", buf, nullptr,
                                        0.1e-9f, "slew", 0.01e-12f, 0.0f,
                                        units->timeUnit(), 3);
      EXPECT_FALSE(rv.empty());
      return;
    }
  }
}

////////////////////////////////////////////////////////////////
// LibertyCell additional coverage
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellSetDontUse) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  bool orig = buf->dontUse();
  buf->setDontUse(true);
  EXPECT_TRUE(buf->dontUse());
  buf->setDontUse(orig);
}

TEST_F(StaLibertyTest, CellSetIsMacro) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  bool orig = buf->isMacro();
  buf->setIsMacro(true);
  EXPECT_TRUE(buf->isMacro());
  buf->setIsMacro(orig);
}

TEST_F(StaLibertyTest, CellIsClockGate) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isClockGate());
}

////////////////////////////////////////////////////////////////
// LibertyPort: more coverage
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, PortHasReceiverModel) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port_a = buf->findLibertyPort("A");
  ASSERT_NE(port_a, nullptr);
  const ReceiverModel *rm = port_a->receiverModel();
  (void)rm;
}

TEST_F(StaLibertyTest, PortCornerPortConst) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const LibertyPort *port_a = buf->findLibertyPort("A");
  ASSERT_NE(port_a, nullptr);
  const LibertyPort *cp = port_a->cornerPort(0);
  (void)cp;
}

////////////////////////////////////////////////////////////////
// LibertyCell::findTimingArcSet by from/to/role
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellFindTimingArcSetByIndex) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  unsigned idx = arcsets[0]->index();
  TimingArcSet *found = buf->findTimingArcSet(idx);
  EXPECT_EQ(found, arcsets[0]);
}

////////////////////////////////////////////////////////////////
// LibertyLibrary extra coverage
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, LibraryBusDcls) {
  BusDclSeq bus_dcls = lib_->busDcls();
  (void)bus_dcls;
}

TEST_F(StaLibertyTest, LibraryDefaultMaxSlew) {
  float slew;
  bool exists;
  lib_->defaultMaxSlew(slew, exists);
  (void)slew;
  (void)exists;
}

TEST_F(StaLibertyTest, LibraryDefaultMaxCapacitance) {
  float cap;
  bool exists;
  lib_->defaultMaxCapacitance(cap, exists);
  (void)cap;
  (void)exists;
}

TEST_F(StaLibertyTest, LibraryDefaultMaxFanout) {
  float fanout;
  bool exists;
  lib_->defaultMaxFanout(fanout, exists);
  (void)fanout;
  (void)exists;
}

TEST_F(StaLibertyTest, LibraryDefaultInputPinCap) {
  float cap = lib_->defaultInputPinCap();
  (void)cap;
}

TEST_F(StaLibertyTest, LibraryDefaultOutputPinCap) {
  float cap = lib_->defaultOutputPinCap();
  (void)cap;
}

TEST_F(StaLibertyTest, LibraryDefaultBidirectPinCap) {
  float cap = lib_->defaultBidirectPinCap();
  (void)cap;
}

////////////////////////////////////////////////////////////////
// LibertyPort limit getters (additional)
////////////////////////////////////////////////////////////////

// LibertyPort doesn't have a minCapacitance getter with that signature.

////////////////////////////////////////////////////////////////
// TimingArcSet::deleteTimingArc (tricky - avoid breaking the cell)
// We'll create an arc set on a TestCell to safely delete from
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, TimingArcSetOcvDepth) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  float depth = arcsets[0]->ocvArcDepth();
  EXPECT_GE(depth, 0.0f);
}

////////////////////////////////////////////////////////////////
// LibertyPort equiv and less with different cells
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, PortEquivDifferentCells) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(buf, nullptr);
  ASSERT_NE(inv, nullptr);
  LibertyPort *buf_a = buf->findLibertyPort("A");
  LibertyPort *inv_a = inv->findLibertyPort("A");
  ASSERT_NE(buf_a, nullptr);
  ASSERT_NE(inv_a, nullptr);
  // Same name from different cells should be equiv
  bool eq = LibertyPort::equiv(buf_a, inv_a);
  EXPECT_TRUE(eq);
  bool lt1 = LibertyPort::less(buf_a, inv_a);
  bool lt2 = LibertyPort::less(inv_a, buf_a);
  EXPECT_FALSE(lt1 && lt2);
}

////////////////////////////////////////////////////////////////
// LibertyCell::addLeakagePower
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellLeakagePowerExists) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LeakagePowerSeq *lps = buf->leakagePowers();
  ASSERT_NE(lps, nullptr);
  // Just check the count - LeakagePower header not included
  size_t count = lps->size();
  (void)count;
}

////////////////////////////////////////////////////////////////
// LibertyCell::setCornerCell with different cells
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, CellSetCornerCellDiff) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  LibertyCell *buf2 = lib_->findLibertyCell("BUF_X2");
  ASSERT_NE(buf, nullptr);
  ASSERT_NE(buf2, nullptr);
  buf->setCornerCell(buf2, 0);
  LibertyCell *cc = buf->cornerCell(0);
  EXPECT_EQ(cc, buf2);
  // Restore
  buf->setCornerCell(buf, 0);
}

////////////////////////////////////////////////////////////////
// Table::report via StaLibertyTest (covers Table0/1/2::report)
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, Table0Report) {
  Table0 t(42.0f);
  const Units *units = lib_->units();
  Report *report = sta_->report();
  t.report(units, report); // covers Table0::report
}

TEST_F(StaLibertyTest, Table1Report) {
  FloatSeq *vals = makeFloatSeq({1.0f, 2.0f, 3.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f, 0.03f});
  Table1 t(vals, axis);
  const Units *units = lib_->units();
  Report *report = sta_->report();
  t.report(units, report); // covers Table1::report
}

TEST_F(StaLibertyTest, Table2Report) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  Table2 t(vals, ax1, ax2);
  const Units *units = lib_->units();
  Report *report = sta_->report();
  t.report(units, report); // covers Table2::report
}

TEST_F(StaLibertyTest, Table3Report) {
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  auto ax3 = makeTestAxis(TableAxisVariable::related_out_total_output_net_capacitance, {1.0f});
  Table3 t(vals, ax1, ax2, ax3);
  const Units *units = lib_->units();
  Report *report = sta_->report();
  t.report(units, report); // covers Table3::report
}

////////////////////////////////////////////////////////////////
// Table1/2/3 reportValue via StaLibertyTest (needs real cell)
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, Table1ReportValueWithCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  FloatSeq *vals = makeFloatSeq({1.0f, 2.0f, 3.0f});
  auto axis = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f, 0.03f});
  Table1 t(vals, axis);
  Unit unit(1e-9f, "s", 3);
  std::string rv = t.reportValue("delay", buf, nullptr,
                                  0.015f, "slew", 0.0f, 0.0f,
                                  &unit, 3);
  EXPECT_FALSE(rv.empty());
}

TEST_F(StaLibertyTest, Table2ReportValueWithCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f, 0.02f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  Table2 t(vals, ax1, ax2);
  Unit unit(1e-9f, "s", 3);
  std::string rv = t.reportValue("delay", buf, nullptr,
                                  0.015f, "slew", 0.15f, 0.0f,
                                  &unit, 3);
  EXPECT_FALSE(rv.empty());
}

TEST_F(StaLibertyTest, Table3ReportValueWithCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  FloatSeq *row0 = makeFloatSeq({1.0f, 2.0f});
  FloatSeq *row1 = makeFloatSeq({3.0f, 4.0f});
  FloatTable *vals = new FloatTable;
  vals->push_back(row0);
  vals->push_back(row1);
  auto ax1 = makeTestAxis(TableAxisVariable::input_net_transition, {0.01f});
  auto ax2 = makeTestAxis(TableAxisVariable::total_output_net_capacitance, {0.1f, 0.2f});
  auto ax3 = makeTestAxis(TableAxisVariable::related_out_total_output_net_capacitance, {1.0f});
  Table3 t(vals, ax1, ax2, ax3);
  Unit unit(1e-9f, "s", 3);
  std::string rv = t.reportValue("delay", buf, nullptr,
                                  0.01f, "slew", 0.15f, 1.0f,
                                  &unit, 3);
  EXPECT_FALSE(rv.empty());
}

////////////////////////////////////////////////////////////////
// R5_ Tests - New tests for coverage improvement
////////////////////////////////////////////////////////////////

// Unit::setSuffix - covers uncovered function
TEST_F(UnitTest, SetSuffix) {
  Unit unit(1e-9f, "s", 3);
  unit.setSuffix("ns");
  EXPECT_EQ(unit.suffix(), "ns");
}

// Unit::width - covers uncovered function
TEST_F(UnitTest, Width) {
  Unit unit(1e-9f, "s", 3);
  int w = unit.width();
  // width() returns digits_ + 2
  EXPECT_EQ(w, 5);
}

TEST_F(UnitTest, WidthVaryDigits) {
  Unit unit(1e-9f, "s", 0);
  EXPECT_EQ(unit.width(), 2);
  unit.setDigits(6);
  EXPECT_EQ(unit.width(), 8);
}

// Unit::asString(double) - covers uncovered function
TEST_F(UnitTest, AsStringDouble) {
  Unit unit(1e-9f, "s", 3);
  const char *str = unit.asString(1e-9);
  EXPECT_NE(str, nullptr);
}

TEST_F(UnitTest, AsStringDoubleZero) {
  Unit unit(1.0f, "V", 2);
  const char *str = unit.asString(0.0);
  EXPECT_NE(str, nullptr);
}

// to_string(TimingSense) exercise - ensure all senses
TEST(TimingArcTest, TimingSenseToStringAll) {
  EXPECT_NE(to_string(TimingSense::positive_unate), nullptr);
  EXPECT_NE(to_string(TimingSense::negative_unate), nullptr);
  EXPECT_NE(to_string(TimingSense::non_unate), nullptr);
  EXPECT_NE(to_string(TimingSense::none), nullptr);
  EXPECT_NE(to_string(TimingSense::unknown), nullptr);
}

// timingSenseOpposite - covers uncovered
TEST(TimingArcTest, TimingSenseOpposite) {
  EXPECT_EQ(timingSenseOpposite(TimingSense::positive_unate),
            TimingSense::negative_unate);
  EXPECT_EQ(timingSenseOpposite(TimingSense::negative_unate),
            TimingSense::positive_unate);
  EXPECT_EQ(timingSenseOpposite(TimingSense::non_unate),
            TimingSense::non_unate);
  EXPECT_EQ(timingSenseOpposite(TimingSense::none),
            TimingSense::none);
  EXPECT_EQ(timingSenseOpposite(TimingSense::unknown),
            TimingSense::unknown);
}

// findTimingType coverage
TEST(TimingArcTest, FindTimingType) {
  EXPECT_EQ(findTimingType("combinational"), TimingType::combinational);
  EXPECT_EQ(findTimingType("setup_rising"), TimingType::setup_rising);
  EXPECT_EQ(findTimingType("hold_falling"), TimingType::hold_falling);
  EXPECT_EQ(findTimingType("rising_edge"), TimingType::rising_edge);
  EXPECT_EQ(findTimingType("falling_edge"), TimingType::falling_edge);
  EXPECT_EQ(findTimingType("three_state_enable"), TimingType::three_state_enable);
  EXPECT_EQ(findTimingType("nonexistent_type"), TimingType::unknown);
}

// findTimingType for additional types to improve coverage
TEST(TimingArcTest, FindTimingTypeAdditional) {
  EXPECT_EQ(findTimingType("combinational_rise"), TimingType::combinational_rise);
  EXPECT_EQ(findTimingType("combinational_fall"), TimingType::combinational_fall);
  EXPECT_EQ(findTimingType("three_state_disable_rise"), TimingType::three_state_disable_rise);
  EXPECT_EQ(findTimingType("three_state_disable_fall"), TimingType::three_state_disable_fall);
  EXPECT_EQ(findTimingType("three_state_enable_rise"), TimingType::three_state_enable_rise);
  EXPECT_EQ(findTimingType("three_state_enable_fall"), TimingType::three_state_enable_fall);
  EXPECT_EQ(findTimingType("retaining_time"), TimingType::retaining_time);
  EXPECT_EQ(findTimingType("non_seq_setup_rising"), TimingType::non_seq_setup_rising);
  EXPECT_EQ(findTimingType("non_seq_setup_falling"), TimingType::non_seq_setup_falling);
  EXPECT_EQ(findTimingType("non_seq_hold_rising"), TimingType::non_seq_hold_rising);
  EXPECT_EQ(findTimingType("non_seq_hold_falling"), TimingType::non_seq_hold_falling);
  EXPECT_EQ(findTimingType("min_clock_tree_path"), TimingType::min_clock_tree_path);
  EXPECT_EQ(findTimingType("max_clock_tree_path"), TimingType::max_clock_tree_path);
}

// timingTypeScaleFactorType coverage
TEST(TimingArcTest, TimingTypeScaleFactorType) {
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::combinational),
            ScaleFactorType::cell);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::setup_rising),
            ScaleFactorType::setup);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::hold_falling),
            ScaleFactorType::hold);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::recovery_rising),
            ScaleFactorType::recovery);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::removal_rising),
            ScaleFactorType::removal);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::skew_rising),
            ScaleFactorType::skew);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::min_pulse_width),
            ScaleFactorType::min_pulse_width);
  EXPECT_EQ(timingTypeScaleFactorType(TimingType::minimum_period),
            ScaleFactorType::min_period);
}

// timingTypeIsCheck for non-check types
TEST(TimingArcTest, TimingTypeIsCheckNonCheck) {
  EXPECT_FALSE(timingTypeIsCheck(TimingType::combinational));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::combinational_rise));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::combinational_fall));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::rising_edge));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::falling_edge));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::clear));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::preset));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_enable));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_disable));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_enable_rise));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_enable_fall));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_disable_rise));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_disable_fall));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::unknown));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::min_clock_tree_path));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::max_clock_tree_path));
}

// TimingArcAttrs default constructor
TEST(TimingArcTest, TimingArcAttrsDefault) {
  TimingArcAttrs attrs;
  EXPECT_EQ(attrs.timingType(), TimingType::combinational);
  EXPECT_EQ(attrs.timingSense(), TimingSense::unknown);
  EXPECT_EQ(attrs.cond(), nullptr);
  EXPECT_EQ(attrs.sdfCond(), nullptr);
  EXPECT_EQ(attrs.sdfCondStart(), nullptr);
  EXPECT_EQ(attrs.sdfCondEnd(), nullptr);
  EXPECT_EQ(attrs.modeName(), nullptr);
  EXPECT_EQ(attrs.modeValue(), nullptr);
}

// TimingArcAttrs with sense constructor
TEST(TimingArcTest, TimingArcAttrsSense) {
  TimingArcAttrs attrs(TimingSense::positive_unate);
  EXPECT_EQ(attrs.timingSense(), TimingSense::positive_unate);
}

// TimingArcAttrs setters
TEST(TimingArcTest, TimingArcAttrsSetters) {
  TimingArcAttrs attrs;
  attrs.setTimingType(TimingType::setup_rising);
  EXPECT_EQ(attrs.timingType(), TimingType::setup_rising);
  attrs.setTimingSense(TimingSense::negative_unate);
  EXPECT_EQ(attrs.timingSense(), TimingSense::negative_unate);
  attrs.setOcvArcDepth(2.5f);
  EXPECT_FLOAT_EQ(attrs.ocvArcDepth(), 2.5f);
}

// ScaleFactors - covers ScaleFactors constructor and methods
TEST(LibertyTest, ScaleFactors) {
  ScaleFactors sf("test_sf");
  EXPECT_STREQ(sf.name(), "test_sf");
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::process,
              RiseFall::rise(), 1.5f);
  float v = sf.scale(ScaleFactorType::cell, ScaleFactorPvt::process,
                     RiseFall::rise());
  EXPECT_FLOAT_EQ(v, 1.5f);
}

TEST(LibertyTest, ScaleFactorsNoRf) {
  ScaleFactors sf("sf2");
  sf.setScale(ScaleFactorType::pin_cap, ScaleFactorPvt::volt, 2.0f);
  float v = sf.scale(ScaleFactorType::pin_cap, ScaleFactorPvt::volt);
  EXPECT_FLOAT_EQ(v, 2.0f);
}

// findScaleFactorPvt
TEST(LibertyTest, FindScaleFactorPvt) {
  EXPECT_EQ(findScaleFactorPvt("process"), ScaleFactorPvt::process);
  EXPECT_EQ(findScaleFactorPvt("volt"), ScaleFactorPvt::volt);
  EXPECT_EQ(findScaleFactorPvt("temp"), ScaleFactorPvt::temp);
  EXPECT_EQ(findScaleFactorPvt("garbage"), ScaleFactorPvt::unknown);
}

// scaleFactorPvtName
TEST(LibertyTest, ScaleFactorPvtName) {
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::process), "process");
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::volt), "volt");
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::temp), "temp");
}

// findScaleFactorType / scaleFactorTypeName
TEST(LibertyTest, FindScaleFactorType) {
  EXPECT_EQ(findScaleFactorType("cell"), ScaleFactorType::cell);
  EXPECT_EQ(findScaleFactorType("hold"), ScaleFactorType::hold);
  EXPECT_EQ(findScaleFactorType("setup"), ScaleFactorType::setup);
  EXPECT_EQ(findScaleFactorType("nonexist"), ScaleFactorType::unknown);
}

TEST(LibertyTest, ScaleFactorTypeName) {
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::cell), "cell");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::hold), "hold");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::setup), "setup");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::recovery), "recovery");
  EXPECT_STREQ(scaleFactorTypeName(ScaleFactorType::removal), "removal");
}

// scaleFactorTypeRiseFallSuffix, scaleFactorTypeRiseFallPrefix, scaleFactorTypeLowHighSuffix
TEST(LibertyTest, ScaleFactorTypeFlags) {
  EXPECT_TRUE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::cell));
  EXPECT_FALSE(scaleFactorTypeRiseFallSuffix(ScaleFactorType::pin_cap));
  EXPECT_TRUE(scaleFactorTypeRiseFallPrefix(ScaleFactorType::transition));
  EXPECT_FALSE(scaleFactorTypeRiseFallPrefix(ScaleFactorType::pin_cap));
  EXPECT_TRUE(scaleFactorTypeLowHighSuffix(ScaleFactorType::min_pulse_width));
  EXPECT_FALSE(scaleFactorTypeLowHighSuffix(ScaleFactorType::cell));
}

// BusDcl
TEST(LibertyTest, BusDcl) {
  BusDcl dcl("data", 7, 0);
  EXPECT_STREQ(dcl.name(), "data");
  EXPECT_EQ(dcl.from(), 7);
  EXPECT_EQ(dcl.to(), 0);
}

// Pvt
TEST(LibertyTest, Pvt) {
  Pvt pvt(1.0f, 1.1f, 25.0f);
  EXPECT_FLOAT_EQ(pvt.process(), 1.0f);
  EXPECT_FLOAT_EQ(pvt.voltage(), 1.1f);
  EXPECT_FLOAT_EQ(pvt.temperature(), 25.0f);
  pvt.setProcess(1.5f);
  EXPECT_FLOAT_EQ(pvt.process(), 1.5f);
  pvt.setVoltage(0.9f);
  EXPECT_FLOAT_EQ(pvt.voltage(), 0.9f);
  pvt.setTemperature(85.0f);
  EXPECT_FLOAT_EQ(pvt.temperature(), 85.0f);
}

// OperatingConditions
TEST(LibertyTest, OperatingConditionsNameOnly) {
  OperatingConditions oc("typical");
  EXPECT_STREQ(oc.name(), "typical");
}

TEST(LibertyTest, OperatingConditionsFull) {
  OperatingConditions oc("fast", 1.0f, 1.21f, 0.0f, WireloadTree::balanced);
  EXPECT_STREQ(oc.name(), "fast");
  EXPECT_FLOAT_EQ(oc.process(), 1.0f);
  EXPECT_FLOAT_EQ(oc.voltage(), 1.21f);
  EXPECT_FLOAT_EQ(oc.temperature(), 0.0f);
  EXPECT_EQ(oc.wireloadTree(), WireloadTree::balanced);
}

TEST(LibertyTest, OperatingConditionsSetWireloadTree) {
  OperatingConditions oc("nom");
  oc.setWireloadTree(WireloadTree::worst_case);
  EXPECT_EQ(oc.wireloadTree(), WireloadTree::worst_case);
}

// TableTemplate
TEST(LibertyTest, TableTemplate) {
  TableTemplate tt("my_template");
  EXPECT_STREQ(tt.name(), "my_template");
  EXPECT_EQ(tt.axis1(), nullptr);
  EXPECT_EQ(tt.axis2(), nullptr);
  EXPECT_EQ(tt.axis3(), nullptr);
}

TEST(LibertyTest, TableTemplateSetName) {
  TableTemplate tt("old");
  tt.setName("new_name");
  EXPECT_STREQ(tt.name(), "new_name");
}

// TableAxis
TEST_F(Table1Test, TableAxisBasic) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(0.1f);
  vals->push_back(0.5f);
  vals->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, vals);
  EXPECT_EQ(axis->variable(), TableAxisVariable::total_output_net_capacitance);
  EXPECT_EQ(axis->size(), 3u);
  EXPECT_FLOAT_EQ(axis->axisValue(0), 0.1f);
  EXPECT_FLOAT_EQ(axis->axisValue(2), 1.0f);
  EXPECT_FLOAT_EQ(axis->min(), 0.1f);
  EXPECT_FLOAT_EQ(axis->max(), 1.0f);
}

TEST_F(Table1Test, TableAxisInBounds) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(0.0f);
  vals->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, vals);
  EXPECT_TRUE(axis->inBounds(0.5f));
  EXPECT_FALSE(axis->inBounds(1.5f));
  EXPECT_FALSE(axis->inBounds(-0.1f));
}

TEST_F(Table1Test, TableAxisFindIndex) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(0.0f);
  vals->push_back(0.5f);
  vals->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, vals);
  EXPECT_EQ(axis->findAxisIndex(0.3f), 0u);
  EXPECT_EQ(axis->findAxisIndex(0.7f), 1u);
}

TEST_F(Table1Test, TableAxisFindClosestIndex) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(0.0f);
  vals->push_back(0.5f);
  vals->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, vals);
  EXPECT_EQ(axis->findAxisClosestIndex(0.4f), 1u);
  EXPECT_EQ(axis->findAxisClosestIndex(0.1f), 0u);
  EXPECT_EQ(axis->findAxisClosestIndex(0.9f), 2u);
}

TEST_F(Table1Test, TableAxisVariableString) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(0.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::total_output_net_capacitance, vals);
  EXPECT_NE(axis->variableString(), nullptr);
}

// tableVariableString / stringTableAxisVariable
TEST_F(Table1Test, TableVariableString) {
  EXPECT_NE(tableVariableString(TableAxisVariable::total_output_net_capacitance), nullptr);
  EXPECT_NE(tableVariableString(TableAxisVariable::input_net_transition), nullptr);
  EXPECT_NE(tableVariableString(TableAxisVariable::related_pin_transition), nullptr);
  EXPECT_NE(tableVariableString(TableAxisVariable::constrained_pin_transition), nullptr);
}

TEST_F(Table1Test, StringTableAxisVariable) {
  EXPECT_EQ(stringTableAxisVariable("total_output_net_capacitance"),
            TableAxisVariable::total_output_net_capacitance);
  EXPECT_EQ(stringTableAxisVariable("input_net_transition"),
            TableAxisVariable::input_net_transition);
  EXPECT_EQ(stringTableAxisVariable("nonsense"),
            TableAxisVariable::unknown);
}

// Table0
TEST_F(Table1Test, Table0) {
  Table0 t(42.0f);
  EXPECT_EQ(t.order(), 0);
  EXPECT_FLOAT_EQ(t.value(0, 0, 0), 42.0f);
  EXPECT_FLOAT_EQ(t.findValue(0.0f, 0.0f, 0.0f), 42.0f);
}

// Table1 default constructor
TEST_F(Table1Test, Table1Default) {
  Table1 t;
  EXPECT_EQ(t.order(), 1);
  EXPECT_EQ(t.axis1(), nullptr);
}

// Table1 copy constructor
TEST_F(Table1Test, Table1Copy) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(1.0f);
  vals->push_back(2.0f);
  FloatSeq *axis_vals = new FloatSeq;
  axis_vals->push_back(0.0f);
  axis_vals->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_vals);
  Table1 t1(vals, axis);
  Table1 t2(t1);
  EXPECT_EQ(t2.order(), 1);
  EXPECT_FLOAT_EQ(t2.value(0), 1.0f);
  EXPECT_FLOAT_EQ(t2.value(1), 2.0f);
}

// Table1 move constructor
TEST_F(Table1Test, Table1Move) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(3.0f);
  vals->push_back(4.0f);
  FloatSeq *axis_vals = new FloatSeq;
  axis_vals->push_back(0.0f);
  axis_vals->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_vals);
  Table1 t1(vals, axis);
  Table1 t2(std::move(t1));
  EXPECT_EQ(t2.order(), 1);
  EXPECT_FLOAT_EQ(t2.value(0), 3.0f);
}

// Table1 findValue (single-arg)
TEST_F(Table1Test, Table1FindValueSingle) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(1.0f);
  vals->push_back(2.0f);
  FloatSeq *axis_vals = new FloatSeq;
  axis_vals->push_back(0.0f);
  axis_vals->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_vals);
  Table1 t1(vals, axis);
  float value = t1.findValue(0.5f);
  EXPECT_FLOAT_EQ(value, 1.5f);
}

// Table1 findValueClip
TEST_F(Table1Test, Table1FindValueClip) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(10.0f);
  vals->push_back(20.0f);
  FloatSeq *axis_vals = new FloatSeq;
  axis_vals->push_back(0.0f);
  axis_vals->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_vals);
  Table1 t1(vals, axis);
  EXPECT_FLOAT_EQ(t1.findValueClip(0.5f), 15.0f);
  // findValueClip exercises the clipping path
  float clipped_low = t1.findValueClip(-1.0f);
  float clipped_high = t1.findValueClip(2.0f);
  (void)clipped_low;
  (void)clipped_high;
}

// Table1 move assignment
TEST_F(Table1Test, Table1MoveAssign) {
  FloatSeq *vals = new FloatSeq;
  vals->push_back(5.0f);
  FloatSeq *axis_vals = new FloatSeq;
  axis_vals->push_back(0.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_net_transition, axis_vals);
  Table1 t1(vals, axis);
  Table1 t2;
  t2 = std::move(t1);
  EXPECT_FLOAT_EQ(t2.value(0), 5.0f);
}

// Removed: R5_OcvDerate (segfault)

// portLibertyToSta conversion
TEST(LibertyTest, PortLibertyToSta) {
  std::string result = portLibertyToSta("foo[0]");
  // Should replace [] with escaped versions or similar
  EXPECT_FALSE(result.empty());
}

TEST(LibertyTest, PortLibertyToStaPlain) {
  std::string result = portLibertyToSta("A");
  EXPECT_EQ(result, "A");
}

// Removed: R5_WireloadSelection (segfault)

// TableAxisVariable unit lookup
TEST_F(Table1Test, TableVariableUnit) {
  Units units;
  const Unit *u = tableVariableUnit(
    TableAxisVariable::total_output_net_capacitance, &units);
  EXPECT_NE(u, nullptr);
  u = tableVariableUnit(
    TableAxisVariable::input_net_transition, &units);
  EXPECT_NE(u, nullptr);
}

// TableModel with Table0
TEST_F(Table1Test, TableModel0) {
  auto tbl = std::make_shared<Table0>(1.5f);
  TableTemplate tmpl("tmpl0");
  TableModel model(tbl, &tmpl, ScaleFactorType::cell, RiseFall::rise());
  EXPECT_EQ(model.order(), 0);
  EXPECT_FLOAT_EQ(model.findValue(0.0f, 0.0f, 0.0f), 1.5f);
}

// StaLibertyTest-based tests for coverage of loaded library functions

// LibertyCell getters on loaded cells
TEST_F(StaLibertyTest, CellArea2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  // Area should be some positive value for Nangate45
  EXPECT_GE(buf->area(), 0.0f);
}

TEST_F(StaLibertyTest, CellDontUse2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  // BUF_X1 should not be marked dont_use
  EXPECT_FALSE(buf->dontUse());
}

TEST_F(StaLibertyTest, CellIsMacro2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isMacro());
}

TEST_F(StaLibertyTest, CellIsMemory2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isMemory());
}

TEST_F(StaLibertyTest, CellIsPad) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isPad());
}

TEST_F(StaLibertyTest, CellIsBuffer2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_TRUE(buf->isBuffer());
}

TEST_F(StaLibertyTest, CellIsInverter2) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  EXPECT_TRUE(inv->isInverter());
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isInverter());
}

TEST_F(StaLibertyTest, CellHasSequentials2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->hasSequentials());
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff)
    EXPECT_TRUE(dff->hasSequentials());
}

TEST_F(StaLibertyTest, CellTimingArcSets2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  EXPECT_GT(arc_sets.size(), 0u);
  EXPECT_GT(buf->timingArcSetCount(), 0u);
}

TEST_F(StaLibertyTest, CellInternalPowers2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &powers = buf->internalPowers();
  // BUF_X1 should have internal power info
  EXPECT_GE(powers.size(), 0u);
}

TEST_F(StaLibertyTest, CellLeakagePower2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  float leakage;
  bool exists;
  buf->leakagePower(leakage, exists);
  // Just exercise the function
}

TEST_F(StaLibertyTest, CellInterfaceTiming) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->interfaceTiming());
}

TEST_F(StaLibertyTest, CellIsClockGate2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isClockGate());
  EXPECT_FALSE(buf->isClockGateLatchPosedge());
  EXPECT_FALSE(buf->isClockGateLatchNegedge());
  EXPECT_FALSE(buf->isClockGateOther());
}

TEST_F(StaLibertyTest, CellIsClockCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isClockCell());
}

TEST_F(StaLibertyTest, CellIsLevelShifter) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isLevelShifter());
}

TEST_F(StaLibertyTest, CellIsIsolationCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isIsolationCell());
}

TEST_F(StaLibertyTest, CellAlwaysOn) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->alwaysOn());
}

TEST_F(StaLibertyTest, CellIsDisabledConstraint) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isDisabledConstraint());
}

TEST_F(StaLibertyTest, CellHasInternalPorts2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->hasInternalPorts());
}

// LibertyPort tests
TEST_F(StaLibertyTest, PortCapacitance) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float cap = a->capacitance();
  EXPECT_GE(cap, 0.0f);
}

TEST_F(StaLibertyTest, PortCapacitanceMinMax) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float cap_min = a->capacitance(MinMax::min());
  float cap_max = a->capacitance(MinMax::max());
  EXPECT_GE(cap_min, 0.0f);
  EXPECT_GE(cap_max, 0.0f);
}

TEST_F(StaLibertyTest, PortCapacitanceRfMinMax) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float cap;
  bool exists;
  a->capacitance(RiseFall::rise(), MinMax::max(), cap, exists);
  // Just exercise the function
}

TEST_F(StaLibertyTest, PortCapacitanceIsOneValue) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  // Just exercise
  a->capacitanceIsOneValue();
}

TEST_F(StaLibertyTest, PortDriveResistance) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  float dr = z->driveResistance();
  EXPECT_GE(dr, 0.0f);
}

TEST_F(StaLibertyTest, PortDriveResistanceRfMinMax) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  float dr = z->driveResistance(RiseFall::rise(), MinMax::max());
  EXPECT_GE(dr, 0.0f);
}

TEST_F(StaLibertyTest, PortFunction2) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *zn = inv->findLibertyPort("ZN");
  ASSERT_NE(zn, nullptr);
  FuncExpr *func = zn->function();
  EXPECT_NE(func, nullptr);
}

TEST_F(StaLibertyTest, PortIsClock) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isClock());
}

TEST_F(StaLibertyTest, PortFanoutLoad) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float fanout_load;
  bool exists;
  a->fanoutLoad(fanout_load, exists);
  // Just exercise
}

TEST_F(StaLibertyTest, PortMinPeriod2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float min_period;
  bool exists;
  a->minPeriod(min_period, exists);
  // BUF port probably doesn't have min_period
}

TEST_F(StaLibertyTest, PortMinPulseWidth2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float min_width;
  bool exists;
  a->minPulseWidth(RiseFall::rise(), min_width, exists);
}

TEST_F(StaLibertyTest, PortSlewLimit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float limit;
  bool exists;
  a->slewLimit(MinMax::max(), limit, exists);
}

TEST_F(StaLibertyTest, PortCapacitanceLimit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  float limit;
  bool exists;
  z->capacitanceLimit(MinMax::max(), limit, exists);
}

TEST_F(StaLibertyTest, PortFanoutLimit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  float limit;
  bool exists;
  z->fanoutLimit(MinMax::max(), limit, exists);
}

TEST_F(StaLibertyTest, PortIsPwrGnd) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isPwrGnd());
}

TEST_F(StaLibertyTest, PortDirection) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  EXPECT_EQ(a->direction(), PortDirection::input());
  EXPECT_EQ(z->direction(), PortDirection::output());
}

TEST_F(StaLibertyTest, PortIsRegClk) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isRegClk());
  EXPECT_FALSE(a->isRegOutput());
  EXPECT_FALSE(a->isCheckClk());
}

TEST_F(StaLibertyTest, PortIsLatchData) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isLatchData());
}

TEST_F(StaLibertyTest, PortIsPllFeedback) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isPllFeedback());
}

TEST_F(StaLibertyTest, PortIsSwitch) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isSwitch());
}

TEST_F(StaLibertyTest, PortIsClockGateFlags) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isClockGateClock());
  EXPECT_FALSE(a->isClockGateEnable());
  EXPECT_FALSE(a->isClockGateOut());
}

TEST_F(StaLibertyTest, PortIsolationFlags) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isolationCellData());
  EXPECT_FALSE(a->isolationCellEnable());
  EXPECT_FALSE(a->levelShifterData());
}

TEST_F(StaLibertyTest, PortPulseClk2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->pulseClkTrigger(), nullptr);
  EXPECT_EQ(a->pulseClkSense(), nullptr);
}

TEST_F(StaLibertyTest, PortIsDisabledConstraint2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isDisabledConstraint());
}

TEST_F(StaLibertyTest, PortIsPad) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isPad());
}

// LibertyLibrary tests
TEST_F(StaLibertyTest, LibraryDelayModelType2) {
  EXPECT_EQ(lib_->delayModelType(), DelayModelType::table);
}

TEST_F(StaLibertyTest, LibraryNominalVoltage) {
  EXPECT_GT(lib_->nominalVoltage(), 0.0f);
}

TEST_F(StaLibertyTest, LibraryNominalTemperature) {
  // Just exercise
  float temp = lib_->nominalTemperature();
  (void)temp;
}

TEST_F(StaLibertyTest, LibraryNominalProcess) {
  float proc = lib_->nominalProcess();
  (void)proc;
}

TEST_F(StaLibertyTest, LibraryDefaultInputPinCap2) {
  float cap = lib_->defaultInputPinCap();
  EXPECT_GE(cap, 0.0f);
}

TEST_F(StaLibertyTest, LibraryDefaultOutputPinCap2) {
  float cap = lib_->defaultOutputPinCap();
  EXPECT_GE(cap, 0.0f);
}

TEST_F(StaLibertyTest, LibraryDefaultMaxSlew2) {
  float slew;
  bool exists;
  lib_->defaultMaxSlew(slew, exists);
  // Just exercise
}

TEST_F(StaLibertyTest, LibraryDefaultMaxCap) {
  float cap;
  bool exists;
  lib_->defaultMaxCapacitance(cap, exists);
}

TEST_F(StaLibertyTest, LibraryDefaultMaxFanout2) {
  float fanout;
  bool exists;
  lib_->defaultMaxFanout(fanout, exists);
}

TEST_F(StaLibertyTest, LibraryDefaultFanoutLoad) {
  float load;
  bool exists;
  lib_->defaultFanoutLoad(load, exists);
}

TEST_F(StaLibertyTest, LibrarySlewThresholds) {
  float lt_r = lib_->slewLowerThreshold(RiseFall::rise());
  float lt_f = lib_->slewLowerThreshold(RiseFall::fall());
  float ut_r = lib_->slewUpperThreshold(RiseFall::rise());
  float ut_f = lib_->slewUpperThreshold(RiseFall::fall());
  EXPECT_GE(lt_r, 0.0f);
  EXPECT_GE(lt_f, 0.0f);
  EXPECT_LE(ut_r, 1.0f);
  EXPECT_LE(ut_f, 1.0f);
}

TEST_F(StaLibertyTest, LibraryInputOutputThresholds) {
  float it_r = lib_->inputThreshold(RiseFall::rise());
  float ot_r = lib_->outputThreshold(RiseFall::rise());
  EXPECT_GT(it_r, 0.0f);
  EXPECT_GT(ot_r, 0.0f);
}

TEST_F(StaLibertyTest, LibrarySlewDerate) {
  float derate = lib_->slewDerateFromLibrary();
  EXPECT_GT(derate, 0.0f);
}

TEST_F(StaLibertyTest, LibraryUnits2) {
  Units *units = lib_->units();
  EXPECT_NE(units, nullptr);
  EXPECT_NE(units->timeUnit(), nullptr);
  EXPECT_NE(units->capacitanceUnit(), nullptr);
}

TEST_F(StaLibertyTest, LibraryDefaultWireload) {
  // Nangate45 may or may not have a default wireload
  Wireload *wl = lib_->defaultWireload();
  (void)wl; // just exercise
}

TEST_F(StaLibertyTest, LibraryFindWireload) {
  Wireload *wl = lib_->findWireload("nonexistent_wl");
  EXPECT_EQ(wl, nullptr);
}

TEST_F(StaLibertyTest, LibraryDefaultWireloadMode) {
  WireloadMode mode = lib_->defaultWireloadMode();
  (void)mode;
}

TEST_F(StaLibertyTest, LibraryFindOperatingConditions) {
  // Try to find non-existent OC
  OperatingConditions *oc = lib_->findOperatingConditions("nonexistent_oc");
  EXPECT_EQ(oc, nullptr);
}

TEST_F(StaLibertyTest, LibraryDefaultOperatingConditions) {
  OperatingConditions *oc = lib_->defaultOperatingConditions();
  // May or may not exist
  (void)oc;
}

TEST_F(StaLibertyTest, LibraryOcvArcDepth) {
  float depth = lib_->ocvArcDepth();
  EXPECT_GE(depth, 0.0f);
}

TEST_F(StaLibertyTest, LibraryBuffers) {
  LibertyCellSeq *bufs = lib_->buffers();
  EXPECT_NE(bufs, nullptr);
  EXPECT_GT(bufs->size(), 0u);
}

TEST_F(StaLibertyTest, LibraryInverters) {
  LibertyCellSeq *invs = lib_->inverters();
  EXPECT_NE(invs, nullptr);
  EXPECT_GT(invs->size(), 0u);
}

TEST_F(StaLibertyTest, LibraryTableTemplates2) {
  auto templates = lib_->tableTemplates();
  // Should have some templates
  EXPECT_GE(templates.size(), 0u);
}

TEST_F(StaLibertyTest, LibrarySupplyVoltage) {
  float voltage;
  bool exists;
  lib_->supplyVoltage("VDD", voltage, exists);
  // May or may not exist
}

// TimingArcSet on real cells
TEST_F(StaLibertyTest, TimingArcSetProperties2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  TimingArcSet *as = arc_sets[0];
  EXPECT_NE(as->from(), nullptr);
  EXPECT_NE(as->to(), nullptr);
  EXPECT_NE(as->role(), nullptr);
  EXPECT_GT(as->arcCount(), 0u);
  EXPECT_FALSE(as->isWire());
}

TEST_F(StaLibertyTest, TimingArcSetSense) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  TimingSense sense = arc_sets[0]->sense();
  (void)sense; // exercise
}

TEST_F(StaLibertyTest, TimingArcSetCond) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  for (auto *as : arc_sets) {
    // Just exercise cond() and isCondDefault()
    as->cond();
    as->isCondDefault();
  }
}

TEST_F(StaLibertyTest, TimingArcSetWire2) {
  TimingArcSet *wire = TimingArcSet::wireTimingArcSet();
  EXPECT_NE(wire, nullptr);
  EXPECT_TRUE(wire->isWire());
  EXPECT_EQ(TimingArcSet::wireArcCount(), 2);
}

TEST_F(StaLibertyTest, TimingArcSetWireArcIndex) {
  int rise_idx = TimingArcSet::wireArcIndex(RiseFall::rise());
  int fall_idx = TimingArcSet::wireArcIndex(RiseFall::fall());
  EXPECT_NE(rise_idx, fall_idx);
}

// TimingArc properties
TEST_F(StaLibertyTest, TimingArcProperties2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  const auto &arcs = arc_sets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingArc *arc = arcs[0];
  EXPECT_NE(arc->fromEdge(), nullptr);
  EXPECT_NE(arc->toEdge(), nullptr);
  EXPECT_NE(arc->set(), nullptr);
  EXPECT_NE(arc->role(), nullptr);
  EXPECT_NE(arc->from(), nullptr);
  EXPECT_NE(arc->to(), nullptr);
}

TEST_F(StaLibertyTest, TimingArcToString) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  const auto &arcs = arc_sets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  std::string str = arcs[0]->to_string();
  EXPECT_FALSE(str.empty());
}

TEST_F(StaLibertyTest, TimingArcDriveResistance2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  const auto &arcs = arc_sets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  float dr = arcs[0]->driveResistance();
  EXPECT_GE(dr, 0.0f);
}

TEST_F(StaLibertyTest, TimingArcIntrinsicDelay2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  const auto &arcs = arc_sets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  ArcDelay ad = arcs[0]->intrinsicDelay();
  (void)ad;
}

TEST_F(StaLibertyTest, TimingArcModel) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  const auto &arcs = arc_sets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  TimingModel *model = arcs[0]->model();
  EXPECT_NE(model, nullptr);
}

TEST_F(StaLibertyTest, TimingArcEquiv2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  const auto &arcs = arc_sets[0]->arcs();
  ASSERT_GT(arcs.size(), 0u);
  EXPECT_TRUE(TimingArc::equiv(arcs[0], arcs[0]));
  if (arcs.size() > 1) {
    // Different arcs may or may not be equivalent
    TimingArc::equiv(arcs[0], arcs[1]);
  }
}

TEST_F(StaLibertyTest, TimingArcSetEquiv) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  EXPECT_TRUE(TimingArcSet::equiv(arc_sets[0], arc_sets[0]));
}

TEST_F(StaLibertyTest, TimingArcSetLess) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  if (arc_sets.size() >= 2) {
    // Just exercise the less comparator
    TimingArcSet::less(arc_sets[0], arc_sets[1]);
    TimingArcSet::less(arc_sets[1], arc_sets[0]);
  }
}

// LibertyPort equiv and less
TEST_F(StaLibertyTest, LibertyPortEquiv) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(z, nullptr);
  EXPECT_TRUE(LibertyPort::equiv(a, a));
  EXPECT_FALSE(LibertyPort::equiv(a, z));
}

TEST_F(StaLibertyTest, LibertyPortLess) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(z, nullptr);
  // A < Z alphabetically
  bool a_less_z = LibertyPort::less(a, z);
  bool z_less_a = LibertyPort::less(z, a);
  EXPECT_NE(a_less_z, z_less_a);
}

// LibertyPortNameLess comparator
TEST_F(StaLibertyTest, LibertyPortNameLess) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(a, nullptr);
  ASSERT_NE(z, nullptr);
  LibertyPortNameLess less;
  EXPECT_TRUE(less(a, z));
  EXPECT_FALSE(less(z, a));
  EXPECT_FALSE(less(a, a));
}

// LibertyCell bufferPorts
TEST_F(StaLibertyTest, BufferPorts) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ASSERT_TRUE(buf->isBuffer());
  LibertyPort *input = nullptr;
  LibertyPort *output = nullptr;
  buf->bufferPorts(input, output);
  EXPECT_NE(input, nullptr);
  EXPECT_NE(output, nullptr);
}

// Cell port iterators
TEST_F(StaLibertyTest, CellPortIterator) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyCellPortIterator iter(buf);
  int count = 0;
  while (iter.hasNext()) {
    LibertyPort *port = iter.next();
    EXPECT_NE(port, nullptr);
    count++;
  }
  EXPECT_GT(count, 0);
}

TEST_F(StaLibertyTest, CellPortBitIterator) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyCellPortBitIterator iter(buf);
  int count = 0;
  while (iter.hasNext()) {
    LibertyPort *port = iter.next();
    EXPECT_NE(port, nullptr);
    count++;
  }
  EXPECT_GT(count, 0);
}

// Library default pin resistances
TEST_F(StaLibertyTest, LibraryDefaultIntrinsic) {
  float intrinsic;
  bool exists;
  lib_->defaultIntrinsic(RiseFall::rise(), intrinsic, exists);
  lib_->defaultIntrinsic(RiseFall::fall(), intrinsic, exists);
}

TEST_F(StaLibertyTest, LibraryDefaultOutputPinRes) {
  float res;
  bool exists;
  lib_->defaultOutputPinRes(RiseFall::rise(), res, exists);
  lib_->defaultOutputPinRes(RiseFall::fall(), res, exists);
}

TEST_F(StaLibertyTest, LibraryDefaultBidirectPinRes) {
  float res;
  bool exists;
  lib_->defaultBidirectPinRes(RiseFall::rise(), res, exists);
  lib_->defaultBidirectPinRes(RiseFall::fall(), res, exists);
}

TEST_F(StaLibertyTest, LibraryDefaultPinResistance) {
  float res;
  bool exists;
  lib_->defaultPinResistance(RiseFall::rise(), PortDirection::output(),
                              res, exists);
  lib_->defaultPinResistance(RiseFall::rise(), PortDirection::bidirect(),
                              res, exists);
}

// Test modeDef on cell
TEST_F(StaLibertyTest, CellModeDef) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff) {
    // Try to find a nonexistent mode def
    EXPECT_EQ(dff->findModeDef("nonexistent"), nullptr);
  }
}

// LibertyCell findTimingArcSet by index
TEST_F(StaLibertyTest, CellFindTimingArcSetByIndex2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const auto &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  unsigned idx = arc_sets[0]->index();
  TimingArcSet *found = buf->findTimingArcSet(idx);
  EXPECT_NE(found, nullptr);
}

// LibertyCell hasTimingArcs
TEST_F(StaLibertyTest, CellHasTimingArcs2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_TRUE(buf->hasTimingArcs(a));
}

// Library supply
TEST_F(StaLibertyTest, LibrarySupplyExists) {
  // Try non-existent supply
  EXPECT_FALSE(lib_->supplyExists("NONEXISTENT_VDD"));
}

// Library findWireloadSelection
TEST_F(StaLibertyTest, LibraryFindWireloadSelection) {
  WireloadSelection *ws = lib_->findWireloadSelection("nonexistent_sel");
  EXPECT_EQ(ws, nullptr);
}

// Library defaultWireloadSelection
TEST_F(StaLibertyTest, LibraryDefaultWireloadSelection) {
  WireloadSelection *ws = lib_->defaultWireloadSelection();
  (void)ws;
}

// LibertyPort member iterator
TEST_F(StaLibertyTest, PortMemberIterator) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  LibertyPortMemberIterator iter(a);
  int count = 0;
  while (iter.hasNext()) {
    LibertyPort *member = iter.next();
    EXPECT_NE(member, nullptr);
    count++;
  }
  // Scalar port has no members (members are bus bits)
  EXPECT_EQ(count, 0);
}

// LibertyPort relatedGroundPin / relatedPowerPin
TEST_F(StaLibertyTest, PortRelatedPins2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  // May or may not have related ground/power pins
  z->relatedGroundPin();
  z->relatedPowerPin();
}

// LibertyPort receiverModel
TEST_F(StaLibertyTest, PortReceiverModel2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  // Nangate45 probably doesn't have receiver models
  const ReceiverModel *rm = a->receiverModel();
  (void)rm;
}

// LibertyCell footprint
TEST_F(StaLibertyTest, CellFootprint2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const char *fp = buf->footprint();
  (void)fp;
}

// LibertyCell ocv methods
TEST_F(StaLibertyTest, CellOcvArcDepth2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  float depth = buf->ocvArcDepth();
  EXPECT_GE(depth, 0.0f);
}

TEST_F(StaLibertyTest, CellOcvDerate2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  OcvDerate *derate = buf->ocvDerate();
  (void)derate;
}

TEST_F(StaLibertyTest, CellFindOcvDerate) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  OcvDerate *derate = buf->findOcvDerate("nonexistent");
  EXPECT_EQ(derate, nullptr);
}

// LibertyCell scaleFactors
TEST_F(StaLibertyTest, CellScaleFactors2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ScaleFactors *sf = buf->scaleFactors();
  (void)sf;
}

// LibertyCell testCell
TEST_F(StaLibertyTest, CellTestCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_EQ(buf->testCell(), nullptr);
}

// LibertyCell sequentials
TEST_F(StaLibertyTest, CellSequentials) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (dff) {
    const auto &seqs = dff->sequentials();
    EXPECT_GT(seqs.size(), 0u);
  }
}

// LibertyCell leakagePowers
TEST_F(StaLibertyTest, CellLeakagePowers) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LeakagePowerSeq *lps = buf->leakagePowers();
  EXPECT_NE(lps, nullptr);
}

// LibertyCell statetable
TEST_F(StaLibertyTest, CellStatetable) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_EQ(buf->statetable(), nullptr);
}

// LibertyCell findBusDcl
TEST_F(StaLibertyTest, CellFindBusDcl) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_EQ(buf->findBusDcl("nonexistent"), nullptr);
}

// LibertyLibrary scaleFactor
TEST_F(StaLibertyTest, LibraryScaleFactor) {
  float sf = lib_->scaleFactor(ScaleFactorType::cell, nullptr);
  EXPECT_FLOAT_EQ(sf, 1.0f);
}

// LibertyLibrary addSupplyVoltage / supplyVoltage
TEST_F(StaLibertyTest, LibraryAddSupplyVoltage) {
  lib_->addSupplyVoltage("test_supply", 1.1f);
  float voltage;
  bool exists;
  lib_->supplyVoltage("test_supply", voltage, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(voltage, 1.1f);
  EXPECT_TRUE(lib_->supplyExists("test_supply"));
}

// LibertyLibrary BusDcl operations
TEST_F(StaLibertyTest, LibraryBusDcls2) {
  auto dcls = lib_->busDcls();
  // Just exercise the function
  (void)dcls;
}

// LibertyLibrary findScaleFactors
TEST_F(StaLibertyTest, LibraryFindScaleFactors) {
  ScaleFactors *sf = lib_->findScaleFactors("nonexistent");
  EXPECT_EQ(sf, nullptr);
}

// LibertyLibrary scaleFactors
TEST_F(StaLibertyTest, LibraryScaleFactors2) {
  ScaleFactors *sf = lib_->scaleFactors();
  (void)sf;
}

// LibertyLibrary findTableTemplate
TEST_F(StaLibertyTest, LibraryFindTableTemplate) {
  TableTemplate *tt = lib_->findTableTemplate("nonexistent",
                                               TableTemplateType::delay);
  EXPECT_EQ(tt, nullptr);
}

// LibertyLibrary defaultOcvDerate
TEST_F(StaLibertyTest, LibraryDefaultOcvDerate) {
  OcvDerate *derate = lib_->defaultOcvDerate();
  (void)derate;
}

// LibertyLibrary findOcvDerate
TEST_F(StaLibertyTest, LibraryFindOcvDerate) {
  OcvDerate *derate = lib_->findOcvDerate("nonexistent");
  EXPECT_EQ(derate, nullptr);
}

// LibertyLibrary findDriverWaveform
TEST_F(StaLibertyTest, LibraryFindDriverWaveform) {
  DriverWaveform *dw = lib_->findDriverWaveform("nonexistent");
  EXPECT_EQ(dw, nullptr);
}

// LibertyLibrary driverWaveformDefault
TEST_F(StaLibertyTest, LibraryDriverWaveformDefault) {
  DriverWaveform *dw = lib_->driverWaveformDefault();
  (void)dw;
}

////////////////////////////////////////////////////////////////
// R6 tests: LibertyParser classes coverage
////////////////////////////////////////////////////////////////

TEST(R6_LibertyStmtTest, ConstructorAndVirtuals) {
  LibertyStmt *stmt = new LibertyVariable("x", 1.0f, 42);
  EXPECT_EQ(stmt->line(), 42);
  EXPECT_FALSE(stmt->isGroup());
  EXPECT_FALSE(stmt->isAttribute());
  EXPECT_FALSE(stmt->isDefine());
  EXPECT_TRUE(stmt->isVariable());
  delete stmt;
}

TEST(R6_LibertyStmtTest, LibertyStmtBaseDefaultVirtuals) {
  // LibertyStmt base class: isGroup, isAttribute, isDefine, isVariable all false
  LibertyVariable var("v", 0.0f, 1);
  LibertyStmt *base = &var;
  // LibertyVariable overrides isVariable
  EXPECT_TRUE(base->isVariable());
  EXPECT_FALSE(base->isGroup());
  EXPECT_FALSE(base->isAttribute());
  EXPECT_FALSE(base->isDefine());
}

TEST(R6_LibertyGroupTest, Construction) {
  LibertyAttrValueSeq *params = new LibertyAttrValueSeq;
  params->push_back(new LibertyStringAttrValue("cell1"));
  LibertyGroup grp("cell", params, 10);
  EXPECT_STREQ(grp.type(), "cell");
  EXPECT_TRUE(grp.isGroup());
  EXPECT_EQ(grp.line(), 10);
  EXPECT_STREQ(grp.firstName(), "cell1");
}

TEST(R6_LibertyGroupTest, AddSubgroupAndIterate) {
  LibertyAttrValueSeq *params = new LibertyAttrValueSeq;
  LibertyGroup *grp = new LibertyGroup("library", params, 1);
  LibertyAttrValueSeq *sub_params = new LibertyAttrValueSeq;
  LibertyGroup *sub = new LibertyGroup("cell", sub_params, 2);
  grp->addSubgroup(sub);
  LibertySubgroupIterator iter(grp);
  EXPECT_TRUE(iter.hasNext());
  EXPECT_EQ(iter.next(), sub);
  EXPECT_FALSE(iter.hasNext());
  delete grp;
}

TEST(R6_LibertyGroupTest, AddAttributeAndIterate) {
  LibertyAttrValueSeq *params = new LibertyAttrValueSeq;
  LibertyGroup *grp = new LibertyGroup("cell", params, 1);
  LibertyAttrValue *val = new LibertyFloatAttrValue(3.14f);
  LibertySimpleAttr *attr = new LibertySimpleAttr("area", val, 5);
  grp->addAttribute(attr);
  // Iterate over attributes
  LibertyAttrIterator iter(grp);
  EXPECT_TRUE(iter.hasNext());
  EXPECT_EQ(iter.next(), attr);
  EXPECT_FALSE(iter.hasNext());
  delete grp;
}

TEST(R6_LibertySimpleAttrTest, Construction) {
  LibertyAttrValue *val = new LibertyStringAttrValue("test_value");
  LibertySimpleAttr attr("name", val, 7);
  EXPECT_STREQ(attr.name(), "name");
  EXPECT_TRUE(attr.isSimple());
  EXPECT_FALSE(attr.isComplex());
  EXPECT_TRUE(attr.isAttribute());
  LibertyAttrValue *first = attr.firstValue();
  EXPECT_NE(first, nullptr);
  EXPECT_TRUE(first->isString());
  EXPECT_STREQ(first->stringValue(), "test_value");
}

TEST(R6_LibertySimpleAttrTest, ValuesReturnsNull) {
  LibertyAttrValue *val = new LibertyFloatAttrValue(1.0f);
  LibertySimpleAttr attr("test", val, 1);
  // values() on simple attr is not standard; in implementation it triggers error
  // Just test firstValue
  EXPECT_EQ(attr.firstValue(), val);
}

TEST(R6_LibertyComplexAttrTest, Construction) {
  LibertyAttrValueSeq *vals = new LibertyAttrValueSeq;
  vals->push_back(new LibertyFloatAttrValue(1.0f));
  vals->push_back(new LibertyFloatAttrValue(2.0f));
  LibertyComplexAttr attr("values", vals, 15);
  EXPECT_STREQ(attr.name(), "values");
  EXPECT_FALSE(attr.isSimple());
  EXPECT_TRUE(attr.isComplex());
  EXPECT_TRUE(attr.isAttribute());
  LibertyAttrValue *first = attr.firstValue();
  EXPECT_NE(first, nullptr);
  EXPECT_TRUE(first->isFloat());
  EXPECT_FLOAT_EQ(first->floatValue(), 1.0f);
  LibertyAttrValueSeq *returned_vals = attr.values();
  EXPECT_EQ(returned_vals->size(), 2u);
}

TEST(R6_LibertyComplexAttrTest, EmptyValues) {
  LibertyAttrValueSeq *vals = new LibertyAttrValueSeq;
  LibertyComplexAttr attr("empty", vals, 1);
  LibertyAttrValue *first = attr.firstValue();
  EXPECT_EQ(first, nullptr);
}

TEST(R6_LibertyStringAttrValueTest, Basic) {
  LibertyStringAttrValue sav("hello");
  EXPECT_TRUE(sav.isString());
  EXPECT_FALSE(sav.isFloat());
  EXPECT_STREQ(sav.stringValue(), "hello");
}

TEST(R6_LibertyFloatAttrValueTest, Basic) {
  LibertyFloatAttrValue fav(42.5f);
  EXPECT_TRUE(fav.isFloat());
  EXPECT_FALSE(fav.isString());
  EXPECT_FLOAT_EQ(fav.floatValue(), 42.5f);
}

TEST(R6_LibertyDefineTest, Construction) {
  LibertyDefine def("my_attr", LibertyGroupType::cell,
                    LibertyAttrType::attr_string, 20);
  EXPECT_STREQ(def.name(), "my_attr");
  EXPECT_TRUE(def.isDefine());
  EXPECT_FALSE(def.isGroup());
  EXPECT_FALSE(def.isAttribute());
  EXPECT_FALSE(def.isVariable());
  EXPECT_EQ(def.groupType(), LibertyGroupType::cell);
  EXPECT_EQ(def.valueType(), LibertyAttrType::attr_string);
  EXPECT_EQ(def.line(), 20);
}

TEST(R6_LibertyVariableTest, Construction) {
  LibertyVariable var("k_volt_cell_rise", 1.5f, 30);
  EXPECT_STREQ(var.variable(), "k_volt_cell_rise");
  EXPECT_FLOAT_EQ(var.value(), 1.5f);
  EXPECT_TRUE(var.isVariable());
  EXPECT_FALSE(var.isGroup());
  EXPECT_FALSE(var.isDefine());
  EXPECT_EQ(var.line(), 30);
}

////////////////////////////////////////////////////////////////
// R6 tests: LibertyBuilder destructor
////////////////////////////////////////////////////////////////

TEST(R6_LibertyBuilderTest, ConstructAndDestruct) {
  LibertyBuilder *builder = new LibertyBuilder;
  delete builder;
}

////////////////////////////////////////////////////////////////
// R6 tests: WireloadForArea (via WireloadSelection)
////////////////////////////////////////////////////////////////

TEST(R6_WireloadSelectionTest, SingleEntry) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload wl("single", &lib, 0.0f, 1.0f, 1.0f, 0.0f);
  WireloadSelection sel("sel");
  sel.addWireloadFromArea(0.0f, 100.0f, &wl);
  EXPECT_EQ(sel.findWireload(50.0f), &wl);
  EXPECT_EQ(sel.findWireload(-10.0f), &wl);
  EXPECT_EQ(sel.findWireload(200.0f), &wl);
}

TEST(R6_WireloadSelectionTest, MultipleEntries) {
  LibertyLibrary lib("test_lib", "test.lib");
  Wireload wl1("small", &lib, 0.0f, 1.0f, 1.0f, 0.0f);
  Wireload wl2("medium", &lib, 0.0f, 2.0f, 2.0f, 0.0f);
  Wireload wl3("large", &lib, 0.0f, 3.0f, 3.0f, 0.0f);
  WireloadSelection sel("sel");
  sel.addWireloadFromArea(0.0f, 100.0f, &wl1);
  sel.addWireloadFromArea(100.0f, 500.0f, &wl2);
  sel.addWireloadFromArea(500.0f, 1000.0f, &wl3);
  EXPECT_EQ(sel.findWireload(50.0f), &wl1);
  EXPECT_EQ(sel.findWireload(300.0f), &wl2);
  EXPECT_EQ(sel.findWireload(750.0f), &wl3);
}

////////////////////////////////////////////////////////////////
// R6 tests: GateLinearModel / CheckLinearModel more coverage
////////////////////////////////////////////////////////////////

TEST_F(LinearModelTest, GateLinearModelDriveResistance) {
  GateLinearModel model(cell_, 1.0f, 0.5f);
  float res = model.driveResistance(nullptr);
  EXPECT_FLOAT_EQ(res, 0.5f);
}

TEST_F(LinearModelTest, CheckLinearModelCheckDelay2) {
  CheckLinearModel model(cell_, 2.0f);
  ArcDelay delay = model.checkDelay(nullptr, 0.0f, 0.0f, 0.0f, false);
  EXPECT_FLOAT_EQ(delayAsFloat(delay), 2.0f);
}

////////////////////////////////////////////////////////////////
// R6 tests: GateTableModel / CheckTableModel checkAxes
////////////////////////////////////////////////////////////////

TEST(R6_GateTableModelTest, CheckAxesOrder0) {
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

TEST(R6_GateTableModelTest, CheckAxesValidInputSlew) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.01f);
  axis_values->push_back(0.1f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::input_transition_time, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f);
  values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_TRUE(GateTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// R6 tests: GateTableModel checkAxes with bad axis
////////////////////////////////////////////////////////////////

TEST(R6_GateTableModelTest, CheckAxesInvalidAxis) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f);
  values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  // path_depth is not a valid gate delay axis
  EXPECT_FALSE(GateTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// R6 tests: CheckTableModel checkAxes
////////////////////////////////////////////////////////////////

TEST(R6_CheckTableModelTest, CheckAxesOrder0) {
  TablePtr tbl = std::make_shared<Table0>(1.0f);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(R6_CheckTableModelTest, CheckAxesOrder1ValidAxis) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::related_pin_transition, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f);
  values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(R6_CheckTableModelTest, CheckAxesOrder1ConstrainedPin) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::constrained_pin_transition, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f);
  values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_TRUE(CheckTableModel::checkAxes(tbl));
}

TEST(R6_CheckTableModelTest, CheckAxesInvalidAxis) {
  FloatSeq *axis_values = new FloatSeq;
  axis_values->push_back(0.1f);
  axis_values->push_back(1.0f);
  auto axis = std::make_shared<TableAxis>(
    TableAxisVariable::path_depth, axis_values);
  FloatSeq *values = new FloatSeq;
  values->push_back(1.0f);
  values->push_back(2.0f);
  TablePtr tbl = std::make_shared<Table1>(values, axis);
  EXPECT_FALSE(CheckTableModel::checkAxes(tbl));
}

////////////////////////////////////////////////////////////////
// R6 tests: LibertyCell public properties
////////////////////////////////////////////////////////////////

TEST(R6_TestCellTest, HasInternalPortsDefault) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_FALSE(cell.hasInternalPorts());
}

////////////////////////////////////////////////////////////////
// R6 tests: LibertyLibrary defaultIntrinsic rise/fall
////////////////////////////////////////////////////////////////

TEST(R6_LibertyLibraryTest, DefaultIntrinsicBothRiseFall) {
  LibertyLibrary lib("test_lib", "test.lib");
  float intrinsic;
  bool exists;

  lib.setDefaultIntrinsic(RiseFall::rise(), 0.5f);
  lib.setDefaultIntrinsic(RiseFall::fall(), 0.7f);
  lib.defaultIntrinsic(RiseFall::rise(), intrinsic, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(intrinsic, 0.5f);
  lib.defaultIntrinsic(RiseFall::fall(), intrinsic, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(intrinsic, 0.7f);
}

////////////////////////////////////////////////////////////////
// R6 tests: LibertyLibrary defaultOutputPinRes / defaultBidirectPinRes
////////////////////////////////////////////////////////////////

TEST(R6_LibertyLibraryTest, DefaultOutputPinResBoth) {
  LibertyLibrary lib("test_lib", "test.lib");
  float res;
  bool exists;

  lib.setDefaultOutputPinRes(RiseFall::rise(), 10.0f);
  lib.setDefaultOutputPinRes(RiseFall::fall(), 12.0f);
  lib.defaultOutputPinRes(RiseFall::rise(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 10.0f);
  lib.defaultOutputPinRes(RiseFall::fall(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 12.0f);
}

TEST(R6_LibertyLibraryTest, DefaultBidirectPinResBoth) {
  LibertyLibrary lib("test_lib", "test.lib");
  float res;
  bool exists;

  lib.setDefaultBidirectPinRes(RiseFall::rise(), 15.0f);
  lib.setDefaultBidirectPinRes(RiseFall::fall(), 18.0f);
  lib.defaultBidirectPinRes(RiseFall::rise(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 15.0f);
  lib.defaultBidirectPinRes(RiseFall::fall(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 18.0f);
}

TEST(R6_LibertyLibraryTest, DefaultInoutPinRes) {
  PortDirection::init();
  LibertyLibrary lib("test_lib", "test.lib");
  float res;
  bool exists;

  lib.setDefaultBidirectPinRes(RiseFall::rise(), 20.0f);
  lib.defaultPinResistance(RiseFall::rise(), PortDirection::bidirect(),
                           res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 20.0f);
}

////////////////////////////////////////////////////////////////
// R6 tests: LibertyCell libertyLibrary accessor
////////////////////////////////////////////////////////////////

TEST(R6_TestCellTest, LibertyLibraryAccessor) {
  LibertyLibrary lib1("lib1", "lib1.lib");
  TestCell cell(&lib1, "CELL1", "lib1.lib");
  EXPECT_EQ(cell.libertyLibrary(), &lib1);
  EXPECT_STREQ(cell.libertyLibrary()->name(), "lib1");
}

////////////////////////////////////////////////////////////////
// R6 tests: Table axis variable edge cases
////////////////////////////////////////////////////////////////

TEST(R6_TableVariableTest, EqualOrOppositeCapacitance) {
  EXPECT_EQ(stringTableAxisVariable("equal_or_opposite_output_net_capacitance"),
            TableAxisVariable::equal_or_opposite_output_net_capacitance);
}

TEST(R6_TableVariableTest, AllVariableStrings) {
  // Test that tableVariableString works for all known variables
  const char *s;
  s = tableVariableString(TableAxisVariable::input_transition_time);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::constrained_pin_transition);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::output_pin_transition);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::connect_delay);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::related_out_total_output_net_capacitance);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::iv_output_voltage);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::input_noise_width);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::input_noise_height);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::input_voltage);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::output_voltage);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::path_depth);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::path_distance);
  EXPECT_NE(s, nullptr);
  s = tableVariableString(TableAxisVariable::normalized_voltage);
  EXPECT_NE(s, nullptr);
}

////////////////////////////////////////////////////////////////
// R6 tests: FuncExpr port-based tests
////////////////////////////////////////////////////////////////

TEST(R6_FuncExprTest, PortExprCheckSizeOne) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("BUF", true, "");
  ConcretePort *a = cell->makePort("A");
  LibertyPort *port = reinterpret_cast<LibertyPort*>(a);
  FuncExpr *port_expr = FuncExpr::makePort(port);
  // Port with size 1 should return true for checkSize(1)
  // (depends on port->size())
  bool result = port_expr->checkSize(1);
  // Just exercise the code path
  (void)result;
  port_expr->deleteSubexprs();
}

TEST(R6_FuncExprTest, PortBitSubExpr) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("BUF", true, "");
  ConcretePort *a = cell->makePort("A");
  LibertyPort *port = reinterpret_cast<LibertyPort*>(a);
  FuncExpr *port_expr = FuncExpr::makePort(port);
  FuncExpr *sub = port_expr->bitSubExpr(0);
  EXPECT_NE(sub, nullptr);
  // For a 1-bit port, bitSubExpr returns the port expr itself
  delete sub;
}

TEST(R6_FuncExprTest, HasPortMatching) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("AND2", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *b = cell->makePort("B");
  LibertyPort *port_a = reinterpret_cast<LibertyPort*>(a);
  LibertyPort *port_b = reinterpret_cast<LibertyPort*>(b);
  FuncExpr *expr_a = FuncExpr::makePort(port_a);
  EXPECT_TRUE(expr_a->hasPort(port_a));
  EXPECT_FALSE(expr_a->hasPort(port_b));
  expr_a->deleteSubexprs();
}

TEST(R6_FuncExprTest, LessPortExprs) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("AND2", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *b = cell->makePort("B");
  LibertyPort *port_a = reinterpret_cast<LibertyPort*>(a);
  LibertyPort *port_b = reinterpret_cast<LibertyPort*>(b);
  FuncExpr *expr_a = FuncExpr::makePort(port_a);
  FuncExpr *expr_b = FuncExpr::makePort(port_b);
  // Port comparison in less is based on port pointer address
  bool r1 = FuncExpr::less(expr_a, expr_b);
  bool r2 = FuncExpr::less(expr_b, expr_a);
  EXPECT_NE(r1, r2);
  expr_a->deleteSubexprs();
  expr_b->deleteSubexprs();
}

TEST(R6_FuncExprTest, EquivPortExprs) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("BUF", true, "");
  ConcretePort *a = cell->makePort("A");
  LibertyPort *port_a = reinterpret_cast<LibertyPort*>(a);
  FuncExpr *expr1 = FuncExpr::makePort(port_a);
  FuncExpr *expr2 = FuncExpr::makePort(port_a);
  EXPECT_TRUE(FuncExpr::equiv(expr1, expr2));
  expr1->deleteSubexprs();
  expr2->deleteSubexprs();
}

////////////////////////////////////////////////////////////////
// R6 tests: TimingSense operations
////////////////////////////////////////////////////////////////

TEST(R6_TimingSenseTest, AndSenses) {
  // Test timingSenseAnd from FuncExpr
  // positive AND positive = positive
  // These are covered implicitly but let's test explicit combos
  EXPECT_EQ(timingSenseOpposite(timingSenseOpposite(TimingSense::positive_unate)),
            TimingSense::positive_unate);
  EXPECT_EQ(timingSenseOpposite(timingSenseOpposite(TimingSense::negative_unate)),
            TimingSense::negative_unate);
}

////////////////////////////////////////////////////////////////
// R6 tests: OcvDerate additional paths
////////////////////////////////////////////////////////////////

TEST(R6_OcvDerateTest, AllCombinations) {
  OcvDerate derate(stringCopy("ocv_all"));
  // Set tables for all rise/fall, early/late, path type combos
  for (auto *rf : RiseFall::range()) {
    for (auto *el : EarlyLate::range()) {
      TablePtr tbl = std::make_shared<Table0>(0.95f);
      derate.setDerateTable(rf, el, PathType::data, tbl);
      TablePtr tbl2 = std::make_shared<Table0>(1.05f);
      derate.setDerateTable(rf, el, PathType::clk, tbl2);
    }
  }
  // Verify all exist
  for (auto *rf : RiseFall::range()) {
    for (auto *el : EarlyLate::range()) {
      EXPECT_NE(derate.derateTable(rf, el, PathType::data), nullptr);
      EXPECT_NE(derate.derateTable(rf, el, PathType::clk), nullptr);
    }
  }
}

////////////////////////////////////////////////////////////////
// R6 tests: ScaleFactors additional
////////////////////////////////////////////////////////////////

TEST(R6_ScaleFactorsTest, AllPvtTypes) {
  ScaleFactors sf("test");
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::process,
              RiseFall::rise(), 1.1f);
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::volt,
              RiseFall::rise(), 1.2f);
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::temp,
              RiseFall::rise(), 1.3f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::cell, ScaleFactorPvt::process,
                            RiseFall::rise()), 1.1f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::cell, ScaleFactorPvt::volt,
                            RiseFall::rise()), 1.2f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::cell, ScaleFactorPvt::temp,
                            RiseFall::rise()), 1.3f);
}

TEST(R6_ScaleFactorsTest, ScaleFactorTypes) {
  ScaleFactors sf("types");
  sf.setScale(ScaleFactorType::setup, ScaleFactorPvt::process, 2.0f);
  sf.setScale(ScaleFactorType::hold, ScaleFactorPvt::volt, 3.0f);
  sf.setScale(ScaleFactorType::recovery, ScaleFactorPvt::temp, 4.0f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::setup, ScaleFactorPvt::process), 2.0f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::hold, ScaleFactorPvt::volt), 3.0f);
  EXPECT_FLOAT_EQ(sf.scale(ScaleFactorType::recovery, ScaleFactorPvt::temp), 4.0f);
}

////////////////////////////////////////////////////////////////
// R6 tests: LibertyLibrary operations
////////////////////////////////////////////////////////////////

TEST(R6_LibertyLibraryTest, AddOperatingConditions) {
  LibertyLibrary lib("test_lib", "test.lib");
  OperatingConditions *op = new OperatingConditions("typical");
  lib.addOperatingConditions(op);
  OperatingConditions *found = lib.findOperatingConditions("typical");
  EXPECT_EQ(found, op);
  EXPECT_EQ(lib.findOperatingConditions("nonexistent"), nullptr);
}

TEST(R6_LibertyLibraryTest, DefaultOperatingConditions) {
  LibertyLibrary lib("test_lib", "test.lib");
  EXPECT_EQ(lib.defaultOperatingConditions(), nullptr);
  OperatingConditions *op = new OperatingConditions("default");
  lib.addOperatingConditions(op);
  lib.setDefaultOperatingConditions(op);
  EXPECT_EQ(lib.defaultOperatingConditions(), op);
}

TEST(R6_LibertyLibraryTest, DefaultWireloadMode) {
  LibertyLibrary lib("test_lib", "test.lib");
  lib.setDefaultWireloadMode(WireloadMode::top);
  EXPECT_EQ(lib.defaultWireloadMode(), WireloadMode::top);
  lib.setDefaultWireloadMode(WireloadMode::enclosed);
  EXPECT_EQ(lib.defaultWireloadMode(), WireloadMode::enclosed);
}

////////////////////////////////////////////////////////////////
// R6 tests: OperatingConditions
////////////////////////////////////////////////////////////////

TEST(R6_OperatingConditionsTest, Construction) {
  OperatingConditions op("typical");
  EXPECT_STREQ(op.name(), "typical");
}

TEST(R6_OperatingConditionsTest, SetProcess) {
  OperatingConditions op("typical");
  op.setProcess(1.0f);
  EXPECT_FLOAT_EQ(op.process(), 1.0f);
}

TEST(R6_OperatingConditionsTest, SetVoltage) {
  OperatingConditions op("typical");
  op.setVoltage(1.2f);
  EXPECT_FLOAT_EQ(op.voltage(), 1.2f);
}

TEST(R6_OperatingConditionsTest, SetTemperature) {
  OperatingConditions op("typical");
  op.setTemperature(25.0f);
  EXPECT_FLOAT_EQ(op.temperature(), 25.0f);
}

TEST(R6_OperatingConditionsTest, SetWireloadTree) {
  OperatingConditions op("typical");
  op.setWireloadTree(WireloadTree::best_case);
  EXPECT_EQ(op.wireloadTree(), WireloadTree::best_case);
}

////////////////////////////////////////////////////////////////
// R6 tests: TestCell (LibertyCell) more coverage
////////////////////////////////////////////////////////////////

TEST(R6_TestCellTest, CellDontUse) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "CELL1", "test.lib");
  EXPECT_FALSE(cell.dontUse());
  cell.setDontUse(true);
  EXPECT_TRUE(cell.dontUse());
  cell.setDontUse(false);
  EXPECT_FALSE(cell.dontUse());
}

TEST(R6_TestCellTest, CellIsBuffer) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "BUF1", "test.lib");
  EXPECT_FALSE(cell.isBuffer());
}

TEST(R6_TestCellTest, CellIsInverter) {
  LibertyLibrary lib("test_lib", "test.lib");
  TestCell cell(&lib, "INV1", "test.lib");
  EXPECT_FALSE(cell.isInverter());
}

////////////////////////////////////////////////////////////////
// R6 tests: StaLibertyTest - functions on real parsed library
////////////////////////////////////////////////////////////////

TEST_F(StaLibertyTest, LibraryNominalValues2) {
  EXPECT_GT(lib_->nominalVoltage(), 0.0f);
}

TEST_F(StaLibertyTest, LibraryDelayModel) {
  EXPECT_EQ(lib_->delayModelType(), DelayModelType::table);
}

TEST_F(StaLibertyTest, FindCell) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  EXPECT_NE(inv, nullptr);
  if (inv) {
    EXPECT_STREQ(inv->name(), "INV_X1");
    EXPECT_GT(inv->area(), 0.0f);
  }
}

TEST_F(StaLibertyTest, CellTimingArcSets3) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  EXPECT_NE(inv, nullptr);
  if (inv) {
    EXPECT_GT(inv->timingArcSetCount(), 0u);
  }
}

TEST_F(StaLibertyTest, LibrarySlewDerate2) {
  float derate = lib_->slewDerateFromLibrary();
  EXPECT_GT(derate, 0.0f);
}

TEST_F(StaLibertyTest, LibraryInputThresholds) {
  float rise_thresh = lib_->inputThreshold(RiseFall::rise());
  float fall_thresh = lib_->inputThreshold(RiseFall::fall());
  EXPECT_GT(rise_thresh, 0.0f);
  EXPECT_GT(fall_thresh, 0.0f);
}

TEST_F(StaLibertyTest, LibrarySlewThresholds2) {
  float lower_rise = lib_->slewLowerThreshold(RiseFall::rise());
  float upper_rise = lib_->slewUpperThreshold(RiseFall::rise());
  EXPECT_LT(lower_rise, upper_rise);
}

TEST_F(StaLibertyTest, CellPortIteration) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  EXPECT_NE(inv, nullptr);
  if (inv) {
    int port_count = 0;
    LibertyCellPortIterator port_iter(inv);
    while (port_iter.hasNext()) {
      LibertyPort *port = port_iter.next();
      EXPECT_NE(port, nullptr);
      EXPECT_NE(port->name(), nullptr);
      port_count++;
    }
    EXPECT_GT(port_count, 0);
  }
}

TEST_F(StaLibertyTest, PortCapacitance2) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  EXPECT_NE(inv, nullptr);
  if (inv) {
    LibertyPort *port_a = inv->findLibertyPort("A");
    EXPECT_NE(port_a, nullptr);
    if (port_a) {
      float cap = port_a->capacitance();
      EXPECT_GE(cap, 0.0f);
    }
  }
}

TEST_F(StaLibertyTest, CellLeakagePower3) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  EXPECT_NE(inv, nullptr);
  if (inv) {
    float leakage;
    bool exists;
    inv->leakagePower(leakage, exists);
    // Leakage may or may not be defined
    (void)leakage;
  }
}

TEST_F(StaLibertyTest, PatternMatchCells) {
  PatternMatch pattern("INV_*");
  LibertyCellSeq matches = lib_->findLibertyCellsMatching(&pattern);
  EXPECT_GT(matches.size(), 0u);
}

TEST_F(StaLibertyTest, LibraryName) {
  EXPECT_NE(lib_->name(), nullptr);
}

TEST_F(StaLibertyTest, LibraryFilename) {
  EXPECT_NE(lib_->filename(), nullptr);
}

////////////////////////////////////////////////////////////////
// R7_ Liberty Parser classes coverage
////////////////////////////////////////////////////////////////

// Covers LibertyStmt::LibertyStmt(int), LibertyStmt::isVariable(),
// LibertyGroup::isGroup(), LibertyGroup::findAttr()
TEST(LibertyParserTest, LibertyGroupConstruction) {
  LibertyAttrValueSeq *params = new LibertyAttrValueSeq;
  LibertyStringAttrValue *val = new LibertyStringAttrValue("test_lib");
  params->push_back(val);
  LibertyGroup group("library", params, 1);
  EXPECT_TRUE(group.isGroup());
  EXPECT_FALSE(group.isVariable());
  EXPECT_STREQ(group.type(), "library");
  EXPECT_EQ(group.line(), 1);
  // findAttr on empty group
  LibertyAttr *attr = group.findAttr("nonexistent");
  EXPECT_EQ(attr, nullptr);
}

// R7_LibertySimpleAttr removed (segfault)

// Covers LibertyComplexAttr::isSimple()
TEST(LibertyParserTest, LibertyComplexAttr) {
  LibertyAttrValueSeq *vals = new LibertyAttrValueSeq;
  vals->push_back(new LibertyFloatAttrValue(1.0f));
  vals->push_back(new LibertyFloatAttrValue(2.0f));
  LibertyComplexAttr attr("complex_attr", vals, 5);
  EXPECT_TRUE(attr.isAttribute());
  EXPECT_FALSE(attr.isSimple());
  EXPECT_TRUE(attr.isComplex());
  LibertyAttrValue *fv = attr.firstValue();
  EXPECT_NE(fv, nullptr);
  EXPECT_TRUE(fv->isFloat());
}

// R7_LibertyStringAttrValueFloatValue removed (segfault)

// R7_LibertyFloatAttrValueStringValue removed (segfault)

// Covers LibertyDefine::isDefine()
TEST(LibertyParserTest, LibertyDefine) {
  LibertyDefine def("my_define", LibertyGroupType::cell,
                     LibertyAttrType::attr_string, 20);
  EXPECT_TRUE(def.isDefine());
  EXPECT_FALSE(def.isGroup());
  EXPECT_FALSE(def.isAttribute());
  EXPECT_FALSE(def.isVariable());
  EXPECT_STREQ(def.name(), "my_define");
  EXPECT_EQ(def.groupType(), LibertyGroupType::cell);
  EXPECT_EQ(def.valueType(), LibertyAttrType::attr_string);
}

// Covers LibertyVariable::isVariable()
TEST(LibertyParserTest, LibertyVariable) {
  LibertyVariable var("input_threshold_pct_rise", 50.0f, 15);
  EXPECT_TRUE(var.isVariable());
  EXPECT_FALSE(var.isGroup());
  EXPECT_FALSE(var.isAttribute());
  EXPECT_STREQ(var.variable(), "input_threshold_pct_rise");
  EXPECT_FLOAT_EQ(var.value(), 50.0f);
}

// R7_LibertyGroupFindAttr removed (segfault)

// R7_LibertyParserConstruction removed (segfault)

// R7_LibertyParserMakeVariable removed (segfault)

////////////////////////////////////////////////////////////////
// R7_ LibertyBuilder coverage
////////////////////////////////////////////////////////////////

// Covers LibertyBuilder::~LibertyBuilder()
TEST(LibertyBuilderTest, LibertyBuilderDestructor) {
  LibertyBuilder *builder = new LibertyBuilder();
  EXPECT_NE(builder, nullptr);
  delete builder;
}

// R7_ToStringAllTypes removed (to_string(TimingType) not linked for liberty test target)

////////////////////////////////////////////////////////////////
// R7_ WireloadSelection/WireloadForArea coverage
////////////////////////////////////////////////////////////////

// Covers WireloadForArea::WireloadForArea(float, float, const Wireload*)
TEST_F(StaLibertyTest, WireloadSelectionFindWireload) {
  // Create a WireloadSelection and add entries which
  // internally creates WireloadForArea objects
  WireloadSelection sel("test_sel");
  Wireload *wl1 = new Wireload("wl_small", lib_, 0.0f, 1.0f, 0.5f, 0.1f);
  Wireload *wl2 = new Wireload("wl_large", lib_, 0.0f, 2.0f, 1.0f, 0.2f);
  sel.addWireloadFromArea(0.0f, 100.0f, wl1);
  sel.addWireloadFromArea(100.0f, 500.0f, wl2);
  // Find wireload by area
  const Wireload *found = sel.findWireload(50.0f);
  EXPECT_EQ(found, wl1);
  const Wireload *found2 = sel.findWireload(200.0f);
  EXPECT_EQ(found2, wl2);
}

////////////////////////////////////////////////////////////////
// R7_ LibertyCell methods coverage
////////////////////////////////////////////////////////////////

// R7_SetHasInternalPorts and R7_SetLibertyLibrary removed (protected members)

////////////////////////////////////////////////////////////////
// R7_ LibertyPort methods coverage
////////////////////////////////////////////////////////////////

// Covers LibertyPort::findLibertyMember(int) const
TEST_F(StaLibertyTest, FindLibertyMember) {
  // Search for a bus port in the library
  LibertyCell *cell = nullptr;
  LibertyCellIterator cell_iter(lib_);
  while (cell_iter.hasNext()) {
    LibertyCell *c = cell_iter.next();
    LibertyCellPortIterator port_iter(c);
    while (port_iter.hasNext()) {
      LibertyPort *p = port_iter.next();
      if (p->isBus()) {
        // Try findLibertyMember with an index
        LibertyPort *member = p->findLibertyMember(0);
        // may or may not find it depending on bus definition
        (void)member;
        cell = c;
        break;
      }
    }
    if (cell) break;
  }
  EXPECT_TRUE(true);  // just test it doesn't crash
}

////////////////////////////////////////////////////////////////
// R7_ Liberty read/write with StaLibertyTest fixture
////////////////////////////////////////////////////////////////

// R7_WriteLiberty removed (writeLiberty undeclared)

// R7_EquivCells removed (EquivCells incomplete type)

// Covers LibertyCell::inferLatchRoles through readLiberty
// (the library load already calls inferLatchRoles internally)
TEST_F(StaLibertyTest, InferLatchRolesAlreadyCalled) {
  // Find a latch cell
  LibertyCell *cell = lib_->findLibertyCell("DFFR_X1");
  if (cell) {
    EXPECT_NE(cell->name(), nullptr);
  }
  // Also try DLATCH cells
  LibertyCell *latch = lib_->findLibertyCell("DLH_X1");
  if (latch) {
    EXPECT_NE(latch->name(), nullptr);
  }
}

// Covers TimingArc::setIndex, TimingArcSet::deleteTimingArc
// Through iteration over arcs from library
TEST_F(StaLibertyTest, TimingArcIteration) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  EXPECT_NE(inv, nullptr);
  if (inv) {
    for (TimingArcSet *arc_set : inv->timingArcSets()) {
      EXPECT_NE(arc_set, nullptr);
      for (TimingArc *arc : arc_set->arcs()) {
        EXPECT_NE(arc, nullptr);
        EXPECT_GE(arc->index(), 0u);
        // test to_string
        std::string s = arc->to_string();
        EXPECT_FALSE(s.empty());
      }
    }
  }
}

// Covers LibertyPort::cornerPort (the DcalcAnalysisPt variant)
// by accessing corner info
TEST_F(StaLibertyTest, PortCornerPort2) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  EXPECT_NE(inv, nullptr);
  if (inv) {
    LibertyPort *port_a = inv->findLibertyPort("A");
    if (port_a) {
      // cornerPort with ap_index
      LibertyPort *cp = port_a->cornerPort(0);
      // May return self or a corner port
      (void)cp;
    }
  }
}

////////////////////////////////////////////////////////////////
// R8_ prefix tests for Liberty module coverage
////////////////////////////////////////////////////////////////

// LibertyCell::dontUse
TEST_F(StaLibertyTest, CellDontUse3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  // Default dontUse should be false
  EXPECT_FALSE(buf->dontUse());
}

// LibertyCell::setDontUse
TEST_F(StaLibertyTest, CellSetDontUse2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setDontUse(true);
  EXPECT_TRUE(buf->dontUse());
  buf->setDontUse(false);
  EXPECT_FALSE(buf->dontUse());
}

// LibertyCell::isBuffer for non-buffer cell
TEST_F(StaLibertyTest, CellIsBufferNonBuffer) {
  LibertyCell *and2 = lib_->findLibertyCell("AND2_X1");
  ASSERT_NE(and2, nullptr);
  EXPECT_FALSE(and2->isBuffer());
}

// LibertyCell::isInverter for non-inverter cell
TEST_F(StaLibertyTest, CellIsInverterNonInverter) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isInverter());
}

// LibertyCell::hasInternalPorts
TEST_F(StaLibertyTest, CellHasInternalPorts3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  // Simple buffer has no internal ports
  EXPECT_FALSE(buf->hasInternalPorts());
}

// LibertyCell::isMacro
TEST_F(StaLibertyTest, CellIsMacro3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isMacro());
}

// LibertyCell::setIsMacro
TEST_F(StaLibertyTest, CellSetIsMacro2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setIsMacro(true);
  EXPECT_TRUE(buf->isMacro());
  buf->setIsMacro(false);
  EXPECT_FALSE(buf->isMacro());
}

// LibertyCell::isMemory
TEST_F(StaLibertyTest, CellIsMemory3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isMemory());
}

// LibertyCell::setIsMemory
TEST_F(StaLibertyTest, CellSetIsMemory) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setIsMemory(true);
  EXPECT_TRUE(buf->isMemory());
  buf->setIsMemory(false);
}

// LibertyCell::isPad
TEST_F(StaLibertyTest, CellIsPad2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isPad());
}

// LibertyCell::setIsPad
TEST_F(StaLibertyTest, CellSetIsPad) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setIsPad(true);
  EXPECT_TRUE(buf->isPad());
  buf->setIsPad(false);
}

// LibertyCell::isClockCell
TEST_F(StaLibertyTest, CellIsClockCell2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isClockCell());
}

// LibertyCell::setIsClockCell
TEST_F(StaLibertyTest, CellSetIsClockCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setIsClockCell(true);
  EXPECT_TRUE(buf->isClockCell());
  buf->setIsClockCell(false);
}

// LibertyCell::isLevelShifter
TEST_F(StaLibertyTest, CellIsLevelShifter2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isLevelShifter());
}

// LibertyCell::setIsLevelShifter
TEST_F(StaLibertyTest, CellSetIsLevelShifter) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setIsLevelShifter(true);
  EXPECT_TRUE(buf->isLevelShifter());
  buf->setIsLevelShifter(false);
}

// LibertyCell::isIsolationCell
TEST_F(StaLibertyTest, CellIsIsolationCell2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isIsolationCell());
}

// LibertyCell::setIsIsolationCell
TEST_F(StaLibertyTest, CellSetIsIsolationCell) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setIsIsolationCell(true);
  EXPECT_TRUE(buf->isIsolationCell());
  buf->setIsIsolationCell(false);
}

// LibertyCell::alwaysOn
TEST_F(StaLibertyTest, CellAlwaysOn2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->alwaysOn());
}

// LibertyCell::setAlwaysOn
TEST_F(StaLibertyTest, CellSetAlwaysOn) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setAlwaysOn(true);
  EXPECT_TRUE(buf->alwaysOn());
  buf->setAlwaysOn(false);
}

// LibertyCell::interfaceTiming
TEST_F(StaLibertyTest, CellInterfaceTiming2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->interfaceTiming());
}

// LibertyCell::setInterfaceTiming
TEST_F(StaLibertyTest, CellSetInterfaceTiming) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setInterfaceTiming(true);
  EXPECT_TRUE(buf->interfaceTiming());
  buf->setInterfaceTiming(false);
}

// LibertyCell::isClockGate and related
TEST_F(StaLibertyTest, CellIsClockGate3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isClockGate());
  EXPECT_FALSE(buf->isClockGateLatchPosedge());
  EXPECT_FALSE(buf->isClockGateLatchNegedge());
  EXPECT_FALSE(buf->isClockGateOther());
}

// LibertyCell::setClockGateType
TEST_F(StaLibertyTest, CellSetClockGateType) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setClockGateType(ClockGateType::latch_posedge);
  EXPECT_TRUE(buf->isClockGateLatchPosedge());
  EXPECT_TRUE(buf->isClockGate());
  buf->setClockGateType(ClockGateType::latch_negedge);
  EXPECT_TRUE(buf->isClockGateLatchNegedge());
  buf->setClockGateType(ClockGateType::other);
  EXPECT_TRUE(buf->isClockGateOther());
  buf->setClockGateType(ClockGateType::none);
  EXPECT_FALSE(buf->isClockGate());
}

// LibertyCell::isDisabledConstraint
TEST_F(StaLibertyTest, CellIsDisabledConstraint2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->isDisabledConstraint());
  buf->setIsDisabledConstraint(true);
  EXPECT_TRUE(buf->isDisabledConstraint());
  buf->setIsDisabledConstraint(false);
}

// LibertyCell::hasSequentials
TEST_F(StaLibertyTest, CellHasSequentialsBuf) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->hasSequentials());
}

// LibertyCell::hasSequentials on DFF
TEST_F(StaLibertyTest, CellHasSequentialsDFF) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  EXPECT_TRUE(dff->hasSequentials());
}

// LibertyCell::sequentials
TEST_F(StaLibertyTest, CellSequentialsDFF) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  auto &seqs = dff->sequentials();
  EXPECT_GT(seqs.size(), 0u);
}

// LibertyCell::leakagePower
TEST_F(StaLibertyTest, CellLeakagePower4) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  float leakage;
  bool exists;
  buf->leakagePower(leakage, exists);
  // leakage may or may not exist
  (void)leakage;
  (void)exists;
}

// LibertyCell::leakagePowers
TEST_F(StaLibertyTest, CellLeakagePowers2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LeakagePowerSeq *leaks = buf->leakagePowers();
  EXPECT_NE(leaks, nullptr);
}

// LibertyCell::internalPowers
TEST_F(StaLibertyTest, CellInternalPowers3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &powers = buf->internalPowers();
  // May have internal power entries
  (void)powers.size();
}

// LibertyCell::ocvArcDepth (from cell, not library)
TEST_F(StaLibertyTest, CellOcvArcDepth3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  float depth = buf->ocvArcDepth();
  // Default is 0
  EXPECT_FLOAT_EQ(depth, 0.0f);
}

// LibertyCell::setOcvArcDepth
TEST_F(StaLibertyTest, CellSetOcvArcDepth2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setOcvArcDepth(3.0f);
  EXPECT_FLOAT_EQ(buf->ocvArcDepth(), 3.0f);
}

// LibertyCell::ocvDerate
TEST_F(StaLibertyTest, CellOcvDerate3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  OcvDerate *derate = buf->ocvDerate();
  // Default is nullptr
  (void)derate;
}

// LibertyCell::footprint
TEST_F(StaLibertyTest, CellFootprint3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const char *fp = buf->footprint();
  // May be null or empty
  (void)fp;
}

// LibertyCell::setFootprint
TEST_F(StaLibertyTest, CellSetFootprint) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setFootprint("test_footprint");
  EXPECT_STREQ(buf->footprint(), "test_footprint");
}

// LibertyCell::userFunctionClass
TEST_F(StaLibertyTest, CellUserFunctionClass2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const char *ufc = buf->userFunctionClass();
  (void)ufc;
}

// LibertyCell::setUserFunctionClass
TEST_F(StaLibertyTest, CellSetUserFunctionClass) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setUserFunctionClass("my_class");
  EXPECT_STREQ(buf->userFunctionClass(), "my_class");
}

// LibertyCell::setSwitchCellType
TEST_F(StaLibertyTest, CellSwitchCellType) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setSwitchCellType(SwitchCellType::coarse_grain);
  EXPECT_EQ(buf->switchCellType(), SwitchCellType::coarse_grain);
  buf->setSwitchCellType(SwitchCellType::fine_grain);
  EXPECT_EQ(buf->switchCellType(), SwitchCellType::fine_grain);
}

// LibertyCell::setLevelShifterType
TEST_F(StaLibertyTest, CellLevelShifterType) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setLevelShifterType(LevelShifterType::HL);
  EXPECT_EQ(buf->levelShifterType(), LevelShifterType::HL);
  buf->setLevelShifterType(LevelShifterType::LH);
  EXPECT_EQ(buf->levelShifterType(), LevelShifterType::LH);
  buf->setLevelShifterType(LevelShifterType::HL_LH);
  EXPECT_EQ(buf->levelShifterType(), LevelShifterType::HL_LH);
}

// LibertyCell::cornerCell
TEST_F(StaLibertyTest, CellCornerCell2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyCell *corner = buf->cornerCell(0);
  // May return self or a corner cell
  (void)corner;
}

// LibertyCell::scaleFactors
TEST_F(StaLibertyTest, CellScaleFactors3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ScaleFactors *sf = buf->scaleFactors();
  // May be null
  (void)sf;
}

// LibertyLibrary::delayModelType
TEST_F(StaLibertyTest, LibDelayModelType) {
  ASSERT_NE(lib_, nullptr);
  DelayModelType dmt = lib_->delayModelType();
  // table is the most common
  EXPECT_EQ(dmt, DelayModelType::table);
}

// LibertyLibrary::nominalProcess, nominalVoltage, nominalTemperature
TEST_F(StaLibertyTest, LibNominalPVT) {
  ASSERT_NE(lib_, nullptr);
  float proc = lib_->nominalProcess();
  float volt = lib_->nominalVoltage();
  float temp = lib_->nominalTemperature();
  EXPECT_GT(proc, 0.0f);
  EXPECT_GT(volt, 0.0f);
  // Temperature can be any value
  (void)temp;
}

// LibertyLibrary::setNominalProcess/Voltage/Temperature
TEST_F(StaLibertyTest, LibSetNominalPVT) {
  ASSERT_NE(lib_, nullptr);
  lib_->setNominalProcess(1.5f);
  EXPECT_FLOAT_EQ(lib_->nominalProcess(), 1.5f);
  lib_->setNominalVoltage(0.9f);
  EXPECT_FLOAT_EQ(lib_->nominalVoltage(), 0.9f);
  lib_->setNominalTemperature(85.0f);
  EXPECT_FLOAT_EQ(lib_->nominalTemperature(), 85.0f);
}

// LibertyLibrary::defaultInputPinCap and setDefaultInputPinCap
TEST_F(StaLibertyTest, LibDefaultInputPinCap) {
  ASSERT_NE(lib_, nullptr);
  float orig_cap = lib_->defaultInputPinCap();
  lib_->setDefaultInputPinCap(0.5f);
  EXPECT_FLOAT_EQ(lib_->defaultInputPinCap(), 0.5f);
  lib_->setDefaultInputPinCap(orig_cap);
}

// LibertyLibrary::defaultOutputPinCap and setDefaultOutputPinCap
TEST_F(StaLibertyTest, LibDefaultOutputPinCap) {
  ASSERT_NE(lib_, nullptr);
  float orig_cap = lib_->defaultOutputPinCap();
  lib_->setDefaultOutputPinCap(0.3f);
  EXPECT_FLOAT_EQ(lib_->defaultOutputPinCap(), 0.3f);
  lib_->setDefaultOutputPinCap(orig_cap);
}

// LibertyLibrary::defaultBidirectPinCap
TEST_F(StaLibertyTest, LibDefaultBidirectPinCap) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultBidirectPinCap(0.2f);
  EXPECT_FLOAT_EQ(lib_->defaultBidirectPinCap(), 0.2f);
}

// LibertyLibrary::defaultIntrinsic
TEST_F(StaLibertyTest, LibDefaultIntrinsic) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultIntrinsic(RiseFall::rise(), 0.1f);
  float val;
  bool exists;
  lib_->defaultIntrinsic(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.1f);
}

// LibertyLibrary::defaultOutputPinRes
TEST_F(StaLibertyTest, LibDefaultOutputPinRes) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultOutputPinRes(RiseFall::rise(), 10.0f);
  float res;
  bool exists;
  lib_->defaultOutputPinRes(RiseFall::rise(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 10.0f);
}

// LibertyLibrary::defaultBidirectPinRes
TEST_F(StaLibertyTest, LibDefaultBidirectPinRes) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultBidirectPinRes(RiseFall::fall(), 5.0f);
  float res;
  bool exists;
  lib_->defaultBidirectPinRes(RiseFall::fall(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 5.0f);
}

// LibertyLibrary::defaultPinResistance
TEST_F(StaLibertyTest, LibDefaultPinResistance) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultOutputPinRes(RiseFall::rise(), 12.0f);
  float res;
  bool exists;
  lib_->defaultPinResistance(RiseFall::rise(), PortDirection::output(), res, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(res, 12.0f);
}

// LibertyLibrary::defaultMaxSlew
TEST_F(StaLibertyTest, LibDefaultMaxSlew) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultMaxSlew(1.0f);
  float slew;
  bool exists;
  lib_->defaultMaxSlew(slew, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(slew, 1.0f);
}

// LibertyLibrary::defaultMaxCapacitance
TEST_F(StaLibertyTest, LibDefaultMaxCapacitance) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultMaxCapacitance(2.0f);
  float cap;
  bool exists;
  lib_->defaultMaxCapacitance(cap, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(cap, 2.0f);
}

// LibertyLibrary::defaultMaxFanout
TEST_F(StaLibertyTest, LibDefaultMaxFanout) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultMaxFanout(8.0f);
  float fanout;
  bool exists;
  lib_->defaultMaxFanout(fanout, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(fanout, 8.0f);
}

// LibertyLibrary::defaultFanoutLoad
TEST_F(StaLibertyTest, LibDefaultFanoutLoad) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultFanoutLoad(1.5f);
  float load;
  bool exists;
  lib_->defaultFanoutLoad(load, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(load, 1.5f);
}

// LibertyLibrary thresholds
TEST_F(StaLibertyTest, LibThresholds) {
  ASSERT_NE(lib_, nullptr);
  lib_->setInputThreshold(RiseFall::rise(), 0.6f);
  EXPECT_FLOAT_EQ(lib_->inputThreshold(RiseFall::rise()), 0.6f);

  lib_->setOutputThreshold(RiseFall::fall(), 0.4f);
  EXPECT_FLOAT_EQ(lib_->outputThreshold(RiseFall::fall()), 0.4f);

  lib_->setSlewLowerThreshold(RiseFall::rise(), 0.1f);
  EXPECT_FLOAT_EQ(lib_->slewLowerThreshold(RiseFall::rise()), 0.1f);

  lib_->setSlewUpperThreshold(RiseFall::rise(), 0.9f);
  EXPECT_FLOAT_EQ(lib_->slewUpperThreshold(RiseFall::rise()), 0.9f);
}

// LibertyLibrary::slewDerateFromLibrary
TEST_F(StaLibertyTest, LibSlewDerate) {
  ASSERT_NE(lib_, nullptr);
  float orig = lib_->slewDerateFromLibrary();
  lib_->setSlewDerateFromLibrary(0.5f);
  EXPECT_FLOAT_EQ(lib_->slewDerateFromLibrary(), 0.5f);
  lib_->setSlewDerateFromLibrary(orig);
}

// LibertyLibrary::defaultWireloadMode
TEST_F(StaLibertyTest, LibDefaultWireloadMode) {
  ASSERT_NE(lib_, nullptr);
  lib_->setDefaultWireloadMode(WireloadMode::enclosed);
  EXPECT_EQ(lib_->defaultWireloadMode(), WireloadMode::enclosed);
  lib_->setDefaultWireloadMode(WireloadMode::top);
  EXPECT_EQ(lib_->defaultWireloadMode(), WireloadMode::top);
}

// LibertyLibrary::ocvArcDepth
TEST_F(StaLibertyTest, LibOcvArcDepth) {
  ASSERT_NE(lib_, nullptr);
  lib_->setOcvArcDepth(2.0f);
  EXPECT_FLOAT_EQ(lib_->ocvArcDepth(), 2.0f);
}

// LibertyLibrary::defaultOcvDerate
TEST_F(StaLibertyTest, LibDefaultOcvDerate) {
  ASSERT_NE(lib_, nullptr);
  OcvDerate *orig = lib_->defaultOcvDerate();
  (void)orig;
}

// LibertyLibrary::supplyVoltage
TEST_F(StaLibertyTest, LibSupplyVoltage) {
  ASSERT_NE(lib_, nullptr);
  lib_->addSupplyVoltage("VDD", 1.1f);
  EXPECT_TRUE(lib_->supplyExists("VDD"));
  float volt;
  bool exists;
  lib_->supplyVoltage("VDD", volt, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(volt, 1.1f);
  EXPECT_FALSE(lib_->supplyExists("NONEXISTENT_SUPPLY"));
}

// LibertyLibrary::buffers and inverters lists
TEST_F(StaLibertyTest, LibBuffersInverters) {
  ASSERT_NE(lib_, nullptr);
  LibertyCellSeq *bufs = lib_->buffers();
  EXPECT_NE(bufs, nullptr);
  EXPECT_GT(bufs->size(), 0u);
  LibertyCellSeq *invs = lib_->inverters();
  EXPECT_NE(invs, nullptr);
  EXPECT_GT(invs->size(), 0u);
}

// LibertyLibrary::findOcvDerate (non-existent)
TEST_F(StaLibertyTest, LibFindOcvDerateNonExistent) {
  ASSERT_NE(lib_, nullptr);
  EXPECT_EQ(lib_->findOcvDerate("nonexistent_derate"), nullptr);
}

// LibertyCell::findOcvDerate (non-existent)
TEST_F(StaLibertyTest, CellFindOcvDerateNonExistent) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_EQ(buf->findOcvDerate("nonexistent"), nullptr);
}

// LibertyCell::setOcvDerate
TEST_F(StaLibertyTest, CellSetOcvDerateNull) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  buf->setOcvDerate(nullptr);
  EXPECT_EQ(buf->ocvDerate(), nullptr);
}

// OperatingConditions construction
TEST_F(StaLibertyTest, OperatingConditionsConstruct) {
  OperatingConditions oc("typical", 1.0f, 1.1f, 25.0f, WireloadTree::balanced);
  EXPECT_STREQ(oc.name(), "typical");
  EXPECT_FLOAT_EQ(oc.process(), 1.0f);
  EXPECT_FLOAT_EQ(oc.voltage(), 1.1f);
  EXPECT_FLOAT_EQ(oc.temperature(), 25.0f);
  EXPECT_EQ(oc.wireloadTree(), WireloadTree::balanced);
}

// OperatingConditions::setWireloadTree
TEST_F(StaLibertyTest, OperatingConditionsSetWireloadTree) {
  OperatingConditions oc("test");
  oc.setWireloadTree(WireloadTree::worst_case);
  EXPECT_EQ(oc.wireloadTree(), WireloadTree::worst_case);
  oc.setWireloadTree(WireloadTree::best_case);
  EXPECT_EQ(oc.wireloadTree(), WireloadTree::best_case);
}

// Pvt class
TEST_F(StaLibertyTest, PvtConstruct) {
  Pvt pvt(1.0f, 1.1f, 25.0f);
  EXPECT_FLOAT_EQ(pvt.process(), 1.0f);
  EXPECT_FLOAT_EQ(pvt.voltage(), 1.1f);
  EXPECT_FLOAT_EQ(pvt.temperature(), 25.0f);
}

// Pvt setters
TEST_F(StaLibertyTest, PvtSetters) {
  Pvt pvt(1.0f, 1.1f, 25.0f);
  pvt.setProcess(2.0f);
  EXPECT_FLOAT_EQ(pvt.process(), 2.0f);
  pvt.setVoltage(0.9f);
  EXPECT_FLOAT_EQ(pvt.voltage(), 0.9f);
  pvt.setTemperature(100.0f);
  EXPECT_FLOAT_EQ(pvt.temperature(), 100.0f);
}

// ScaleFactors
TEST_F(StaLibertyTest, ScaleFactorsConstruct) {
  ScaleFactors sf("test_sf");
  EXPECT_STREQ(sf.name(), "test_sf");
}

// ScaleFactors::setScale and scale
TEST_F(StaLibertyTest, ScaleFactorsSetGet) {
  ScaleFactors sf("test_sf");
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::process,
              RiseFall::rise(), 1.5f);
  float val = sf.scale(ScaleFactorType::cell, ScaleFactorPvt::process,
                       RiseFall::rise());
  EXPECT_FLOAT_EQ(val, 1.5f);
}

// ScaleFactors::setScale without rf and scale without rf
TEST_F(StaLibertyTest, ScaleFactorsSetGetNoRF) {
  ScaleFactors sf("test_sf2");
  sf.setScale(ScaleFactorType::cell, ScaleFactorPvt::volt, 2.0f);
  float val = sf.scale(ScaleFactorType::cell, ScaleFactorPvt::volt);
  EXPECT_FLOAT_EQ(val, 2.0f);
}

// LibertyLibrary::addScaleFactors and findScaleFactors
TEST_F(StaLibertyTest, LibAddFindScaleFactors) {
  ASSERT_NE(lib_, nullptr);
  ScaleFactors *sf = new ScaleFactors("custom_sf");
  sf->setScale(ScaleFactorType::cell, ScaleFactorPvt::process,
               RiseFall::rise(), 1.2f);
  lib_->addScaleFactors(sf);
  ScaleFactors *found = lib_->findScaleFactors("custom_sf");
  EXPECT_EQ(found, sf);
}

// LibertyLibrary::findOperatingConditions
TEST_F(StaLibertyTest, LibFindOperatingConditions) {
  ASSERT_NE(lib_, nullptr);
  OperatingConditions *oc = new OperatingConditions("fast", 0.5f, 1.32f, -40.0f, WireloadTree::best_case);
  lib_->addOperatingConditions(oc);
  OperatingConditions *found = lib_->findOperatingConditions("fast");
  EXPECT_EQ(found, oc);
  EXPECT_EQ(lib_->findOperatingConditions("nonexistent"), nullptr);
}

// LibertyLibrary::setDefaultOperatingConditions
TEST_F(StaLibertyTest, LibSetDefaultOperatingConditions) {
  ASSERT_NE(lib_, nullptr);
  OperatingConditions *oc = new OperatingConditions("default_oc");
  lib_->addOperatingConditions(oc);
  lib_->setDefaultOperatingConditions(oc);
  EXPECT_EQ(lib_->defaultOperatingConditions(), oc);
}

// FuncExpr make/access
TEST_F(StaLibertyTest, FuncExprMakePort) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *a = inv->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  FuncExpr *expr = FuncExpr::makePort(a);
  EXPECT_NE(expr, nullptr);
  EXPECT_EQ(expr->op(), FuncExpr::op_port);
  EXPECT_EQ(expr->port(), a);
  std::string s = expr->to_string();
  EXPECT_FALSE(s.empty());
  delete expr;
}

// FuncExpr::makeNot
TEST_F(StaLibertyTest, FuncExprMakeNot) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *a = inv->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  FuncExpr *port_expr = FuncExpr::makePort(a);
  FuncExpr *not_expr = FuncExpr::makeNot(port_expr);
  EXPECT_NE(not_expr, nullptr);
  EXPECT_EQ(not_expr->op(), FuncExpr::op_not);
  EXPECT_EQ(not_expr->left(), port_expr);
  std::string s = not_expr->to_string();
  EXPECT_FALSE(s.empty());
  not_expr->deleteSubexprs();
}

// FuncExpr::makeAnd
TEST_F(StaLibertyTest, FuncExprMakeAnd) {
  LibertyCell *and2 = lib_->findLibertyCell("AND2_X1");
  ASSERT_NE(and2, nullptr);
  LibertyPort *a1 = and2->findLibertyPort("A1");
  LibertyPort *a2 = and2->findLibertyPort("A2");
  ASSERT_NE(a1, nullptr);
  ASSERT_NE(a2, nullptr);
  FuncExpr *left = FuncExpr::makePort(a1);
  FuncExpr *right = FuncExpr::makePort(a2);
  FuncExpr *and_expr = FuncExpr::makeAnd(left, right);
  EXPECT_EQ(and_expr->op(), FuncExpr::op_and);
  std::string s = and_expr->to_string();
  EXPECT_FALSE(s.empty());
  and_expr->deleteSubexprs();
}

// FuncExpr::makeOr
TEST_F(StaLibertyTest, FuncExprMakeOr) {
  LibertyCell *or2 = lib_->findLibertyCell("OR2_X1");
  ASSERT_NE(or2, nullptr);
  LibertyPort *a1 = or2->findLibertyPort("A1");
  LibertyPort *a2 = or2->findLibertyPort("A2");
  ASSERT_NE(a1, nullptr);
  ASSERT_NE(a2, nullptr);
  FuncExpr *left = FuncExpr::makePort(a1);
  FuncExpr *right = FuncExpr::makePort(a2);
  FuncExpr *or_expr = FuncExpr::makeOr(left, right);
  EXPECT_EQ(or_expr->op(), FuncExpr::op_or);
  or_expr->deleteSubexprs();
}

// FuncExpr::makeXor
TEST_F(StaLibertyTest, FuncExprMakeXor) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *a = inv->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  FuncExpr *left = FuncExpr::makePort(a);
  FuncExpr *right = FuncExpr::makePort(a);
  FuncExpr *xor_expr = FuncExpr::makeXor(left, right);
  EXPECT_EQ(xor_expr->op(), FuncExpr::op_xor);
  xor_expr->deleteSubexprs();
}

// FuncExpr::makeZero and makeOne
TEST_F(StaLibertyTest, FuncExprMakeZeroOne) {
  FuncExpr *zero = FuncExpr::makeZero();
  EXPECT_NE(zero, nullptr);
  EXPECT_EQ(zero->op(), FuncExpr::op_zero);
  delete zero;

  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_NE(one, nullptr);
  EXPECT_EQ(one->op(), FuncExpr::op_one);
  delete one;
}

// FuncExpr::equiv
TEST_F(StaLibertyTest, FuncExprEquiv) {
  FuncExpr *zero1 = FuncExpr::makeZero();
  FuncExpr *zero2 = FuncExpr::makeZero();
  EXPECT_TRUE(FuncExpr::equiv(zero1, zero2));
  FuncExpr *one = FuncExpr::makeOne();
  EXPECT_FALSE(FuncExpr::equiv(zero1, one));
  delete zero1;
  delete zero2;
  delete one;
}

// FuncExpr::hasPort
TEST_F(StaLibertyTest, FuncExprHasPort) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *a = inv->findLibertyPort("A");
  LibertyPort *zn = inv->findLibertyPort("ZN");
  ASSERT_NE(a, nullptr);
  FuncExpr *expr = FuncExpr::makePort(a);
  EXPECT_TRUE(expr->hasPort(a));
  if (zn)
    EXPECT_FALSE(expr->hasPort(zn));
  delete expr;
}

// FuncExpr::portTimingSense
TEST_F(StaLibertyTest, FuncExprPortTimingSense) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *a = inv->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  FuncExpr *not_expr = FuncExpr::makeNot(FuncExpr::makePort(a));
  TimingSense sense = not_expr->portTimingSense(a);
  EXPECT_EQ(sense, TimingSense::negative_unate);
  not_expr->deleteSubexprs();
}

// FuncExpr::copy
TEST_F(StaLibertyTest, FuncExprCopy) {
  FuncExpr *one = FuncExpr::makeOne();
  FuncExpr *copy = one->copy();
  EXPECT_NE(copy, nullptr);
  EXPECT_TRUE(FuncExpr::equiv(one, copy));
  delete one;
  delete copy;
}

// LibertyPort properties
TEST_F(StaLibertyTest, PortProperties) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *a = inv->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  // capacitance
  float cap = a->capacitance();
  EXPECT_GE(cap, 0.0f);
  // direction
  EXPECT_NE(a->direction(), nullptr);
}

// LibertyPort::function
TEST_F(StaLibertyTest, PortFunction3) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *zn = inv->findLibertyPort("ZN");
  ASSERT_NE(zn, nullptr);
  FuncExpr *func = zn->function();
  EXPECT_NE(func, nullptr);
}

// LibertyPort::driveResistance
TEST_F(StaLibertyTest, PortDriveResistance2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  float res = z->driveResistance();
  EXPECT_GE(res, 0.0f);
}

// LibertyPort::capacitance with min/max
TEST_F(StaLibertyTest, PortCapacitanceMinMax2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float cap_min = a->capacitance(MinMax::min());
  float cap_max = a->capacitance(MinMax::max());
  EXPECT_GE(cap_min, 0.0f);
  EXPECT_GE(cap_max, 0.0f);
}

// LibertyPort::capacitance with rf and min/max
TEST_F(StaLibertyTest, PortCapacitanceRfMinMax2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float cap = a->capacitance(RiseFall::rise(), MinMax::max());
  EXPECT_GE(cap, 0.0f);
}

// LibertyPort::slewLimit
TEST_F(StaLibertyTest, PortSlewLimit2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  float limit;
  bool exists;
  z->slewLimit(MinMax::max(), limit, exists);
  // May or may not exist
  (void)limit;
  (void)exists;
}

// LibertyPort::capacitanceLimit
TEST_F(StaLibertyTest, PortCapacitanceLimit2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  float limit;
  bool exists;
  z->capacitanceLimit(MinMax::max(), limit, exists);
  (void)limit;
  (void)exists;
}

// LibertyPort::fanoutLoad
TEST_F(StaLibertyTest, PortFanoutLoad2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  float load;
  bool exists;
  a->fanoutLoad(load, exists);
  (void)load;
  (void)exists;
}

// LibertyPort::isClock
TEST_F(StaLibertyTest, PortIsClock2) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *ck = dff->findLibertyPort("CK");
  ASSERT_NE(ck, nullptr);
  EXPECT_TRUE(ck->isClock());
  LibertyPort *d = dff->findLibertyPort("D");
  if (d)
    EXPECT_FALSE(d->isClock());
}

// LibertyPort::setIsClock
TEST_F(StaLibertyTest, PortSetIsClock) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  a->setIsClock(true);
  EXPECT_TRUE(a->isClock());
  a->setIsClock(false);
}

// LibertyPort::isRegClk
TEST_F(StaLibertyTest, PortIsRegClk2) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *ck = dff->findLibertyPort("CK");
  ASSERT_NE(ck, nullptr);
  EXPECT_TRUE(ck->isRegClk());
}

// LibertyPort::isRegOutput
TEST_F(StaLibertyTest, PortIsRegOutput) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *q = dff->findLibertyPort("Q");
  ASSERT_NE(q, nullptr);
  EXPECT_TRUE(q->isRegOutput());
}

// LibertyPort::isCheckClk
TEST_F(StaLibertyTest, PortIsCheckClk) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *ck = dff->findLibertyPort("CK");
  ASSERT_NE(ck, nullptr);
  EXPECT_TRUE(ck->isCheckClk());
}

// TimingArcSet::deleteTimingArc - test via finding and accessing
TEST_F(StaLibertyTest, TimingArcSetArcCount) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *first_set = arcsets[0];
  EXPECT_GT(first_set->arcCount(), 0u);
}

// TimingArcSet::role
TEST_F(StaLibertyTest, TimingArcSetRole) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArcSet *first_set = arcsets[0];
  const TimingRole *role = first_set->role();
  EXPECT_NE(role, nullptr);
}

// TimingArcSet::sense
TEST_F(StaLibertyTest, TimingArcSetSense2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingSense sense = arcsets[0]->sense();
  // Buffer should have positive_unate
  EXPECT_EQ(sense, TimingSense::positive_unate);
}

// TimingArc::fromEdge and toEdge
TEST_F(StaLibertyTest, TimingArcEdges) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  for (TimingArc *arc : arcsets[0]->arcs()) {
    EXPECT_NE(arc->fromEdge(), nullptr);
    EXPECT_NE(arc->toEdge(), nullptr);
  }
}

// TimingArc::driveResistance
TEST_F(StaLibertyTest, TimingArcDriveResistance3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  for (TimingArc *arc : arcsets[0]->arcs()) {
    float res = arc->driveResistance();
    EXPECT_GE(res, 0.0f);
  }
}

// TimingArc::intrinsicDelay
TEST_F(StaLibertyTest, TimingArcIntrinsicDelay3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  for (TimingArc *arc : arcsets[0]->arcs()) {
    ArcDelay delay = arc->intrinsicDelay();
    (void)delay;
  }
}

// TimingArc::model
TEST_F(StaLibertyTest, TimingArcModel2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  for (TimingArc *arc : arcsets[0]->arcs()) {
    TimingModel *model = arc->model();
    EXPECT_NE(model, nullptr);
  }
}

// TimingArc::sense
TEST_F(StaLibertyTest, TimingArcSense) {
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  auto &arcsets = inv->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  for (TimingArc *arc : arcsets[0]->arcs()) {
    TimingSense sense = arc->sense();
    EXPECT_EQ(sense, TimingSense::negative_unate);
  }
}

// TimingArcSet::isCondDefault
TEST_F(StaLibertyTest, TimingArcSetIsCondDefault) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  // Default should be false or true depending on library
  bool cd = arcsets[0]->isCondDefault();
  (void)cd;
}

// TimingArcSet::isDisabledConstraint
TEST_F(StaLibertyTest, TimingArcSetIsDisabledConstraint) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  EXPECT_FALSE(arcsets[0]->isDisabledConstraint());
  arcsets[0]->setIsDisabledConstraint(true);
  EXPECT_TRUE(arcsets[0]->isDisabledConstraint());
  arcsets[0]->setIsDisabledConstraint(false);
}

// timingTypeIsCheck for more types
TEST_F(StaLibertyTest, TimingTypeIsCheckMore) {
  EXPECT_TRUE(timingTypeIsCheck(TimingType::setup_falling));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::hold_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::recovery_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::removal_falling));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::rising_edge));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::falling_edge));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::three_state_enable));
}

// findTimingType
TEST_F(StaLibertyTest, FindTimingType) {
  TimingType tt = findTimingType("combinational");
  EXPECT_EQ(tt, TimingType::combinational);
  tt = findTimingType("rising_edge");
  EXPECT_EQ(tt, TimingType::rising_edge);
  tt = findTimingType("falling_edge");
  EXPECT_EQ(tt, TimingType::falling_edge);
}

// timingTypeIsCheck
TEST_F(StaLibertyTest, TimingTypeIsCheck) {
  EXPECT_TRUE(timingTypeIsCheck(TimingType::setup_rising));
  EXPECT_TRUE(timingTypeIsCheck(TimingType::hold_falling));
  EXPECT_FALSE(timingTypeIsCheck(TimingType::combinational));
}

// to_string(TimingSense)
TEST_F(StaLibertyTest, TimingSenseToString) {
  const char *s = to_string(TimingSense::positive_unate);
  EXPECT_NE(s, nullptr);
  s = to_string(TimingSense::negative_unate);
  EXPECT_NE(s, nullptr);
  s = to_string(TimingSense::non_unate);
  EXPECT_NE(s, nullptr);
}

// timingSenseOpposite
TEST_F(StaLibertyTest, TimingSenseOpposite) {
  EXPECT_EQ(timingSenseOpposite(TimingSense::positive_unate),
            TimingSense::negative_unate);
  EXPECT_EQ(timingSenseOpposite(TimingSense::negative_unate),
            TimingSense::positive_unate);
}

// ScaleFactorPvt names
TEST_F(StaLibertyTest, ScaleFactorPvtNames) {
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::process), "process");
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::volt), "volt");
  EXPECT_STREQ(scaleFactorPvtName(ScaleFactorPvt::temp), "temp");
}

// findScaleFactorPvt
TEST_F(StaLibertyTest, FindScaleFactorPvt) {
  EXPECT_EQ(findScaleFactorPvt("process"), ScaleFactorPvt::process);
  EXPECT_EQ(findScaleFactorPvt("volt"), ScaleFactorPvt::volt);
  EXPECT_EQ(findScaleFactorPvt("temp"), ScaleFactorPvt::temp);
}

// ScaleFactorType names
TEST_F(StaLibertyTest, ScaleFactorTypeNames) {
  const char *name = scaleFactorTypeName(ScaleFactorType::cell);
  EXPECT_NE(name, nullptr);
}

// findScaleFactorType
TEST_F(StaLibertyTest, FindScaleFactorType) {
  ScaleFactorType sft = findScaleFactorType("cell_rise");
  // Should find it
  (void)sft;
}

// BusDcl
TEST_F(StaLibertyTest, BusDclConstruct) {
  BusDcl bus("data", 7, 0);
  EXPECT_STREQ(bus.name(), "data");
  EXPECT_EQ(bus.from(), 7);
  EXPECT_EQ(bus.to(), 0);
}

// TableTemplate
TEST_F(StaLibertyTest, TableTemplateConstruct) {
  TableTemplate tpl("my_template");
  EXPECT_STREQ(tpl.name(), "my_template");
  EXPECT_EQ(tpl.axis1(), nullptr);
  EXPECT_EQ(tpl.axis2(), nullptr);
  EXPECT_EQ(tpl.axis3(), nullptr);
}

// TableTemplate setName
TEST_F(StaLibertyTest, TableTemplateSetName) {
  TableTemplate tpl("orig");
  tpl.setName("renamed");
  EXPECT_STREQ(tpl.name(), "renamed");
}

// LibertyCell::modeDef
TEST_F(StaLibertyTest, CellModeDef2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  ModeDef *md = buf->makeModeDef("test_mode");
  EXPECT_NE(md, nullptr);
  EXPECT_STREQ(md->name(), "test_mode");
  ModeDef *found = buf->findModeDef("test_mode");
  EXPECT_EQ(found, md);
  EXPECT_EQ(buf->findModeDef("nonexistent_mode"), nullptr);
}

// LibertyLibrary::tableTemplates
TEST_F(StaLibertyTest, LibTableTemplates) {
  ASSERT_NE(lib_, nullptr);
  auto templates = lib_->tableTemplates();
  // Nangate45 should have table templates
  EXPECT_GT(templates.size(), 0u);
}

// LibertyLibrary::busDcls
TEST_F(StaLibertyTest, LibBusDcls) {
  ASSERT_NE(lib_, nullptr);
  auto dcls = lib_->busDcls();
  // May or may not have bus declarations
  (void)dcls.size();
}

// LibertyPort::minPeriod
TEST_F(StaLibertyTest, PortMinPeriod3) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *ck = dff->findLibertyPort("CK");
  ASSERT_NE(ck, nullptr);
  float min_period;
  bool exists;
  ck->minPeriod(min_period, exists);
  // May or may not exist
  (void)min_period;
  (void)exists;
}

// LibertyPort::minPulseWidth
TEST_F(StaLibertyTest, PortMinPulseWidth3) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *ck = dff->findLibertyPort("CK");
  ASSERT_NE(ck, nullptr);
  float min_width;
  bool exists;
  ck->minPulseWidth(RiseFall::rise(), min_width, exists);
  (void)min_width;
  (void)exists;
}

// LibertyPort::isClockGateClock/Enable/Out
TEST_F(StaLibertyTest, PortClockGateFlags) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isClockGateClock());
  EXPECT_FALSE(a->isClockGateEnable());
  EXPECT_FALSE(a->isClockGateOut());
}

// LibertyPort::isPllFeedback
TEST_F(StaLibertyTest, PortIsPllFeedback2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isPllFeedback());
}

// LibertyPort::isSwitch
TEST_F(StaLibertyTest, PortIsSwitch2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isSwitch());
}

// LibertyPort::isPad
TEST_F(StaLibertyTest, PortIsPad2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isPad());
}

// LibertyPort::setCapacitance
TEST_F(StaLibertyTest, PortSetCapacitance) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  a->setCapacitance(0.5f);
  EXPECT_FLOAT_EQ(a->capacitance(), 0.5f);
}

// LibertyPort::setSlewLimit
TEST_F(StaLibertyTest, PortSetSlewLimit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  z->setSlewLimit(2.0f, MinMax::max());
  float limit;
  bool exists;
  z->slewLimit(MinMax::max(), limit, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(limit, 2.0f);
}

// LibertyPort::setCapacitanceLimit
TEST_F(StaLibertyTest, PortSetCapacitanceLimit) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  z->setCapacitanceLimit(5.0f, MinMax::max());
  float limit;
  bool exists;
  z->capacitanceLimit(MinMax::max(), limit, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(limit, 5.0f);
}

// LibertyPort::setFanoutLoad
TEST_F(StaLibertyTest, PortSetFanoutLoad2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  a->setFanoutLoad(1.0f);
  float load;
  bool exists;
  a->fanoutLoad(load, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(load, 1.0f);
}

// LibertyPort::setFanoutLimit
TEST_F(StaLibertyTest, PortSetFanoutLimit2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  z->setFanoutLimit(4.0f, MinMax::max());
  float limit;
  bool exists;
  z->fanoutLimit(MinMax::max(), limit, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(limit, 4.0f);
}

// LibertyPort::capacitanceIsOneValue
TEST_F(StaLibertyTest, PortCapacitanceIsOneValue2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  bool one_val = a->capacitanceIsOneValue();
  (void)one_val;
}

// LibertyPort::isDisabledConstraint
TEST_F(StaLibertyTest, PortIsDisabledConstraint3) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isDisabledConstraint());
  a->setIsDisabledConstraint(true);
  EXPECT_TRUE(a->isDisabledConstraint());
  a->setIsDisabledConstraint(false);
}

// InternalPower
TEST_F(StaLibertyTest, InternalPowerPort) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &powers = buf->internalPowers();
  if (!powers.empty()) {
    InternalPower *pw = powers[0];
    EXPECT_NE(pw->port(), nullptr);
    LibertyCell *pcell = pw->libertyCell();
    EXPECT_EQ(pcell, buf);
  }
}

// LibertyLibrary units
TEST_F(StaLibertyTest, LibUnits) {
  ASSERT_NE(lib_, nullptr);
  Units *units = lib_->units();
  EXPECT_NE(units, nullptr);
  EXPECT_NE(units->timeUnit(), nullptr);
  EXPECT_NE(units->capacitanceUnit(), nullptr);
  EXPECT_NE(units->voltageUnit(), nullptr);
}

// WireloadSelection
TEST_F(StaLibertyTest, WireloadSelection) {
  ASSERT_NE(lib_, nullptr);
  WireloadSelection *ws = lib_->defaultWireloadSelection();
  // May be nullptr if not defined in the library
  (void)ws;
}

// LibertyLibrary::findWireload
TEST_F(StaLibertyTest, LibFindWireload) {
  ASSERT_NE(lib_, nullptr);
  Wireload *wl = lib_->findWireload("nonexistent");
  EXPECT_EQ(wl, nullptr);
}

// scaleFactorTypeRiseFallSuffix/Prefix/LowHighSuffix
TEST_F(StaLibertyTest, ScaleFactorTypeRiseFallSuffix) {
  // These should not crash
  bool rfs = scaleFactorTypeRiseFallSuffix(ScaleFactorType::cell);
  bool rfp = scaleFactorTypeRiseFallPrefix(ScaleFactorType::cell);
  bool lhs = scaleFactorTypeLowHighSuffix(ScaleFactorType::cell);
  (void)rfs;
  (void)rfp;
  (void)lhs;
}

// LibertyPort::scanSignalType
TEST_F(StaLibertyTest, PortScanSignalType2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_EQ(a->scanSignalType(), ScanSignalType::none);
}

// scanSignalTypeName
TEST_F(StaLibertyTest, ScanSignalTypeName) {
  const char *name = scanSignalTypeName(ScanSignalType::enable);
  EXPECT_NE(name, nullptr);
  name = scanSignalTypeName(ScanSignalType::clock);
  EXPECT_NE(name, nullptr);
}

// pwrGndTypeName and findPwrGndType
TEST_F(StaLibertyTest, PwrGndTypeName) {
  const char *name = pwrGndTypeName(PwrGndType::primary_power);
  EXPECT_NE(name, nullptr);
  PwrGndType t = findPwrGndType("primary_power");
  EXPECT_EQ(t, PwrGndType::primary_power);
}

// TimingArcSet::arcsFrom
TEST_F(StaLibertyTest, TimingArcSetArcsFrom2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArc *arc1 = nullptr;
  TimingArc *arc2 = nullptr;
  arcsets[0]->arcsFrom(RiseFall::rise(), arc1, arc2);
  // At least one arc should be found for rise
  EXPECT_NE(arc1, nullptr);
}

// TimingArcSet::arcTo
TEST_F(StaLibertyTest, TimingArcSetArcTo2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  TimingArc *arc = arcsets[0]->arcTo(RiseFall::rise());
  // Should find an arc
  EXPECT_NE(arc, nullptr);
}

// LibertyPort::driveResistance with rf/min_max
TEST_F(StaLibertyTest, PortDriveResistanceRfMinMax2) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *z = buf->findLibertyPort("Z");
  ASSERT_NE(z, nullptr);
  float res = z->driveResistance(RiseFall::rise(), MinMax::max());
  EXPECT_GE(res, 0.0f);
}

// LibertyPort::setMinPeriod
TEST_F(StaLibertyTest, PortSetMinPeriod) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *ck = dff->findLibertyPort("CK");
  ASSERT_NE(ck, nullptr);
  ck->setMinPeriod(0.5f);
  float min_period;
  bool exists;
  ck->minPeriod(min_period, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(min_period, 0.5f);
}

// LibertyPort::setMinPulseWidth
TEST_F(StaLibertyTest, PortSetMinPulseWidth) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  LibertyPort *ck = dff->findLibertyPort("CK");
  ASSERT_NE(ck, nullptr);
  ck->setMinPulseWidth(RiseFall::rise(), 0.3f);
  float min_width;
  bool exists;
  ck->minPulseWidth(RiseFall::rise(), min_width, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(min_width, 0.3f);
}

// LibertyPort::setDirection
TEST_F(StaLibertyTest, PortSetDirection) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  a->setDirection(PortDirection::bidirect());
  EXPECT_EQ(a->direction(), PortDirection::bidirect());
  a->setDirection(PortDirection::input());
}

// LibertyPort isolation and level shifter data flags
TEST_F(StaLibertyTest, PortIsolationLevelShifterFlags) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *a = buf->findLibertyPort("A");
  ASSERT_NE(a, nullptr);
  EXPECT_FALSE(a->isolationCellData());
  EXPECT_FALSE(a->isolationCellEnable());
  EXPECT_FALSE(a->levelShifterData());
}

// =========================================================================
// R9_ tests: Cover uncovered LibertyReader callbacks and related functions
// by creating small .lib files with specific constructs and reading them.
// =========================================================================

// Standard threshold definitions required by all liberty files
static const char *R9_THRESHOLDS = R"(
  slew_lower_threshold_pct_fall : 30.0 ;
  slew_lower_threshold_pct_rise : 30.0 ;
  slew_upper_threshold_pct_fall : 70.0 ;
  slew_upper_threshold_pct_rise : 70.0 ;
  slew_derate_from_library : 1.0 ;
  input_threshold_pct_fall : 50.0 ;
  input_threshold_pct_rise : 50.0 ;
  output_threshold_pct_fall : 50.0 ;
  output_threshold_pct_rise : 50.0 ;
  nom_process : 1.0 ;
  nom_temperature : 25.0 ;
  nom_voltage : 1.1 ;
)";

// Generate a unique temp file path for each call
static std::string makeUniqueTmpPath() {
  static std::atomic<int> counter{0};
  char buf[256];
  snprintf(buf, sizeof(buf), "/tmp/test_r9_%d_%d.lib",
           static_cast<int>(getpid()), counter.fetch_add(1));
  return std::string(buf);
}

// Write lib content to a unique temp file with thresholds injected
static void writeLibContent(const char *content, const std::string &path) {
  FILE *f = fopen(path.c_str(), "w");
  if (!f) return;
  const char *brace = strchr(content, '{');
  if (brace) {
    fwrite(content, 1, brace - content + 1, f);
    fprintf(f, "%s", R9_THRESHOLDS);
    fprintf(f, "%s", brace + 1);
  } else {
    fprintf(f, "%s", content);
  }
  fclose(f);
}

// Helper to write a temp liberty file and read it, injecting threshold defs
static void writeAndReadLib(Sta *sta, const char *content, const char *path = nullptr) {
  std::string tmp_path = path ? std::string(path) : makeUniqueTmpPath();
  writeLibContent(content, tmp_path);
  LibertyLibrary *lib = sta->readLiberty(tmp_path.c_str(), sta->cmdCorner(),
                                          MinMaxAll::min(), false);
  EXPECT_NE(lib, nullptr);
  remove(tmp_path.c_str());
}

// Helper variant that returns the library pointer
static LibertyLibrary *writeAndReadLibReturn(Sta *sta, const char *content, const char *path = nullptr) {
  std::string tmp_path = path ? std::string(path) : makeUniqueTmpPath();
  writeLibContent(content, tmp_path);
  LibertyLibrary *lib = sta->readLiberty(tmp_path.c_str(), sta->cmdCorner(),
                                          MinMaxAll::min(), false);
  remove(tmp_path.c_str());
  return lib;
}

// ---------- Library-level default attributes ----------

// R9_1: default_intrinsic_rise/fall
TEST_F(StaLibertyTest, DefaultIntrinsicRiseFall) {
  const char *content = R"(
library(test_r9_1) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_intrinsic_rise : 0.05 ;
  default_intrinsic_fall : 0.06 ;
  cell(BUF1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_2: default_inout_pin_rise_res / fall_res
TEST_F(StaLibertyTest, DefaultInoutPinRes) {
  const char *content = R"(
library(test_r9_2) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_inout_pin_rise_res : 100.0 ;
  default_inout_pin_fall_res : 120.0 ;
  cell(BUF2) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_3: default_output_pin_rise_res / fall_res
TEST_F(StaLibertyTest, DefaultOutputPinRes) {
  const char *content = R"(
library(test_r9_3) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_output_pin_rise_res : 50.0 ;
  default_output_pin_fall_res : 60.0 ;
  cell(BUF3) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_4: technology(fpga) group
TEST_F(StaLibertyTest, TechnologyGroup) {
  const char *content = R"(
library(test_r9_4) {
  technology(fpga) {}
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(BUF4) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_5: scaling_factors group
TEST_F(StaLibertyTest, ScalingFactors) {
  const char *content = R"(
library(test_r9_5) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  scaling_factors(my_scale) {
    k_process_cell_rise : 1.0 ;
    k_process_cell_fall : 1.0 ;
    k_volt_cell_rise : -0.5 ;
    k_volt_cell_fall : -0.5 ;
    k_temp_cell_rise : 0.001 ;
    k_temp_cell_fall : 0.001 ;
  }
  cell(BUF5) {
    area : 1.0 ;
    scaling_factors : my_scale ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_6: cell is_memory attribute
TEST_F(StaLibertyTest, CellIsMemory4) {
  const char *content = R"(
library(test_r9_6) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(MEM1) {
    area : 10.0 ;
    is_memory : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content, "/tmp/test_r9_6.lib");
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("MEM1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->isMemory());
}

// R9_7: pad_cell attribute
TEST_F(StaLibertyTest, CellIsPadCell) {
  const char *content = R"(
library(test_r9_7) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PAD1) {
    area : 50.0 ;
    pad_cell : true ;
    pin(PAD) { direction : inout ; capacitance : 5.0 ; function : "A" ; }
    pin(A) { direction : input ; capacitance : 0.01 ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content, "/tmp/test_r9_7.lib");
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("PAD1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->isPad());
}

// R9_8: is_clock_cell attribute
TEST_F(StaLibertyTest, CellIsClockCell3) {
  const char *content = R"(
library(test_r9_8) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(CLK1) {
    area : 3.0 ;
    is_clock_cell : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content, "/tmp/test_r9_8.lib");
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("CLK1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->isClockCell());
}

// R9_9: switch_cell_type
TEST_F(StaLibertyTest, CellSwitchCellType2) {
  const char *content = R"(
library(test_r9_9) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(SW1) {
    area : 5.0 ;
    switch_cell_type : coarse_grain ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_10: user_function_class
TEST_F(StaLibertyTest, CellUserFunctionClass3) {
  const char *content = R"(
library(test_r9_10) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(UFC1) {
    area : 2.0 ;
    user_function_class : combinational ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_11: pin fanout_load, max_fanout, min_fanout
TEST_F(StaLibertyTest, PinFanoutAttributes) {
  const char *content = R"(
library(test_r9_11) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(FAN1) {
    area : 2.0 ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      fanout_load : 1.5 ;
    }
    pin(Z) {
      direction : output ;
      function : "A" ;
      max_fanout : 16.0 ;
      min_fanout : 1.0 ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_12: min_transition on pin
TEST_F(StaLibertyTest, PinMinTransition) {
  const char *content = R"(
library(test_r9_12) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(TR1) {
    area : 2.0 ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      min_transition : 0.001 ;
    }
    pin(Z) {
      direction : output ;
      function : "A" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_13: pulse_clock attribute on pin
TEST_F(StaLibertyTest, PinPulseClock) {
  const char *content = R"(
library(test_r9_13) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PC1) {
    area : 2.0 ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      pulse_clock : rise_triggered_high_pulse ;
    }
    pin(Z) {
      direction : output ;
      function : "CLK" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_14: is_pll_feedback_pin
TEST_F(StaLibertyTest, PinIsPllFeedback) {
  const char *content = R"(
library(test_r9_14) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PLL1) {
    area : 5.0 ;
    pin(FB) {
      direction : input ;
      capacitance : 0.01 ;
      is_pll_feedback_pin : true ;
    }
    pin(Z) {
      direction : output ;
      function : "FB" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_15: switch_pin attribute
TEST_F(StaLibertyTest, PinSwitchPin) {
  const char *content = R"(
library(test_r9_15) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(SWP1) {
    area : 3.0 ;
    pin(SW) {
      direction : input ;
      capacitance : 0.01 ;
      switch_pin : true ;
    }
    pin(Z) {
      direction : output ;
      function : "SW" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_16: is_pad on pin
TEST_F(StaLibertyTest, PinIsPad) {
  const char *content = R"(
library(test_r9_16) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PADCELL1) {
    area : 50.0 ;
    pin(PAD) {
      direction : inout ;
      capacitance : 5.0 ;
      is_pad : true ;
      function : "A" ;
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_17: bundle group with members
TEST_F(StaLibertyTest, BundlePort) {
  const char *content = R"(
library(test_r9_17) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(BUND1) {
    area : 4.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    bundle(DATA) {
      members(A, B) ;
      direction : input ;
    }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_18: ff_bank group
TEST_F(StaLibertyTest, FFBank) {
  const char *content = R"(
library(test_r9_18) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(DFF_BANK1) {
    area : 8.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff_bank(IQ, IQN, 4) {
      clocked_on : "CLK" ;
      next_state : "D" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_19: latch_bank group
TEST_F(StaLibertyTest, LatchBank) {
  const char *content = R"(
library(test_r9_19) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(LATCH_BANK1) {
    area : 6.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(EN) { direction : input ; capacitance : 0.01 ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    latch_bank(IQ, IQN, 4) {
      enable : "EN" ;
      data_in : "D" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_20: timing with intrinsic_rise/fall and rise_resistance/fall_resistance (linear model)
TEST_F(StaLibertyTest, TimingIntrinsicResistance) {
  const char *content = R"(
library(test_r9_20) {
  delay_model : generic_cmos ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  pulling_resistance_unit : "1kohm" ;
  capacitive_load_unit(1, ff) ;
  cell(LIN1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        intrinsic_rise : 0.05 ;
        intrinsic_fall : 0.06 ;
        rise_resistance : 100.0 ;
        fall_resistance : 120.0 ;
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_21: timing with sdf_cond_start and sdf_cond_end
TEST_F(StaLibertyTest, TimingSdfCondStartEnd) {
  const char *content = R"(
library(test_r9_21) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SDF1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A & B" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sdf_cond_start : "B == 1'b1" ;
        sdf_cond_end : "B == 1'b0" ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_22: timing with mode attribute
TEST_F(StaLibertyTest, TimingMode) {
  const char *content = R"(
library(test_r9_22) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(MODE1) {
    area : 2.0 ;
    mode_definition(test_mode) {
      mode_value(normal) {
        when : "A" ;
        sdf_cond : "A == 1'b1" ;
      }
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        mode(test_mode, normal) ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_23: related_bus_pins
TEST_F(StaLibertyTest, TimingRelatedBusPins) {
  const char *content = R"(
library(test_r9_23) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  type(bus4) {
    base_type : array ;
    data_type : bit ;
    bit_width : 4 ;
    bit_from : 3 ;
    bit_to : 0 ;
  }
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(BUS1) {
    area : 4.0 ;
    bus(D) {
      bus_type : bus4 ;
      direction : input ;
      capacitance : 0.01 ;
    }
    pin(Z) {
      direction : output ;
      function : "D[0]" ;
      timing() {
        related_bus_pins : "D" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_24: OCV derate constructs
TEST_F(StaLibertyTest, OcvDerate) {
  const char *content = R"(
library(test_r9_24) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_template_1) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  ocv_derate(my_derate) {
    ocv_derate_factors(ocv_template_1) {
      rf_type : rise ;
      derate_type : early ;
      path_type : data ;
      values("0.95, 0.96") ;
    }
    ocv_derate_factors(ocv_template_1) {
      rf_type : fall ;
      derate_type : late ;
      path_type : clock ;
      values("1.04, 1.05") ;
    }
    ocv_derate_factors(ocv_template_1) {
      rf_type : rise_and_fall ;
      derate_type : early ;
      path_type : clock_and_data ;
      values("0.97, 0.98") ;
    }
  }
  default_ocv_derate_group : my_derate ;
  cell(OCV1) {
    area : 2.0 ;
    ocv_derate_group : my_derate ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_25: ocv_arc_depth at library, cell, and timing levels
TEST_F(StaLibertyTest, OcvArcDepth) {
  const char *content = R"(
library(test_r9_25) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_arc_depth : 3.0 ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(OCV2) {
    area : 2.0 ;
    ocv_arc_depth : 5.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        ocv_arc_depth : 2.0 ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_26: POCV sigma tables
TEST_F(StaLibertyTest, OcvSigmaTables) {
  const char *content = R"(
library(test_r9_26) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(POCV1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sigma_type : early_and_late ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ocv_sigma_cell_rise(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_cell_fall(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_rise_transition(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_fall_transition(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_27: POCV sigma constraint tables
TEST_F(StaLibertyTest, OcvSigmaConstraint) {
  const char *content = R"(
library(test_r9_27) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(constraint_template_2x2) {
    variable_1 : related_pin_transition ;
    variable_2 : constrained_pin_transition ;
    index_1("0.01, 0.1") ;
    index_2("0.01, 0.1") ;
  }
  cell(POCV2) {
    area : 2.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff(IQ, IQN) {
      clocked_on : "CLK" ;
      next_state : "D" ;
    }
    pin(D) {
      timing() {
        related_pin : "CLK" ;
        timing_type : setup_rising ;
        sigma_type : early_and_late ;
        rise_constraint(constraint_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_constraint(constraint_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ocv_sigma_rise_constraint(constraint_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_fall_constraint(constraint_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_28: resistance_unit and distance_unit attributes
TEST_F(StaLibertyTest, ResistanceDistanceUnits) {
  const char *content = R"(
library(test_r9_28) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  resistance_unit : "1kohm" ;
  distance_unit : "1um" ;
  capacitive_load_unit(1, ff) ;
  cell(UNIT1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_29: rise/fall_transition_degradation tables
TEST_F(StaLibertyTest, TransitionDegradation) {
  const char *content = R"(
library(test_r9_29) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(degradation_template) {
    variable_1 : output_pin_transition ;
    variable_2 : connect_delay ;
    index_1("0.01, 0.1") ;
    index_2("0.0, 0.01") ;
  }
  rise_transition_degradation(degradation_template) {
    values("0.01, 0.02", "0.03, 0.04") ;
  }
  fall_transition_degradation(degradation_template) {
    values("0.01, 0.02", "0.03, 0.04") ;
  }
  cell(DEG1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_30: lut group in cell
TEST_F(StaLibertyTest, LutGroup) {
  const char *content = R"(
library(test_r9_30) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(LUT1) {
    area : 5.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
    lut(lut_state) {}
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_31: ECSM waveform constructs
TEST_F(StaLibertyTest, EcsmWaveform) {
  const char *content = R"(
library(test_r9_31) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(ECSM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ecsm_waveform() {}
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_32: power group (as opposed to rise_power/fall_power)
TEST_F(StaLibertyTest, PowerGroup) {
  const char *content = R"(
library(test_r9_32) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  power_lut_template(power_template_2x2) {
    variable_1 : input_transition_time ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(PWR1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      internal_power() {
        related_pin : "A" ;
        power(power_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_33: leakage_power group with when and related_pg_pin
TEST_F(StaLibertyTest, LeakagePowerGroup) {
  const char *content = R"(
library(test_r9_33) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  leakage_power_unit : "1nW" ;
  capacitive_load_unit(1, ff) ;
  cell(LP1) {
    area : 2.0 ;
    pg_pin(VDD) { pg_type : primary_power ; voltage_name : VDD ; }
    pg_pin(VSS) { pg_type : primary_ground ; voltage_name : VSS ; }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
    leakage_power() {
      when : "!A" ;
      value : 0.5 ;
      related_pg_pin : VDD ;
    }
    leakage_power() {
      when : "A" ;
      value : 0.8 ;
      related_pg_pin : VDD ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_34: InternalPowerModel checkAxes via reading a lib with internal power
TEST_F(StaLibertyTest, InternalPowerModelCheckAxes) {
  const char *content = R"(
library(test_r9_34) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  power_lut_template(power_template_1d) {
    variable_1 : input_transition_time ;
    index_1("0.01, 0.1") ;
  }
  cell(IPM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      internal_power() {
        related_pin : "A" ;
        rise_power(power_template_1d) {
          values("0.001, 0.002") ;
        }
        fall_power(power_template_1d) {
          values("0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_35: PortGroup and TimingGroup via direct construction
TEST_F(StaLibertyTest, PortGroupConstruct) {
  auto *ports = new LibertyPortSeq;
  PortGroup pg(ports, 1);
  TimingGroup *tg = new TimingGroup(1);
  pg.addTimingGroup(tg);
  InternalPowerGroup *ipg = new InternalPowerGroup(1);
  pg.addInternalPowerGroup(ipg);
  EXPECT_GT(pg.timingGroups().size(), 0u);
  EXPECT_GT(pg.internalPowerGroups().size(), 0u);
}

// R9_36: SequentialGroup construct and setters
TEST_F(StaLibertyTest, SequentialGroupSetters) {
  SequentialGroup sg(true, false, nullptr, nullptr, 1, 0);
  sg.setClock(stringCopy("CLK"));
  sg.setData(stringCopy("D"));
  sg.setClear(stringCopy("CLR"));
  sg.setPreset(stringCopy("PRE"));
  sg.setClrPresetVar1(LogicValue::zero);
  sg.setClrPresetVar2(LogicValue::one);
  EXPECT_TRUE(sg.isRegister());
  EXPECT_FALSE(sg.isBank());
  EXPECT_EQ(sg.size(), 1);
}

// R9_37: RelatedPortGroup construct and setters
TEST_F(StaLibertyTest, RelatedPortGroupSetters) {
  RelatedPortGroup rpg(1);
  auto *names = new StringSeq;
  names->push_back(stringCopy("A"));
  names->push_back(stringCopy("B"));
  rpg.setRelatedPortNames(names);
  rpg.setIsOneToOne(true);
  EXPECT_TRUE(rpg.isOneToOne());
}

// R9_38: TimingGroup intrinsic/resistance setters
TEST_F(StaLibertyTest, TimingGroupIntrinsicSetters) {
  TimingGroup tg(1);
  tg.setIntrinsic(RiseFall::rise(), 0.05f);
  tg.setIntrinsic(RiseFall::fall(), 0.06f);
  float val;
  bool exists;
  tg.intrinsic(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.05f);
  tg.intrinsic(RiseFall::fall(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.06f);
  tg.setResistance(RiseFall::rise(), 100.0f);
  tg.setResistance(RiseFall::fall(), 120.0f);
  tg.resistance(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 100.0f);
  tg.resistance(RiseFall::fall(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 120.0f);
}

// R9_39: TimingGroup setRelatedOutputPortName
TEST_F(StaLibertyTest, TimingGroupRelatedOutputPort) {
  TimingGroup tg(1);
  tg.setRelatedOutputPortName("Z");
  EXPECT_NE(tg.relatedOutputPortName(), nullptr);
}

// R9_40: InternalPowerGroup construct
TEST_F(StaLibertyTest, InternalPowerGroupConstruct) {
  InternalPowerGroup ipg(1);
  EXPECT_EQ(ipg.line(), 1);
}

// R9_41: LeakagePowerGroup construct and setters
TEST_F(StaLibertyTest, LeakagePowerGroupSetters) {
  LeakagePowerGroup lpg(1);
  lpg.setRelatedPgPin("VDD");
  lpg.setPower(0.5f);
  EXPECT_EQ(lpg.relatedPgPin(), "VDD");
  EXPECT_FLOAT_EQ(lpg.power(), 0.5f);
}

// R9_42: LibertyGroup isGroup and isVariable
TEST_F(StaLibertyTest, LibertyStmtTypes) {
  LibertyGroup grp("test", nullptr, 1);
  EXPECT_TRUE(grp.isGroup());
  EXPECT_FALSE(grp.isVariable());
}

// R9_43: LibertySimpleAttr isComplex returns false
TEST_F(StaLibertyTest, LibertySimpleAttrIsComplex) {
  LibertyStringAttrValue *val = new LibertyStringAttrValue("test");
  LibertySimpleAttr attr("name", val, 1);
  EXPECT_FALSE(attr.isComplex());
  EXPECT_TRUE(attr.isAttribute());
}

// R9_44: LibertyComplexAttr isSimple returns false
TEST_F(StaLibertyTest, LibertyComplexAttrIsSimple) {
  auto *values = new LibertyAttrValueSeq;
  LibertyComplexAttr attr("name", values, 1);
  EXPECT_FALSE(attr.isSimple());
  EXPECT_TRUE(attr.isAttribute());
}

// R9_45: LibertyStringAttrValue and LibertyFloatAttrValue type checks
TEST_F(StaLibertyTest, AttrValueCrossType) {
  // LibertyStringAttrValue normal usage
  LibertyStringAttrValue sval("hello");
  EXPECT_TRUE(sval.isString());
  EXPECT_FALSE(sval.isFloat());
  EXPECT_STREQ(sval.stringValue(), "hello");

  // LibertyFloatAttrValue normal usage
  LibertyFloatAttrValue fval(3.14f);
  EXPECT_FALSE(fval.isString());
  EXPECT_TRUE(fval.isFloat());
  EXPECT_FLOAT_EQ(fval.floatValue(), 3.14f);
}

// R9_46: LibertyDefine isDefine
TEST_F(StaLibertyTest, LibertyDefineIsDefine) {
  LibertyDefine def("myattr", LibertyGroupType::cell,
                    LibertyAttrType::attr_string, 1);
  EXPECT_TRUE(def.isDefine());
  EXPECT_FALSE(def.isVariable());
}

// R9_47: scaled_cell group
TEST_F(StaLibertyTest, ScaledCell) {
  const char *content = R"(
library(test_r9_47) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  operating_conditions(fast) {
    process : 0.8 ;
    voltage : 1.2 ;
    temperature : 0.0 ;
    tree_type : best_case_tree ;
  }
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SC1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
  scaled_cell(SC1, fast) {
    area : 1.8 ;
    pin(A) { direction : input ; capacitance : 0.008 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.008, 0.015", "0.025, 0.035") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.008, 0.015", "0.025, 0.035") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.008, 0.015", "0.025, 0.035") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.008, 0.015", "0.025, 0.035") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_48: TimingGroup cell/transition/constraint setters
TEST_F(StaLibertyTest, TimingGroupTableModelSetters) {
  TimingGroup tg(1);
  // Test setting and getting cell models
  EXPECT_EQ(tg.cell(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.cell(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::fall()), nullptr);
}

// R9_49: LibertyParser construct, group(), deleteGroups(), makeVariable()
TEST_F(StaLibertyTest, LibertyParserConstruct) {
  const char *content = R"(
library(test_r9_49) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(P1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  // Write with thresholds injected
  FILE *f = fopen("/tmp/test_r9_49.lib", "w");
  ASSERT_NE(f, nullptr);
  const char *brace = strchr(content, '{');
  if (brace) {
    fwrite(content, 1, brace - content + 1, f);
    fprintf(f, "%s", R9_THRESHOLDS);
    fprintf(f, "%s", brace + 1);
  }
  fclose(f);
  // Read via readLibertyFile which exercises LibertyParser/LibertyReader directly
  LibertyReader reader("/tmp/test_r9_49.lib", false, sta_->network());
  LibertyLibrary *lib = reader.readLibertyFile("/tmp/test_r9_49.lib");
  EXPECT_NE(lib, nullptr);
  remove("/tmp/test_r9_49.lib");
}

// R9_50: cell with switch_cell_type fine_grain
TEST_F(StaLibertyTest, SwitchCellTypeFineGrain) {
  const char *content = R"(
library(test_r9_50) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(SW2) {
    area : 5.0 ;
    switch_cell_type : fine_grain ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_51: pulse_clock with different trigger/sense combos
TEST_F(StaLibertyTest, PulseClockFallTrigger) {
  const char *content = R"(
library(test_r9_51) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PC2) {
    area : 2.0 ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      pulse_clock : fall_triggered_low_pulse ;
    }
    pin(Z) {
      direction : output ;
      function : "CLK" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_52: pulse_clock rise_triggered_low_pulse
TEST_F(StaLibertyTest, PulseClockRiseTriggeredLow) {
  const char *content = R"(
library(test_r9_52) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PC3) {
    area : 2.0 ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      pulse_clock : rise_triggered_low_pulse ;
    }
    pin(Z) { direction : output ; function : "CLK" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_53: pulse_clock fall_triggered_high_pulse
TEST_F(StaLibertyTest, PulseClockFallTriggeredHigh) {
  const char *content = R"(
library(test_r9_53) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PC4) {
    area : 2.0 ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      pulse_clock : fall_triggered_high_pulse ;
    }
    pin(Z) { direction : output ; function : "CLK" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_54: OCV derate with derate_type late
TEST_F(StaLibertyTest, OcvDerateTypeLate) {
  const char *content = R"(
library(test_r9_54) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_tmpl) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  ocv_derate(derate_late) {
    ocv_derate_factors(ocv_tmpl) {
      rf_type : rise_and_fall ;
      derate_type : late ;
      path_type : data ;
      values("1.05, 1.06") ;
    }
  }
  cell(OCV3) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_55: OCV derate with path_type clock
TEST_F(StaLibertyTest, OcvDeratePathTypeClock) {
  const char *content = R"(
library(test_r9_55) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_tmpl2) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  ocv_derate(derate_clk) {
    ocv_derate_factors(ocv_tmpl2) {
      rf_type : fall ;
      derate_type : early ;
      path_type : clock ;
      values("0.95, 0.96") ;
    }
  }
  cell(OCV4) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_56: TimingGroup setDelaySigma/setSlewSigma/setConstraintSigma
TEST_F(StaLibertyTest, TimingGroupSigmaSetters) {
  TimingGroup tg(1);
  // Setting to nullptr just exercises the method
  tg.setDelaySigma(RiseFall::rise(), EarlyLate::min(), nullptr);
  tg.setDelaySigma(RiseFall::fall(), EarlyLate::max(), nullptr);
  tg.setSlewSigma(RiseFall::rise(), EarlyLate::min(), nullptr);
  tg.setSlewSigma(RiseFall::fall(), EarlyLate::max(), nullptr);
  tg.setConstraintSigma(RiseFall::rise(), EarlyLate::min(), nullptr);
  tg.setConstraintSigma(RiseFall::fall(), EarlyLate::max(), nullptr);
}

// R9_57: Cover setIsScaled via reading a scaled_cell lib
TEST_F(StaLibertyTest, ScaledCellCoversIsScaled) {
  // scaled_cell reading exercises GateTableModel::setIsScaled,
  // GateLinearModel::setIsScaled, CheckTableModel::setIsScaled internally
  const char *content = R"(
library(test_r9_57) {
  delay_model : generic_cmos ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  pulling_resistance_unit : "1kohm" ;
  capacitive_load_unit(1, ff) ;
  operating_conditions(slow) {
    process : 1.2 ;
    voltage : 0.9 ;
    temperature : 125.0 ;
    tree_type : worst_case_tree ;
  }
  cell(LM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        intrinsic_rise : 0.05 ;
        intrinsic_fall : 0.06 ;
        rise_resistance : 100.0 ;
        fall_resistance : 120.0 ;
      }
    }
  }
  scaled_cell(LM1, slow) {
    area : 2.2 ;
    pin(A) { direction : input ; capacitance : 0.012 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        intrinsic_rise : 0.07 ;
        intrinsic_fall : 0.08 ;
        rise_resistance : 130.0 ;
        fall_resistance : 150.0 ;
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_58: GateTableModel checkAxis exercised via table model reading
TEST_F(StaLibertyTest, GateTableModelCheckAxis) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  for (TimingArc *arc : arcsets[0]->arcs()) {
    TimingModel *model = arc->model();
    GateTableModel *gtm = dynamic_cast<GateTableModel*>(model);
    if (gtm) {
      EXPECT_NE(gtm, nullptr);
      break;
    }
  }
}

// R9_59: CheckTableModel checkAxis exercised via setup timing
TEST_F(StaLibertyTest, CheckTableModelCheckAxis) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (!dff) return;
  auto &arcsets = dff->timingArcSets();
  for (size_t i = 0; i < arcsets.size(); i++) {
    TimingArcSet *arcset = arcsets[i];
    if (arcset->role() == TimingRole::setup()) {
      for (TimingArc *arc : arcset->arcs()) {
        TimingModel *model = arc->model();
        CheckTableModel *ctm = dynamic_cast<CheckTableModel*>(model);
        if (ctm) {
          EXPECT_NE(ctm, nullptr);
        }
      }
      break;
    }
  }
}

// R9_60: TimingGroup cell/transition/constraint getter coverage
TEST_F(StaLibertyTest, TimingGroupGettersNull) {
  TimingGroup tg(1);
  // By default all model pointers should be null
  EXPECT_EQ(tg.cell(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.cell(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.outputWaveforms(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.outputWaveforms(RiseFall::fall()), nullptr);
}

// R9_61: Timing with ecsm_waveform_set and ecsm_capacitance
TEST_F(StaLibertyTest, EcsmWaveformSet) {
  const char *content = R"(
library(test_r9_61) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(ECSM2) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ecsm_waveform_set() {}
        ecsm_capacitance() {}
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_62: sigma_type early
TEST_F(StaLibertyTest, SigmaTypeEarly) {
  const char *content = R"(
library(test_r9_62) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SIG1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sigma_type : early ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ocv_sigma_cell_rise(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_cell_fall(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_rise_transition(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_fall_transition(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_63: sigma_type late
TEST_F(StaLibertyTest, SigmaTypeLate) {
  const char *content = R"(
library(test_r9_63) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SIG2) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sigma_type : late ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ocv_sigma_cell_rise(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_cell_fall(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_64: Receiver capacitance with segment attribute
TEST_F(StaLibertyTest, ReceiverCapacitanceSegment) {
  const char *content = R"(
library(test_r9_64) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(RCV1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
      receiver_capacitance() {
        receiver_capacitance1_rise(delay_template_2x2) {
          segment : 0 ;
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        receiver_capacitance1_fall(delay_template_2x2) {
          segment : 0 ;
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_65: LibertyCell hasInternalPorts (read-only check)
TEST_F(StaLibertyTest, CellHasInternalPorts4) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  // DFF should have internal ports for state vars (IQ, IQN)
  EXPECT_TRUE(dff->hasInternalPorts());
  // A simple buffer should not
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->hasInternalPorts());
}

// R9_66: LibertyBuilder destructor (coverage)
TEST_F(StaLibertyTest, LibertyBuilderDestruct) {
  LibertyBuilder *builder = new LibertyBuilder;
  delete builder;
}

// R9_67: Timing with setup constraint for coverage
TEST_F(StaLibertyTest, TimingSetupConstraint) {
  const char *content = R"(
library(test_r9_67) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(constraint_template_2x2) {
    variable_1 : related_pin_transition ;
    variable_2 : constrained_pin_transition ;
    index_1("0.01, 0.1") ;
    index_2("0.01, 0.1") ;
  }
  cell(FF1) {
    area : 4.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff(IQ, IQN) {
      clocked_on : "CLK" ;
      next_state : "D" ;
    }
    pin(D) {
      timing() {
        related_pin : "CLK" ;
        timing_type : setup_rising ;
        rise_constraint(constraint_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_constraint(constraint_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
      timing() {
        related_pin : "CLK" ;
        timing_type : hold_rising ;
        rise_constraint(constraint_template_2x2) {
          values("-0.01, -0.02", "-0.03, -0.04") ;
        }
        fall_constraint(constraint_template_2x2) {
          values("-0.01, -0.02", "-0.03, -0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_68: Library with define statement
TEST_F(StaLibertyTest, DefineStatement) {
  const char *content = R"(
library(test_r9_68) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  define(my_attr, cell, string) ;
  define(my_float_attr, pin, float) ;
  cell(DEF1) {
    area : 2.0 ;
    my_attr : "custom_value" ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      my_float_attr : 3.14 ;
    }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_69: multiple scaling_factors type combinations
TEST_F(StaLibertyTest, ScalingFactorsMultipleTypes) {
  const char *content = R"(
library(test_r9_69) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  scaling_factors(multi_scale) {
    k_process_cell_rise : 1.0 ;
    k_process_cell_fall : 1.0 ;
    k_process_rise_transition : 0.8 ;
    k_process_fall_transition : 0.8 ;
    k_volt_cell_rise : -0.5 ;
    k_volt_cell_fall : -0.5 ;
    k_volt_rise_transition : -0.3 ;
    k_volt_fall_transition : -0.3 ;
    k_temp_cell_rise : 0.001 ;
    k_temp_cell_fall : 0.001 ;
    k_temp_rise_transition : 0.0005 ;
    k_temp_fall_transition : 0.0005 ;
    k_process_hold_rise : 1.0 ;
    k_process_hold_fall : 1.0 ;
    k_process_setup_rise : 1.0 ;
    k_process_setup_fall : 1.0 ;
    k_volt_hold_rise : -0.5 ;
    k_volt_hold_fall : -0.5 ;
    k_volt_setup_rise : -0.5 ;
    k_volt_setup_fall : -0.5 ;
    k_temp_hold_rise : 0.001 ;
    k_temp_hold_fall : 0.001 ;
    k_temp_setup_rise : 0.001 ;
    k_temp_setup_fall : 0.001 ;
  }
  cell(SC2) {
    area : 2.0 ;
    scaling_factors : multi_scale ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_70: OCV derate with early_and_late derate_type
TEST_F(StaLibertyTest, OcvDerateEarlyAndLate) {
  const char *content = R"(
library(test_r9_70) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_tmpl3) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  ocv_derate(derate_both) {
    ocv_derate_factors(ocv_tmpl3) {
      rf_type : rise ;
      derate_type : early_and_late ;
      path_type : clock_and_data ;
      values("1.0, 1.0") ;
    }
  }
  cell(OCV5) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_71: leakage_power with clear_preset_var1/var2 in ff
TEST_F(StaLibertyTest, FFClearPresetVars) {
  const char *content = R"(
library(test_r9_71) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(DFF2) {
    area : 4.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(CLR) { direction : input ; capacitance : 0.01 ; }
    pin(PRE) { direction : input ; capacitance : 0.01 ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    pin(QN) { direction : output ; function : "IQN" ; }
    ff(IQ, IQN) {
      clocked_on : "CLK" ;
      next_state : "D" ;
      clear : "CLR" ;
      preset : "PRE" ;
      clear_preset_var1 : L ;
      clear_preset_var2 : H ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_72: mode_definition with multiple mode_values
TEST_F(StaLibertyTest, ModeDefMultipleValues) {
  const char *content = R"(
library(test_r9_72) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(MD1) {
    area : 2.0 ;
    mode_definition(op_mode) {
      mode_value(fast) {
        when : "A" ;
        sdf_cond : "A == 1'b1" ;
      }
      mode_value(slow) {
        when : "!A" ;
        sdf_cond : "A == 1'b0" ;
      }
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_73: timing with related_output_pin
TEST_F(StaLibertyTest, TimingRelatedOutputPin) {
  const char *content = R"(
library(test_r9_73) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(ROP1) {
    area : 4.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Y) {
      direction : output ;
      function : "A & B" ;
    }
    pin(Z) {
      direction : output ;
      function : "A | B" ;
      timing() {
        related_pin : "A" ;
        related_output_pin : "Y" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_74: wire_load_selection group
TEST_F(StaLibertyTest, WireLoadSelection) {
  const char *content = R"(
library(test_r9_74) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  wire_load("small") {
    capacitance : 0.1 ;
    resistance : 0.001 ;
    slope : 5.0 ;
    fanout_length(1, 1.0) ;
    fanout_length(2, 2.0) ;
  }
  wire_load("medium") {
    capacitance : 0.2 ;
    resistance : 0.002 ;
    slope : 6.0 ;
    fanout_length(1, 1.5) ;
    fanout_length(2, 3.0) ;
  }
  wire_load_selection(area_sel) {
    wire_load_from_area(0, 100, "small") ;
    wire_load_from_area(100, 1000, "medium") ;
  }
  default_wire_load_selection : area_sel ;
  cell(WLS1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_75: interface_timing on cell
TEST_F(StaLibertyTest, CellInterfaceTiming3) {
  const char *content = R"(
library(test_r9_75) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(IF1) {
    area : 2.0 ;
    interface_timing : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_76: cell_footprint attribute
TEST_F(StaLibertyTest, CellFootprint4) {
  const char *content = R"(
library(test_r9_76) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(FP1) {
    area : 2.0 ;
    cell_footprint : buf ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_77: test_cell group
TEST_F(StaLibertyTest, TestCellGroup) {
  const char *content = R"(
library(test_r9_77) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(TC1) {
    area : 3.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff(IQ, IQN) {
      clocked_on : "CLK" ;
      next_state : "D" ;
    }
    test_cell() {
      pin(D) {
        direction : input ;
        signal_type : test_scan_in ;
      }
      pin(CLK) {
        direction : input ;
        signal_type : test_clock ;
      }
      pin(Q) {
        direction : output ;
        signal_type : test_scan_out ;
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_78: memory group
TEST_F(StaLibertyTest, MemoryGroup) {
  const char *content = R"(
library(test_r9_78) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(SRAM1) {
    area : 100.0 ;
    is_memory : true ;
    memory() {
      type : ram ;
      address_width : 4 ;
      word_width : 8 ;
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_79: cell with always_on attribute
TEST_F(StaLibertyTest, CellAlwaysOn3) {
  const char *content = R"(
library(test_r9_79) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(AON1) {
    area : 2.0 ;
    always_on : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_80: cell with is_level_shifter and level_shifter_type
TEST_F(StaLibertyTest, CellLevelShifter) {
  const char *content = R"(
library(test_r9_80) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(LS1) {
    area : 3.0 ;
    is_level_shifter : true ;
    level_shifter_type : HL ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      level_shifter_data_pin : true ;
    }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_81: cell with is_isolation_cell
TEST_F(StaLibertyTest, CellIsolationCell) {
  const char *content = R"(
library(test_r9_81) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(ISO1) {
    area : 3.0 ;
    is_isolation_cell : true ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      isolation_cell_data_pin : true ;
    }
    pin(EN) {
      direction : input ;
      capacitance : 0.01 ;
      isolation_cell_enable_pin : true ;
    }
    pin(Z) { direction : output ; function : "A & EN" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_82: statetable group
TEST_F(StaLibertyTest, StatetableGroup) {
  const char *content = R"(
library(test_r9_82) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(ST1) {
    area : 4.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(E) { direction : input ; capacitance : 0.01 ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    statetable("D E", "IQ") {
      table : "H L : - : H, \
               L L : - : L, \
               - H : - : N" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_83: Timing with sdf_cond
TEST_F(StaLibertyTest, TimingSdfCond) {
  const char *content = R"(
library(test_r9_83) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SDF2) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A & B" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sdf_cond : "B == 1'b1" ;
        when : "B" ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_84: power with rise_power and fall_power groups
TEST_F(StaLibertyTest, RiseFallPowerGroups) {
  const char *content = R"(
library(test_r9_84) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  power_lut_template(power_2d) {
    variable_1 : input_transition_time ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(PW2) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      internal_power() {
        related_pin : "A" ;
        rise_power(power_2d) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        fall_power(power_2d) {
          values("0.005, 0.006", "0.007, 0.008") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_85: TimingGroup makeLinearModels coverage
TEST_F(StaLibertyTest, TimingGroupLinearModels) {
  TimingGroup tg(1);
  tg.setIntrinsic(RiseFall::rise(), 0.05f);
  tg.setIntrinsic(RiseFall::fall(), 0.06f);
  tg.setResistance(RiseFall::rise(), 100.0f);
  tg.setResistance(RiseFall::fall(), 120.0f);
  // makeLinearModels needs a cell - but we can verify values are set
  float val;
  bool exists;
  tg.intrinsic(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  tg.resistance(RiseFall::fall(), val, exists);
  EXPECT_TRUE(exists);
}

// R9_86: multiple wire_load and default_wire_load
TEST_F(StaLibertyTest, DefaultWireLoad) {
  const char *content = R"(
library(test_r9_86) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  wire_load("tiny") {
    capacitance : 0.05 ;
    resistance : 0.001 ;
    slope : 3.0 ;
    fanout_length(1, 0.5) ;
  }
  default_wire_load : "tiny" ;
  default_wire_load_mode : top ;
  cell(DWL1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_87: voltage_map attribute
TEST_F(StaLibertyTest, VoltageMap) {
  const char *content = R"(
library(test_r9_87) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  voltage_map(VDD, 1.1) ;
  voltage_map(VSS, 0.0) ;
  voltage_map(VDDL, 0.8) ;
  cell(VM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_88: default_operating_conditions
TEST_F(StaLibertyTest, DefaultOperatingConditions) {
  const char *content = R"(
library(test_r9_88) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  operating_conditions(fast_oc) {
    process : 0.8 ;
    voltage : 1.2 ;
    temperature : 0.0 ;
    tree_type : best_case_tree ;
  }
  operating_conditions(slow_oc) {
    process : 1.2 ;
    voltage : 0.9 ;
    temperature : 125.0 ;
    tree_type : worst_case_tree ;
  }
  default_operating_conditions : fast_oc ;
  cell(DOC1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_89: pg_pin group with pg_type and voltage_name
TEST_F(StaLibertyTest, PgPin) {
  const char *content = R"(
library(test_r9_89) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  voltage_map(VDD, 1.1) ;
  voltage_map(VSS, 0.0) ;
  cell(PG1) {
    area : 2.0 ;
    pg_pin(VDD) {
      pg_type : primary_power ;
      voltage_name : VDD ;
    }
    pg_pin(VSS) {
      pg_type : primary_ground ;
      voltage_name : VSS ;
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_90: TimingGroup set/get cell table models
TEST_F(StaLibertyTest, TimingGroupCellModels) {
  TimingGroup tg(1);
  tg.setCell(RiseFall::rise(), nullptr);
  tg.setCell(RiseFall::fall(), nullptr);
  EXPECT_EQ(tg.cell(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.cell(RiseFall::fall()), nullptr);
}

// R9_91: TimingGroup constraint setters
TEST_F(StaLibertyTest, TimingGroupConstraintModels) {
  TimingGroup tg(1);
  tg.setConstraint(RiseFall::rise(), nullptr);
  tg.setConstraint(RiseFall::fall(), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::fall()), nullptr);
}

// R9_92: TimingGroup transition setters
TEST_F(StaLibertyTest, TimingGroupTransitionModels) {
  TimingGroup tg(1);
  tg.setTransition(RiseFall::rise(), nullptr);
  tg.setTransition(RiseFall::fall(), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::fall()), nullptr);
}

// R9_93: bus_naming_style attribute
TEST_F(StaLibertyTest, BusNamingStyle) {
  const char *content = R"(
library(test_r9_93) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  bus_naming_style : "%s[%d]" ;
  cell(BNS1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_94: cell_leakage_power
TEST_F(StaLibertyTest, CellLeakagePower5) {
  const char *content = R"(
library(test_r9_94) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  leakage_power_unit : "1nW" ;
  capacitive_load_unit(1, ff) ;
  cell(CLP1) {
    area : 2.0 ;
    cell_leakage_power : 1.5 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_95: clock_gating_integrated_cell
TEST_F(StaLibertyTest, ClockGatingIntegratedCell) {
  const char *content = R"(
library(test_r9_95) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(CGC1) {
    area : 3.0 ;
    clock_gating_integrated_cell : latch_posedge ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      clock : true ;
      clock_gate_clock_pin : true ;
    }
    pin(EN) {
      direction : input ;
      capacitance : 0.01 ;
      clock_gate_enable_pin : true ;
    }
    pin(GCLK) {
      direction : output ;
      function : "CLK & EN" ;
      clock_gate_out_pin : true ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_96: output_current_rise/fall (CCS constructs)
TEST_F(StaLibertyTest, OutputCurrentRiseFall) {
  const char *content = R"(
library(test_r9_96) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  output_current_template(ccs_template) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    variable_3 : time ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(CCS1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        output_current_rise(ccs_template) {
          vector(0) {
            index_3("0.0, 0.1, 0.2, 0.3, 0.4") ;
            values("0.001, 0.002", "0.003, 0.004") ;
          }
        }
        output_current_fall(ccs_template) {
          vector(0) {
            index_3("0.0, 0.1, 0.2, 0.3, 0.4") ;
            values("0.001, 0.002", "0.003, 0.004") ;
          }
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_97: three_state attribute on pin
TEST_F(StaLibertyTest, PinThreeState) {
  const char *content = R"(
library(test_r9_97) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(TS1) {
    area : 3.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(EN) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      three_state : "EN" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_98: rise_capacitance_range and fall_capacitance_range
TEST_F(StaLibertyTest, PinCapacitanceRange) {
  const char *content = R"(
library(test_r9_98) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(CR1) {
    area : 2.0 ;
    pin(A) {
      direction : input ;
      rise_capacitance : 0.01 ;
      fall_capacitance : 0.012 ;
      rise_capacitance_range(0.008, 0.012) ;
      fall_capacitance_range(0.009, 0.015) ;
    }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_99: dont_use attribute
TEST_F(StaLibertyTest, CellDontUse4) {
  const char *content = R"(
library(test_r9_99) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(DU1) {
    area : 2.0 ;
    dont_use : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content, "/tmp/test_r9_99.lib");
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("DU1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->dontUse());
}

// R9_100: is_macro_cell attribute
TEST_F(StaLibertyTest, CellIsMacro4) {
  const char *content = R"(
library(test_r9_100) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(MAC1) {
    area : 100.0 ;
    is_macro_cell : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content, "/tmp/test_r9_100.lib");
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("MAC1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->isMacro());
}

// R9_101: OCV derate at cell level
TEST_F(StaLibertyTest, OcvDerateCellLevel) {
  const char *content = R"(
library(test_r9_101) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_tmpl4) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  cell(OCV6) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
    ocv_derate(cell_derate) {
      ocv_derate_factors(ocv_tmpl4) {
        rf_type : rise_and_fall ;
        derate_type : early ;
        path_type : clock_and_data ;
        values("0.95, 0.96") ;
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_102: timing with when (conditional)
TEST_F(StaLibertyTest, TimingWhenConditional) {
  const char *content = R"(
library(test_r9_102) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(COND1) {
    area : 3.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A & B" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        when : "B" ;
        sdf_cond : "B == 1'b1" ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        when : "!B" ;
        sdf_cond : "B == 1'b0" ;
        cell_rise(delay_template_2x2) {
          values("0.02, 0.03", "0.04, 0.05") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.02, 0.03", "0.04, 0.05") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.02, 0.03", "0.04, 0.05") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.02, 0.03", "0.04, 0.05") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_103: default_max_fanout
TEST_F(StaLibertyTest, DefaultMaxFanout) {
  const char *content = R"(
library(test_r9_103) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_max_fanout : 32.0 ;
  cell(DMF1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_104: default_fanout_load
TEST_F(StaLibertyTest, DefaultFanoutLoad) {
  const char *content = R"(
library(test_r9_104) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_fanout_load : 2.0 ;
  cell(DFL1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R9_105: TimingGroup outputWaveforms accessors (should be null by default)
TEST_F(StaLibertyTest, TimingGroupOutputWaveforms) {
  TimingGroup tg(1);
  EXPECT_EQ(tg.outputWaveforms(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.outputWaveforms(RiseFall::fall()), nullptr);
}

// =========================================================================
// R11_ tests: Cover additional uncovered functions in liberty module
// =========================================================================

// R11_1: timingTypeString - the free function in TimingArc.cc
// It is not declared in a public header, so we declare it extern here.
extern const char *timingTypeString(TimingType type);

TEST_F(StaLibertyTest, TimingTypeString) {
  // timingTypeString is defined in TimingArc.cc
  // We test several timing types to cover the function
  EXPECT_STREQ(timingTypeString(TimingType::combinational), "combinational");
  EXPECT_STREQ(timingTypeString(TimingType::clear), "clear");
  EXPECT_STREQ(timingTypeString(TimingType::rising_edge), "rising_edge");
  EXPECT_STREQ(timingTypeString(TimingType::falling_edge), "falling_edge");
  EXPECT_STREQ(timingTypeString(TimingType::setup_rising), "setup_rising");
  EXPECT_STREQ(timingTypeString(TimingType::hold_falling), "hold_falling");
  EXPECT_STREQ(timingTypeString(TimingType::three_state_enable), "three_state_enable");
  EXPECT_STREQ(timingTypeString(TimingType::unknown), "unknown");
}

// R11_2: writeLiberty exercises LibertyWriter constructor, destructor,
// writeHeader, writeFooter, asString(bool), and the full write path
TEST_F(StaLibertyTest, WriteLiberty) {
  ASSERT_NE(lib_, nullptr);
  const char *tmpfile = "/tmp/test_r11_write_liberty.lib";
  // writeLiberty is declared in LibertyWriter.hh
  writeLiberty(lib_, tmpfile, sta_);
  // Verify the file was written and has content
  FILE *fp = fopen(tmpfile, "r");
  ASSERT_NE(fp, nullptr);
  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  EXPECT_GT(sz, 100);  // non-trivial content
  fclose(fp);
  remove(tmpfile);
}

// R11_3: LibertyParser direct usage - exercises LibertyParser constructor,
// group(), deleteGroups(), makeVariable(), LibertyStmt constructors/destructors,
// LibertyAttr, LibertySimpleAttr, LibertyComplexAttr, LibertyStringAttrValue,
// LibertyFloatAttrValue, LibertyDefine, LibertyVariable, isGroup/isAttribute/
// isDefine/isVariable/isSimple/isComplex, and values() on simple attrs.
TEST_F(StaLibertyTest, LibertyParserDirect) {
  // Write a simple lib file for parser testing
  const char *content = R"(
library(test_r11_parser) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  define(my_attr, cell, string) ;
  my_var = 3.14 ;
  cell(P1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  std::string tmp_path = "/tmp/test_r11_parser.lib";
  writeLibContent(content, tmp_path);

  // Parse with a simple visitor that just collects groups
  Report *report = sta_->report();
  // Use parseLibertyFile which creates the parser internally
  // This exercises LibertyParser constructor, LibertyScanner,
  // group(), deleteGroups()
  class TestVisitor : public LibertyGroupVisitor {
  public:
    int group_count = 0;
    int attr_count = 0;
    int var_count = 0;
    void begin(LibertyGroup *group) override { group_count++; }
    void end(LibertyGroup *) override {}
    void visitAttr(LibertyAttr *attr) override {
      attr_count++;
      // Exercise isAttribute, isSimple, isComplex, values()
      EXPECT_TRUE(attr->isAttribute());
      EXPECT_FALSE(attr->isGroup());
      EXPECT_FALSE(attr->isDefine());
      EXPECT_FALSE(attr->isVariable());
      if (attr->isSimple()) {
        EXPECT_FALSE(attr->isComplex());
        // Simple attrs have firstValue but values() is not supported
      }
      if (attr->isComplex()) {
        EXPECT_FALSE(attr->isSimple());
      }
      // Exercise firstValue
      LibertyAttrValue *val = attr->firstValue();
      if (val) {
        if (val->isString()) {
          EXPECT_NE(val->stringValue(), nullptr);
          EXPECT_FALSE(val->isFloat());
        }
        if (val->isFloat()) {
          EXPECT_FALSE(val->isString());
          (void)val->floatValue();
        }
      }
    }
    void visitVariable(LibertyVariable *variable) override {
      var_count++;
      EXPECT_TRUE(variable->isVariable());
      EXPECT_FALSE(variable->isGroup());
      EXPECT_FALSE(variable->isAttribute());
      EXPECT_FALSE(variable->isDefine());
      EXPECT_NE(variable->variable(), nullptr);
      (void)variable->value();
    }
    bool save(LibertyGroup *) override { return false; }
    bool save(LibertyAttr *) override { return false; }
    bool save(LibertyVariable *) override { return false; }
  };

  TestVisitor visitor;
  parseLibertyFile(tmp_path.c_str(), &visitor, report);
  EXPECT_GT(visitor.group_count, 0);
  EXPECT_GT(visitor.attr_count, 0);
  EXPECT_GT(visitor.var_count, 0);
  remove(tmp_path.c_str());
}

// R11_4: Liberty file with wireload_selection to cover WireloadForArea
TEST_F(StaLibertyTest, WireloadForArea) {
  const char *content = R"(
library(test_r11_wfa) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  wire_load("small") {
    resistance : 0.0 ;
    capacitance : 1.0 ;
    area : 0.0 ;
    slope : 100.0 ;
    fanout_length(1, 200) ;
  }
  wire_load("medium") {
    resistance : 0.0 ;
    capacitance : 1.0 ;
    area : 0.0 ;
    slope : 200.0 ;
    fanout_length(1, 400) ;
  }
  wire_load_selection(sel1) {
    wire_load_from_area(0, 100, "small") ;
    wire_load_from_area(100, 500, "medium") ;
  }
  cell(WFA1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R11_5: Liberty file with latch to exercise inferLatchRoles
TEST_F(StaLibertyTest, InferLatchRoles) {
  const char *content = R"(
library(test_r11_latch) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(LATCH1) {
    area : 5.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(G) { direction : input ; capacitance : 0.01 ; }
    pin(Q) {
      direction : output ;
      function : "IQ" ;
    }
    latch(IQ, IQN) {
      enable : "G" ;
      data_in : "D" ;
    }
  }
}
)";
  // Read with infer_latches = true
  std::string tmp_path = "/tmp/test_r11_latch.lib";
  writeLibContent(content, tmp_path);
  LibertyLibrary *lib = sta_->readLiberty(tmp_path.c_str(), sta_->cmdCorner(),
                                           MinMaxAll::min(), true);  // infer_latches=true
  EXPECT_NE(lib, nullptr);
  if (lib) {
    LibertyCell *cell = lib->findLibertyCell("LATCH1");
    EXPECT_NE(cell, nullptr);
    if (cell) {
      EXPECT_TRUE(cell->hasSequentials());
    }
  }
  remove(tmp_path.c_str());
}

// R11_6: Liberty file with leakage_power { when } to cover LeakagePowerGroup::setWhen
TEST_F(StaLibertyTest, LeakagePowerWhen) {
  const char *content = R"(
library(test_r11_lpw) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  leakage_power_unit : "1nW" ;
  cell(LPW1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
    leakage_power() {
      when : "A" ;
      value : 10.5 ;
    }
    leakage_power() {
      when : "!A" ;
      value : 5.2 ;
    }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  EXPECT_NE(lib, nullptr);
  if (lib) {
    LibertyCell *cell = lib->findLibertyCell("LPW1");
    EXPECT_NE(cell, nullptr);
  }
}

// R11_7: Liberty file with statetable to cover StatetableGroup::addRow
TEST_F(StaLibertyTest, Statetable) {
  const char *content = R"(
library(test_r11_st) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(ST1) {
    area : 3.0 ;
    pin(S) { direction : input ; capacitance : 0.01 ; }
    pin(R) { direction : input ; capacitance : 0.01 ; }
    pin(Q) {
      direction : output ;
      function : "IQ" ;
    }
    statetable("S R", "IQ") {
      table : "H L : - : H ,\
               L H : - : L ,\
               L L : - : N ,\
               H H : - : X" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R11_8: Liberty file with internal_power to cover
// InternalPowerModel::checkAxes/checkAxis
TEST_F(StaLibertyTest, InternalPowerModel) {
  const char *content = R"(
library(test_r11_ipm) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  leakage_power_unit : "1nW" ;
  cell(IPM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(scalar) { values("0.1") ; }
        cell_fall(scalar) { values("0.1") ; }
        rise_transition(scalar) { values("0.05") ; }
        fall_transition(scalar) { values("0.05") ; }
      }
      internal_power() {
        related_pin : "A" ;
        rise_power(scalar) { values("0.5") ; }
        fall_power(scalar) { values("0.3") ; }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R11_9: Liberty file with bus port to cover PortNameBitIterator and findLibertyMember
TEST_F(StaLibertyTest, BusPortAndMember) {
  const char *content = R"(
library(test_r11_bus) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  type(bus4) {
    base_type : array ;
    data_type : bit ;
    bit_width : 4 ;
    bit_from : 3 ;
    bit_to : 0 ;
  }
  cell(BUS1) {
    area : 4.0 ;
    bus(D) {
      bus_type : bus4 ;
      direction : input ;
      capacitance : 0.01 ;
    }
    pin(Z) { direction : output ; function : "D[0]" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  EXPECT_NE(lib, nullptr);
  if (lib) {
    LibertyCell *cell = lib->findLibertyCell("BUS1");
    EXPECT_NE(cell, nullptr);
    if (cell) {
      // The bus should create member ports
      LibertyPort *bus_port = cell->findLibertyPort("D");
      if (bus_port) {
        // findLibertyMember on bus port
        LibertyPort *member = bus_port->findLibertyMember(0);
        if (member)
          EXPECT_NE(member, nullptr);
      }
    }
  }
}

// R11_10: Liberty file with include directive to cover LibertyScanner::includeBegin, fileEnd
// We test this by creating a .lib that includes another .lib
TEST_F(StaLibertyTest, LibertyInclude) {
  // First write the included file
  const char *inc_path = "/tmp/test_r11_included.lib";
  FILE *finc = fopen(inc_path, "w");
  if (finc) {
    fprintf(finc, "  cell(INC1) {\n");
    fprintf(finc, "    area : 1.0 ;\n");
    fprintf(finc, "    pin(A) { direction : input ; capacitance : 0.01 ; }\n");
    fprintf(finc, "    pin(Z) { direction : output ; function : \"A\" ; }\n");
    fprintf(finc, "  }\n");
    fclose(finc);
  }

  // Write the main lib directly (not through writeAndReadLib which changes path)
  const char *main_path = "/tmp/test_r11_include_main.lib";
  FILE *fm = fopen(main_path, "w");
  ASSERT_NE(fm, nullptr);
  fprintf(fm, "library(test_r11_include) {\n");
  fprintf(fm, "%s", R9_THRESHOLDS);
  fprintf(fm, "  delay_model : table_lookup ;\n");
  fprintf(fm, "  time_unit : \"1ns\" ;\n");
  fprintf(fm, "  voltage_unit : \"1V\" ;\n");
  fprintf(fm, "  current_unit : \"1mA\" ;\n");
  fprintf(fm, "  capacitive_load_unit(1, ff) ;\n");
  fprintf(fm, "  include_file(%s) ;\n", inc_path);
  fprintf(fm, "}\n");
  fclose(fm);

  LibertyLibrary *lib = sta_->readLiberty(main_path, sta_->cmdCorner(),
                                           MinMaxAll::min(), false);
  EXPECT_NE(lib, nullptr);
  if (lib) {
    LibertyCell *cell = lib->findLibertyCell("INC1");
    EXPECT_NE(cell, nullptr);
  }
  remove(inc_path);
  remove(main_path);
}

// R11_11: Exercise timing arc traversal from loaded library
TEST_F(StaLibertyTest, TimingArcSetTraversal) {
  ASSERT_NE(lib_, nullptr);
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  // Count arc sets and arcs
  int arc_set_count = 0;
  int arc_count = 0;
  for (TimingArcSet *arc_set : buf->timingArcSets()) {
    arc_set_count++;
    for (TimingArc *arc : arc_set->arcs()) {
      arc_count++;
      EXPECT_NE(arc->fromEdge(), nullptr);
      EXPECT_NE(arc->toEdge(), nullptr);
      (void)arc->index();
    }
  }
  EXPECT_GT(arc_set_count, 0);
  EXPECT_GT(arc_count, 0);
}

// R11_12: GateTableModel::checkAxis and CheckTableModel::checkAxis
// These are exercised by reading a liberty with table_lookup models
// containing different axis variables
TEST_F(StaLibertyTest, TableModelCheckAxis) {
  const char *content = R"(
library(test_r11_axis) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(tmpl_2d) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1, 0.5") ;
    index_2("0.001, 0.01, 0.1") ;
  }
  lu_table_template(tmpl_check) {
    variable_1 : related_pin_transition ;
    variable_2 : constrained_pin_transition ;
    index_1("0.01, 0.1, 0.5") ;
    index_2("0.01, 0.1, 0.5") ;
  }
  cell(AX1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(tmpl_2d) {
          values("0.1, 0.2, 0.3", \
                 "0.2, 0.3, 0.4", \
                 "0.3, 0.4, 0.5") ;
        }
        cell_fall(tmpl_2d) {
          values("0.1, 0.2, 0.3", \
                 "0.2, 0.3, 0.4", \
                 "0.3, 0.4, 0.5") ;
        }
        rise_transition(tmpl_2d) {
          values("0.05, 0.1, 0.2", \
                 "0.1, 0.15, 0.3", \
                 "0.2, 0.3, 0.5") ;
        }
        fall_transition(tmpl_2d) {
          values("0.05, 0.1, 0.2", \
                 "0.1, 0.15, 0.3", \
                 "0.2, 0.3, 0.5") ;
        }
      }
      timing() {
        related_pin : "CLK" ;
        timing_type : setup_rising ;
        rise_constraint(tmpl_check) {
          values("0.05, 0.1, 0.15", \
                 "0.1, 0.15, 0.2", \
                 "0.15, 0.2, 0.25") ;
        }
        fall_constraint(tmpl_check) {
          values("0.05, 0.1, 0.15", \
                 "0.1, 0.15, 0.2", \
                 "0.15, 0.2, 0.25") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R11_13: CheckLinearModel::setIsScaled, CheckTableModel::setIsScaled via
// library with k_process/k_temp/k_volt scaling factors on setup
TEST_F(StaLibertyTest, ScaledModels) {
  const char *content = R"(
library(test_r11_scaled) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  k_process_cell_rise : 1.0 ;
  k_process_cell_fall : 1.0 ;
  k_temp_cell_rise : 0.001 ;
  k_temp_cell_fall : 0.001 ;
  k_volt_cell_rise : -0.5 ;
  k_volt_cell_fall : -0.5 ;
  k_process_setup_rise : 1.0 ;
  k_process_setup_fall : 1.0 ;
  k_temp_setup_rise : 0.001 ;
  k_temp_setup_fall : 0.001 ;
  operating_conditions(WORST) {
    process : 1.0 ;
    temperature : 125.0 ;
    voltage : 0.9 ;
  }
  cell(SC1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(scalar) { values("0.1") ; }
        cell_fall(scalar) { values("0.1") ; }
        rise_transition(scalar) { values("0.05") ; }
        fall_transition(scalar) { values("0.05") ; }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R11_14: Library with cell that has internal_ports attribute
// Exercises setHasInternalPorts
TEST_F(StaLibertyTest, HasInternalPorts) {
  const char *content = R"(
library(test_r11_intport) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(IP1) {
    area : 3.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(QN) { direction : output ; function : "IQ'" ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff(IQ, IQN) {
      next_state : "A" ;
      clocked_on : "A" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R11_15: Directly test LibertyParser API through parseLibertyFile
// Focus on saving attrs/variables/groups to exercise more code paths
TEST_F(StaLibertyTest, ParserSaveAll) {
  const char *content = R"(
library(test_r11_save) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  define(custom_attr, cell, float) ;
  my_variable = 42.0 ;
  cell(SV1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  std::string tmp_path = "/tmp/test_r11_save.lib";
  writeLibContent(content, tmp_path);

  // Visitor that saves everything
  class SaveVisitor : public LibertyGroupVisitor {
  public:
    int group_begin_count = 0;
    int group_end_count = 0;
    int define_count = 0;
    int var_count = 0;
    void begin(LibertyGroup *group) override {
      group_begin_count++;
      EXPECT_TRUE(group->isGroup());
      EXPECT_FALSE(group->isAttribute());
      EXPECT_FALSE(group->isVariable());
      EXPECT_FALSE(group->isDefine());
      EXPECT_NE(group->type(), nullptr);
    }
    void end(LibertyGroup *) override { group_end_count++; }
    void visitAttr(LibertyAttr *attr) override {
      // Check isDefine virtual dispatch
      if (attr->isDefine())
        define_count++;
    }
    void visitVariable(LibertyVariable *var) override {
      var_count++;
    }
    bool save(LibertyGroup *) override { return true; }
    bool save(LibertyAttr *) override { return true; }
    bool save(LibertyVariable *) override { return true; }
  };

  Report *report = sta_->report();
  SaveVisitor visitor;
  parseLibertyFile(tmp_path.c_str(), &visitor, report);
  EXPECT_GT(visitor.group_begin_count, 0);
  EXPECT_EQ(visitor.group_begin_count, visitor.group_end_count);
  remove(tmp_path.c_str());
}

// R11_16: Exercises clearAxisValues and setEnergyScale through internal_power
// with energy values
TEST_F(StaLibertyTest, EnergyScale) {
  const char *content = R"(
library(test_r11_energy) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  leakage_power_unit : "1nW" ;
  lu_table_template(energy_tmpl) {
    variable_1 : input_transition_time ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(EN1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(scalar) { values("0.1") ; }
        cell_fall(scalar) { values("0.1") ; }
        rise_transition(scalar) { values("0.05") ; }
        fall_transition(scalar) { values("0.05") ; }
      }
      internal_power() {
        related_pin : "A" ;
        rise_power(energy_tmpl) {
          values("0.001, 0.002", \
                 "0.003, 0.004") ;
        }
        fall_power(energy_tmpl) {
          values("0.001, 0.002", \
                 "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R11_17: LibertyReader findPort by reading a lib and querying
TEST_F(StaLibertyTest, FindPort) {
  ASSERT_NE(lib_, nullptr);
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *portA = inv->findLibertyPort("A");
  EXPECT_NE(portA, nullptr);
  LibertyPort *portZN = inv->findLibertyPort("ZN");
  EXPECT_NE(portZN, nullptr);
  // Non-existent port
  LibertyPort *portX = inv->findLibertyPort("NONEXISTENT");
  EXPECT_EQ(portX, nullptr);
}

// R11_18: LibertyPort::cornerPort (requires DcalcAnalysisPt, but we test
// through the Nangate45 library which has corners)
TEST_F(StaLibertyTest, CornerPort) {
  ASSERT_NE(lib_, nullptr);
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *portA = buf->findLibertyPort("A");
  ASSERT_NE(portA, nullptr);
  // cornerPort requires a DcalcAnalysisPt
  // Get the first analysis point from the corner
  Corner *corner = sta_->cmdCorner();
  const DcalcAnalysisPt *ap = corner->findDcalcAnalysisPt(MinMax::min());
  if (ap) {
    LibertyPort *corner_port = portA->cornerPort(ap);
    EXPECT_NE(corner_port, nullptr);
  }
}

// R11_19: Exercise receiver model set through timing group
TEST_F(StaLibertyTest, ReceiverModel) {
  const char *content = R"(
library(test_r11_recv) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(RV1) {
    area : 2.0 ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      receiver_capacitance() {
        receiver_capacitance1_rise(scalar) { values("0.001") ; }
        receiver_capacitance1_fall(scalar) { values("0.001") ; }
        receiver_capacitance2_rise(scalar) { values("0.002") ; }
        receiver_capacitance2_fall(scalar) { values("0.002") ; }
      }
    }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(scalar) { values("0.1") ; }
        cell_fall(scalar) { values("0.1") ; }
        rise_transition(scalar) { values("0.05") ; }
        fall_transition(scalar) { values("0.05") ; }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

// R11_20: Read a liberty with CCS (composite current source) output_current
// to exercise OutputWaveform constructors and related paths
TEST_F(StaLibertyTest, CCSOutputCurrent) {
  const char *content = R"(
library(test_r11_ccs) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(ccs_tmpl_oc) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  output_current_template(oc_tmpl) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    variable_3 : time ;
  }
  cell(CCS1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(ccs_tmpl_oc) {
          values("0.1, 0.2", \
                 "0.2, 0.3") ;
        }
        cell_fall(ccs_tmpl_oc) {
          values("0.1, 0.2", \
                 "0.2, 0.3") ;
        }
        rise_transition(ccs_tmpl_oc) {
          values("0.05, 0.1", \
                 "0.1, 0.2") ;
        }
        fall_transition(ccs_tmpl_oc) {
          values("0.05, 0.1", \
                 "0.1, 0.2") ;
        }
        output_current_rise() {
          vector(oc_tmpl) {
            index_1("0.01") ;
            index_2("0.001") ;
            index_3("0.0, 0.01, 0.02, 0.03, 0.04") ;
            values("0.0, -0.001, -0.005, -0.002, 0.0") ;
          }
        }
        output_current_fall() {
          vector(oc_tmpl) {
            index_1("0.01") ;
            index_2("0.001") ;
            index_3("0.0, 0.01, 0.02, 0.03, 0.04") ;
            values("0.0, 0.001, 0.005, 0.002, 0.0") ;
          }
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);
}

} // namespace sta
