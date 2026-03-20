#include <gtest/gtest.h>
#include <tcl.h>
#include "MinMax.hh"
#include "Transition.hh"
#include "Units.hh"
#include "Property.hh"
#include "ExceptionPath.hh"
#include "TimingRole.hh"
#include "Scene.hh"
#include "Sta.hh"
#include "Sdc.hh"
#include "ReportTcl.hh"
#include "RiseFallMinMax.hh"
#include "Variables.hh"
#include "LibertyClass.hh"
#include "Search.hh"
#include "Path.hh"
#include "PathGroup.hh"
#include "PathExpanded.hh"
#include "SearchPred.hh"
#include "SearchClass.hh"
#include "ClkNetwork.hh"
#include "VisitPathEnds.hh"
#include "search/CheckMinPulseWidths.hh"
#include "search/CheckMinPeriods.hh"
#include "search/CheckMaxSkews.hh"
#include "search/ClkSkew.hh"
#include "search/ClkInfo.hh"
#include "search/Tag.hh"
#include "search/PathEnum.hh"
#include "search/Genclks.hh"
#include "search/Levelize.hh"
#include "search/Sim.hh"
#include "Bfs.hh"
#include "search/WorstSlack.hh"
#include "search/ReportPath.hh"
#include "GraphDelayCalc.hh"
#include "Debug.hh"
#include "PowerClass.hh"
#include "search/CheckCapacitances.hh"
#include "search/CheckSlews.hh"
#include "search/CheckFanouts.hh"
#include "search/Crpr.hh"
#include "search/GatedClk.hh"
#include "search/ClkLatency.hh"
#include "search/FindRegister.hh"
#include "search/TagGroup.hh"
#include "search/MakeTimingModelPvt.hh"
#include "search/CheckTiming.hh"
#include "search/Latches.hh"
#include "Graph.hh"
#include "Liberty.hh"
#include "Network.hh"

namespace sta {


class SearchMinMaxTest : public ::testing::Test {};

TEST_F(SearchMinMaxTest, MinCompare) {
  // For min: value1 < value2 returns true
  EXPECT_TRUE(MinMax::min()->compare(1.0f, 2.0f));
  EXPECT_FALSE(MinMax::min()->compare(2.0f, 1.0f));
  EXPECT_FALSE(MinMax::min()->compare(1.0f, 1.0f));
}

TEST_F(SearchMinMaxTest, MaxCompare) {
  // For max: value1 > value2 returns true
  EXPECT_TRUE(MinMax::max()->compare(2.0f, 1.0f));
  EXPECT_FALSE(MinMax::max()->compare(1.0f, 2.0f));
  EXPECT_FALSE(MinMax::max()->compare(1.0f, 1.0f));
}

TEST_F(SearchMinMaxTest, MinMaxFunc) {
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(1.0f, 2.0f), 1.0f);
  EXPECT_FLOAT_EQ(MinMax::min()->minMax(2.0f, 1.0f), 1.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(1.0f, 2.0f), 2.0f);
  EXPECT_FLOAT_EQ(MinMax::max()->minMax(2.0f, 1.0f), 2.0f);
}

TEST_F(SearchMinMaxTest, FindByName) {
  EXPECT_EQ(MinMax::find("min"), MinMax::min());
  EXPECT_EQ(MinMax::find("max"), MinMax::max());
  EXPECT_EQ(MinMax::find("early"), MinMax::early());
  EXPECT_EQ(MinMax::find("late"), MinMax::late());
}

TEST_F(SearchMinMaxTest, FindByIndex) {
  EXPECT_EQ(MinMax::find(MinMax::minIndex()), MinMax::min());
  EXPECT_EQ(MinMax::find(MinMax::maxIndex()), MinMax::max());
}

TEST_F(SearchMinMaxTest, EarlyLateAliases) {
  EXPECT_EQ(MinMax::early(), MinMax::min());
  EXPECT_EQ(MinMax::late(), MinMax::max());
  EXPECT_EQ(MinMax::earlyIndex(), MinMax::minIndex());
  EXPECT_EQ(MinMax::lateIndex(), MinMax::maxIndex());
}

TEST_F(SearchMinMaxTest, RangeSize) {
  auto &range = MinMax::range();
  EXPECT_EQ(range.size(), 2u);
  auto &range_idx = MinMax::rangeIndex();
  EXPECT_EQ(range_idx.size(), 2u);
}

// MinMaxAll tests
class SearchMinMaxAllTest : public ::testing::Test {};

TEST_F(SearchMinMaxAllTest, MatchesMinMax) {
  EXPECT_TRUE(MinMaxAll::min()->matches(MinMax::min()));
  EXPECT_FALSE(MinMaxAll::min()->matches(MinMax::max()));
  EXPECT_TRUE(MinMaxAll::max()->matches(MinMax::max()));
  EXPECT_FALSE(MinMaxAll::max()->matches(MinMax::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMax::max()));
}

TEST_F(SearchMinMaxAllTest, MatchesMinMaxAll) {
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::min()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::max()));
  EXPECT_TRUE(MinMaxAll::all()->matches(MinMaxAll::all()));
}

TEST_F(SearchMinMaxAllTest, AllRange) {
  auto &range = MinMaxAll::all()->range();
  EXPECT_EQ(range.size(), 2u);
  EXPECT_EQ(range[0], MinMax::min());
  EXPECT_EQ(range[1], MinMax::max());
}

// Transition tests used in search
class SearchTransitionTest : public ::testing::Test {};

TEST_F(SearchTransitionTest, RiseFallSingletons) {
  EXPECT_NE(Transition::rise(), nullptr);
  EXPECT_NE(Transition::fall(), nullptr);
  EXPECT_NE(Transition::rise(), Transition::fall());
}

TEST_F(SearchTransitionTest, RiseFallMatch) {
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::rise()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::fall()));
}

TEST_F(SearchTransitionTest, SdfTransitions) {
  // All SDF transition types should have unique indices
  EXPECT_NE(Transition::rise()->sdfTripleIndex(),
            Transition::fall()->sdfTripleIndex());
  EXPECT_NE(Transition::tr0Z()->sdfTripleIndex(),
            Transition::trZ1()->sdfTripleIndex());
}

TEST_F(SearchTransitionTest, AsRiseFall) {
  EXPECT_EQ(Transition::rise()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::fall()->asRiseFall(), RiseFall::fall());
}

////////////////////////////////////////////////////////////////
// PropertyValue tests
////////////////////////////////////////////////////////////////

class PropertyValueTest : public ::testing::Test {};

TEST_F(PropertyValueTest, DefaultConstructor) {
  PropertyValue pv;
  EXPECT_EQ(pv.type(), PropertyValue::Type::none);
}

TEST_F(PropertyValueTest, StringConstructor) {
  PropertyValue pv("hello");
  EXPECT_EQ(pv.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv.stringValue(), "hello");
}

TEST_F(PropertyValueTest, StdStringConstructor) {
  std::string s("world");
  PropertyValue pv(s);
  EXPECT_EQ(pv.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv.stringValue(), "world");
}

TEST_F(PropertyValueTest, BoolConstructorTrue) {
  PropertyValue pv(true);
  EXPECT_EQ(pv.type(), PropertyValue::Type::bool_);
  EXPECT_TRUE(pv.boolValue());
}

TEST_F(PropertyValueTest, BoolConstructorFalse) {
  PropertyValue pv(false);
  EXPECT_EQ(pv.type(), PropertyValue::Type::bool_);
  EXPECT_FALSE(pv.boolValue());
}

TEST_F(PropertyValueTest, FloatConstructor) {
  Unit time_unit(1.0f, "s", 3);
  PropertyValue pv(3.14f, &time_unit);
  EXPECT_EQ(pv.type(), PropertyValue::Type::float_);
  EXPECT_FLOAT_EQ(pv.floatValue(), 3.14f);
  std::string value_text = pv.to_string(nullptr);
  EXPECT_FALSE(value_text.empty());
}

