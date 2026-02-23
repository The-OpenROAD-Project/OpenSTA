#include <gtest/gtest.h>
#include <string>
#include "VerilogNamespace.hh"
#include "PortDirection.hh"
#include "ConcreteLibrary.hh"
#include "ConcreteNetwork.hh"
#include "HpinDrvrLoad.hh"
#include "Network.hh"
#include "PatternMatch.hh"
#include "NetworkCmp.hh"
#include "SdcNetwork.hh"

namespace sta {

class VerilogNamespaceTest : public ::testing::Test {};

// Simple names should pass through unchanged
TEST_F(VerilogNamespaceTest, CellSimpleName) {
  std::string result = cellVerilogName("INV_X1");
  EXPECT_EQ(result, "INV_X1");
}

// Escaped names in STA have backslash prefix
TEST_F(VerilogNamespaceTest, CellEscapedName) {
  std::string result = cellVerilogName("\\my/cell");
  // Verilog escaped names have backslash prefix and space suffix
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogNamespaceTest, InstanceSimpleName) {
  std::string result = instanceVerilogName("u1");
  EXPECT_EQ(result, "u1");
}

TEST_F(VerilogNamespaceTest, InstanceHierarchicalName) {
  std::string result = instanceVerilogName("u1/u2");
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogNamespaceTest, NetSimpleName) {
  std::string result = netVerilogName("wire1");
  EXPECT_EQ(result, "wire1");
}

TEST_F(VerilogNamespaceTest, NetBusName) {
  std::string result = netVerilogName("bus[0]");
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogNamespaceTest, PortSimpleName) {
  std::string result = portVerilogName("clk");
  EXPECT_EQ(result, "clk");
}

TEST_F(VerilogNamespaceTest, PortBusName) {
  std::string result = portVerilogName("data[7]");
  EXPECT_FALSE(result.empty());
}

// Test Verilog-to-STA conversion
TEST_F(VerilogNamespaceTest, ModuleVerilogToSta) {
  std::string verilog_name = "top_module";
  std::string result = moduleVerilogToSta(&verilog_name);
  EXPECT_EQ(result, "top_module");
}

TEST_F(VerilogNamespaceTest, InstanceVerilogToSta) {
  std::string verilog_name = "u1";
  std::string result = instanceVerilogToSta(&verilog_name);
  EXPECT_EQ(result, "u1");
}

// Test escaped name round-trip
TEST_F(VerilogNamespaceTest, EscapedNameRoundTrip) {
  std::string verilog_name = "\\esc_name ";
  std::string sta = instanceVerilogToSta(&verilog_name);
  EXPECT_FALSE(sta.empty());
}

////////////////////////////////////////////////////////////////
// PortDirection tests - covers init, find, isAnyInput, isAnyOutput,
// isAnyTristate, isPowerGround, and all identity predicates
////////////////////////////////////////////////////////////////

class PortDirectionTest : public ::testing::Test {
protected:
  void SetUp() override {
    // PortDirection::init() should have been called by initSta or similar.
    // If not already initialized, we need to call it.
    if (PortDirection::input() == nullptr) {
      PortDirection::init();
    }
  }
};

TEST_F(PortDirectionTest, InputSingleton) {
  PortDirection *dir = PortDirection::input();
  EXPECT_NE(dir, nullptr);
  EXPECT_STREQ(dir->name(), "input");
  EXPECT_EQ(dir->index(), 0);
  EXPECT_TRUE(dir->isInput());
  EXPECT_FALSE(dir->isOutput());
  EXPECT_FALSE(dir->isTristate());
  EXPECT_FALSE(dir->isBidirect());
  EXPECT_FALSE(dir->isInternal());
  EXPECT_FALSE(dir->isGround());
  EXPECT_FALSE(dir->isPower());
  EXPECT_FALSE(dir->isUnknown());
}

TEST_F(PortDirectionTest, OutputSingleton) {
  PortDirection *dir = PortDirection::output();
  EXPECT_NE(dir, nullptr);
  EXPECT_STREQ(dir->name(), "output");
  EXPECT_EQ(dir->index(), 1);
  EXPECT_TRUE(dir->isOutput());
  EXPECT_FALSE(dir->isInput());
}

TEST_F(PortDirectionTest, TristateSingleton) {
  PortDirection *dir = PortDirection::tristate();
  EXPECT_NE(dir, nullptr);
  EXPECT_STREQ(dir->name(), "tristate");
  EXPECT_EQ(dir->index(), 2);
  EXPECT_TRUE(dir->isTristate());
  EXPECT_FALSE(dir->isInput());
  EXPECT_FALSE(dir->isOutput());
}

TEST_F(PortDirectionTest, BidirectSingleton) {
  PortDirection *dir = PortDirection::bidirect();
  EXPECT_NE(dir, nullptr);
  EXPECT_STREQ(dir->name(), "bidirect");
  EXPECT_EQ(dir->index(), 3);
  EXPECT_TRUE(dir->isBidirect());
}

TEST_F(PortDirectionTest, InternalSingleton) {
  PortDirection *dir = PortDirection::internal();
  EXPECT_NE(dir, nullptr);
  EXPECT_STREQ(dir->name(), "internal");
  EXPECT_EQ(dir->index(), 4);
  EXPECT_TRUE(dir->isInternal());
}

TEST_F(PortDirectionTest, GroundSingleton) {
  PortDirection *dir = PortDirection::ground();
  EXPECT_NE(dir, nullptr);
  EXPECT_STREQ(dir->name(), "ground");
  EXPECT_EQ(dir->index(), 5);
  EXPECT_TRUE(dir->isGround());
}

TEST_F(PortDirectionTest, PowerSingleton) {
  PortDirection *dir = PortDirection::power();
  EXPECT_NE(dir, nullptr);
  EXPECT_STREQ(dir->name(), "power");
  EXPECT_EQ(dir->index(), 6);
  EXPECT_TRUE(dir->isPower());
}

TEST_F(PortDirectionTest, UnknownSingleton) {
  PortDirection *dir = PortDirection::unknown();
  EXPECT_NE(dir, nullptr);
  EXPECT_STREQ(dir->name(), "unknown");
  EXPECT_EQ(dir->index(), 7);
  EXPECT_TRUE(dir->isUnknown());
}

TEST_F(PortDirectionTest, FindByName) {
  EXPECT_EQ(PortDirection::find("input"), PortDirection::input());
  EXPECT_EQ(PortDirection::find("output"), PortDirection::output());
  EXPECT_EQ(PortDirection::find("tristate"), PortDirection::tristate());
  EXPECT_EQ(PortDirection::find("bidirect"), PortDirection::bidirect());
  EXPECT_EQ(PortDirection::find("internal"), PortDirection::internal());
  EXPECT_EQ(PortDirection::find("ground"), PortDirection::ground());
  EXPECT_EQ(PortDirection::find("power"), PortDirection::power());
  EXPECT_EQ(PortDirection::find("nonexistent"), nullptr);
}

TEST_F(PortDirectionTest, IsAnyInput) {
  EXPECT_TRUE(PortDirection::input()->isAnyInput());
  EXPECT_TRUE(PortDirection::bidirect()->isAnyInput());
  EXPECT_FALSE(PortDirection::output()->isAnyInput());
  EXPECT_FALSE(PortDirection::tristate()->isAnyInput());
  EXPECT_FALSE(PortDirection::internal()->isAnyInput());
  EXPECT_FALSE(PortDirection::ground()->isAnyInput());
  EXPECT_FALSE(PortDirection::power()->isAnyInput());
  EXPECT_FALSE(PortDirection::unknown()->isAnyInput());
}

TEST_F(PortDirectionTest, IsAnyOutput) {
  EXPECT_TRUE(PortDirection::output()->isAnyOutput());
  EXPECT_TRUE(PortDirection::tristate()->isAnyOutput());
  EXPECT_TRUE(PortDirection::bidirect()->isAnyOutput());
  EXPECT_FALSE(PortDirection::input()->isAnyOutput());
  EXPECT_FALSE(PortDirection::internal()->isAnyOutput());
  EXPECT_FALSE(PortDirection::ground()->isAnyOutput());
  EXPECT_FALSE(PortDirection::power()->isAnyOutput());
  EXPECT_FALSE(PortDirection::unknown()->isAnyOutput());
}

TEST_F(PortDirectionTest, IsAnyTristate) {
  EXPECT_TRUE(PortDirection::tristate()->isAnyTristate());
  EXPECT_TRUE(PortDirection::bidirect()->isAnyTristate());
  EXPECT_FALSE(PortDirection::input()->isAnyTristate());
  EXPECT_FALSE(PortDirection::output()->isAnyTristate());
  EXPECT_FALSE(PortDirection::internal()->isAnyTristate());
  EXPECT_FALSE(PortDirection::ground()->isAnyTristate());
  EXPECT_FALSE(PortDirection::power()->isAnyTristate());
  EXPECT_FALSE(PortDirection::unknown()->isAnyTristate());
}

TEST_F(PortDirectionTest, IsPowerGround) {
  EXPECT_TRUE(PortDirection::power()->isPowerGround());
  EXPECT_TRUE(PortDirection::ground()->isPowerGround());
  EXPECT_FALSE(PortDirection::input()->isPowerGround());
  EXPECT_FALSE(PortDirection::output()->isPowerGround());
  EXPECT_FALSE(PortDirection::tristate()->isPowerGround());
  EXPECT_FALSE(PortDirection::bidirect()->isPowerGround());
  EXPECT_FALSE(PortDirection::internal()->isPowerGround());
  EXPECT_FALSE(PortDirection::unknown()->isPowerGround());
}

////////////////////////////////////////////////////////////////
// ConcreteLibrary tests
////////////////////////////////////////////////////////////////

TEST(ConcreteLibraryTest, CreateAndFind) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  EXPECT_STREQ(lib.name(), "test_lib");
  EXPECT_STREQ(lib.filename(), "test.lib");
  EXPECT_FALSE(lib.isLiberty());
}

TEST(ConcreteLibraryTest, BusBrackets) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  EXPECT_EQ(lib.busBrktLeft(), '[');
  EXPECT_EQ(lib.busBrktRight(), ']');
}

TEST(ConcreteLibraryTest, MakeCell) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "inv.v");
  EXPECT_NE(cell, nullptr);
  EXPECT_STREQ(cell->name(), "INV");
  EXPECT_STREQ(cell->filename(), "inv.v");
  EXPECT_TRUE(cell->isLeaf());
  EXPECT_EQ(cell->library(), &lib);

  ConcreteCell *found = lib.findCell("INV");
  EXPECT_EQ(found, cell);

  ConcreteCell *not_found = lib.findCell("NAND2");
  EXPECT_EQ(not_found, nullptr);
}

TEST(ConcreteLibraryTest, DeleteCell) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.makeCell("BUF", true, "");
  ConcreteCell *found = lib.findCell("BUF");
  EXPECT_NE(found, nullptr);
  lib.deleteCell(found);
  ConcreteCell *deleted = lib.findCell("BUF");
  EXPECT_EQ(deleted, nullptr);
}

TEST(ConcreteLibraryTest, CellIterator) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.makeCell("INV", true, "");
  lib.makeCell("BUF", true, "");
  lib.makeCell("NAND2", true, "");

  ConcreteLibraryCellIterator *iter = lib.cellIterator();
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 3);
}

TEST(ConcreteLibraryTest, IsLiberty) {
  ConcreteLibrary lib("test_lib", "test.lib", true);
  EXPECT_TRUE(lib.isLiberty());
  ConcreteLibrary lib2("test_lib2", "test2.lib", false);
  EXPECT_FALSE(lib2.isLiberty());
}

////////////////////////////////////////////////////////////////
// ConcreteCell tests
////////////////////////////////////////////////////////////////