TEST_F(PropertyValueTest, NullPinConstructor) {
  const Pin *pin = nullptr;
  PropertyValue pv(pin);
  EXPECT_EQ(pv.type(), PropertyValue::Type::pin);
  EXPECT_EQ(pv.pin(), nullptr);
}

TEST_F(PropertyValueTest, NullNetConstructor) {
  const Net *net = nullptr;
  PropertyValue pv(net);
  EXPECT_EQ(pv.type(), PropertyValue::Type::net);
  EXPECT_EQ(pv.net(), nullptr);
}

TEST_F(PropertyValueTest, NullInstanceConstructor) {
  const Instance *inst = nullptr;
  PropertyValue pv(inst);
  EXPECT_EQ(pv.type(), PropertyValue::Type::instance);
  EXPECT_EQ(pv.instance(), nullptr);
}

TEST_F(PropertyValueTest, NullCellConstructor) {
  const Cell *cell = nullptr;
  PropertyValue pv(cell);
  EXPECT_EQ(pv.type(), PropertyValue::Type::cell);
  EXPECT_EQ(pv.cell(), nullptr);
}

TEST_F(PropertyValueTest, NullPortConstructor) {
  const Port *port = nullptr;
  PropertyValue pv(port);
  EXPECT_EQ(pv.type(), PropertyValue::Type::port);
  EXPECT_EQ(pv.port(), nullptr);
}

TEST_F(PropertyValueTest, NullLibraryConstructor) {
  const Library *lib = nullptr;
  PropertyValue pv(lib);
  EXPECT_EQ(pv.type(), PropertyValue::Type::library);
  EXPECT_EQ(pv.library(), nullptr);
}

TEST_F(PropertyValueTest, NullLibertyLibraryConstructor) {
  const LibertyLibrary *lib = nullptr;
  PropertyValue pv(lib);
  EXPECT_EQ(pv.type(), PropertyValue::Type::liberty_library);
  EXPECT_EQ(pv.libertyLibrary(), nullptr);
}

TEST_F(PropertyValueTest, NullLibertyCellConstructor) {
  const LibertyCell *cell = nullptr;
  PropertyValue pv(cell);
  EXPECT_EQ(pv.type(), PropertyValue::Type::liberty_cell);
  EXPECT_EQ(pv.libertyCell(), nullptr);
}

TEST_F(PropertyValueTest, NullLibertyPortConstructor) {
  const LibertyPort *port = nullptr;
  PropertyValue pv(port);
  EXPECT_EQ(pv.type(), PropertyValue::Type::liberty_port);
  EXPECT_EQ(pv.libertyPort(), nullptr);
}

TEST_F(PropertyValueTest, NullClockConstructor) {
  const Clock *clk = nullptr;
  PropertyValue pv(clk);
  EXPECT_EQ(pv.type(), PropertyValue::Type::clk);
  EXPECT_EQ(pv.clock(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorString) {
  PropertyValue pv1("copy_test");
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv2.stringValue(), "copy_test");
}

TEST_F(PropertyValueTest, CopyConstructorFloat) {
  PropertyValue pv1(2.718f, nullptr);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::float_);
  EXPECT_FLOAT_EQ(pv2.floatValue(), 2.718f);
}

TEST_F(PropertyValueTest, CopyConstructorBool) {
  PropertyValue pv1(true);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::bool_);
  EXPECT_TRUE(pv2.boolValue());
}

TEST_F(PropertyValueTest, CopyConstructorNone) {
  PropertyValue pv1;
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::none);
}

TEST_F(PropertyValueTest, CopyConstructorLibrary) {
  const Library *lib = nullptr;
  PropertyValue pv1(lib);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::library);
  EXPECT_EQ(pv2.library(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorCell) {
  const Cell *cell = nullptr;
  PropertyValue pv1(cell);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::cell);
  EXPECT_EQ(pv2.cell(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorPort) {
  const Port *port = nullptr;
  PropertyValue pv1(port);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::port);
  EXPECT_EQ(pv2.port(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorLibertyLibrary) {
  const LibertyLibrary *lib = nullptr;
  PropertyValue pv1(lib);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_library);
  EXPECT_EQ(pv2.libertyLibrary(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorLibertyCell) {
  const LibertyCell *cell = nullptr;
  PropertyValue pv1(cell);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_cell);
  EXPECT_EQ(pv2.libertyCell(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorLibertyPort) {
  const LibertyPort *port = nullptr;
  PropertyValue pv1(port);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_port);
  EXPECT_EQ(pv2.libertyPort(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorInstance) {
  const Instance *inst = nullptr;
  PropertyValue pv1(inst);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::instance);
  EXPECT_EQ(pv2.instance(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorPin) {
  const Pin *pin = nullptr;
  PropertyValue pv1(pin);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pin);
  EXPECT_EQ(pv2.pin(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorNet) {
  const Net *net = nullptr;
  PropertyValue pv1(net);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::net);
  EXPECT_EQ(pv2.net(), nullptr);
}

TEST_F(PropertyValueTest, CopyConstructorClock) {
  const Clock *clk = nullptr;
  PropertyValue pv1(clk);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::clk);
  EXPECT_EQ(pv2.clock(), nullptr);
}

TEST_F(PropertyValueTest, MoveConstructorString) {
  PropertyValue pv1("move_test");
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv2.stringValue(), "move_test");
}

TEST_F(PropertyValueTest, MoveConstructorFloat) {
  PropertyValue pv1(1.414f, nullptr);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::float_);
  EXPECT_FLOAT_EQ(pv2.floatValue(), 1.414f);
}

TEST_F(PropertyValueTest, MoveConstructorBool) {
  PropertyValue pv1(false);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::bool_);
  EXPECT_FALSE(pv2.boolValue());
}

TEST_F(PropertyValueTest, MoveConstructorNone) {
  PropertyValue pv1;
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::none);
}

TEST_F(PropertyValueTest, MoveConstructorLibrary) {
  const Library *lib = nullptr;
  PropertyValue pv1(lib);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::library);
}

TEST_F(PropertyValueTest, MoveConstructorCell) {
  const Cell *cell = nullptr;
  PropertyValue pv1(cell);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::cell);
}

TEST_F(PropertyValueTest, MoveConstructorPort) {
  const Port *port = nullptr;
  PropertyValue pv1(port);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::port);
}

TEST_F(PropertyValueTest, MoveConstructorLibertyLibrary) {
  const LibertyLibrary *lib = nullptr;
  PropertyValue pv1(lib);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_library);
}

TEST_F(PropertyValueTest, MoveConstructorLibertyCell) {
  const LibertyCell *cell = nullptr;
  PropertyValue pv1(cell);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_cell);
}

TEST_F(PropertyValueTest, MoveConstructorLibertyPort) {
  const LibertyPort *port = nullptr;
  PropertyValue pv1(port);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_port);
}

TEST_F(PropertyValueTest, MoveConstructorInstance) {
  const Instance *inst = nullptr;
  PropertyValue pv1(inst);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::instance);
}

TEST_F(PropertyValueTest, MoveConstructorPin) {
  const Pin *pin = nullptr;
  PropertyValue pv1(pin);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pin);
}

TEST_F(PropertyValueTest, MoveConstructorNet) {
  const Net *net = nullptr;
  PropertyValue pv1(net);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::net);
}

TEST_F(PropertyValueTest, MoveConstructorClock) {
  const Clock *clk = nullptr;
  PropertyValue pv1(clk);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::clk);
}

TEST_F(PropertyValueTest, CopyAssignmentString) {
  PropertyValue pv1("assign_test");
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv2.stringValue(), "assign_test");
}

TEST_F(PropertyValueTest, CopyAssignmentFloat) {
  PropertyValue pv1(9.81f, nullptr);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::float_);
  EXPECT_FLOAT_EQ(pv2.floatValue(), 9.81f);
}