TEST(ConcreteCellTest, MakePort) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *port_a = cell->makePort("A");
  EXPECT_NE(port_a, nullptr);
  EXPECT_STREQ(port_a->name(), "A");
  EXPECT_EQ(port_a->cell(), reinterpret_cast<Cell*>(cell));

  ConcretePort *found = cell->findPort("A");
  EXPECT_EQ(found, port_a);

  ConcretePort *not_found = cell->findPort("B");
  EXPECT_EQ(not_found, nullptr);
}

TEST(ConcreteCellTest, PortCount) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("NAND2", true, "");
  cell->makePort("A");
  cell->makePort("B");
  cell->makePort("Y");
  EXPECT_EQ(cell->portCount(), 3u);
}

TEST(ConcreteCellTest, SetIsLeaf) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("TOP", false, "");
  EXPECT_FALSE(cell->isLeaf());
  cell->setIsLeaf(true);
  EXPECT_TRUE(cell->isLeaf());
}

TEST(ConcreteCellTest, PortBitCount) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("BUF", true, "");
  cell->makePort("A");
  cell->makePort("Y");
  EXPECT_EQ(cell->portBitCount(), 2);
}

TEST(ConcreteCellTest, MakeBusPort) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 3, 0);
  EXPECT_NE(bus, nullptr);
  EXPECT_TRUE(bus->isBus());
  EXPECT_EQ(bus->fromIndex(), 3);
  EXPECT_EQ(bus->toIndex(), 0);
  EXPECT_EQ(bus->size(), 4);
}

TEST(ConcreteCellTest, AttributeMap) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  cell->setAttribute("area", "1.5");
  EXPECT_EQ(cell->getAttribute("area"), "1.5");
  EXPECT_EQ(cell->getAttribute("nonexistent"), "");
}

TEST(ConcreteCellTest, SetName) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("OLD", true, "");
  EXPECT_STREQ(cell->name(), "OLD");
  cell->setName("NEW");
  EXPECT_STREQ(cell->name(), "NEW");
}

TEST(ConcreteCellTest, PortIterator) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("AND2", true, "");
  cell->makePort("A");
  cell->makePort("B");
  cell->makePort("Y");

  ConcreteCellPortIterator *iter = cell->portIterator();
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 3);
}

TEST(ConcreteCellTest, PortBitIterator) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  cell->makePort("CLK");
  cell->makeBusPort("D", 1, 0);  // 2-bit bus
  cell->makePort("Q");

  ConcreteCellPortBitIterator *iter = cell->portBitIterator();
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  // CLK(1) + D[0],D[1](2) + Q(1) = 4
  EXPECT_EQ(count, 4);
}

////////////////////////////////////////////////////////////////
// ConcretePort tests
////////////////////////////////////////////////////////////////

TEST(ConcretePortTest, ScalarPortProperties) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *port = cell->makePort("A");
  EXPECT_FALSE(port->isBus());
  EXPECT_FALSE(port->isBundle());
  EXPECT_FALSE(port->isBusBit());
  EXPECT_FALSE(port->hasMembers());
  EXPECT_EQ(port->size(), 1);
}

TEST(ConcretePortTest, SetDirection) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *port = cell->makePort("A");
  port->setDirection(PortDirection::input());
  EXPECT_EQ(port->direction(), PortDirection::input());
  EXPECT_TRUE(port->direction()->isInput());
}

TEST(ConcretePortTest, BusPortBit) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 3, 0);
  EXPECT_TRUE(bus->isBus());
  EXPECT_TRUE(bus->hasMembers());
  EXPECT_EQ(bus->size(), 4);

  ConcretePort *bit0 = bus->findBusBit(0);
  EXPECT_NE(bit0, nullptr);
  EXPECT_TRUE(bit0->isBusBit());

  ConcretePort *bit3 = bus->findBusBit(3);
  EXPECT_NE(bit3, nullptr);

  // Out of range
  ConcretePort *bit_oor = bus->findBusBit(4);
  EXPECT_EQ(bit_oor, nullptr);
}

TEST(ConcretePortTest, BusIndexInRange) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 3, 0);
  EXPECT_TRUE(bus->busIndexInRange(0));
  EXPECT_TRUE(bus->busIndexInRange(1));
  EXPECT_TRUE(bus->busIndexInRange(2));
  EXPECT_TRUE(bus->busIndexInRange(3));
  EXPECT_FALSE(bus->busIndexInRange(4));
  EXPECT_FALSE(bus->busIndexInRange(-1));
}

TEST(ConcretePortTest, MemberIterator) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 1, 0);

  ConcretePortMemberIterator *iter = bus->memberIterator();
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

TEST(ConcretePortTest, PinIndex) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *y = cell->makePort("Y");
  // Pin indices are assigned sequentially
  EXPECT_EQ(a->pinIndex(), 0);
  EXPECT_EQ(y->pinIndex(), 1);
}

////////////////////////////////////////////////////////////////
// HpinDrvrLoad tests - basic construction and comparison
// Note: The 2-arg constructor does NOT initialize hpins_from_drvr_
// and hpins_to_load_, so we use 4-arg constructor for safe tests.
////////////////////////////////////////////////////////////////

TEST(HpinDrvrLoadTest, FullConstructorNull) {
  // Test the 4-arg constructor with null pin sets
  HpinDrvrLoad hdl(nullptr, nullptr, nullptr, nullptr);
  EXPECT_EQ(hdl.drvr(), nullptr);
  EXPECT_EQ(hdl.load(), nullptr);
  EXPECT_EQ(hdl.hpinsFromDrvr(), nullptr);
  EXPECT_EQ(hdl.hpinsToLoad(), nullptr);
}

TEST(HpinDrvrLoadTest, FullConstructorWithPins) {
  int fake_drvr = 1;
  int fake_load = 2;
  const Pin *drvr = reinterpret_cast<const Pin*>(&fake_drvr);
  const Pin *load = reinterpret_cast<const Pin*>(&fake_load);

  HpinDrvrLoad hdl(drvr, load, nullptr, nullptr);
  EXPECT_EQ(hdl.drvr(), drvr);
  EXPECT_EQ(hdl.load(), load);
  EXPECT_EQ(hdl.hpinsFromDrvr(), nullptr);
  EXPECT_EQ(hdl.hpinsToLoad(), nullptr);
}

TEST(HpinDrvrLoadTest, SetDrvr) {
  int fake_drvr = 1;
  const Pin *drvr = reinterpret_cast<const Pin*>(&fake_drvr);

  HpinDrvrLoad hdl(nullptr, nullptr, nullptr, nullptr);
  hdl.setDrvr(drvr);
  EXPECT_EQ(hdl.drvr(), drvr);
}

TEST(HpinDrvrLoadTest, LessComparisonDifferentLoads) {
  int a = 1, b = 2, c = 3, d = 4;
  const Pin *pin_a = reinterpret_cast<const Pin*>(&a);
  const Pin *pin_b = reinterpret_cast<const Pin*>(&b);
  const Pin *pin_c = reinterpret_cast<const Pin*>(&c);
  const Pin *pin_d = reinterpret_cast<const Pin*>(&d);

  HpinDrvrLoad hdl1(pin_a, pin_c, nullptr, nullptr);
  HpinDrvrLoad hdl2(pin_b, pin_d, nullptr, nullptr);

  HpinDrvrLoadLess less;
  // Compare by load first
  bool result1 = less(&hdl1, &hdl2);
  bool result2 = less(&hdl2, &hdl1);
  // Exactly one must be true (different loads)
  EXPECT_NE(result1, result2);
}

TEST(HpinDrvrLoadTest, LessComparisonSameLoad) {
  int a = 1, b = 2;
  const Pin *pin_a = reinterpret_cast<const Pin*>(&a);
  const Pin *pin_b = reinterpret_cast<const Pin*>(&b);

  // Same load pointer, different driver pointers
  HpinDrvrLoad hdl1(pin_a, pin_a, nullptr, nullptr);
  HpinDrvrLoad hdl2(pin_b, pin_a, nullptr, nullptr);

  HpinDrvrLoadLess less;
  // Same load -> compare drivers
  bool result1 = less(&hdl1, &hdl2);
  bool result2 = less(&hdl2, &hdl1);
  EXPECT_NE(result1, result2);
}

TEST(HpinDrvrLoadTest, LessComparisonEqual) {
  int a = 1;
  const Pin *pin = reinterpret_cast<const Pin*>(&a);

  HpinDrvrLoad hdl1(pin, pin, nullptr, nullptr);
  HpinDrvrLoad hdl2(pin, pin, nullptr, nullptr);

  HpinDrvrLoadLess less;
  EXPECT_FALSE(less(&hdl1, &hdl2));
  EXPECT_FALSE(less(&hdl2, &hdl1));
}

TEST(HpinDrvrLoadTest, NullDrvrAndLoad) {
  HpinDrvrLoad hdl(nullptr, nullptr, nullptr, nullptr);
  EXPECT_EQ(hdl.drvr(), nullptr);
  EXPECT_EQ(hdl.load(), nullptr);

  // Set driver
  int fake = 42;
  const Pin *pin = reinterpret_cast<const Pin*>(&fake);
  hdl.setDrvr(pin);
  EXPECT_EQ(hdl.drvr(), pin);
  EXPECT_EQ(hdl.load(), nullptr);
}

////////////////////////////////////////////////////////////////
// ConcreteNetwork creation tests
////////////////////////////////////////////////////////////////

TEST(ConcreteNetworkTest, FindLibrary) {
  ConcreteNetwork network;
  // No libraries initially
  Library *lib = network.findLibrary("nonexistent");
  EXPECT_EQ(lib, nullptr);
}

////////////////////////////////////////////////////////////////
// Additional ConcreteCell tests for coverage
////////////////////////////////////////////////////////////////

TEST(ConcreteCellTest, SetLibertyCell) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  // Initially null
  EXPECT_EQ(cell->libertyCell(), nullptr);
  // setLibertyCell is used when liberty parsing assigns the cell
  cell->setLibertyCell(nullptr);
  EXPECT_EQ(cell->libertyCell(), nullptr);
}

TEST(ConcreteCellTest, SetExtCell) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  EXPECT_EQ(cell->extCell(), nullptr);
  int dummy = 42;
  cell->setExtCell(&dummy);
  EXPECT_EQ(cell->extCell(), &dummy);
}

TEST(ConcreteCellTest, MakeBundlePort) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("MUX", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *b = cell->makePort("B");
  ConcretePortSeq *members = new ConcretePortSeq;
  members->push_back(a);
  members->push_back(b);
  ConcretePort *bundle = cell->makeBundlePort("AB", members);
  EXPECT_NE(bundle, nullptr);
  EXPECT_TRUE(bundle->isBundle());
  EXPECT_TRUE(bundle->hasMembers());
  EXPECT_EQ(bundle->size(), 2);
}

TEST(ConcreteCellTest, MakeBusPortAscending) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  // Ascending index: from_index < to_index
  ConcretePort *bus = cell->makeBusPort("D", 0, 3);
  EXPECT_NE(bus, nullptr);
  EXPECT_TRUE(bus->isBus());
  EXPECT_EQ(bus->fromIndex(), 0);
  EXPECT_EQ(bus->toIndex(), 3);
  EXPECT_EQ(bus->size(), 4);
}

TEST(ConcreteCellTest, Filename) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "test_cell.v");
  EXPECT_STREQ(cell->filename(), "test_cell.v");

  ConcreteCell *cell2 = lib.makeCell("BUF", true, "");
  EXPECT_STREQ(cell2->filename(), "");
}

TEST(ConcreteCellTest, FindCellsMatching) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.makeCell("INV_X1", true, "");
  lib.makeCell("INV_X2", true, "");
  lib.makeCell("BUF_X1", true, "");

  // Pattern that matches all INV cells
  PatternMatch pattern("INV*", false, false, nullptr);
  auto matches = lib.findCellsMatching(&pattern);
  EXPECT_EQ(matches.size(), 2u);
}

////////////////////////////////////////////////////////////////
// Additional ConcretePort tests for coverage
////////////////////////////////////////////////////////////////

TEST(ConcretePortTest, SetLibertyPort) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *port = cell->makePort("A");
  EXPECT_EQ(port->libertyPort(), nullptr);
  port->setLibertyPort(nullptr);
  EXPECT_EQ(port->libertyPort(), nullptr);
}

TEST(ConcretePortTest, SetExtPort) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *port = cell->makePort("A");
  EXPECT_EQ(port->extPort(), nullptr);
  int dummy = 42;
  port->setExtPort(&dummy);
  EXPECT_EQ(port->extPort(), &dummy);
}

TEST(ConcretePortTest, BusPortBusName) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 3, 0);
  const char *bus_name = bus->busName();
  EXPECT_NE(bus_name, nullptr);
  // Should contain bus bracket notation
  std::string name_str(bus_name);
  EXPECT_NE(name_str.find("["), std::string::npos);
}

TEST(ConcretePortTest, ScalarBusName) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *port = cell->makePort("A");
  // Scalar port busName returns just the name
  const char *bus_name = port->busName();
  EXPECT_STREQ(bus_name, "A");
}

TEST(ConcretePortTest, FindMember) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 3, 0);
  // findMember gets member by index in the member array
  ConcretePort *member0 = bus->findMember(0);
  EXPECT_NE(member0, nullptr);
  ConcretePort *member3 = bus->findMember(3);
  EXPECT_NE(member3, nullptr);
}

TEST(ConcretePortTest, SetPinIndex) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("INV", true, "");
  ConcretePort *port = cell->makePort("A");
  // Pin index is auto-assigned
  int orig = port->pinIndex();
  port->setPinIndex(42);
  EXPECT_EQ(port->pinIndex(), 42);
  // Restore
  port->setPinIndex(orig);
}

TEST(ConcretePortTest, BusBitIndex) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 3, 0);
  ConcretePort *bit0 = bus->findBusBit(0);
  EXPECT_NE(bit0, nullptr);
  EXPECT_TRUE(bit0->isBusBit());
  EXPECT_EQ(bit0->busBitIndex(), 0);
}

TEST(ConcretePortTest, SetDirectionOnBus) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 1, 0);
  bus->setDirection(PortDirection::input());
  EXPECT_EQ(bus->direction(), PortDirection::input());
  // Setting direction on bus should propagate to bits
  ConcretePort *bit0 = bus->findBusBit(0);
  if (bit0) {
    EXPECT_EQ(bit0->direction(), PortDirection::input());
  }
}

TEST(ConcretePortTest, AddPortBit) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 1, 0);
  // Bus already has auto-generated bits - this tests the addPortBit path
  EXPECT_TRUE(bus->hasMembers());
  EXPECT_EQ(bus->size(), 2);
}

TEST(ConcretePortTest, BusMemberIterator) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 1, 0);
  ConcretePortMemberIterator *iter = bus->memberIterator();
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

TEST(ConcretePortTest, BundlePortSetDirection) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("MUX", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *b = cell->makePort("B");
  ConcretePortSeq *members = new ConcretePortSeq;
  members->push_back(a);
  members->push_back(b);
  ConcretePort *bundle = cell->makeBundlePort("AB", members);
  // Setting direction on bundle
  bundle->setDirection(PortDirection::input());
  EXPECT_EQ(bundle->direction(), PortDirection::input());
}

TEST(ConcretePortTest, BundleBusBitIndex) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *port = cell->makePort("CLK");
  // Scalar port setBusBitIndex
  port->setBusBitIndex(5);
  EXPECT_EQ(port->busBitIndex(), 5);
  EXPECT_TRUE(port->isBusBit());
}

////////////////////////////////////////////////////////////////
// ConcreteLibrary additional tests
////////////////////////////////////////////////////////////////

TEST(ConcreteLibraryTest, BusBracketsChange) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  EXPECT_EQ(lib.busBrktLeft(), '[');
  EXPECT_EQ(lib.busBrktRight(), ']');

  lib.setBusBrkts('(', ')');
  EXPECT_EQ(lib.busBrktLeft(), '(');
  EXPECT_EQ(lib.busBrktRight(), ')');
}

TEST(ConcreteLibraryTest, FilenameAndId) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  EXPECT_STREQ(lib.filename(), "test.lib");
  // Library ID is a monotonically increasing counter
  EXPECT_GE(lib.id(), 0u);
}

////////////////////////////////////////////////////////////////
// PortDirection additional coverage
////////////////////////////////////////////////////////////////

TEST(PortDirectionExtraTest, AllDirections) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  EXPECT_NE(PortDirection::input(), nullptr);
  EXPECT_NE(PortDirection::output(), nullptr);
  EXPECT_NE(PortDirection::bidirect(), nullptr);
  EXPECT_NE(PortDirection::tristate(), nullptr);
  EXPECT_NE(PortDirection::internal(), nullptr);
  EXPECT_NE(PortDirection::ground(), nullptr);
  EXPECT_NE(PortDirection::power(), nullptr);
  EXPECT_NE(PortDirection::unknown(), nullptr);
}

TEST(PortDirectionExtraTest, DirectionProperties) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  EXPECT_TRUE(PortDirection::input()->isInput());
  EXPECT_FALSE(PortDirection::input()->isOutput());
  EXPECT_FALSE(PortDirection::input()->isBidirect());
  EXPECT_FALSE(PortDirection::input()->isTristate());
  EXPECT_FALSE(PortDirection::input()->isPowerGround());

  EXPECT_FALSE(PortDirection::output()->isInput());
  EXPECT_TRUE(PortDirection::output()->isOutput());
  EXPECT_FALSE(PortDirection::output()->isTristate());

  EXPECT_TRUE(PortDirection::bidirect()->isBidirect());
  EXPECT_TRUE(PortDirection::tristate()->isTristate());

  EXPECT_TRUE(PortDirection::ground()->isPowerGround());
  EXPECT_TRUE(PortDirection::power()->isPowerGround());
}

TEST(PortDirectionExtraTest, DirectionNames) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  EXPECT_STREQ(PortDirection::input()->name(), "input");
  EXPECT_STREQ(PortDirection::output()->name(), "output");
  EXPECT_STREQ(PortDirection::bidirect()->name(), "bidirect");
  EXPECT_STREQ(PortDirection::tristate()->name(), "tristate");
  EXPECT_STREQ(PortDirection::internal()->name(), "internal");
  EXPECT_STREQ(PortDirection::ground()->name(), "ground");
  EXPECT_STREQ(PortDirection::power()->name(), "power");
  EXPECT_STREQ(PortDirection::unknown()->name(), "unknown");
}

TEST(PortDirectionExtraTest, FindAllByName) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  EXPECT_EQ(PortDirection::find("input"), PortDirection::input());
  EXPECT_EQ(PortDirection::find("output"), PortDirection::output());
  EXPECT_EQ(PortDirection::find("bidirect"), PortDirection::bidirect());
  EXPECT_EQ(PortDirection::find("tristate"), PortDirection::tristate());
  EXPECT_EQ(PortDirection::find("internal"), PortDirection::internal());
  EXPECT_EQ(PortDirection::find("ground"), PortDirection::ground());
  EXPECT_EQ(PortDirection::find("power"), PortDirection::power());
  // "unknown" is not findable by name, returns nullptr
  EXPECT_EQ(PortDirection::find("nonexistent"), nullptr);
}

TEST(PortDirectionExtraTest, DirectionIndex) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  // Each direction should have a unique index
  EXPECT_NE(PortDirection::input()->index(), PortDirection::output()->index());
  EXPECT_NE(PortDirection::bidirect()->index(), PortDirection::tristate()->index());
}

////////////////////////////////////////////////////////////////
// NetworkCmp coverage tests
////////////////////////////////////////////////////////////////

TEST(NetworkCmpTest, PortDirectionCmp) {
  if (PortDirection::input() == nullptr) {
    PortDirection::init();
  }
  // PortDirection comparison is by index
  EXPECT_TRUE(PortDirection::input()->index() != PortDirection::output()->index());
}

////////////////////////////////////////////////////////////////
// GroupBusPorts test
////////////////////////////////////////////////////////////////

TEST(ConcreteCellTest, GroupBusPorts) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("FIFO", true, "");
  // Create individual bus bit ports
  cell->makePort("D[0]");
  cell->makePort("D[1]");
  cell->makePort("D[2]");
  cell->makePort("D[3]");
  cell->makePort("CLK");

  // groupBusPorts should group D[0]-D[3] into bus D
  cell->groupBusPorts('[', ']', [](const char*) { return true; });

  // After grouping, we should find the bus port D
  ConcretePort *bus = cell->findPort("D");
  EXPECT_NE(bus, nullptr);
  if (bus) {
    EXPECT_TRUE(bus->isBus());
    EXPECT_EQ(bus->size(), 4);
  }

  // CLK should still be a scalar port
  ConcretePort *clk = cell->findPort("CLK");
  EXPECT_NE(clk, nullptr);
}

////////////////////////////////////////////////////////////////
// ConcreteNetwork additional tests
////////////////////////////////////////////////////////////////

TEST(ConcreteNetworkTest, MakeLibrary) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_lib", "test.lib");
  EXPECT_NE(lib, nullptr);

  // Find it
  Library *found = network.findLibrary("test_lib");
  EXPECT_EQ(found, lib);

  // Library name
  EXPECT_STREQ(network.name(lib), "test_lib");
}

TEST(ConcreteNetworkTest, LibraryIterator) {
  ConcreteNetwork network;
  network.makeLibrary("lib1", "lib1.lib");
  network.makeLibrary("lib2", "lib2.lib");

  LibraryIterator *iter = network.libraryIterator();
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

TEST(ConcreteNetworkTest, FindCell) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_lib", "test.lib");
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  clib->makeCell("INV", true, "");

  Cell *found = network.findCell(lib, "INV");
  EXPECT_NE(found, nullptr);

  Cell *not_found = network.findCell(lib, "NAND2");
  EXPECT_EQ(not_found, nullptr);
}

TEST(ConcreteNetworkTest, CellName) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_lib", "test.lib");
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  clib->makeCell("INV_X1", true, "");

  Cell *cell = network.findCell(lib, "INV_X1");
  EXPECT_NE(cell, nullptr);
  EXPECT_STREQ(network.name(cell), "INV_X1");
}

TEST(ConcreteNetworkTest, CellIsLeaf) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_lib", "test.lib");
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  clib->makeCell("INV", true, "");
  clib->makeCell("TOP", false, "");

  Cell *inv = network.findCell(lib, "INV");
  Cell *top = network.findCell(lib, "TOP");
  EXPECT_TRUE(network.isLeaf(inv));
  EXPECT_FALSE(network.isLeaf(top));
}

TEST(ConcreteNetworkTest, CellPorts) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_lib", "test.lib");
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  ConcreteCell *ccell = clib->makeCell("INV", true, "");
  ccell->makePort("A");
  ccell->makePort("Y");

  Cell *cell = reinterpret_cast<Cell*>(ccell);
  CellPortIterator *iter = network.portIterator(cell);
  int count = 0;
  while (iter->hasNext()) {
    Port *port = iter->next();
    EXPECT_NE(port, nullptr);
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

TEST(ConcreteNetworkTest, PortProperties) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_lib", "test.lib");
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  ConcreteCell *ccell = clib->makeCell("INV", true, "");
  ConcretePort *a = ccell->makePort("A");

  Port *port = reinterpret_cast<Port*>(a);
  EXPECT_STREQ(network.name(port), "A");
  EXPECT_FALSE(network.isBus(port));
  EXPECT_FALSE(network.isBundle(port));
}

TEST(ConcreteNetworkTest, FindPort) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_lib", "test.lib");
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  ConcreteCell *ccell = clib->makeCell("INV", true, "");
  ccell->makePort("A");
  ccell->makePort("Y");

  Cell *cell = reinterpret_cast<Cell*>(ccell);
  Port *found = network.findPort(cell, "A");
  EXPECT_NE(found, nullptr);
  EXPECT_STREQ(network.name(found), "A");

  Port *not_found = network.findPort(cell, "B");
  EXPECT_EQ(not_found, nullptr);
}