TEST_F(PropertyValueTest, CopyAssignmentBool) {
  PropertyValue pv1(true);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::bool_);
  EXPECT_TRUE(pv2.boolValue());
}

TEST_F(PropertyValueTest, CopyAssignmentNone) {
  PropertyValue pv1;
  PropertyValue pv2("replace_me");
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::none);
}

TEST_F(PropertyValueTest, CopyAssignmentLibrary) {
  const Library *lib = nullptr;
  PropertyValue pv1(lib);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::library);
}

TEST_F(PropertyValueTest, CopyAssignmentCell) {
  const Cell *cell = nullptr;
  PropertyValue pv1(cell);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::cell);
}

TEST_F(PropertyValueTest, CopyAssignmentPort) {
  const Port *port = nullptr;
  PropertyValue pv1(port);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::port);
}

TEST_F(PropertyValueTest, CopyAssignmentLibertyLibrary) {
  const LibertyLibrary *lib = nullptr;
  PropertyValue pv1(lib);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_library);
}

TEST_F(PropertyValueTest, CopyAssignmentLibertyCell) {
  const LibertyCell *cell = nullptr;
  PropertyValue pv1(cell);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_cell);
}

TEST_F(PropertyValueTest, CopyAssignmentLibertyPort) {
  const LibertyPort *port = nullptr;
  PropertyValue pv1(port);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_port);
}

TEST_F(PropertyValueTest, CopyAssignmentInstance) {
  const Instance *inst = nullptr;
  PropertyValue pv1(inst);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::instance);
}

TEST_F(PropertyValueTest, CopyAssignmentPin) {
  const Pin *pin = nullptr;
  PropertyValue pv1(pin);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pin);
}

TEST_F(PropertyValueTest, CopyAssignmentNet) {
  const Net *net = nullptr;
  PropertyValue pv1(net);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::net);
}

TEST_F(PropertyValueTest, CopyAssignmentClock) {
  const Clock *clk = nullptr;
  PropertyValue pv1(clk);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::clk);
}

TEST_F(PropertyValueTest, MoveAssignmentString) {
  PropertyValue pv1("move_assign");
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv2.stringValue(), "move_assign");
}

TEST_F(PropertyValueTest, MoveAssignmentFloat) {
  PropertyValue pv1(6.28f, nullptr);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::float_);
  EXPECT_FLOAT_EQ(pv2.floatValue(), 6.28f);
}

TEST_F(PropertyValueTest, MoveAssignmentBool) {
  PropertyValue pv1(false);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::bool_);
  EXPECT_FALSE(pv2.boolValue());
}

TEST_F(PropertyValueTest, MoveAssignmentNone) {
  PropertyValue pv1;
  PropertyValue pv2("stuff");
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::none);
}

TEST_F(PropertyValueTest, MoveAssignmentLibrary) {
  const Library *lib = nullptr;
  PropertyValue pv1(lib);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::library);
}

TEST_F(PropertyValueTest, MoveAssignmentCell) {
  const Cell *cell = nullptr;
  PropertyValue pv1(cell);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::cell);
}

TEST_F(PropertyValueTest, MoveAssignmentPort) {
  const Port *port = nullptr;
  PropertyValue pv1(port);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::port);
}

TEST_F(PropertyValueTest, MoveAssignmentLibertyLibrary) {
  const LibertyLibrary *lib = nullptr;
  PropertyValue pv1(lib);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_library);
}

TEST_F(PropertyValueTest, MoveAssignmentLibertyCell) {
  const LibertyCell *cell = nullptr;
  PropertyValue pv1(cell);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_cell);
}

TEST_F(PropertyValueTest, MoveAssignmentLibertyPort) {
  const LibertyPort *port = nullptr;
  PropertyValue pv1(port);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::liberty_port);
}

TEST_F(PropertyValueTest, MoveAssignmentInstance) {
  const Instance *inst = nullptr;
  PropertyValue pv1(inst);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::instance);
}

TEST_F(PropertyValueTest, MoveAssignmentPin) {
  const Pin *pin = nullptr;
  PropertyValue pv1(pin);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pin);
}

TEST_F(PropertyValueTest, MoveAssignmentNet) {
  const Net *net = nullptr;
  PropertyValue pv1(net);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::net);
}

TEST_F(PropertyValueTest, MoveAssignmentClock) {
  const Clock *clk = nullptr;
  PropertyValue pv1(clk);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::clk);
}

// Test type-checking exceptions
TEST_F(PropertyValueTest, StringValueThrowsOnWrongType) {
  PropertyValue pv(true);
  EXPECT_THROW(pv.stringValue(), Exception);
}

TEST_F(PropertyValueTest, FloatValueThrowsOnWrongType) {
  PropertyValue pv("not_a_float");
  EXPECT_THROW(pv.floatValue(), Exception);
}

TEST_F(PropertyValueTest, BoolValueThrowsOnWrongType) {
  PropertyValue pv("not_a_bool");
  EXPECT_THROW(pv.boolValue(), Exception);
}

// Test PinSeq constructor
TEST_F(PropertyValueTest, PinSeqConstructor) {
  PinSeq *pins = new PinSeq;
  PropertyValue pv(pins);
  EXPECT_EQ(pv.type(), PropertyValue::Type::pins);
  EXPECT_EQ(pv.pins(), pins);
}

// Test ClockSeq constructor
TEST_F(PropertyValueTest, ClockSeqConstructor) {
  ClockSeq *clks = new ClockSeq;
  PropertyValue pv(clks);
  EXPECT_EQ(pv.type(), PropertyValue::Type::clks);
  EXPECT_NE(pv.clocks(), nullptr);
}

// Test ConstPathSeq constructor
TEST_F(PropertyValueTest, ConstPathSeqConstructor) {
  ConstPathSeq *paths = new ConstPathSeq;
  PropertyValue pv(paths);
  EXPECT_EQ(pv.type(), PropertyValue::Type::paths);
  EXPECT_NE(pv.paths(), nullptr);
}

// Test PwrActivity constructor
TEST_F(PropertyValueTest, PwrActivityConstructor) {
  PwrActivity activity;
  PropertyValue pv(&activity);
  EXPECT_EQ(pv.type(), PropertyValue::Type::pwr_activity);
}

// Copy constructor for pins
TEST_F(PropertyValueTest, CopyConstructorPins) {
  PinSeq *pins = new PinSeq;
  PropertyValue pv1(pins);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pins);
  // Should be a separate copy
  EXPECT_NE(pv2.pins(), pv1.pins());
}

// Copy constructor for clocks
TEST_F(PropertyValueTest, CopyConstructorClocks) {
  ClockSeq *clks = new ClockSeq;
  PropertyValue pv1(clks);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::clks);
  EXPECT_NE(pv2.clocks(), pv1.clocks());
}

// Copy constructor for paths
TEST_F(PropertyValueTest, CopyConstructorPaths) {
  ConstPathSeq *paths = new ConstPathSeq;
  PropertyValue pv1(paths);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::paths);
  EXPECT_NE(pv2.paths(), pv1.paths());
}

// Copy constructor for PwrActivity
TEST_F(PropertyValueTest, CopyConstructorPwrActivity) {
  PwrActivity activity;
  PropertyValue pv1(&activity);
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pwr_activity);
}

// Move constructor for pins
TEST_F(PropertyValueTest, MoveConstructorPins) {
  PinSeq *pins = new PinSeq;
  PropertyValue pv1(pins);
  PinSeq *orig_pins = pv1.pins();
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pins);
  EXPECT_EQ(pv2.pins(), orig_pins);
}

// Move constructor for clocks
TEST_F(PropertyValueTest, MoveConstructorClocks) {
  ClockSeq *clks = new ClockSeq;
  PropertyValue pv1(clks);
  ClockSeq *orig_clks = pv1.clocks();
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::clks);
  EXPECT_EQ(pv2.clocks(), orig_clks);
}