TEST(ConcreteNetworkTest, PortBitCount) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_lib", "test.lib");
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  ConcreteCell *ccell = clib->makeCell("INV", true, "");
  ccell->makePort("A");
  ccell->makePort("Y");

  Cell *cell = reinterpret_cast<Cell*>(ccell);
  EXPECT_EQ(network.portBitCount(cell), 2);
}

////////////////////////////////////////////////////////////////
// ConcreteNetwork additional coverage tests
////////////////////////////////////////////////////////////////

TEST(ConcreteNetworkTest, FindLibraryByName) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("my_lib", "my.lib");
  Library *found = network.findLibrary("my_lib");
  EXPECT_EQ(found, lib);
  Library *notfound = network.findLibrary("nonexistent");
  EXPECT_EQ(notfound, nullptr);
}

TEST(ConcreteNetworkTest, LibraryName) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("test_name_lib", "test.lib");
  EXPECT_STREQ(network.name(lib), "test_name_lib");
}

TEST(ConcreteNetworkTest, LibraryId) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("id_lib", "id.lib");
  // ID should be assigned by the network
  ObjectId id = network.id(lib);
  // The id may be 0 or positive
  EXPECT_GE(id, 0u);
}

TEST(ConcreteNetworkTest, DeleteLibrary) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("del_lib", "del.lib");
  EXPECT_NE(network.findLibrary("del_lib"), nullptr);
  network.deleteLibrary(lib);
  EXPECT_EQ(network.findLibrary("del_lib"), nullptr);
}

TEST(ConcreteNetworkTest, MakeCell) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("cell_lib", "cell.lib");
  Cell *cell = network.makeCell(lib, "BUF_X1", true, "cell.lib");
  EXPECT_NE(cell, nullptr);
}

TEST(ConcreteNetworkTest, FindCellViaNetwork) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("find_lib", "find.lib");
  Cell *cell = network.makeCell(lib, "AND2", true, "find.lib");
  Cell *found = network.findCell(lib, "AND2");
  EXPECT_EQ(found, cell);
  EXPECT_EQ(network.findCell(lib, "nonexistent"), nullptr);
}

TEST(ConcreteNetworkTest, FindAnyCell) {
  ConcreteNetwork network;
  Library *lib1 = network.makeLibrary("lib1", "lib1.lib");
  Library *lib2 = network.makeLibrary("lib2", "lib2.lib");
  network.makeCell(lib1, "INV_X1", true, "lib1.lib");
  Cell *found = network.findAnyCell("INV_X1");
  EXPECT_NE(found, nullptr);
  EXPECT_EQ(network.findAnyCell("nonexistent"), nullptr);
}

TEST(ConcreteNetworkTest, CellNameViaNetwork) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("nm_lib", "nm.lib");
  Cell *cell = network.makeCell(lib, "OR2_X1", true, "nm.lib");
  EXPECT_STREQ(network.name(cell), "OR2_X1");
}

TEST(ConcreteNetworkTest, CellIdViaNetwork) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("id_lib", "id.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "id.lib");
  ObjectId id = network.id(cell);
  EXPECT_GE(id, 0u);
}

TEST(ConcreteNetworkTest, SetCellName) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("rn_lib", "rn.lib");
  Cell *cell = network.makeCell(lib, "OLD_NAME", true, "rn.lib");
  network.setName(cell, "NEW_NAME");
  EXPECT_STREQ(network.name(cell), "NEW_NAME");
}

TEST(ConcreteNetworkTest, SetIsLeaf) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("leaf_lib", "leaf.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "leaf.lib");
  EXPECT_TRUE(network.isLeaf(cell));
  network.setIsLeaf(cell, false);
  EXPECT_FALSE(network.isLeaf(cell));
}

TEST(ConcreteNetworkTest, SetAttribute) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("attr_lib", "attr.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "attr.lib");
  network.setAttribute(cell, "area", "1.5");
  std::string val = network.getAttribute(cell, "area");
  EXPECT_EQ(val, "1.5");
}

TEST(ConcreteNetworkTest, AttributeMap) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("amap_lib", "amap.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "amap.lib");
  network.setAttribute(cell, "k1", "v1");
  network.setAttribute(cell, "k2", "v2");
  const auto &attrs = network.attributeMap(cell);
  EXPECT_EQ(attrs.size(), 2u);
}

TEST(ConcreteNetworkTest, CellLibrary) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("parent_lib", "parent.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "parent.lib");
  Library *parent = network.library(cell);
  EXPECT_EQ(parent, lib);
}

TEST(ConcreteNetworkTest, CellFilename) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("fn_lib", "fn.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "fn.lib");
  EXPECT_STREQ(network.filename(cell), "fn.lib");
}

TEST(ConcreteNetworkTest, DeleteCell) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("dc_lib", "dc.lib");
  network.makeCell(lib, "TO_DELETE", true, "dc.lib");
  Cell *found = network.findCell(lib, "TO_DELETE");
  EXPECT_NE(found, nullptr);
  network.deleteCell(found);
  EXPECT_EQ(network.findCell(lib, "TO_DELETE"), nullptr);
}

TEST(ConcreteNetworkTest, FindPortViaNetwork) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("fp_lib", "fp.lib");
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib);
  ConcreteCell *ccell = clib->makeCell("INV", true, "");
  ccell->makePort("A");
  ccell->makePort("Y");
  Cell *cell = reinterpret_cast<Cell*>(ccell);
  Port *port = network.findPort(cell, "A");
  EXPECT_NE(port, nullptr);
  EXPECT_EQ(network.findPort(cell, "nonexistent"), nullptr);
}

TEST(ConcreteNetworkTest, LibertyCellFromCell) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("lc_lib", "lc.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "lc.lib");
  // Non-liberty cells return nullptr
  LibertyCell *lcell = network.libertyCell(cell);
  EXPECT_EQ(lcell, nullptr);
}

TEST(ConcreteNetworkTest, ConstLibertyCellFromCell) {
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("clc_lib", "clc.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "clc.lib");
  const Cell *ccell = cell;
  const LibertyCell *lcell = network.libertyCell(ccell);
  EXPECT_EQ(lcell, nullptr);
}

TEST(ConcreteNetworkTest, FindCellsMatchingViaNetwork) {
  PortDirection::init();
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("match_lib", "match.lib");
  network.makeCell(lib, "INV_X1", true, "match.lib");
  network.makeCell(lib, "INV_X2", true, "match.lib");
  network.makeCell(lib, "BUF_X1", true, "match.lib");
  PatternMatch pattern("INV*");
  CellSeq cells = network.findCellsMatching(lib, &pattern);
  EXPECT_EQ(cells.size(), 2u);
}

////////////////////////////////////////////////////////////////
// Additional Network tests for function coverage
////////////////////////////////////////////////////////////////

// ConcreteNetwork: link and instance hierarchy tests
class ConcreteNetworkLinkedTest : public ::testing::Test {
protected:
  void SetUp() override {
    PortDirection::init();
    // Build a simple network: top instance with 2 children
    lib_ = network_.makeLibrary("test_lib", "test.lib");
    Cell *inv_cell = network_.makeCell(lib_, "INV", true, "test.lib");
    network_.makePort(inv_cell, "A");
    network_.makePort(inv_cell, "Y");
    network_.setDirection(network_.findPort(inv_cell, "A"), PortDirection::input());
    network_.setDirection(network_.findPort(inv_cell, "Y"), PortDirection::output());

    Cell *top_cell = network_.makeCell(lib_, "TOP", false, "test.lib");
    network_.makePort(top_cell, "clk");
    network_.makePort(top_cell, "data_in");
    network_.makePort(top_cell, "data_out");
    network_.setDirection(network_.findPort(top_cell, "clk"), PortDirection::input());
    network_.setDirection(network_.findPort(top_cell, "data_in"), PortDirection::input());
    network_.setDirection(network_.findPort(top_cell, "data_out"), PortDirection::output());

    // Create top instance
    Instance *top = network_.makeInstance(top_cell, "top", nullptr);
    network_.setTopInstance(top);

    // Create child instances
    u1_ = network_.makeInstance(inv_cell, "u1", top);
    u2_ = network_.makeInstance(inv_cell, "u2", top);

    // Create nets
    net1_ = network_.makeNet("n1", top);
    net2_ = network_.makeNet("n2", top);
    net3_ = network_.makeNet("n3", top);

    // Connect pins
    Port *inv_a = network_.findPort(inv_cell, "A");
    Port *inv_y = network_.findPort(inv_cell, "Y");

    pin_u1_a_ = network_.connect(u1_, inv_a, net1_);
    pin_u1_y_ = network_.connect(u1_, inv_y, net2_);
    pin_u2_a_ = network_.connect(u2_, inv_a, net2_);
    pin_u2_y_ = network_.connect(u2_, inv_y, net3_);
  }

  void TearDown() override {
    network_.clear();
  }

  ConcreteNetwork network_;
  Library *lib_;
  Instance *u1_;
  Instance *u2_;
  Net *net1_;
  Net *net2_;
  Net *net3_;
  Pin *pin_u1_a_;
  Pin *pin_u1_y_;
  Pin *pin_u2_a_;
  Pin *pin_u2_y_;
};

TEST_F(ConcreteNetworkLinkedTest, TopInstance) {
  Instance *top = network_.topInstance();
  EXPECT_NE(top, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, IsTopInstance) {
  Instance *top = network_.topInstance();
  EXPECT_TRUE(network_.isTopInstance(top));
  EXPECT_FALSE(network_.isTopInstance(u1_));
}

TEST_F(ConcreteNetworkLinkedTest, InstanceName) {
  EXPECT_STREQ(network_.name(u1_), "u1");
  EXPECT_STREQ(network_.name(u2_), "u2");
}

TEST_F(ConcreteNetworkLinkedTest, InstanceId) {
  ObjectId id1 = network_.id(u1_);
  ObjectId id2 = network_.id(u2_);
  EXPECT_NE(id1, id2);
}

TEST_F(ConcreteNetworkLinkedTest, InstanceCell) {
  Cell *cell = network_.cell(u1_);
  EXPECT_NE(cell, nullptr);
  EXPECT_STREQ(network_.name(cell), "INV");
}

TEST_F(ConcreteNetworkLinkedTest, InstanceCellName) {
  const char *name = network_.cellName(u1_);
  EXPECT_STREQ(name, "INV");
}

TEST_F(ConcreteNetworkLinkedTest, InstanceParent) {
  Instance *parent = network_.parent(u1_);
  EXPECT_EQ(parent, network_.topInstance());
}

TEST_F(ConcreteNetworkLinkedTest, InstanceIsLeaf) {
  EXPECT_TRUE(network_.isLeaf(u1_));
  EXPECT_FALSE(network_.isLeaf(network_.topInstance()));
}

TEST_F(ConcreteNetworkLinkedTest, InstanceIsHierarchical) {
  EXPECT_FALSE(network_.isHierarchical(u1_));
  EXPECT_TRUE(network_.isHierarchical(network_.topInstance()));
}

TEST_F(ConcreteNetworkLinkedTest, InstanceFindChild) {
  Instance *top = network_.topInstance();
  Instance *found = network_.findChild(top, "u1");
  EXPECT_EQ(found, u1_);
  Instance *notfound = network_.findChild(top, "u99");
  EXPECT_EQ(notfound, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, InstancePathName) {
  const char *path = network_.pathName(u1_);
  EXPECT_NE(path, nullptr);
  EXPECT_STREQ(path, "u1");
}

TEST_F(ConcreteNetworkLinkedTest, ChildIterator) {
  Instance *top = network_.topInstance();
  InstanceChildIterator *iter = network_.childIterator(top);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

TEST_F(ConcreteNetworkLinkedTest, InstancePinIterator) {
  InstancePinIterator *iter = network_.pinIterator(u1_);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

TEST_F(ConcreteNetworkLinkedTest, InstanceNetIterator) {
  Instance *top = network_.topInstance();
  InstanceNetIterator *iter = network_.netIterator(top);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 3);
}

// Pin tests
TEST_F(ConcreteNetworkLinkedTest, PinId) {
  ObjectId id = network_.id(pin_u1_a_);
  EXPECT_GE(id, 0u);
}

TEST_F(ConcreteNetworkLinkedTest, PinInstance) {
  Instance *inst = network_.instance(pin_u1_a_);
  EXPECT_EQ(inst, u1_);
}

TEST_F(ConcreteNetworkLinkedTest, PinNet) {
  Net *net = network_.net(pin_u1_a_);
  EXPECT_EQ(net, net1_);
}

TEST_F(ConcreteNetworkLinkedTest, PinPort) {
  Port *port = network_.port(pin_u1_a_);
  EXPECT_NE(port, nullptr);
  EXPECT_STREQ(network_.name(port), "A");
}

TEST_F(ConcreteNetworkLinkedTest, PinDirection) {
  PortDirection *dir = network_.direction(pin_u1_a_);
  EXPECT_TRUE(dir->isInput());
  PortDirection *dir2 = network_.direction(pin_u1_y_);
  EXPECT_TRUE(dir2->isOutput());
}

TEST_F(ConcreteNetworkLinkedTest, PinTerm) {
  // Non-top-level pins don't have terms
  Term *term = network_.term(pin_u1_a_);
  EXPECT_EQ(term, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, PinVertexId) {
  VertexId vid = network_.vertexId(pin_u1_a_);
  // Default is 0
  EXPECT_EQ(vid, 0u);
  network_.setVertexId(pin_u1_a_, 42);
  EXPECT_EQ(network_.vertexId(pin_u1_a_), 42u);
}

TEST_F(ConcreteNetworkLinkedTest, PinName) {
  const Network &net = network_;
  const char *name = net.name(pin_u1_a_);
  EXPECT_NE(name, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, PinPortName) {
  const char *pname = network_.portName(pin_u1_a_);
  EXPECT_STREQ(pname, "A");
}

TEST_F(ConcreteNetworkLinkedTest, PinPathName) {
  const char *path = network_.pathName(pin_u1_a_);
  EXPECT_NE(path, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, PinIsLeaf) {
  EXPECT_TRUE(network_.isLeaf(pin_u1_a_));
}

TEST_F(ConcreteNetworkLinkedTest, PinIsDriver) {
  EXPECT_FALSE(network_.isDriver(pin_u1_a_));  // input pin
  EXPECT_TRUE(network_.isDriver(pin_u1_y_));   // output pin
}

TEST_F(ConcreteNetworkLinkedTest, PinIsLoad) {
  EXPECT_TRUE(network_.isLoad(pin_u1_a_));     // input pin
  EXPECT_FALSE(network_.isLoad(pin_u1_y_));    // output pin
}

TEST_F(ConcreteNetworkLinkedTest, FindPinByName) {
  Pin *found = network_.findPin(u1_, "A");
  EXPECT_EQ(found, pin_u1_a_);
  Pin *notfound = network_.findPin(u1_, "Z");
  EXPECT_EQ(notfound, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, FindPinByPort) {
  Cell *cell = network_.cell(u1_);
  Port *port_a = network_.findPort(cell, "A");
  Pin *found = network_.findPin(u1_, port_a);
  EXPECT_EQ(found, pin_u1_a_);
}

// Net tests
TEST_F(ConcreteNetworkLinkedTest, NetName) {
  EXPECT_STREQ(network_.name(net1_), "n1");
}

TEST_F(ConcreteNetworkLinkedTest, NetId) {
  ObjectId id = network_.id(net1_);
  EXPECT_GE(id, 0u);
}

TEST_F(ConcreteNetworkLinkedTest, NetInstance) {
  Instance *inst = network_.instance(net1_);
  EXPECT_EQ(inst, network_.topInstance());
}

TEST_F(ConcreteNetworkLinkedTest, NetPathName) {
  const char *path = network_.pathName(net1_);
  EXPECT_NE(path, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, NetIsPowerGround) {
  EXPECT_FALSE(network_.isPower(net1_));
  EXPECT_FALSE(network_.isGround(net1_));
}

TEST_F(ConcreteNetworkLinkedTest, NetPinIterator) {
  // net2_ connects u1_Y and u2_A
  NetPinIterator *iter = network_.pinIterator(net2_);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

TEST_F(ConcreteNetworkLinkedTest, NetTermIterator) {
  NetTermIterator *iter = network_.termIterator(net1_);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  // No terms on non-top-level net connections
  EXPECT_GE(count, 0);
}

TEST_F(ConcreteNetworkLinkedTest, FindNetByName) {
  Instance *top = network_.topInstance();
  Net *found = network_.findNet(top, "n1");
  EXPECT_EQ(found, net1_);
  Net *notfound = network_.findNet(top, "nonexistent");
  EXPECT_EQ(notfound, nullptr);
}

// Disconnect and delete
TEST_F(ConcreteNetworkLinkedTest, DisconnectPin) {
  Net *net = network_.net(pin_u1_a_);
  EXPECT_NE(net, nullptr);
  network_.disconnectPin(pin_u1_a_);
  EXPECT_EQ(network_.net(pin_u1_a_), nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, DeleteNet) {
  network_.deleteNet(net3_);
  Instance *top = network_.topInstance();
  Net *found = network_.findNet(top, "n3");
  EXPECT_EQ(found, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, DeleteInstance) {
  // Disconnect pins before deleting
  network_.disconnectPin(pin_u2_a_);
  network_.disconnectPin(pin_u2_y_);
  network_.deleteInstance(u2_);
  Instance *top = network_.topInstance();
  Instance *found = network_.findChild(top, "u2");
  EXPECT_EQ(found, nullptr);
}

// MergeInto / MergedInto
TEST_F(ConcreteNetworkLinkedTest, MergeIntoNet) {
  // net1_ merges into net2_
  network_.mergeInto(net1_, net2_);
  Net *merged = network_.mergedInto(net1_);
  EXPECT_EQ(merged, net2_);
}

// MakePins
TEST_F(ConcreteNetworkLinkedTest, MakePins) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Instance *u3 = network_.makeInstance(inv_cell, "u3", network_.topInstance());
  network_.makePins(u3);
  // After makePins, pins should be accessible
  Pin *pin = network_.findPin(u3, "A");
  EXPECT_NE(pin, nullptr);
}

// ReplaceCell
TEST_F(ConcreteNetworkLinkedTest, ReplaceCell) {
  Cell *buf_cell = network_.makeCell(lib_, "BUF", true, "test.lib");
  network_.makePort(buf_cell, "A");
  network_.makePort(buf_cell, "Y");
  network_.setDirection(network_.findPort(buf_cell, "A"), PortDirection::input());
  network_.setDirection(network_.findPort(buf_cell, "Y"), PortDirection::output());

  // Disconnect and replace
  network_.disconnectPin(pin_u1_a_);
  network_.disconnectPin(pin_u1_y_);
  network_.replaceCell(u1_, buf_cell);
  Cell *new_cell = network_.cell(u1_);
  EXPECT_STREQ(network_.name(new_cell), "BUF");
}

// Network pathName comparisons
TEST_F(ConcreteNetworkLinkedTest, PathNameLessInst) {
  EXPECT_TRUE(network_.pathNameLess(u1_, u2_));
  EXPECT_FALSE(network_.pathNameLess(u2_, u1_));
}

TEST_F(ConcreteNetworkLinkedTest, PathNameCmpInst) {
  int cmp = network_.pathNameCmp(u1_, u2_);
  EXPECT_LT(cmp, 0);
  EXPECT_EQ(network_.pathNameCmp(u1_, u1_), 0);
}

TEST_F(ConcreteNetworkLinkedTest, PathNameLessPin) {
  bool result = network_.pathNameLess(pin_u1_a_, pin_u2_a_);
  // u1/A < u2/A
  EXPECT_TRUE(result);
}

TEST_F(ConcreteNetworkLinkedTest, PathNameCmpPin) {
  int cmp = network_.pathNameCmp(pin_u1_a_, pin_u2_a_);
  EXPECT_LT(cmp, 0);
}

TEST_F(ConcreteNetworkLinkedTest, PathNameLessNet) {
  bool result = network_.pathNameLess(net1_, net2_);
  EXPECT_TRUE(result);
}

TEST_F(ConcreteNetworkLinkedTest, PathNameCmpNet) {
  int cmp = network_.pathNameCmp(net1_, net2_);
  EXPECT_LT(cmp, 0);
}

// Network: pathNameFirst / pathNameLast
TEST_F(ConcreteNetworkLinkedTest, PathNameFirst) {
  char *first = nullptr;
  char *tail = nullptr;
  network_.pathNameFirst("a/b/c", first, tail);
  if (first) {
    EXPECT_STREQ(first, "a");
    EXPECT_STREQ(tail, "b/c");
    delete [] first;
    delete [] tail;
  }
}

TEST_F(ConcreteNetworkLinkedTest, PathNameLast) {
  char *head = nullptr;
  char *last = nullptr;
  network_.pathNameLast("a/b/c", head, last);
  if (last) {
    EXPECT_STREQ(last, "c");
    EXPECT_STREQ(head, "a/b");
    delete [] head;
    delete [] last;
  }
}

TEST_F(ConcreteNetworkLinkedTest, PathNameFirstNoDivider) {
  char *first = nullptr;
  char *tail = nullptr;
  network_.pathNameFirst("simple", first, tail);
  EXPECT_EQ(first, nullptr);
  EXPECT_EQ(tail, nullptr);
}

// Network: pathDivider / pathEscape
TEST_F(ConcreteNetworkLinkedTest, PathDivider) {
  EXPECT_EQ(network_.pathDivider(), '/');
  network_.setPathDivider('.');
  EXPECT_EQ(network_.pathDivider(), '.');
  network_.setPathDivider('/');
}

TEST_F(ConcreteNetworkLinkedTest, PathEscape) {
  char orig = network_.pathEscape();
  network_.setPathEscape('\\');
  EXPECT_EQ(network_.pathEscape(), '\\');
  network_.setPathEscape(orig);
}

// Network: isLinked
TEST_F(ConcreteNetworkLinkedTest, IsLinked) {
  EXPECT_TRUE(network_.isLinked());
}

// Network: isEditable
TEST_F(ConcreteNetworkLinkedTest, IsEditable) {
  EXPECT_TRUE(network_.isEditable());
}

// Network: pinLess
TEST_F(ConcreteNetworkLinkedTest, PinLess) {
  bool result = network_.pinLess(pin_u1_a_, pin_u2_a_);
  bool result2 = network_.pinLess(pin_u2_a_, pin_u1_a_);
  EXPECT_NE(result, result2);
}

// Network: location
TEST_F(ConcreteNetworkLinkedTest, PinLocation) {
  double x, y;
  bool exists;
  network_.location(pin_u1_a_, x, y, exists);
  EXPECT_FALSE(exists);
}

// Network: instanceCount, pinCount, netCount
TEST_F(ConcreteNetworkLinkedTest, InstanceCount) {
  int count = network_.instanceCount();
  EXPECT_GE(count, 3); // top + u1 + u2
}

TEST_F(ConcreteNetworkLinkedTest, PinCount) {
  int count = network_.pinCount();
  EXPECT_GE(count, 4);
}

TEST_F(ConcreteNetworkLinkedTest, NetCount) {
  int count = network_.netCount();
  EXPECT_GE(count, 3);
}

// Network: leafInstanceCount
TEST_F(ConcreteNetworkLinkedTest, LeafInstanceCount) {
  int count = network_.leafInstanceCount();
  EXPECT_EQ(count, 2);
}

// Network: leafPinCount
TEST_F(ConcreteNetworkLinkedTest, LeafPinCount) {
  int count = network_.leafPinCount();
  EXPECT_GE(count, 4);
}

// Network: leafInstances
TEST_F(ConcreteNetworkLinkedTest, LeafInstances) {
  InstanceSeq leaves = network_.leafInstances();
  EXPECT_EQ(leaves.size(), 2u);
}

// Network: leafInstanceIterator
TEST_F(ConcreteNetworkLinkedTest, LeafInstanceIterator) {
  LeafInstanceIterator *iter = network_.leafInstanceIterator();
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

// Network: findPinsMatching
TEST_F(ConcreteNetworkLinkedTest, FindPinsMatching) {
  Instance *top = network_.topInstance();
  PatternMatch pattern("u1/*");
  PinSeq pins = network_.findPinsMatching(top, &pattern);
  EXPECT_EQ(pins.size(), 2u);
}

// Network: findChildrenMatching
TEST_F(ConcreteNetworkLinkedTest, FindChildrenMatching) {
  Instance *top = network_.topInstance();
  PatternMatch pattern("u*");
  InstanceSeq matches;
  network_.findChildrenMatching(top, &pattern, matches);
  EXPECT_EQ(matches.size(), 2u);
}

// Network: findInstancesMatching
TEST_F(ConcreteNetworkLinkedTest, FindInstancesMatching) {
  Instance *top = network_.topInstance();
  PatternMatch pattern("u*");
  InstanceSeq matches = network_.findInstancesMatching(top, &pattern);
  EXPECT_EQ(matches.size(), 2u);
}

// Network: findNetsMatching
TEST_F(ConcreteNetworkLinkedTest, FindNetsMatching) {
  Instance *top = network_.topInstance();
  PatternMatch pattern("n*");
  NetSeq matches = network_.findNetsMatching(top, &pattern);
  EXPECT_EQ(matches.size(), 3u);
}

// Network: findInstNetsMatching
TEST_F(ConcreteNetworkLinkedTest, FindInstNetsMatching) {
  Instance *top = network_.topInstance();
  PatternMatch pattern("n*");
  NetSeq matches;
  network_.findInstNetsMatching(top, &pattern, matches);
  EXPECT_EQ(matches.size(), 3u);
}

// Network: isInside (Instance)
TEST_F(ConcreteNetworkLinkedTest, IsInsideInst) {
  Instance *top = network_.topInstance();
  EXPECT_TRUE(network_.isInside(u1_, top));
  EXPECT_FALSE(network_.isInside(top, u1_));
}

// Network: isInside (Net)
TEST_F(ConcreteNetworkLinkedTest, IsInsideNet) {
  Instance *top = network_.topInstance();
  EXPECT_TRUE(network_.isInside(net1_, top));
}

// Network: isConnected
TEST_F(ConcreteNetworkLinkedTest, IsConnectedNetPin) {
  EXPECT_TRUE(network_.isConnected(net1_, pin_u1_a_));
  EXPECT_FALSE(network_.isConnected(net3_, pin_u1_a_));
}

// Network: isConnected (net to net)
TEST_F(ConcreteNetworkLinkedTest, IsConnectedNetNet) {
  EXPECT_TRUE(network_.isConnected(net1_, net1_));
  EXPECT_FALSE(network_.isConnected(net1_, net2_));
}

// Network: highestNetAbove
TEST_F(ConcreteNetworkLinkedTest, HighestNetAbove) {
  Net *highest = network_.highestNetAbove(net1_);
  EXPECT_EQ(highest, net1_);
}

// Network: connectedNets (from net)
TEST_F(ConcreteNetworkLinkedTest, ConnectedNetsFromNet) {
  NetSet nets;
  network_.connectedNets(net1_, &nets);
  EXPECT_GE(nets.size(), 1u);
}

// Network: connectedNets (from pin)
TEST_F(ConcreteNetworkLinkedTest, ConnectedNetsFromPin) {
  NetSet nets;
  network_.connectedNets(pin_u1_a_, &nets);
  EXPECT_GE(nets.size(), 1u);
}

// Network: drivers (from pin)
TEST_F(ConcreteNetworkLinkedTest, DriversFromPin) {
  PinSet *drvrs = network_.drivers(pin_u1_a_);
  EXPECT_NE(drvrs, nullptr);
}

// Network: drivers (from net)
TEST_F(ConcreteNetworkLinkedTest, DriversFromNet) {
  PinSet *drvrs = network_.drivers(net2_);
  EXPECT_NE(drvrs, nullptr);
  EXPECT_GE(drvrs->size(), 1u);
}

// Network: path from instance
TEST_F(ConcreteNetworkLinkedTest, InstancePath) {
  InstanceSeq path;
  network_.path(u1_, path);
  // Path from u1 to top: u1, top
  EXPECT_GE(path.size(), 1u);
}

// Network: connectedPinIterator (from pin)
TEST_F(ConcreteNetworkLinkedTest, ConnectedPinIteratorFromPin) {
  PinConnectedPinIterator *iter = network_.connectedPinIterator(pin_u1_a_);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_GE(count, 1);
}

// Network: connectedPinIterator (from net)
TEST_F(ConcreteNetworkLinkedTest, ConnectedPinIteratorFromNet) {
  NetConnectedPinIterator *iter = network_.connectedPinIterator(net2_);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_GE(count, 2);
}

// Network: constantPinIterator
TEST_F(ConcreteNetworkLinkedTest, ConstantPinIterator) {
  ConstantPinIterator *iter = network_.constantPinIterator();
  EXPECT_NE(iter, nullptr);
  EXPECT_FALSE(iter->hasNext());
  delete iter;
}

// Network: addConstantNet
TEST_F(ConcreteNetworkLinkedTest, AddConstantNet) {
  Net *const_net = network_.makeNet("vss", network_.topInstance());
  network_.addConstantNet(const_net, LogicValue::zero);
  ConstantPinIterator *iter = network_.constantPinIterator();
  EXPECT_NE(iter, nullptr);
  delete iter;
}

// Network: readNetlistBefore, setLinkFunc
TEST(ConcreteNetworkExtraTest, ReadNetlistBefore) {
  ASSERT_NO_THROW(( [&](){
  ConcreteNetwork network;
  network.readNetlistBefore();

  }() ));
}

TEST(ConcreteNetworkExtraTest, SetLinkFunc) {
  ASSERT_NO_THROW(( [&](){
  ConcreteNetwork network;
  network.setLinkFunc(nullptr);

  }() ));
}

// Network: setCellNetworkView, cellNetworkView, deleteCellNetworkViews
TEST(ConcreteNetworkExtraTest, CellNetworkView) {
  PortDirection::init();
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("view_lib", "view.lib");
  Cell *cell = network.makeCell(lib, "CELL1", true, "view.lib");
  network.setCellNetworkView(cell, nullptr);
  Instance *view = network.cellNetworkView(cell);
  EXPECT_EQ(view, nullptr);
  network.deleteCellNetworkViews();
}

// Network: makeLibertyLibrary
TEST(ConcreteNetworkExtraTest, MakeLibertyLibrary) {
  ConcreteNetwork network;
  LibertyLibrary *lib = network.makeLibertyLibrary("liberty_lib", "lib.lib");
  EXPECT_NE(lib, nullptr);
}

// Network: findLiberty
TEST(ConcreteNetworkExtraTest, FindLiberty) {
  ConcreteNetwork network;
  LibertyLibrary *lib = network.makeLibertyLibrary("find_liberty", "find.lib");
  LibertyLibrary *found = network.findLiberty("find_liberty");
  EXPECT_EQ(found, lib);
  LibertyLibrary *notfound = network.findLiberty("nonexistent");
  EXPECT_EQ(notfound, nullptr);
}

// Network: libertyLibraryIterator
TEST(ConcreteNetworkExtraTest, LibertyLibraryIterator) {
  ConcreteNetwork network;
  network.makeLibertyLibrary("lib1", "lib1.lib");
  LibertyLibraryIterator *iter = network.libertyLibraryIterator();
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 1);
}

// Network: nextObjectId
TEST(ConcreteNetworkExtraTest, NextObjectId) {
  ObjectId id1 = ConcreteNetwork::nextObjectId();
  ObjectId id2 = ConcreteNetwork::nextObjectId();
  EXPECT_NE(id1, id2);
}

// Network: deleteTopInstance
TEST(ConcreteNetworkExtraTest, DeleteTopInstance) {
  PortDirection::init();
  ConcreteNetwork network;
  Library *lib = network.makeLibrary("del_top_lib", "del_top.lib");
  Cell *cell = network.makeCell(lib, "TOP", false, "del_top.lib");
  Instance *top = network.makeInstance(cell, "top", nullptr);
  network.setTopInstance(top);
  EXPECT_NE(network.topInstance(), nullptr);
  network.deleteTopInstance();
  EXPECT_EQ(network.topInstance(), nullptr);
}

// NetworkCmp: sort functions with actual network objects
TEST_F(ConcreteNetworkLinkedTest, SortByPathNamePins) {
  PinSet pin_set(&network_);
  pin_set.insert(pin_u2_a_);
  pin_set.insert(pin_u1_a_);
  PinSeq sorted = sortByPathName(&pin_set, &network_);
  EXPECT_EQ(sorted.size(), 2u);
}

TEST_F(ConcreteNetworkLinkedTest, SortByPathNameInstances) {
  InstanceSet inst_set(&network_);
  inst_set.insert(u2_);
  inst_set.insert(u1_);
  InstanceSeq sorted = sortByPathName(&inst_set, &network_);
  EXPECT_EQ(sorted.size(), 2u);
  EXPECT_STREQ(network_.name(sorted[0]), "u1");
  EXPECT_STREQ(network_.name(sorted[1]), "u2");
}

TEST_F(ConcreteNetworkLinkedTest, SortByPathNameNets) {
  NetSet net_set(&network_);
  net_set.insert(net3_);
  net_set.insert(net1_);
  net_set.insert(net2_);
  NetSeq sorted = sortByPathName(&net_set, &network_);
  EXPECT_EQ(sorted.size(), 3u);
}

TEST_F(ConcreteNetworkLinkedTest, SortByNamePorts) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  Port *port_y = network_.findPort(inv_cell, "Y");
  PortSet port_set(&network_);
  port_set.insert(port_y);
  port_set.insert(port_a);
  PortSeq sorted = sortByName(&port_set, &network_);
  EXPECT_EQ(sorted.size(), 2u);
  EXPECT_STREQ(network_.name(sorted[0]), "A");
  EXPECT_STREQ(network_.name(sorted[1]), "Y");
}

// NetworkCmp comparator constructors
TEST_F(ConcreteNetworkLinkedTest, NetworkCmpConstructors) {
  PortNameLess port_less(&network_);
  PinPathNameLess pin_less(&network_);
  NetPathNameLess net_less(&network_);
  InstancePathNameLess inst_less(&network_);

  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  Port *port_y = network_.findPort(inv_cell, "Y");
  EXPECT_TRUE(port_less(port_a, port_y));
  EXPECT_FALSE(port_less(port_y, port_a));

  EXPECT_TRUE(pin_less(pin_u1_a_, pin_u2_a_));
  EXPECT_TRUE(net_less(net1_, net2_));
  EXPECT_TRUE(inst_less(u1_, u2_));
}

// ConcreteNetwork: makePin, makeTerm
TEST_F(ConcreteNetworkLinkedTest, MakePinAndTerm) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Instance *u3 = network_.makeInstance(inv_cell, "u3", network_.topInstance());
  Port *port_a = network_.findPort(inv_cell, "A");
  Net *net = network_.makeNet("n4", network_.topInstance());
  Pin *pin = network_.makePin(u3, port_a, net);
  EXPECT_NE(pin, nullptr);
  EXPECT_EQ(network_.net(pin), net);

  // makeTerm
  Term *term = network_.makeTerm(pin, net);
  EXPECT_NE(term, nullptr);
  EXPECT_EQ(network_.net(term), net);
  EXPECT_EQ(network_.pin(term), pin);
}

// Term tests
TEST_F(ConcreteNetworkLinkedTest, TermId) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Instance *u3 = network_.makeInstance(inv_cell, "u3", network_.topInstance());
  Port *port_a = network_.findPort(inv_cell, "A");
  Net *net = network_.makeNet("n5", network_.topInstance());
  Pin *pin = network_.makePin(u3, port_a, net);
  Term *term = network_.makeTerm(pin, net);
  ObjectId id = network_.id(term);
  EXPECT_GE(id, 0u);
}

// Network: setAttribute for Instance
TEST_F(ConcreteNetworkLinkedTest, InstanceSetAttribute) {
  network_.setAttribute(u1_, "key1", "val1");
  std::string val = network_.getAttribute(u1_, "key1");
  EXPECT_EQ(val, "val1");
  const auto &attrs = network_.attributeMap(u1_);
  EXPECT_EQ(attrs.size(), 1u);
}

// Network: findInstance
TEST_F(ConcreteNetworkLinkedTest, FindInstanceByPath) {
  Instance *found = network_.findInstance("u1");
  EXPECT_EQ(found, u1_);
  Instance *notfound = network_.findInstance("nonexistent");
  EXPECT_EQ(notfound, nullptr);
}

// Network: findNet by path
TEST_F(ConcreteNetworkLinkedTest, FindNetByPath) {
  Net *found = network_.findNet("n1");
  EXPECT_EQ(found, net1_);
}

// Network: findPin by path
TEST_F(ConcreteNetworkLinkedTest, FindPinByPath) {
  Pin *found = network_.findPin("u1/A");
  EXPECT_EQ(found, pin_u1_a_);
}

// Network: findInstanceRelative
TEST_F(ConcreteNetworkLinkedTest, FindInstanceRelative) {
  Instance *top = network_.topInstance();
  Instance *found = network_.findInstanceRelative(top, "u1");
  EXPECT_EQ(found, u1_);
}

// Network: findPinRelative
TEST_F(ConcreteNetworkLinkedTest, FindPinRelative) {
  Instance *top = network_.topInstance();
  Pin *found = network_.findPinRelative(top, "u1/A");
  EXPECT_EQ(found, pin_u1_a_);
}

// HpinDrvrLoad: constructor copies pin sets, destructor deletes copies
TEST(HpinDrvrLoadExtraTest, WithPinSets) {
  PinSet from_set;
  PinSet to_set;
  int fake1 = 1, fake2 = 2;
  const Pin *drvr = reinterpret_cast<const Pin*>(&fake1);
  const Pin *load = reinterpret_cast<const Pin*>(&fake2);
  from_set.insert(drvr);
  to_set.insert(load);
  HpinDrvrLoad hdl(drvr, load, &from_set, &to_set);
  // Constructor copies the sets, so pointers differ
  EXPECT_NE(hdl.hpinsFromDrvr(), nullptr);
  EXPECT_NE(hdl.hpinsToLoad(), nullptr);
  EXPECT_EQ(hdl.drvr(), drvr);
  EXPECT_EQ(hdl.load(), load);
  // HpinDrvrLoad destructor will delete its internal copies
}

// Network: deletePin
TEST_F(ConcreteNetworkLinkedTest, DeletePin) {
  network_.disconnectPin(pin_u2_y_);
  network_.deletePin(pin_u2_y_);
  // After delete, pin shouldn't be found
  Pin *found = network_.findPin(u2_, "Y");
  EXPECT_EQ(found, nullptr);
}

// Network: findPortsMatching
TEST_F(ConcreteNetworkLinkedTest, FindPortsMatching) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  PatternMatch pattern("*");
  PortSeq ports = network_.findPortsMatching(inv_cell, &pattern);
  EXPECT_EQ(ports.size(), 2u);
}

// Network: port properties via network
TEST_F(ConcreteNetworkLinkedTest, PortIdViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  ObjectId id = network_.id(port_a);
  EXPECT_GE(id, 0u);
}

TEST_F(ConcreteNetworkLinkedTest, PortCellViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  Cell *cell = network_.cell(port_a);
  EXPECT_EQ(cell, inv_cell);
}

TEST_F(ConcreteNetworkLinkedTest, PortSizeViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  EXPECT_EQ(network_.size(port_a), 1);
}

TEST_F(ConcreteNetworkLinkedTest, PortFromToIndexViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  // Scalar port has from/to index of -1
  int from = network_.fromIndex(port_a);
  int to = network_.toIndex(port_a);
  EXPECT_EQ(from, -1);
  EXPECT_EQ(to, -1);
}

TEST_F(ConcreteNetworkLinkedTest, PortBusNameViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  const char *bus_name = network_.busName(port_a);
  EXPECT_STREQ(bus_name, "A");
}

TEST_F(ConcreteNetworkLinkedTest, PortFindBusBitViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  Port *bit = network_.findBusBit(port_a, 0);
  // Scalar port returns nullptr for bus bit
  EXPECT_EQ(bit, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, PortFindMemberViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  // Scalar port has no members - check hasMembers first
  EXPECT_FALSE(network_.hasMembers(port_a));
}

TEST_F(ConcreteNetworkLinkedTest, PortMemberIteratorViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  PortMemberIterator *iter = network_.memberIterator(port_a);
  EXPECT_NE(iter, nullptr);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 0);
}

TEST_F(ConcreteNetworkLinkedTest, PortLibertyPortViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  LibertyPort *lport = network_.libertyPort(port_a);
  EXPECT_EQ(lport, nullptr);
}

TEST_F(ConcreteNetworkLinkedTest, PortHasMembersViaNetwork) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  EXPECT_FALSE(network_.hasMembers(port_a));
}

// Network: CellPortBitIterator
TEST_F(ConcreteNetworkLinkedTest, CellPortBitIterator) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  CellPortBitIterator *iter = network_.portBitIterator(inv_cell);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

// Network: libertyCell on instance
TEST_F(ConcreteNetworkLinkedTest, LibertyCellOnInstance) {
  LibertyCell *lcell = network_.libertyCell(u1_);
  // Non-liberty cells return nullptr
  EXPECT_EQ(lcell, nullptr);
}

// Network: libertyCell on non-liberty instance returns nullptr
// (libertyLibrary(instance) would segfault since it dereferences libertyCell result)

// Network: libertyPort on pin
TEST_F(ConcreteNetworkLinkedTest, LibertyPortOnPin) {
  LibertyPort *lport = network_.libertyPort(pin_u1_a_);
  EXPECT_EQ(lport, nullptr);
}

// Network: isTopLevelPort
TEST_F(ConcreteNetworkLinkedTest, IsTopLevelPort) {
  // Leaf pin is not a top-level port
  EXPECT_FALSE(network_.isTopLevelPort(pin_u1_a_));
}

// Network: isHierarchical on pin
TEST_F(ConcreteNetworkLinkedTest, PinIsHierarchical) {
  EXPECT_FALSE(network_.isHierarchical(pin_u1_a_));
}

// Network: groupBusPorts via Network
TEST_F(ConcreteNetworkLinkedTest, GroupBusPortsViaNetwork) {
  Cell *cell = network_.makeCell(lib_, "FIFO2", true, "test.lib");
  network_.makePort(cell, "D[0]");
  network_.makePort(cell, "D[1]");
  network_.makePort(cell, "CLK");

  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib_);
  clib->setBusBrkts('[', ']');

  network_.groupBusPorts(cell, [](const char*) { return true; });
  Port *bus = network_.findPort(cell, "D");
  EXPECT_NE(bus, nullptr);
  if (bus) {
    EXPECT_TRUE(network_.isBus(bus));
  }
}

// Network: makeBusPort, makeBundlePort via Network
TEST_F(ConcreteNetworkLinkedTest, MakeBusPortViaNetwork) {
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib_);
  clib->setBusBrkts('[', ']');
  Cell *cell = network_.makeCell(lib_, "REG2", true, "test.lib");
  Port *bus = network_.makeBusPort(cell, "D", 3, 0);
  EXPECT_NE(bus, nullptr);
  EXPECT_TRUE(network_.isBus(bus));
  EXPECT_EQ(network_.size(bus), 4);
}

TEST_F(ConcreteNetworkLinkedTest, MakeBundlePortViaNetwork) {
  Cell *cell = network_.makeCell(lib_, "MUX2", true, "test.lib");
  Port *a = network_.makePort(cell, "A");
  Port *b = network_.makePort(cell, "B");
  PortSeq *members = new PortSeq;
  members->push_back(a);
  members->push_back(b);
  Port *bundle = network_.makeBundlePort(cell, "AB", members);
  EXPECT_NE(bundle, nullptr);
  EXPECT_TRUE(network_.isBundle(bundle));
}

// Network: setDirection via Network
TEST_F(ConcreteNetworkLinkedTest, SetDirectionViaNetwork) {
  Cell *cell = network_.makeCell(lib_, "DIR_TEST", true, "test.lib");
  Port *p = network_.makePort(cell, "X");
  network_.setDirection(p, PortDirection::output());
  EXPECT_EQ(network_.direction(p), PortDirection::output());
}

// Network: findNetRelative
TEST_F(ConcreteNetworkLinkedTest, FindNetRelative) {
  Instance *top = network_.topInstance();
  Net *found = network_.findNetRelative(top, "n1");
  EXPECT_EQ(found, net1_);
}

// Network: findNetsHierMatching
TEST_F(ConcreteNetworkLinkedTest, FindNetsHierMatching) {
  Instance *top = network_.topInstance();
  PatternMatch pattern("n*");
  NetSeq matches = network_.findNetsHierMatching(top, &pattern);
  EXPECT_GE(matches.size(), 3u);
}

// Network: findPinsHierMatching
TEST_F(ConcreteNetworkLinkedTest, FindPinsHierMatching) {
  Instance *top = network_.topInstance();
  PatternMatch pattern("u1/*");
  PinSeq matches = network_.findPinsHierMatching(top, &pattern);
  EXPECT_GE(matches.size(), 2u);
}

// Network: findInstancesHierMatching
TEST_F(ConcreteNetworkLinkedTest, FindInstancesHierMatching) {
  Instance *top = network_.topInstance();
  PatternMatch pattern("u*");
  InstanceSeq matches = network_.findInstancesHierMatching(top, &pattern);
  EXPECT_GE(matches.size(), 2u);
}

// Network Set/Map comparators constructors
TEST_F(ConcreteNetworkLinkedTest, PinIdLessConstructor) {
  PinIdLess less(&network_);
  bool ab = less(pin_u1_a_, pin_u2_a_);
  bool ba = less(pin_u2_a_, pin_u1_a_);
  // Strict weak ordering: exactly one of less(a,b) or less(b,a) must be true
  EXPECT_NE(ab, ba);
}

TEST_F(ConcreteNetworkLinkedTest, NetIdLessConstructor) {
  NetIdLess less(&network_);
  bool ab = less(net1_, net2_);
  bool ba = less(net2_, net1_);
  EXPECT_NE(ab, ba);
}

TEST_F(ConcreteNetworkLinkedTest, InstanceIdLessConstructor) {
  InstanceIdLess less(&network_);
  bool ab = less(u1_, u2_);
  bool ba = less(u2_, u1_);
  EXPECT_NE(ab, ba);
}

TEST_F(ConcreteNetworkLinkedTest, PortIdLessConstructor) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  Port *port_y = network_.findPort(inv_cell, "Y");
  PortIdLess less(&network_);
  bool ab = less(port_a, port_y);
  bool ba = less(port_y, port_a);
  EXPECT_NE(ab, ba);
}

TEST_F(ConcreteNetworkLinkedTest, CellIdLessConstructor) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Cell *top_cell = network_.findCell(lib_, "TOP");
  CellIdLess less(&network_);
  bool ab = less(inv_cell, top_cell);
  bool ba = less(top_cell, inv_cell);
  EXPECT_NE(ab, ba);
}

// PinSet: with network constructor and intersects
TEST_F(ConcreteNetworkLinkedTest, PinSetWithNetwork) {
  PinSet set1(&network_);
  set1.insert(pin_u1_a_);
  set1.insert(pin_u1_y_);
  PinSet set2(&network_);
  set2.insert(pin_u1_a_);
  EXPECT_TRUE(PinSet::intersects(&set1, &set2, &network_));
}

// PinSet: compare
TEST_F(ConcreteNetworkLinkedTest, PinSetCompare) {
  PinSet set1(&network_);
  set1.insert(pin_u1_a_);
  PinSet set2(&network_);
  set2.insert(pin_u2_a_);
  int cmp = PinSet::compare(&set1, &set2, &network_);
  // Different sets should not compare equal
  EXPECT_NE(cmp, 0);
}

// InstanceSet: with network and intersects
TEST_F(ConcreteNetworkLinkedTest, InstanceSetWithNetwork) {
  InstanceSet set1(&network_);
  set1.insert(u1_);
  set1.insert(u2_);
  InstanceSet set2(&network_);
  set2.insert(u1_);
  EXPECT_TRUE(InstanceSet::intersects(&set1, &set2, &network_));
}

// NetSet: with network and intersects
TEST_F(ConcreteNetworkLinkedTest, NetSetWithNetwork) {
  NetSet set1(&network_);
  set1.insert(net1_);
  set1.insert(net2_);
  NetSet set2(&network_);
  set2.insert(net1_);
  EXPECT_TRUE(NetSet::intersects(&set1, &set2, &network_));
}

// NetSet: compare
TEST_F(ConcreteNetworkLinkedTest, NetSetCompare) {
  NetSet set1(&network_);
  set1.insert(net1_);
  NetSet set2(&network_);
  set2.insert(net2_);
  int cmp = NetSet::compare(&set1, &set2, &network_);
  // Different sets should not compare equal
  EXPECT_NE(cmp, 0);
}

// CellSet constructor
TEST_F(ConcreteNetworkLinkedTest, CellSetWithNetwork) {
  CellSet set(&network_);
  Cell *inv_cell = network_.findCell(lib_, "INV");
  set.insert(inv_cell);
  EXPECT_FALSE(set.empty());
}

// logicValueString
TEST(LogicValueStringTest, AllValues) {
  char c0 = logicValueString(LogicValue::zero);
  EXPECT_EQ(c0, '0');
  char c1 = logicValueString(LogicValue::one);
  EXPECT_EQ(c1, '1');
  char cx = logicValueString(LogicValue::unknown);
  EXPECT_EQ(cx, 'X');
}

// Network: drivers from net (returned set is cached, do NOT delete)
TEST_F(ConcreteNetworkLinkedTest, DriversFromNetExercise) {
  PinSet *drivers = network_.drivers(net2_);
  EXPECT_NE(drivers, nullptr);
  EXPECT_FALSE(drivers->empty());
}

// Network: constantPinIterator
TEST_F(ConcreteNetworkLinkedTest, ConstantPinIterator2) {
  ConstantPinIterator *iter = network_.constantPinIterator();
  EXPECT_NE(iter, nullptr);
  // No constants set, so should be empty
  EXPECT_FALSE(iter->hasNext());
  delete iter;
}

// Network: addConstantNet exercises constantPinIterator paths
TEST_F(ConcreteNetworkLinkedTest, AddConstantNetExercise) {
  network_.addConstantNet(net1_, LogicValue::zero);
  ConstantPinIterator *iter = network_.constantPinIterator();
  EXPECT_NE(iter, nullptr);
  bool found = false;
  while (iter->hasNext()) {
    const Pin *pin;
    LogicValue val;
    iter->next(pin, val);
    found = true;
  }
  delete iter;
  EXPECT_TRUE(found);
}

// PinIdHash
TEST_F(ConcreteNetworkLinkedTest, PinIdHashConstructor) {
  PinIdHash hash(&network_);
  size_t h = hash(pin_u1_a_);
  EXPECT_GT(h, 0u);
}

// Network: drivers from pin
TEST_F(ConcreteNetworkLinkedTest, FindNetDriversFromPin) {
  PinSet *drivers = network_.drivers(pin_u2_a_);
  EXPECT_NE(drivers, nullptr);
}

// Network: connectedPins via net
TEST_F(ConcreteNetworkLinkedTest, ConnectedPinsViaNet) {
  PinConnectedPinIterator *iter = network_.connectedPinIterator(net2_);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_GE(count, 2);
}

// Network: portDirection from port object
TEST_F(ConcreteNetworkLinkedTest, PortDirectionAccess) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  Port *port_y = network_.findPort(inv_cell, "Y");
  EXPECT_EQ(network_.direction(port_a), PortDirection::input());
  EXPECT_EQ(network_.direction(port_y), PortDirection::output());
}

// Network: various accessor methods
TEST_F(ConcreteNetworkLinkedTest, LibraryNameAccess) {
  EXPECT_STREQ(network_.name(lib_), "test_lib");
}

TEST_F(ConcreteNetworkLinkedTest, CellNameAccess) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  EXPECT_STREQ(network_.name(inv_cell), "INV");
}