// Move constructor for paths
TEST_F(PropertyValueTest, MoveConstructorPaths) {
  ConstPathSeq *paths = new ConstPathSeq;
  PropertyValue pv1(paths);
  ConstPathSeq *orig_paths = pv1.paths();
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::paths);
  EXPECT_EQ(pv2.paths(), orig_paths);
}

// Move constructor for PwrActivity
TEST_F(PropertyValueTest, MoveConstructorPwrActivity) {
  PwrActivity activity;
  PropertyValue pv1(&activity);
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pwr_activity);
}

// Copy assignment for pins
TEST_F(PropertyValueTest, CopyAssignmentPins) {
  PinSeq *pins = new PinSeq;
  PropertyValue pv1(pins);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pins);
}

// Copy assignment for clocks
TEST_F(PropertyValueTest, CopyAssignmentClocks) {
  ClockSeq *clks = new ClockSeq;
  PropertyValue pv1(clks);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::clks);
}

// Copy assignment for paths
TEST_F(PropertyValueTest, CopyAssignmentPaths) {
  ConstPathSeq *paths = new ConstPathSeq;
  PropertyValue pv1(paths);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::paths);
}

// Copy assignment for PwrActivity
TEST_F(PropertyValueTest, CopyAssignmentPwrActivity) {
  PwrActivity activity;
  PropertyValue pv1(&activity);
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pwr_activity);
}

// Move assignment for pins
TEST_F(PropertyValueTest, MoveAssignmentPins) {
  PinSeq *pins = new PinSeq;
  PropertyValue pv1(pins);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pins);
}

// Move assignment for clocks
TEST_F(PropertyValueTest, MoveAssignmentClocks) {
  ClockSeq *clks = new ClockSeq;
  PropertyValue pv1(clks);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::clks);
}

// Move assignment for paths
TEST_F(PropertyValueTest, MoveAssignmentPaths) {
  ConstPathSeq *paths = new ConstPathSeq;
  PropertyValue pv1(paths);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::paths);
}

// Move assignment for PwrActivity
TEST_F(PropertyValueTest, MoveAssignmentPwrActivity) {
  PwrActivity activity;
  PropertyValue pv1(&activity);
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::pwr_activity);
}

// to_string for bool values
TEST_F(PropertyValueTest, ToStringBoolTrue) {
  PropertyValue pv(true);
  EXPECT_EQ(pv.to_string(nullptr), "1");
}

TEST_F(PropertyValueTest, ToStringBoolFalse) {
  PropertyValue pv(false);
  EXPECT_EQ(pv.to_string(nullptr), "0");
}

// to_string for string values
TEST_F(PropertyValueTest, ToStringString) {
  PropertyValue pv("test_str");
  EXPECT_EQ(pv.to_string(nullptr), "test_str");
}

// to_string for types that return empty
TEST_F(PropertyValueTest, ToStringNone) {
  PropertyValue pv;
  EXPECT_EQ(pv.to_string(nullptr), "");
}

TEST_F(PropertyValueTest, ToStringPins) {
  PinSeq *pins = new PinSeq;
  PropertyValue pv(pins);
  EXPECT_EQ(pv.to_string(nullptr), "");
}

TEST_F(PropertyValueTest, ToStringClocks) {
  ClockSeq *clks = new ClockSeq;
  PropertyValue pv(clks);
  EXPECT_EQ(pv.to_string(nullptr), "");
}

TEST_F(PropertyValueTest, ToStringPaths) {
  ConstPathSeq *paths = new ConstPathSeq;
  PropertyValue pv(paths);
  EXPECT_EQ(pv.to_string(nullptr), "");
}

TEST_F(PropertyValueTest, ToStringPwrActivity) {
  PwrActivity activity;
  PropertyValue pv(&activity);
  EXPECT_EQ(pv.to_string(nullptr), "");
}

////////////////////////////////////////////////////////////////
// ExceptionPath tests
////////////////////////////////////////////////////////////////

class ExceptionPathTest : public ::testing::Test {
protected:
  void SetUp() override {
    initSta();
  }
};

// FalsePath
TEST_F(ExceptionPathTest, FalsePathBasic) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp.isFalse());
  EXPECT_FALSE(fp.isLoop());
  EXPECT_FALSE(fp.isMultiCycle());
  EXPECT_FALSE(fp.isPathDelay());
  EXPECT_FALSE(fp.isGroupPath());
  EXPECT_FALSE(fp.isFilter());
  EXPECT_EQ(fp.type(), ExceptionPathType::false_path);
  EXPECT_EQ(fp.minMax(), MinMaxAll::all());
  EXPECT_EQ(fp.from(), nullptr);
  EXPECT_EQ(fp.thrus(), nullptr);
  EXPECT_EQ(fp.to(), nullptr);
}

TEST_F(ExceptionPathTest, FalsePathTypeString) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_EQ(fp.typePriority(), ExceptionPath::falsePathPriority());
}

TEST_F(ExceptionPathTest, FalsePathTighterThan) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  // FalsePath tighterThan always returns false
  EXPECT_FALSE(fp1.tighterThan(&fp2));
}

TEST_F(ExceptionPathTest, FalsePathMatches) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp.matches(MinMax::min(), false));
  EXPECT_TRUE(fp.matches(MinMax::max(), false));
}

TEST_F(ExceptionPathTest, FalsePathMatchesMinOnly) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::min(), true, nullptr);
  EXPECT_TRUE(fp.matches(MinMax::min(), false));
  EXPECT_FALSE(fp.matches(MinMax::max(), false));
}

TEST_F(ExceptionPathTest, FalsePathMergeable) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp1.mergeable(&fp2));
}

TEST_F(ExceptionPathTest, FalsePathOverrides) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp1.overrides(&fp2));
}

TEST_F(ExceptionPathTest, FalsePathClone) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, "test comment");
  ExceptionPath *clone = fp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_TRUE(clone->isFalse());
  EXPECT_EQ(clone->minMax(), MinMaxAll::all());
  delete clone;
}

// LoopPath
TEST_F(ExceptionPathTest, LoopPathBasic) {
  LoopPath lp(nullptr, true);
  EXPECT_TRUE(lp.isFalse());
  EXPECT_TRUE(lp.isLoop());
  EXPECT_FALSE(lp.isMultiCycle());
  EXPECT_EQ(lp.type(), ExceptionPathType::loop);
}

TEST_F(ExceptionPathTest, LoopPathNotMergeable) {
  LoopPath lp1(nullptr, true);
  LoopPath lp2(nullptr, true);
  EXPECT_FALSE(lp1.mergeable(&lp2));
}

// PathDelay
TEST_F(ExceptionPathTest, PathDelayBasic) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               10.0e-9f, true, nullptr);
  EXPECT_TRUE(pd.isPathDelay());
  EXPECT_FALSE(pd.isFalse());
  EXPECT_FALSE(pd.isMultiCycle());
  EXPECT_EQ(pd.type(), ExceptionPathType::path_delay);
  EXPECT_FLOAT_EQ(pd.delay(), 10.0e-9f);
  EXPECT_FALSE(pd.ignoreClkLatency());
  EXPECT_FALSE(pd.breakPath());
}

TEST_F(ExceptionPathTest, PathDelayWithFlags) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::min(), true, true,
               5.0e-9f, true, nullptr);
  EXPECT_TRUE(pd.ignoreClkLatency());
  EXPECT_TRUE(pd.breakPath());
  EXPECT_FLOAT_EQ(pd.delay(), 5.0e-9f);
}

TEST_F(ExceptionPathTest, PathDelayTypePriority) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), false, false,
               0.0f, true, nullptr);
  EXPECT_EQ(pd.typePriority(), ExceptionPath::pathDelayPriority());
}

TEST_F(ExceptionPathTest, PathDelayTighterThanMax) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                5.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                10.0e-9f, true, nullptr);
  // For max, tighter means smaller delay
  EXPECT_TRUE(pd1.tighterThan(&pd2));
  EXPECT_FALSE(pd2.tighterThan(&pd1));
}