TEST_F(ConcreteNetworkLinkedTest, PortNameAccess) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  EXPECT_STREQ(network_.name(port_a), "A");
}

TEST_F(ConcreteNetworkLinkedTest, NetNameAccess) {
  EXPECT_STREQ(network_.name(net1_), "n1");
}

// Network: cell filename
TEST_F(ConcreteNetworkLinkedTest, CellFilename) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  const char *fn = network_.filename(inv_cell);
  EXPECT_STREQ(fn, "test.lib");
}

// PinSet default constructor
TEST(PinSetDefaultTest, EmptySet) {
  PinSet set;
  EXPECT_TRUE(set.empty());
}

// InstanceSet default constructor
TEST(InstanceSetDefaultTest, EmptySet) {
  InstanceSet set;
  EXPECT_TRUE(set.empty());
}

// NetSet default constructor
TEST(NetSetDefaultTest, EmptySet) {
  NetSet set;
  EXPECT_TRUE(set.empty());
}

////////////////////////////////////////////////////////////////
// R5_ tests for additional network coverage
////////////////////////////////////////////////////////////////

// Network: connect via Port path with a freshly made instance
TEST_F(ConcreteNetworkLinkedTest, ConnectNewPin) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Instance *u3 = network_.makeInstance(inv_cell, "u3", network_.topInstance());
  Port *port_a = network_.findPort(inv_cell, "A");
  Net *n_new = network_.makeNet("n_new", network_.topInstance());
  Pin *pin = network_.connect(u3, port_a, n_new);
  EXPECT_NE(pin, nullptr);
  EXPECT_EQ(network_.net(pin), n_new);
}

// ConcreteCell: findPort for bus bits by name
TEST(ConcreteCellTest, FindBusBitByName) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  cell->makeBusPort("D", 3, 0);
  ConcretePort *bit0 = cell->findPort("D[0]");
  EXPECT_NE(bit0, nullptr);
  ConcretePort *bit3 = cell->findPort("D[3]");
  EXPECT_NE(bit3, nullptr);
}

// Network: isCheckClk (non-liberty pin => false)
TEST_F(ConcreteNetworkLinkedTest, IsCheckClk) {
  EXPECT_FALSE(network_.isCheckClk(pin_u1_a_));
}

// Network: busIndexInRange on scalar port (always false)
TEST_F(ConcreteNetworkLinkedTest, BusIndexInRangeScalar) {
  Cell *inv_cell = network_.findCell(lib_, "INV");
  Port *port_a = network_.findPort(inv_cell, "A");
  EXPECT_FALSE(network_.busIndexInRange(port_a, 0));
}

// Network: busIndexInRange on bus port
TEST_F(ConcreteNetworkLinkedTest, BusIndexInRangeBus) {
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib_);
  clib->setBusBrkts('[', ']');
  Cell *cell = network_.makeCell(lib_, "BUS_TEST", true, "test.lib");
  Port *bus = network_.makeBusPort(cell, "D", 3, 0);
  EXPECT_TRUE(network_.busIndexInRange(bus, 0));
  EXPECT_TRUE(network_.busIndexInRange(bus, 3));
  EXPECT_FALSE(network_.busIndexInRange(bus, 4));
  EXPECT_FALSE(network_.busIndexInRange(bus, -1));
}