TEST_F(ExceptionPathTest, PathDelayTighterThanMin) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::min(), false, false,
                10.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::min(), false, false,
                5.0e-9f, true, nullptr);
  // For min, tighter means larger delay
  EXPECT_TRUE(pd1.tighterThan(&pd2));
  EXPECT_FALSE(pd2.tighterThan(&pd1));
}

TEST_F(ExceptionPathTest, PathDelayClone) {
  PathDelay pd(nullptr, nullptr, nullptr, MinMax::max(), true, true,
               7.0e-9f, true, nullptr);
  ExceptionPath *clone = pd.clone(nullptr, nullptr, nullptr, true);
  EXPECT_TRUE(clone->isPathDelay());
  EXPECT_FLOAT_EQ(clone->delay(), 7.0e-9f);
  EXPECT_TRUE(clone->ignoreClkLatency());
  EXPECT_TRUE(clone->breakPath());
  delete clone;
}

TEST_F(ExceptionPathTest, PathDelayOverrides) {
  PathDelay pd1(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                5.0e-9f, true, nullptr);
  PathDelay pd2(nullptr, nullptr, nullptr, MinMax::max(), false, false,
                10.0e-9f, true, nullptr);
  EXPECT_TRUE(pd1.overrides(&pd2));
}

// MultiCyclePath
TEST_F(ExceptionPathTest, MultiCyclePathBasic) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  EXPECT_TRUE(mcp.isMultiCycle());
  EXPECT_FALSE(mcp.isFalse());
  EXPECT_FALSE(mcp.isPathDelay());
  EXPECT_EQ(mcp.type(), ExceptionPathType::multi_cycle);
  EXPECT_EQ(mcp.pathMultiplier(), 3);
  EXPECT_TRUE(mcp.useEndClk());
}

TEST_F(ExceptionPathTest, MultiCyclePathTypePriority) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     false, 2, true, nullptr);
  EXPECT_EQ(mcp.typePriority(), ExceptionPath::multiCyclePathPriority());
}

TEST_F(ExceptionPathTest, MultiCyclePathMultiplierAll) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  // When min_max_ is all and min_max arg is min, multiplier is 0
  EXPECT_EQ(mcp.pathMultiplier(MinMax::min()), 0);
  // For max, returns the actual multiplier
  EXPECT_EQ(mcp.pathMultiplier(MinMax::max()), 3);
}

TEST_F(ExceptionPathTest, MultiCyclePathMultiplierSpecific) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::max(),
                     true, 5, true, nullptr);
  EXPECT_EQ(mcp.pathMultiplier(MinMax::min()), 5);
  EXPECT_EQ(mcp.pathMultiplier(MinMax::max()), 5);
}

TEST_F(ExceptionPathTest, MultiCyclePathPriorityAll) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  int base_priority = mcp.priority();
  // priority(min_max) returns priority_ + 1 when min_max_ == all
  EXPECT_EQ(mcp.priority(MinMax::min()), base_priority + 1);
  EXPECT_EQ(mcp.priority(MinMax::max()), base_priority + 1);
}

TEST_F(ExceptionPathTest, MultiCyclePathPrioritySpecific) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::max(),
                     true, 3, true, nullptr);
  int base_priority = mcp.priority();
  // priority(min_max) returns priority_ + 2 when min_max_ matches
  EXPECT_EQ(mcp.priority(MinMax::max()), base_priority + 2);
  // Returns base priority when doesn't match
  EXPECT_EQ(mcp.priority(MinMax::min()), base_priority);
}

TEST_F(ExceptionPathTest, MultiCyclePathMatchesAll) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::all(),
                     true, 3, true, nullptr);
  EXPECT_TRUE(mcp.matches(MinMax::min(), false));
  EXPECT_TRUE(mcp.matches(MinMax::max(), false));
  EXPECT_TRUE(mcp.matches(MinMax::min(), true));
  EXPECT_TRUE(mcp.matches(MinMax::max(), true));
}

TEST_F(ExceptionPathTest, MultiCyclePathMatchesMaxSetup) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::max(),
                     true, 3, true, nullptr);
  EXPECT_TRUE(mcp.matches(MinMax::max(), false));
  EXPECT_TRUE(mcp.matches(MinMax::max(), true));
  // For min path, not exact: should still match because multicycle setup
  // affects hold checks
  EXPECT_TRUE(mcp.matches(MinMax::min(), false));
  // For min exact: should NOT match
  EXPECT_FALSE(mcp.matches(MinMax::min(), true));
}

TEST_F(ExceptionPathTest, MultiCyclePathTighterThan) {
  MultiCyclePath mcp1(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 2, true, nullptr);
  MultiCyclePath mcp2(nullptr, nullptr, nullptr, MinMaxAll::all(),
                      true, 5, true, nullptr);
  EXPECT_TRUE(mcp1.tighterThan(&mcp2));
  EXPECT_FALSE(mcp2.tighterThan(&mcp1));
}

TEST_F(ExceptionPathTest, MultiCyclePathClone) {
  MultiCyclePath mcp(nullptr, nullptr, nullptr, MinMaxAll::max(),
                     true, 4, true, nullptr);
  ExceptionPath *clone = mcp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_TRUE(clone->isMultiCycle());
  EXPECT_EQ(clone->pathMultiplier(), 4);
  EXPECT_TRUE(clone->useEndClk());
  delete clone;
}

// FilterPath
TEST_F(ExceptionPathTest, FilterPathBasic) {
  FilterPath fp(nullptr, nullptr, nullptr, true);
  EXPECT_TRUE(fp.isFilter());
  EXPECT_FALSE(fp.isFalse());
  EXPECT_FALSE(fp.isPathDelay());
  EXPECT_EQ(fp.type(), ExceptionPathType::filter);
}

TEST_F(ExceptionPathTest, FilterPathTypePriority) {
  FilterPath fp(nullptr, nullptr, nullptr, true);
  EXPECT_EQ(fp.typePriority(), ExceptionPath::filterPathPriority());
}

TEST_F(ExceptionPathTest, FilterPathNotMergeable) {
  FilterPath fp1(nullptr, nullptr, nullptr, true);
  FilterPath fp2(nullptr, nullptr, nullptr, true);
  EXPECT_FALSE(fp1.mergeable(&fp2));
}

TEST_F(ExceptionPathTest, FilterPathNotOverrides) {
  FilterPath fp1(nullptr, nullptr, nullptr, true);
  FilterPath fp2(nullptr, nullptr, nullptr, true);
  EXPECT_FALSE(fp1.overrides(&fp2));
}

TEST_F(ExceptionPathTest, FilterPathTighterThan) {
  FilterPath fp1(nullptr, nullptr, nullptr, true);
  FilterPath fp2(nullptr, nullptr, nullptr, true);
  EXPECT_FALSE(fp1.tighterThan(&fp2));
}

TEST_F(ExceptionPathTest, FilterPathResetMatch) {
  FilterPath fp(nullptr, nullptr, nullptr, true);
  EXPECT_FALSE(fp.resetMatch(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr));
}

TEST_F(ExceptionPathTest, FilterPathClone) {
  FilterPath fp(nullptr, nullptr, nullptr, true);
  ExceptionPath *clone = fp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_TRUE(clone->isFilter());
  delete clone;
}

// GroupPath
TEST_F(ExceptionPathTest, GroupPathBasic) {
  GroupPath gp("group1", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp.isGroupPath());
  EXPECT_FALSE(gp.isFalse());
  EXPECT_FALSE(gp.isPathDelay());
  EXPECT_EQ(gp.type(), ExceptionPathType::group_path);
  EXPECT_STREQ(gp.name(), "group1");
  EXPECT_FALSE(gp.isDefault());
}

TEST_F(ExceptionPathTest, GroupPathDefault) {
  GroupPath gp("default_group", true, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_TRUE(gp.isDefault());
  EXPECT_STREQ(gp.name(), "default_group");
}

TEST_F(ExceptionPathTest, GroupPathTypePriority) {
  GroupPath gp("gp", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_EQ(gp.typePriority(), ExceptionPath::groupPathPriority());
}

TEST_F(ExceptionPathTest, GroupPathTighterThan) {
  GroupPath gp1("gp1", false, nullptr, nullptr, nullptr, true, nullptr);
  GroupPath gp2("gp2", false, nullptr, nullptr, nullptr, true, nullptr);
  EXPECT_FALSE(gp1.tighterThan(&gp2));
}

TEST_F(ExceptionPathTest, GroupPathClone) {
  GroupPath gp("gp_clone", true, nullptr, nullptr, nullptr, true, "comment");
  ExceptionPath *clone = gp.clone(nullptr, nullptr, nullptr, true);
  EXPECT_TRUE(clone->isGroupPath());
  EXPECT_STREQ(clone->name(), "gp_clone");
  EXPECT_TRUE(clone->isDefault());
  delete clone;
}

// ExceptionPath general
TEST_F(ExceptionPathTest, PriorityValues) {
  EXPECT_GT(ExceptionPath::falsePathPriority(), ExceptionPath::pathDelayPriority());
  EXPECT_GT(ExceptionPath::pathDelayPriority(), ExceptionPath::multiCyclePathPriority());
  EXPECT_GT(ExceptionPath::multiCyclePathPriority(), ExceptionPath::filterPathPriority());
  EXPECT_GT(ExceptionPath::filterPathPriority(), ExceptionPath::groupPathPriority());
}

TEST_F(ExceptionPathTest, FromThruToPriority) {
  // No from/thru/to
  EXPECT_EQ(ExceptionPath::fromThruToPriority(nullptr, nullptr, nullptr), 0);
}

TEST_F(ExceptionPathTest, SetId) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_EQ(fp.id(), 0u);
  fp.setId(42);
  EXPECT_EQ(fp.id(), 42u);
}

TEST_F(ExceptionPathTest, SetPriority) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  int orig_priority = fp.priority();
  fp.setPriority(9999);
  EXPECT_EQ(fp.priority(), 9999);
  fp.setPriority(orig_priority);
}

TEST_F(ExceptionPathTest, FirstPtNone) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_EQ(fp.firstPt(), nullptr);
}

TEST_F(ExceptionPathTest, FirstState) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState *state = fp.firstState();
  EXPECT_NE(state, nullptr);
  // Should be complete since no from/thru/to
  EXPECT_TRUE(state->isComplete());
}

TEST_F(ExceptionPathTest, Hash) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  // Same structure should produce same hash
  EXPECT_EQ(fp1.hash(), fp2.hash());
}

TEST_F(ExceptionPathTest, MergeablePts) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp1.mergeablePts(&fp2));
}

TEST_F(ExceptionPathTest, IntersectsPts) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_TRUE(fp1.intersectsPts(&fp2, nullptr));
}

// ExceptionState tests
TEST_F(ExceptionPathTest, ExceptionStateBasic) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState *state = fp.firstState();
  EXPECT_EQ(state->exception(), &fp);
  EXPECT_EQ(state->nextThru(), nullptr);
  EXPECT_EQ(state->index(), 0);
}

TEST_F(ExceptionPathTest, ExceptionStateHash) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionState *state = fp.firstState();
  // Hash should be deterministic
  size_t h = state->hash();
  EXPECT_EQ(h, state->hash());
}