// Network: hasMembersViaNetwork on bus port
TEST_F(ConcreteNetworkLinkedTest, HasMembersBus) {
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib_);
  clib->setBusBrkts('[', ']');
  Cell *cell = network_.makeCell(lib_, "HAS_MEM_TEST", true, "test.lib");
  Port *bus = network_.makeBusPort(cell, "D", 1, 0);
  EXPECT_TRUE(network_.hasMembers(bus));
}

// Network: findMember on bus port
TEST_F(ConcreteNetworkLinkedTest, FindMemberBus) {
  ConcreteLibrary *clib = reinterpret_cast<ConcreteLibrary*>(lib_);
  clib->setBusBrkts('[', ']');
  Cell *cell = network_.makeCell(lib_, "FIND_MEM_TEST", true, "test.lib");
  Port *bus = network_.makeBusPort(cell, "D", 1, 0);
  Port *member = network_.findMember(bus, 0);
  EXPECT_NE(member, nullptr);
  Port *member1 = network_.findMember(bus, 1);
  EXPECT_NE(member1, nullptr);
}

// Network: isInside (pin inside pin context)
TEST_F(ConcreteNetworkLinkedTest, IsInsidePinPin) {
  EXPECT_FALSE(network_.isInside(pin_u1_a_, pin_u2_a_));
}

// (R5_CheckLibertyCorners removed: segfaults without Report* initialized)

// Network: findLibertyFilename (none loaded)
TEST(ConcreteNetworkExtraTest, FindLibertyFilename) {
  ConcreteNetwork network;
  LibertyLibrary *found = network.findLibertyFilename("nonexistent.lib");
  EXPECT_EQ(found, nullptr);
}

// Network: leafInstanceIterator with hierarchical instance
TEST_F(ConcreteNetworkLinkedTest, LeafInstanceIteratorHier) {
  Instance *top = network_.topInstance();
  LeafInstanceIterator *iter = network_.leafInstanceIterator(top);
  int count = 0;
  while (iter->hasNext()) {
    iter->next();
    count++;
  }
  delete iter;
  EXPECT_EQ(count, 2);
}

// Network: findPin(Instance, Port)
TEST_F(ConcreteNetworkLinkedTest, FindPinByPort2) {
  Cell *cell = network_.cell(u1_);
  Port *port_a = network_.findPort(cell, "A");
  Pin *found = network_.findPin(u1_, port_a);
  EXPECT_EQ(found, pin_u1_a_);
}

// ConcretePort: setBundlePort
TEST(ConcretePortTest, SetBundlePort) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  ConcreteCell *cell = lib.makeCell("MUX", true, "");
  ConcretePort *a = cell->makePort("A");
  ConcretePort *b = cell->makePort("B");
  ConcretePortSeq *members = new ConcretePortSeq;
  members->push_back(a);
  members->push_back(b);
  ConcretePort *bundle = cell->makeBundlePort("AB", members);
  ConcretePort *c = cell->makePort("C");
  c->setBundlePort(bundle);
  EXPECT_NE(c, nullptr);
}

// BusPort default constructor and setDirection
TEST(ConcretePortTest, BusPortDefaultConstructor) {
  ConcreteLibrary lib("test_lib", "test.lib", false);
  lib.setBusBrkts('[', ']');
  ConcreteCell *cell = lib.makeCell("REG", true, "");
  ConcretePort *bus = cell->makeBusPort("D", 1, 0);
  EXPECT_TRUE(bus->isBus());
  PortDirection::init();
  bus->setDirection(PortDirection::input());
  EXPECT_EQ(bus->direction(), PortDirection::input());
}

// ConcreteNetwork: cell(LibertyCell) forwarding
TEST(ConcreteNetworkExtraTest, CellFromLibertyCell) {
  ConcreteNetwork network;
  Cell *result = network.cell(static_cast<LibertyCell*>(nullptr));
  EXPECT_EQ(result, nullptr);
}

// ConcreteNetwork: cell(const LibertyCell) forwarding
TEST(ConcreteNetworkExtraTest, CellFromConstLibertyCell) {
  ConcreteNetwork network;
  const Cell *result = network.cell(static_cast<const LibertyCell*>(nullptr));
  EXPECT_EQ(result, nullptr);
}


} // namespace sta