TEST_F(ExceptionPathTest, ExceptionStateLess) {
  FalsePath fp1(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  fp1.setId(1);
  FalsePath fp2(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  fp2.setId(2);
  ExceptionState *s1 = fp1.firstState();
  ExceptionState *s2 = fp2.firstState();
  // state1 with lower id should be less
  ExceptionStateLess less;
  EXPECT_TRUE(less(s1, s2));
  EXPECT_FALSE(less(s2, s1));
}

// EmptyExceptionPt
TEST_F(ExceptionPathTest, EmptyExceptionPtWhat) {
  EmptyExpceptionPt e;
  EXPECT_STREQ(e.what(), "empty exception from/through/to.");
}

TEST_F(ExceptionPathTest, CheckFromThrusToWithNulls) {
  // nullptr from, thrus, to - should not throw
  EXPECT_NO_THROW(checkFromThrusTo(nullptr, nullptr, nullptr));
}

// ExceptionPtIterator
TEST_F(ExceptionPathTest, PtIteratorEmpty) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  ExceptionPtIterator iter(&fp);
  EXPECT_FALSE(iter.hasNext());
}

// Default values
TEST_F(ExceptionPathTest, DefaultValues) {
  FalsePath fp(nullptr, nullptr, nullptr, MinMaxAll::all(), true, nullptr);
  EXPECT_FALSE(fp.useEndClk());
  EXPECT_EQ(fp.pathMultiplier(), 0);
  EXPECT_FLOAT_EQ(fp.delay(), 0.0f);
  EXPECT_EQ(fp.name(), nullptr);
  EXPECT_FALSE(fp.isDefault());
  EXPECT_FALSE(fp.ignoreClkLatency());
  EXPECT_FALSE(fp.breakPath());
}

////////////////////////////////////////////////////////////////
// TimingRole tests
////////////////////////////////////////////////////////////////

class TimingRoleTest : public ::testing::Test {};

TEST_F(TimingRoleTest, Singletons) {
  EXPECT_NE(TimingRole::wire(), nullptr);
  EXPECT_NE(TimingRole::combinational(), nullptr);
  EXPECT_NE(TimingRole::setup(), nullptr);
  EXPECT_NE(TimingRole::hold(), nullptr);
  EXPECT_NE(TimingRole::recovery(), nullptr);
  EXPECT_NE(TimingRole::removal(), nullptr);
  EXPECT_NE(TimingRole::regClkToQ(), nullptr);
  EXPECT_NE(TimingRole::latchEnToQ(), nullptr);
  EXPECT_NE(TimingRole::latchDtoQ(), nullptr);
  EXPECT_NE(TimingRole::tristateEnable(), nullptr);
  EXPECT_NE(TimingRole::tristateDisable(), nullptr);
  EXPECT_NE(TimingRole::width(), nullptr);
  EXPECT_NE(TimingRole::period(), nullptr);
  EXPECT_NE(TimingRole::skew(), nullptr);
  EXPECT_NE(TimingRole::nochange(), nullptr);
}

TEST_F(TimingRoleTest, OutputRoles) {
  EXPECT_NE(TimingRole::outputSetup(), nullptr);
  EXPECT_NE(TimingRole::outputHold(), nullptr);
}

TEST_F(TimingRoleTest, GatedClockRoles) {
  EXPECT_NE(TimingRole::gatedClockSetup(), nullptr);
  EXPECT_NE(TimingRole::gatedClockHold(), nullptr);
}

TEST_F(TimingRoleTest, LatchRoles) {
  EXPECT_NE(TimingRole::latchSetup(), nullptr);
  EXPECT_NE(TimingRole::latchHold(), nullptr);
}

TEST_F(TimingRoleTest, DataCheckRoles) {
  EXPECT_NE(TimingRole::dataCheckSetup(), nullptr);
  EXPECT_NE(TimingRole::dataCheckHold(), nullptr);
}

TEST_F(TimingRoleTest, NonSeqRoles) {
  EXPECT_NE(TimingRole::nonSeqSetup(), nullptr);
  EXPECT_NE(TimingRole::nonSeqHold(), nullptr);
}

TEST_F(TimingRoleTest, ClockTreePathRoles) {
  EXPECT_NE(TimingRole::clockTreePathMin(), nullptr);
  EXPECT_NE(TimingRole::clockTreePathMax(), nullptr);
}

TEST_F(TimingRoleTest, SdfIopath) {
  EXPECT_NE(TimingRole::sdfIopath(), nullptr);
}

TEST_F(TimingRoleTest, IsTimingCheck) {
  EXPECT_TRUE(TimingRole::setup()->isTimingCheck());
  EXPECT_TRUE(TimingRole::hold()->isTimingCheck());
  EXPECT_TRUE(TimingRole::recovery()->isTimingCheck());
  EXPECT_TRUE(TimingRole::removal()->isTimingCheck());
  EXPECT_FALSE(TimingRole::combinational()->isTimingCheck());
  EXPECT_FALSE(TimingRole::wire()->isTimingCheck());
  EXPECT_FALSE(TimingRole::regClkToQ()->isTimingCheck());
}

TEST_F(TimingRoleTest, IsWire) {
  EXPECT_TRUE(TimingRole::wire()->isWire());
  EXPECT_FALSE(TimingRole::setup()->isWire());
  EXPECT_FALSE(TimingRole::combinational()->isWire());
}

TEST_F(TimingRoleTest, IsTimingCheckBetween) {
  EXPECT_TRUE(TimingRole::setup()->isTimingCheckBetween());
  EXPECT_TRUE(TimingRole::hold()->isTimingCheckBetween());
  // width and period are timing checks but not "between"
  EXPECT_FALSE(TimingRole::width()->isTimingCheckBetween());
  EXPECT_FALSE(TimingRole::period()->isTimingCheckBetween());
}

TEST_F(TimingRoleTest, IsNonSeqTimingCheck) {
  EXPECT_TRUE(TimingRole::nonSeqSetup()->isNonSeqTimingCheck());
  EXPECT_TRUE(TimingRole::nonSeqHold()->isNonSeqTimingCheck());
  EXPECT_FALSE(TimingRole::setup()->isNonSeqTimingCheck());
}

TEST_F(TimingRoleTest, PathMinMax) {
  EXPECT_EQ(TimingRole::setup()->pathMinMax(), MinMax::max());
  EXPECT_EQ(TimingRole::hold()->pathMinMax(), MinMax::min());
}

TEST_F(TimingRoleTest, FindByName) {
  EXPECT_EQ(TimingRole::find("setup"), TimingRole::setup());
  EXPECT_EQ(TimingRole::find("hold"), TimingRole::hold());
  EXPECT_EQ(TimingRole::find("combinational"), TimingRole::combinational());
}

TEST_F(TimingRoleTest, UniqueIndices) {
  // All timing roles should have unique indices
  EXPECT_NE(TimingRole::setup()->index(), TimingRole::hold()->index());
  EXPECT_NE(TimingRole::setup()->index(), TimingRole::combinational()->index());
  EXPECT_NE(TimingRole::wire()->index(), TimingRole::combinational()->index());
}

TEST_F(TimingRoleTest, GenericRole) {
  // setup generic role is setup itself
  EXPECT_EQ(TimingRole::setup()->genericRole(), TimingRole::setup());
  EXPECT_EQ(TimingRole::hold()->genericRole(), TimingRole::hold());
  // output setup generic role is setup
  EXPECT_EQ(TimingRole::outputSetup()->genericRole(), TimingRole::setup());
  EXPECT_EQ(TimingRole::outputHold()->genericRole(), TimingRole::hold());
  EXPECT_EQ(TimingRole::gatedClockSetup()->genericRole(), TimingRole::setup());
  EXPECT_EQ(TimingRole::gatedClockHold()->genericRole(), TimingRole::hold());
  EXPECT_EQ(TimingRole::latchSetup()->genericRole(), TimingRole::setup());
  EXPECT_EQ(TimingRole::latchHold()->genericRole(), TimingRole::hold());
  EXPECT_EQ(TimingRole::recovery()->genericRole(), TimingRole::setup());
  EXPECT_EQ(TimingRole::removal()->genericRole(), TimingRole::hold());
  EXPECT_EQ(TimingRole::dataCheckSetup()->genericRole(), TimingRole::setup());
  EXPECT_EQ(TimingRole::dataCheckHold()->genericRole(), TimingRole::hold());
}

TEST_F(TimingRoleTest, Less) {
  EXPECT_TRUE(TimingRole::less(TimingRole::wire(), TimingRole::setup()));
}

TEST_F(TimingRoleTest, IsDataCheck) {
  EXPECT_TRUE(TimingRole::dataCheckSetup()->isDataCheck());
  EXPECT_TRUE(TimingRole::dataCheckHold()->isDataCheck());
  EXPECT_FALSE(TimingRole::setup()->isDataCheck());
  EXPECT_FALSE(TimingRole::hold()->isDataCheck());
}

TEST_F(TimingRoleTest, IsLatchDtoQ) {
  EXPECT_TRUE(TimingRole::latchDtoQ()->isLatchDtoQ());
  EXPECT_FALSE(TimingRole::latchEnToQ()->isLatchDtoQ());
  EXPECT_FALSE(TimingRole::regClkToQ()->isLatchDtoQ());
}

TEST_F(TimingRoleTest, IsAsyncTimingCheck) {
  EXPECT_TRUE(TimingRole::recovery()->isAsyncTimingCheck());
  EXPECT_TRUE(TimingRole::removal()->isAsyncTimingCheck());
  EXPECT_FALSE(TimingRole::setup()->isAsyncTimingCheck());
  EXPECT_FALSE(TimingRole::hold()->isAsyncTimingCheck());
}

TEST_F(TimingRoleTest, ToString) {
  EXPECT_EQ(TimingRole::setup()->to_string(), "setup");
  EXPECT_EQ(TimingRole::hold()->to_string(), "hold");
  EXPECT_EQ(TimingRole::combinational()->to_string(), "combinational");
}

TEST_F(TimingRoleTest, IndexMax) {
  int idx_max = TimingRole::index_max;
  EXPECT_GE(idx_max, 20);
}

////////////////////////////////////////////////////////////////
// RiseFallMinMax tests (for coverage of Clock slews)
////////////////////////////////////////////////////////////////

class RiseFallMinMaxTest : public ::testing::Test {};

TEST_F(RiseFallMinMaxTest, DefaultEmpty) {
  RiseFallMinMax rfmm;
  EXPECT_TRUE(rfmm.empty());
  EXPECT_FALSE(rfmm.hasValue());
}

TEST_F(RiseFallMinMaxTest, InitValueConstructor) {
  RiseFallMinMax rfmm(1.0f);
  EXPECT_FALSE(rfmm.empty());
  EXPECT_TRUE(rfmm.hasValue());
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 1.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 1.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::min()), 1.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 1.0f);
}

TEST_F(RiseFallMinMaxTest, CopyConstructor) {
  RiseFallMinMax rfmm1(2.0f);
  RiseFallMinMax rfmm2(&rfmm1);
  EXPECT_FLOAT_EQ(rfmm2.value(RiseFall::rise(), MinMax::min()), 2.0f);
  EXPECT_FLOAT_EQ(rfmm2.value(RiseFall::fall(), MinMax::max()), 2.0f);
}

TEST_F(RiseFallMinMaxTest, SetValueAll) {
  RiseFallMinMax rfmm;
  rfmm.setValue(5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 5.0f);
}

TEST_F(RiseFallMinMaxTest, SetValueRfMm) {
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

TEST_F(RiseFallMinMaxTest, SetValueRfBothMmAll) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFallBoth::riseFall(), MinMaxAll::all(), 10.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 10.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 10.0f);
}

TEST_F(RiseFallMinMaxTest, SetValueRfBothMm) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFallBoth::rise(), MinMax::max(), 7.0f);
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
  EXPECT_FALSE(rfmm.hasValue(RiseFall::fall(), MinMax::max()));
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 7.0f);
}

TEST_F(RiseFallMinMaxTest, HasValue) {
  RiseFallMinMax rfmm;
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 1.0f);
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  EXPECT_FALSE(rfmm.hasValue(RiseFall::fall(), MinMax::min()));
}

TEST_F(RiseFallMinMaxTest, ValueWithExists) {
  RiseFallMinMax rfmm;
  float val;
  bool exists;
  rfmm.value(RiseFall::rise(), MinMax::min(), val, exists);
  EXPECT_FALSE(exists);

  rfmm.setValue(RiseFall::rise(), MinMax::min(), 3.14f);
  rfmm.value(RiseFall::rise(), MinMax::min(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 3.14f);
}

TEST_F(RiseFallMinMaxTest, MaxValue) {
  RiseFallMinMax rfmm;
  float max_val;
  bool exists;
  rfmm.maxValue(max_val, exists);
  EXPECT_FALSE(exists);

  rfmm.setValue(RiseFall::rise(), MinMax::min(), 1.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::max(), 5.0f);
  rfmm.maxValue(max_val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(max_val, 5.0f);
}

TEST_F(RiseFallMinMaxTest, ValueMinMaxOnly) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 3.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 7.0f);
  // value(MinMax) returns the min of rise/fall for min, max of rise/fall for max
  float val = rfmm.value(MinMax::min());
  EXPECT_FLOAT_EQ(val, 3.0f);
}

TEST_F(RiseFallMinMaxTest, ValueMinMaxOnlyMax) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 3.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::max(), 7.0f);
  float val = rfmm.value(MinMax::max());
  EXPECT_FLOAT_EQ(val, 7.0f);
}

TEST_F(RiseFallMinMaxTest, Clear) {
  RiseFallMinMax rfmm(3.0f);
  EXPECT_FALSE(rfmm.empty());
  rfmm.clear();
  EXPECT_TRUE(rfmm.empty());
}

TEST_F(RiseFallMinMaxTest, RemoveValue) {
  RiseFallMinMax rfmm(1.0f);
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  rfmm.removeValue(RiseFallBoth::rise(), MinMax::min());
  EXPECT_FALSE(rfmm.hasValue(RiseFall::rise(), MinMax::min()));
  // Other values still exist
  EXPECT_TRUE(rfmm.hasValue(RiseFall::rise(), MinMax::max()));
}

TEST_F(RiseFallMinMaxTest, RemoveValueAll) {
  RiseFallMinMax rfmm(1.0f);
  rfmm.removeValue(RiseFallBoth::riseFall(), MinMaxAll::all());
  EXPECT_TRUE(rfmm.empty());
}

TEST_F(RiseFallMinMaxTest, MergeValue) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  // Merge a smaller value for min - should take it
  rfmm.mergeValue(RiseFall::rise(), MinMax::min(), 3.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 3.0f);
  // Merge a larger value for min - should not take it
  rfmm.mergeValue(RiseFall::rise(), MinMax::min(), 10.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 3.0f);
}

TEST_F(RiseFallMinMaxTest, MergeValueMax) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::max(), 5.0f);
  // Merge a larger value for max - should take it
  rfmm.mergeValue(RiseFall::rise(), MinMax::max(), 10.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 10.0f);
  // Merge a smaller value for max - should not take it
  rfmm.mergeValue(RiseFall::rise(), MinMax::max(), 3.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::max()), 10.0f);
}

TEST_F(RiseFallMinMaxTest, MergeValueBoth) {
  RiseFallMinMax rfmm;
  rfmm.mergeValue(RiseFallBoth::riseFall(), MinMaxAll::all(), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::rise(), MinMax::min()), 5.0f);
  EXPECT_FLOAT_EQ(rfmm.value(RiseFall::fall(), MinMax::max()), 5.0f);
}

TEST_F(RiseFallMinMaxTest, MergeWith) {
  RiseFallMinMax rfmm1;
  rfmm1.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  rfmm1.setValue(RiseFall::rise(), MinMax::max(), 5.0f);

  RiseFallMinMax rfmm2;
  rfmm2.setValue(RiseFall::rise(), MinMax::min(), 3.0f);
  rfmm2.setValue(RiseFall::rise(), MinMax::max(), 10.0f);
  rfmm2.setValue(RiseFall::fall(), MinMax::min(), 2.0f);

  rfmm1.mergeWith(&rfmm2);
  // min: should take 3 (smaller)
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::rise(), MinMax::min()), 3.0f);
  // max: should take 10 (larger)
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::rise(), MinMax::max()), 10.0f);
  // fall min: rfmm1 had no value, rfmm2 had 2, so should be 2
  EXPECT_FLOAT_EQ(rfmm1.value(RiseFall::fall(), MinMax::min()), 2.0f);
}

TEST_F(RiseFallMinMaxTest, SetValues) {
  RiseFallMinMax rfmm1(3.0f);
  RiseFallMinMax rfmm2;
  rfmm2.setValues(&rfmm1);
  EXPECT_TRUE(rfmm2.equal(&rfmm1));
}

TEST_F(RiseFallMinMaxTest, Equal) {
  RiseFallMinMax rfmm1(1.0f);
  RiseFallMinMax rfmm2(1.0f);
  EXPECT_TRUE(rfmm1.equal(&rfmm2));

  rfmm2.setValue(RiseFall::rise(), MinMax::min(), 2.0f);
  EXPECT_FALSE(rfmm1.equal(&rfmm2));
}

TEST_F(RiseFallMinMaxTest, EqualDifferentExists) {
  RiseFallMinMax rfmm1;
  rfmm1.setValue(RiseFall::rise(), MinMax::min(), 1.0f);
  RiseFallMinMax rfmm2;
  EXPECT_FALSE(rfmm1.equal(&rfmm2));
}

TEST_F(RiseFallMinMaxTest, IsOneValue) {
  RiseFallMinMax rfmm(5.0f);
  EXPECT_TRUE(rfmm.isOneValue());

  rfmm.setValue(RiseFall::rise(), MinMax::min(), 3.0f);
  EXPECT_FALSE(rfmm.isOneValue());
}

TEST_F(RiseFallMinMaxTest, IsOneValueWithReturn) {
  RiseFallMinMax rfmm(5.0f);
  float val;
  EXPECT_TRUE(rfmm.isOneValue(val));
  EXPECT_FLOAT_EQ(val, 5.0f);
}

TEST_F(RiseFallMinMaxTest, IsOneValueEmpty) {
  RiseFallMinMax rfmm;
  float val;
  EXPECT_FALSE(rfmm.isOneValue(val));
}

TEST_F(RiseFallMinMaxTest, IsOneValueMinMax) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 5.0f);
  float val;
  EXPECT_TRUE(rfmm.isOneValue(MinMax::min(), val));
  EXPECT_FLOAT_EQ(val, 5.0f);
}

TEST_F(RiseFallMinMaxTest, IsOneValueMinMaxDifferent) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  rfmm.setValue(RiseFall::fall(), MinMax::min(), 3.0f);
  float val;
  EXPECT_FALSE(rfmm.isOneValue(MinMax::min(), val));
}

TEST_F(RiseFallMinMaxTest, IsOneValueMinMaxEmpty) {
  RiseFallMinMax rfmm;
  float val;
  EXPECT_FALSE(rfmm.isOneValue(MinMax::min(), val));
}

TEST_F(RiseFallMinMaxTest, IsOneValueMinMaxPartialExists) {
  RiseFallMinMax rfmm;
  rfmm.setValue(RiseFall::rise(), MinMax::min(), 5.0f);
  // fall/min does not exist
  float val;
  EXPECT_FALSE(rfmm.isOneValue(MinMax::min(), val));
}

////////////////////////////////////////////////////////////////
// Scene tests (Corner was renamed to Scene)
////////////////////////////////////////////////////////////////

class SceneTest : public ::testing::Test {};

TEST_F(SceneTest, BasicConstruction) {
  Scene scene("default", 0, nullptr, nullptr);
  EXPECT_EQ(scene.name(), "default");
  EXPECT_EQ(scene.index(), 0u);
}

TEST_F(SceneTest, DifferentIndex) {
  Scene scene("fast", 1, nullptr, nullptr);
  EXPECT_EQ(scene.name(), "fast");
  EXPECT_EQ(scene.index(), 1u);
}

} // namespace sta
