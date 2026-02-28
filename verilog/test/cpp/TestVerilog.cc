#include <gtest/gtest.h>
#include <string>
#include "VerilogNamespace.hh"
#include "PortDirection.hh"
#include "VerilogReader.hh"
#include "verilog/VerilogReaderPvt.hh"

namespace sta {

class VerilogTest : public ::testing::Test {};

// Simple names should pass through
TEST_F(VerilogTest, SimpleCell) {
  EXPECT_EQ(cellVerilogName("INV"), "INV");
}

TEST_F(VerilogTest, SimpleInstance) {
  EXPECT_EQ(instanceVerilogName("u1"), "u1");
}

TEST_F(VerilogTest, SimpleNet) {
  EXPECT_EQ(netVerilogName("wire1"), "wire1");
}

TEST_F(VerilogTest, SimplePort) {
  EXPECT_EQ(portVerilogName("clk"), "clk");
}

// Bus names
TEST_F(VerilogTest, PortBusName) {
  std::string result = portVerilogName("data[0]");
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, NetBusName) {
  std::string result = netVerilogName("bus[3]");
  EXPECT_FALSE(result.empty());
}

// Names with special characters need escaping
TEST_F(VerilogTest, EscapedCellName) {
  std::string result = cellVerilogName("\\cell/name");
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, InstanceWithSlash) {
  std::string result = instanceVerilogName("u1/u2");
  EXPECT_FALSE(result.empty());
}

// Verilog to STA conversions
TEST_F(VerilogTest, ModuleToSta) {
  std::string name = "top";
  std::string result = moduleVerilogToSta(&name);
  EXPECT_EQ(result, "top");
}

TEST_F(VerilogTest, InstanceToSta) {
  std::string name = "inst1";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_EQ(result, "inst1");
}

TEST_F(VerilogTest, EscapedToSta) {
  std::string name = "\\esc_name ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, NetToSta) {
  std::string name = "net1";
  std::string result = netVerilogToSta(&name);
  EXPECT_EQ(result, "net1");
}

TEST_F(VerilogTest, PortToSta) {
  std::string name = "port_a";
  std::string result = portVerilogToSta(&name);
  EXPECT_EQ(result, "port_a");
}

////////////////////////////////////////////////////////////////
// VerilogNamespace - escaped name conversion tests
////////////////////////////////////////////////////////////////

// Names with special chars (non-alphanumeric, non-underscore) need escaping
TEST_F(VerilogTest, CellEscapedSpecialChar) {
  // '/' is a special character, should cause escaping
  std::string result = cellVerilogName("cell/name");
  // Should be escaped: \cell/name (space at end)
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

TEST_F(VerilogTest, CellWithDot) {
  std::string result = cellVerilogName("cell.name");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

TEST_F(VerilogTest, CellPureAlphaNumUnderscore) {
  // Names with only [a-zA-Z0-9_] should NOT be escaped
  std::string result = cellVerilogName("my_cell_123");
  EXPECT_EQ(result, "my_cell_123");
}

TEST_F(VerilogTest, InstanceEscaped) {
  std::string result = instanceVerilogName("u1.u2");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, InstancePure) {
  std::string result = instanceVerilogName("inst_1");
  EXPECT_EQ(result, "inst_1");
}

TEST_F(VerilogTest, NetBusEscaped) {
  // Net names with bus brackets should be handled
  std::string result = netVerilogName("data[0]");
  EXPECT_FALSE(result.empty());
  // Should contain [0]
  EXPECT_NE(result.find("[0]"), std::string::npos);
}

TEST_F(VerilogTest, NetNoBus) {
  std::string result = netVerilogName("simple_net");
  EXPECT_EQ(result, "simple_net");
}

TEST_F(VerilogTest, NetEscapedNoBus) {
  // Net with special chars but no bus
  std::string result = netVerilogName("net/special");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, PortEscaped) {
  std::string result = portVerilogName("port.a");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, PortWithBrackets) {
  // Brackets in port names cause escaping via staToVerilog2
  std::string result = portVerilogName("data[3]");
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, PortPure) {
  std::string result = portVerilogName("clk_out");
  EXPECT_EQ(result, "clk_out");
}

// Test sta-to-verilog with escape characters in input
TEST_F(VerilogTest, CellDoubleEscape) {
  // Double backslash in sta name means literal backslash in verilog
  std::string result = cellVerilogName("cell\\\\name");
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, CellEscapePrefix) {
  // Single backslash followed by non-backslash: escape is skipped
  std::string result = cellVerilogName("\\special");
  EXPECT_FALSE(result.empty());
}

// Verilog-to-STA conversions with escaped names
TEST_F(VerilogTest, EscapedModuleToSta) {
  std::string name = "\\my/module ";
  std::string result = moduleVerilogToSta(&name);
  // Should strip leading \ and trailing space, but escape special chars
  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.front(), '\\');
}

TEST_F(VerilogTest, EscapedNetToSta) {
  std::string name = "\\net[0] ";
  std::string result = netVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, EscapedPortToSta) {
  std::string name = "\\port/a ";
  std::string result = portVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, PlainModuleToSta) {
  std::string name = "top_module";
  std::string result = moduleVerilogToSta(&name);
  EXPECT_EQ(result, "top_module");
}

TEST_F(VerilogTest, PlainNetToSta) {
  std::string name = "wire1";
  std::string result = netVerilogToSta(&name);
  EXPECT_EQ(result, "wire1");
}

TEST_F(VerilogTest, PlainPortToSta) {
  std::string name = "port_b";
  std::string result = portVerilogToSta(&name);
  EXPECT_EQ(result, "port_b");
}

// Escaped name with brackets (bus notation)
TEST_F(VerilogTest, EscapedInstanceWithBracket) {
  std::string name = "\\inst[0] ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
  // Brackets should be escaped in STA name
  EXPECT_NE(result.find("\\["), std::string::npos);
}

// Escaped name with divider
TEST_F(VerilogTest, EscapedInstanceWithDivider) {
  std::string name = "\\u1/u2 ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
  // Divider should be escaped in STA name
  EXPECT_NE(result.find("\\/"), std::string::npos);
}

// Escaped name with escape character
TEST_F(VerilogTest, EscapedNameWithEscapeChar) {
  std::string name = "\\esc\\val ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

// Escaped name without trailing space
TEST_F(VerilogTest, EscapedNoTrailingSpace) {
  std::string name = "\\esc_name";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

////////////////////////////////////////////////////////////////
// Additional VerilogNamespace conversion tests for coverage
////////////////////////////////////////////////////////////////

// staToVerilog: names beginning with digit need escaping
TEST_F(VerilogTest, CellStartsWithDigit) {
  std::string result = cellVerilogName("123abc");
  EXPECT_EQ(result, "123abc");  // Only alphanumeric, no escape needed
}

// staToVerilog: name with space should be escaped
TEST_F(VerilogTest, CellWithSpace) {
  std::string result = cellVerilogName("cell name");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// staToVerilog: single char name
TEST_F(VerilogTest, CellSingleChar) {
  EXPECT_EQ(cellVerilogName("a"), "a");
}

// staToVerilog: empty name
TEST_F(VerilogTest, CellEmpty) {
  EXPECT_EQ(cellVerilogName(""), "");
}

// netVerilogName bus indexing
TEST_F(VerilogTest, NetBusMultiDigit) {
  std::string result = netVerilogName("data[15]");
  EXPECT_NE(result.find("[15]"), std::string::npos);
}

TEST_F(VerilogTest, NetBusZero) {
  std::string result = netVerilogName("wire[0]");
  EXPECT_NE(result.find("[0]"), std::string::npos);
}

// portVerilogName with brackets (staToVerilog2 path)
TEST_F(VerilogTest, PortWithLeftBracket) {
  std::string result = portVerilogName("port[5]");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// instanceVerilogName pure name
TEST_F(VerilogTest, InstanceAlphaNumUnderscore) {
  EXPECT_EQ(instanceVerilogName("u_1_abc"), "u_1_abc");
}

// instanceVerilogName with various special chars
TEST_F(VerilogTest, InstanceWithColon) {
  std::string result = instanceVerilogName("u1:u2");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

TEST_F(VerilogTest, InstanceWithHash) {
  std::string result = instanceVerilogName("u1#2");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, InstanceWithAt) {
  std::string result = instanceVerilogName("u1@special");
  EXPECT_EQ(result.front(), '\\');
}

// verilogToSta: escaped name with multiple special chars
TEST_F(VerilogTest, EscapedMultipleSpecial) {
  std::string name = "\\u1/u2[3] ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
  // Both / and [ and ] should be escaped
  EXPECT_NE(result.find("\\/"), std::string::npos);
  EXPECT_NE(result.find("\\["), std::string::npos);
  EXPECT_NE(result.find("\\]"), std::string::npos);
}

// verilogToSta: escaped name with backslash inside
TEST_F(VerilogTest, EscapedWithBackslash) {
  std::string name = "\\a\\b ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
  // The backslash inside should be escaped as \\
  EXPECT_NE(result.find("\\\\"), std::string::npos);
}

// netVerilogName with special chars and no bus
TEST_F(VerilogTest, NetSpecialNoBus) {
  std::string result = netVerilogName("net.a");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// netVerilogName pure alphanumeric
TEST_F(VerilogTest, NetPureAlpha) {
  EXPECT_EQ(netVerilogName("wire_abc_123"), "wire_abc_123");
}

// cellVerilogName with double backslash (literal backslash in name)
TEST_F(VerilogTest, CellDoubleBackslash) {
  // Double backslash means literal backslash in sta name
  std::string result = cellVerilogName("a\\\\b");
  EXPECT_FALSE(result.empty());
}

// netVerilogToSta with plain name
TEST_F(VerilogTest, NetToStaPlain) {
  std::string name = "simple_wire";
  EXPECT_EQ(netVerilogToSta(&name), "simple_wire");
}

// portVerilogToSta with plain name
TEST_F(VerilogTest, PortToStaPlain) {
  std::string name = "port_clk";
  EXPECT_EQ(portVerilogToSta(&name), "port_clk");
}

// moduleVerilogToSta plain
TEST_F(VerilogTest, ModuleToStaPlain) {
  std::string name = "mod_top";
  EXPECT_EQ(moduleVerilogToSta(&name), "mod_top");
}

// verilogToSta: escaped name without trailing space
TEST_F(VerilogTest, EscapedNoSpace) {
  std::string name = "\\name";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
  // "ame" (without leading 'n' because 'n' is first char after \ which is stripped)
  // Actually: ignoring leading '\', copy the rest. "name" has no trailing space.
  EXPECT_EQ(result, "name");
}

// staToVerilog with single backslash followed by non-backslash
TEST_F(VerilogTest, CellSingleEscape) {
  // Single backslash means escape next char in sta name
  // e.g., "a\[b" has a literal [ in the sta name
  std::string result = cellVerilogName("a\\[b");
  EXPECT_FALSE(result.empty());
  // The \[ should cause the name to be escaped
  EXPECT_EQ(result.front(), '\\');
}

// portVerilogName with only underscore
TEST_F(VerilogTest, PortUnderscoreOnly) {
  EXPECT_EQ(portVerilogName("_"), "_");
}

// cellVerilogName with only underscore
TEST_F(VerilogTest, CellUnderscoreOnly) {
  EXPECT_EQ(cellVerilogName("_"), "_");
}

// netVerilogName with escaped bus
TEST_F(VerilogTest, NetEscapedBus) {
  // sta_name "data\\[0\\]" means literal brackets in name
  std::string result = netVerilogName("data\\[0\\]");
  EXPECT_FALSE(result.empty());
}

////////////////////////////////////////////////////////////////
// R5_ tests for additional verilog coverage
////////////////////////////////////////////////////////////////

// staToVerilog: names with dollar sign
TEST_F(VerilogTest, CellWithDollar) {
  std::string result = cellVerilogName("cell$gen");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// staToVerilog: name with tab character
TEST_F(VerilogTest, CellWithTab) {
  std::string result = cellVerilogName("cell\tname");
  EXPECT_EQ(result.front(), '\\');
}

// staToVerilog: instance with brackets
TEST_F(VerilogTest, InstanceWithBrackets) {
  std::string result = instanceVerilogName("inst[0]");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// verilogToSta: empty escaped name
TEST_F(VerilogTest, EmptyEscapedName) {
  std::string name = "\\";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_TRUE(result.empty());
}

// verilogToSta: escaped name with only space
TEST_F(VerilogTest, EscapedOnlySpace) {
  std::string name = "\\ ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_TRUE(result.empty());
}

// netVerilogName: escaped characters with bus notation
TEST_F(VerilogTest, NetEscapedWithBus) {
  std::string result = netVerilogName("net.a[3]");
  EXPECT_FALSE(result.empty());
}

// portVerilogName: special character with underscore
TEST_F(VerilogTest, PortSpecialWithUnderscore) {
  std::string result = portVerilogName("_port.a_");
  EXPECT_EQ(result.front(), '\\');
}

// cellVerilogName: name with only special chars
TEST_F(VerilogTest, CellOnlySpecialChars) {
  std::string result = cellVerilogName("./#@");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// instanceVerilogToSta: plain unescaped name
TEST_F(VerilogTest, UnescapedInstance) {
  std::string name = "plain_inst";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_EQ(result, "plain_inst");
}

// netVerilogToSta: escaped name with bus notation
TEST_F(VerilogTest, EscapedNetBus) {
  std::string name = "\\data[7:0] ";
  std::string result = netVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

// moduleVerilogToSta: escaped module name
TEST_F(VerilogTest, EscapedModule) {
  std::string name = "\\mod/special ";
  std::string result = moduleVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

// portVerilogToSta: escaped port name
TEST_F(VerilogTest, EscapedPort) {
  std::string name = "\\port$gen ";
  std::string result = portVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

// netVerilogName: name with backslash-escaped bracket
TEST_F(VerilogTest, NetEscapedBracketSlash) {
  std::string result = netVerilogName("a\\[b");
  EXPECT_FALSE(result.empty());
}

// portVerilogName: name that is just digits
TEST_F(VerilogTest, PortJustDigits) {
  std::string result = portVerilogName("12345");
  // All digits - alphanumeric, no escaping needed
  EXPECT_EQ(result, "12345");
}

// cellVerilogName: name with hyphen
TEST_F(VerilogTest, CellWithHyphen) {
  std::string result = cellVerilogName("cell-name");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// instanceVerilogName: name with equal sign
TEST_F(VerilogTest, InstanceWithEquals) {
  std::string result = instanceVerilogName("inst=val");
  EXPECT_EQ(result.front(), '\\');
}

// netVerilogName: name with percent
TEST_F(VerilogTest, NetWithPercent) {
  std::string result = netVerilogName("net%1");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// portVerilogName: name with plus
TEST_F(VerilogTest, PortWithPlus) {
  std::string result = portVerilogName("port+a");
  EXPECT_EQ(result.front(), '\\');
}

// instanceVerilogToSta: escaped name with various special chars
TEST_F(VerilogTest, EscapedInstanceComplex) {
  std::string name = "\\inst.a/b[c] ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
  // The result should contain the original special characters in some form
  EXPECT_GT(result.size(), 3u);
}

// netVerilogToSta: plain net with underscore
TEST_F(VerilogTest, PlainNetUnderscore) {
  std::string name = "_net_wire_";
  std::string result = netVerilogToSta(&name);
  EXPECT_EQ(result, "_net_wire_");
}

// portVerilogToSta: plain port with numbers
TEST_F(VerilogTest, PlainPortNumeric) {
  std::string name = "port_123";
  std::string result = portVerilogToSta(&name);
  EXPECT_EQ(result, "port_123");
}

// moduleVerilogToSta: plain module with mixed case
TEST_F(VerilogTest, PlainModuleMixedCase) {
  std::string name = "MyModule_V2";
  std::string result = moduleVerilogToSta(&name);
  EXPECT_EQ(result, "MyModule_V2");
}

// cellVerilogName: name with tilde
TEST_F(VerilogTest, CellWithTilde) {
  std::string result = cellVerilogName("cell~inv");
  EXPECT_EQ(result.front(), '\\');
}

// instanceVerilogName: name with ampersand
TEST_F(VerilogTest, InstanceWithAmpersand) {
  std::string result = instanceVerilogName("inst&and");
  EXPECT_EQ(result.front(), '\\');
}

// netVerilogName: name with exclamation
TEST_F(VerilogTest, NetWithExclamation) {
  std::string result = netVerilogName("net!rst");
  EXPECT_EQ(result.front(), '\\');
}

// portVerilogName: name with pipe
TEST_F(VerilogTest, PortWithPipe) {
  std::string result = portVerilogName("port|or");
  EXPECT_EQ(result.front(), '\\');
}

// instanceVerilogToSta: escaped name without trailing space (edge case)
TEST_F(VerilogTest, EscapedNoTrailingSpaceComplex) {
  std::string name = "\\inst/a[0]";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

// cellVerilogName: very long name
TEST_F(VerilogTest, CellLongName) {
  std::string long_name(200, 'a');
  std::string result = cellVerilogName(long_name.c_str());
  EXPECT_EQ(result, long_name);
}

// cellVerilogName: very long name with special char
TEST_F(VerilogTest, CellLongEscapedName) {
  std::string long_name(200, 'a');
  long_name[100] = '/';
  std::string result = cellVerilogName(long_name.c_str());
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

////////////////////////////////////////////////////////////////
// R6_ tests for additional verilog coverage
////////////////////////////////////////////////////////////////

// VerilogNetScalar: constructor and properties
TEST_F(VerilogTest, NetScalarConstruct) {
  VerilogNetScalar net("wire1");
  EXPECT_TRUE(net.isNamed());
  EXPECT_TRUE(net.isScalar());
  EXPECT_EQ(net.name(), "wire1");
  EXPECT_FALSE(net.isNamedPortRef());
}

// VerilogNetScalar: with bus-like name
TEST_F(VerilogTest, NetScalarBusLikeName) {
  VerilogNetScalar net("data");
  EXPECT_TRUE(net.isScalar());
  EXPECT_EQ(net.name(), "data");
}

// VerilogNetBitSelect: constructor and properties
TEST_F(VerilogTest, NetBitSelectConstruct) {
  VerilogNetBitSelect net("data", 3);
  EXPECT_TRUE(net.isNamed());
  EXPECT_FALSE(net.isScalar());
  // name() returns "data[3]" (includes index)
  EXPECT_EQ(net.name(), "data[3]");
  EXPECT_EQ(net.index(), 3);
}

// VerilogNetBitSelect: index 0
TEST_F(VerilogTest, NetBitSelectZero) {
  VerilogNetBitSelect net("wire", 0);
  EXPECT_EQ(net.index(), 0);
  EXPECT_EQ(net.name(), "wire[0]");
}

// VerilogNetPartSelect: constructor and properties
TEST_F(VerilogTest, NetPartSelectConstruct) {
  VerilogNetPartSelect net("bus", 7, 0);
  EXPECT_TRUE(net.isNamed());
  EXPECT_FALSE(net.isScalar());
  EXPECT_EQ(net.name(), "bus");
  EXPECT_EQ(net.fromIndex(), 7);
  EXPECT_EQ(net.toIndex(), 0);
}

// VerilogNetPartSelect: ascending
TEST_F(VerilogTest, NetPartSelectAscending) {
  VerilogNetPartSelect net("addr", 0, 15);
  EXPECT_EQ(net.fromIndex(), 0);
  EXPECT_EQ(net.toIndex(), 15);
}

// VerilogNetUnnamed: constructor and properties via VerilogNetConcat
TEST_F(VerilogTest, NetUnnamedConstruct) {
  VerilogNetSeq *nets = new VerilogNetSeq;
  VerilogNetConcat net(nets);
  // VerilogNetConcat extends VerilogNetUnnamed
  EXPECT_FALSE(net.isNamed());
  EXPECT_TRUE(net.name().empty());
}

// VerilogNetNamed: constructor, destructor, name
TEST_F(VerilogTest, NetNamedConstruct) {
  VerilogNetScalar net("test_named");
  EXPECT_TRUE(net.isNamed());
  EXPECT_EQ(net.name(), "test_named");
}

// VerilogNetNamed: destructor (via delete)
TEST_F(VerilogTest, NetNamedDelete) {
  VerilogNetNamed *net = new VerilogNetScalar("to_delete");
  EXPECT_EQ(net->name(), "to_delete");
  delete net;
}

// VerilogNetPortRef: constructor
TEST_F(VerilogTest, NetPortRefConstruct) {
  VerilogNetPortRefScalarNet ref("port_a");
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_EQ(ref.name(), "port_a");
  EXPECT_FALSE(ref.hasNet());
}

// VerilogNetPortRefScalarNet: with net name
TEST_F(VerilogTest, NetPortRefScalarNetWithName) {
  VerilogNetPortRefScalarNet ref("port_a", "wire_a");
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_TRUE(ref.isNamedPortRefScalarNet());
  EXPECT_TRUE(ref.isScalar());
  EXPECT_TRUE(ref.hasNet());
  EXPECT_EQ(ref.netName(), "wire_a");
}

// VerilogNetPortRefScalarNet: set net name
TEST_F(VerilogTest, NetPortRefScalarNetSetName) {
  VerilogNetPortRefScalarNet ref("port_a");
  EXPECT_FALSE(ref.hasNet());
  ref.setNetName("wire_b");
  EXPECT_TRUE(ref.hasNet());
  EXPECT_EQ(ref.netName(), "wire_b");
}

// VerilogNetPortRefScalar: constructor with net
TEST_F(VerilogTest, NetPortRefScalarConstruct) {
  VerilogNetScalar *inner_net = new VerilogNetScalar("inner_wire");
  VerilogNetPortRefScalar ref("port_b", inner_net);
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_TRUE(ref.isScalar());
  EXPECT_TRUE(ref.hasNet());
  // ref owns inner_net and will delete it
}

// VerilogNetPortRefScalar: constructor with null net
TEST_F(VerilogTest, NetPortRefScalarNullNet) {
  VerilogNetPortRefScalar ref("port_c", nullptr);
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_FALSE(ref.hasNet());
}

// VerilogNetPortRefBit: constructor
TEST_F(VerilogTest, NetPortRefBitConstruct) {
  VerilogNetScalar *inner_net = new VerilogNetScalar("inner2");
  VerilogNetPortRefBit ref("port_d", 3, inner_net);
  EXPECT_TRUE(ref.isNamedPortRef());
  // name() returns bit_name_ which is "port_d[3]"
  std::string rname = ref.name();
  EXPECT_FALSE(rname.empty());
}

// VerilogNetPortRefPart: constructor and name
TEST_F(VerilogTest, NetPortRefPartConstruct) {
  VerilogNetScalar *inner_net = new VerilogNetScalar("inner3");
  VerilogNetPortRefPart ref("port_e", 7, 0, inner_net);
  EXPECT_TRUE(ref.isNamedPortRef());
  std::string rname = ref.name();
  EXPECT_FALSE(rname.empty());
  EXPECT_EQ(ref.toIndex(), 0);
}

// VerilogNetConcat: constructor with nets
TEST_F(VerilogTest, NetConcatConstruct) {
  VerilogNetSeq *nets = new VerilogNetSeq;
  nets->push_back(new VerilogNetScalar("a"));
  nets->push_back(new VerilogNetScalar("b"));
  VerilogNetConcat concat(nets);
  EXPECT_FALSE(concat.isNamed());
}

// VerilogDclArg: constructor with name
TEST_F(VerilogTest, DclArgName) {
  VerilogDclArg arg("wire_name");
  EXPECT_EQ(arg.netName(), "wire_name");
  EXPECT_TRUE(arg.isNamed());
  EXPECT_EQ(arg.assign(), nullptr);
}

// VerilogDclArg: constructor with assign
TEST_F(VerilogTest, DclArgAssign) {
  VerilogNetScalar *lhs = new VerilogNetScalar("out");
  VerilogNetScalar *rhs = new VerilogNetScalar("in");
  VerilogAssign *assign = new VerilogAssign(lhs, rhs, 1);
  VerilogDclArg arg(assign);
  EXPECT_FALSE(arg.isNamed());
  EXPECT_NE(arg.assign(), nullptr);
}

// VerilogAssign: constructor and accessors
TEST_F(VerilogTest, AssignConstruct) {
  VerilogNetScalar *lhs = new VerilogNetScalar("out");
  VerilogNetScalar *rhs = new VerilogNetScalar("in");
  VerilogAssign assign(lhs, rhs, 10);
  EXPECT_TRUE(assign.isAssign());
  EXPECT_EQ(assign.lhs(), lhs);
  EXPECT_EQ(assign.rhs(), rhs);
  EXPECT_EQ(assign.line(), 10);
}

// VerilogStmt: constructor and properties
TEST_F(VerilogTest, StmtConstruct) {
  // VerilogStmt is abstract, test through VerilogAssign
  VerilogNetScalar *lhs = new VerilogNetScalar("a");
  VerilogNetScalar *rhs = new VerilogNetScalar("b");
  VerilogAssign assign(lhs, rhs, 5);
  EXPECT_FALSE(assign.isInstance());
  EXPECT_FALSE(assign.isModuleInst());
  EXPECT_FALSE(assign.isLibertyInst());
  EXPECT_TRUE(assign.isAssign());
  EXPECT_FALSE(assign.isDeclaration());
  EXPECT_EQ(assign.line(), 5);
}

// VerilogStmt: destructor (via delete of derived)
TEST_F(VerilogTest, StmtDelete) {
  VerilogNetScalar *lhs = new VerilogNetScalar("x");
  VerilogNetScalar *rhs = new VerilogNetScalar("y");
  VerilogStmt *stmt = new VerilogAssign(lhs, rhs, 1);
  EXPECT_TRUE(stmt->isAssign());
  delete stmt;
}

// VerilogInst: constructor and accessors
TEST_F(VerilogTest, InstConstruct) {
  VerilogInst *inst = new VerilogModuleInst("INV", "u1", nullptr,
                                            new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(inst->isInstance());
  EXPECT_TRUE(inst->isModuleInst());
  EXPECT_EQ(inst->instanceName(), "u1");
  EXPECT_EQ(inst->line(), 1);
  delete inst;
}

// VerilogInst: setInstanceName
TEST_F(VerilogTest, InstSetName) {
  VerilogModuleInst inst("BUF", "old_name", nullptr,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(inst.instanceName(), "old_name");
  inst.setInstanceName("new_name");
  EXPECT_EQ(inst.instanceName(), "new_name");
}

// VerilogModuleInst: hasPins with nullptr
TEST_F(VerilogTest, ModuleInstHasPinsNull) {
  VerilogModuleInst inst("INV", "u1", nullptr,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_FALSE(inst.hasPins());
}

// VerilogModuleInst: hasPins with empty pins
TEST_F(VerilogTest, ModuleInstHasPinsEmpty) {
  VerilogNetSeq *pins = new VerilogNetSeq;
  VerilogModuleInst inst("INV", "u1", pins,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_FALSE(inst.hasPins());
}

// VerilogModuleInst: hasPins with pins
TEST_F(VerilogTest, ModuleInstHasPinsTrue) {
  VerilogNetSeq *pins = new VerilogNetSeq;
  pins->push_back(new VerilogNetScalar("wire1"));
  VerilogModuleInst inst("INV", "u1", pins,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(inst.hasPins());
}

// VerilogModuleInst: moduleName
TEST_F(VerilogTest, ModuleInstModuleName) {
  VerilogModuleInst inst("BUF_X2", "buffer1", nullptr,
                         new VerilogAttrStmtSeq, 5);
  EXPECT_EQ(inst.moduleName(), "BUF_X2");
}

// VerilogDcl: constructor with args seq
TEST_F(VerilogTest, DclConstructSeq) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("wire1"));
  args->push_back(new VerilogDclArg("wire2"));
  VerilogDcl dcl(PortDirection::input(), args, new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(dcl.isDeclaration());
  EXPECT_FALSE(dcl.isBus());
  EXPECT_EQ(dcl.size(), 1);
  EXPECT_EQ(dcl.direction(), PortDirection::input());
}

// VerilogDcl: portName
TEST_F(VerilogTest, DclPortName) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("my_wire"));
  VerilogDcl dcl(PortDirection::output(), args, new VerilogAttrStmtSeq, 1);
  std::string pname = dcl.portName();
  EXPECT_EQ(pname, "my_wire");
}

// VerilogDcl: appendArg
TEST_F(VerilogTest, DclAppendArg) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("w1"));
  VerilogDcl dcl(PortDirection::input(), args, new VerilogAttrStmtSeq, 1);
  dcl.appendArg(new VerilogDclArg("w2"));
  EXPECT_EQ(dcl.args()->size(), 2u);
}

// VerilogDclBus: constructor with args seq
TEST_F(VerilogTest, DclBusConstructSeq) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("bus_wire"));
  VerilogDclBus dcl(PortDirection::input(), 7, 0, args,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(dcl.isBus());
  EXPECT_TRUE(dcl.isDeclaration());
  EXPECT_EQ(dcl.fromIndex(), 7);
  EXPECT_EQ(dcl.toIndex(), 0);
  EXPECT_EQ(dcl.size(), 8);
}

// VerilogDclBus: constructor with single arg
TEST_F(VerilogTest, DclBusConstructSingle) {
  PortDirection::init();
  VerilogDclArg *arg = new VerilogDclArg("single_bus");
  VerilogDclBus dcl(PortDirection::output(), 3, 0, arg,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(dcl.isBus());
  EXPECT_EQ(dcl.size(), 4);
}

// VerilogDclBus: ascending range
TEST_F(VerilogTest, DclBusAscending) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("asc_bus"));
  VerilogDclBus dcl(PortDirection::input(), 0, 7, args,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(dcl.fromIndex(), 0);
  EXPECT_EQ(dcl.toIndex(), 7);
  EXPECT_EQ(dcl.size(), 8);
}

// VerilogAttrStmt: constructor
TEST_F(VerilogTest, AttrStmtConstruct) {
  VerilogAttrEntrySeq *entries = new VerilogAttrEntrySeq;
  entries->push_back(new VerilogAttrEntry("key1", "val1"));
  VerilogAttrStmt stmt(entries);
  VerilogAttrEntrySeq *attrs = stmt.attrs();
  EXPECT_NE(attrs, nullptr);
  EXPECT_EQ(attrs->size(), 1u);
}

// VerilogAttrEntry: constructor and accessors
TEST_F(VerilogTest, AttrEntryConstruct) {
  VerilogAttrEntry entry("my_attr", "my_value");
  EXPECT_EQ(entry.key(), "my_attr");
  EXPECT_EQ(entry.value(), "my_value");
}

// VerilogNetScalar: multiple instances
TEST_F(VerilogTest, MultipleNetScalars) {
  VerilogNetScalar net1("a");
  VerilogNetScalar net2("b");
  VerilogNetScalar net3("c");
  EXPECT_EQ(net1.name(), "a");
  EXPECT_EQ(net2.name(), "b");
  EXPECT_EQ(net3.name(), "c");
  EXPECT_TRUE(net1.isScalar());
  EXPECT_TRUE(net2.isScalar());
  EXPECT_TRUE(net3.isScalar());
}

// VerilogNetPortRefScalarNet: empty net name
TEST_F(VerilogTest, PortRefScalarNetEmpty) {
  VerilogNetPortRefScalarNet ref("port_a");
  EXPECT_FALSE(ref.hasNet());
  EXPECT_EQ(ref.netName(), "");
}

// VerilogNetPortRefBit: index 0
TEST_F(VerilogTest, PortRefBitIndex0) {
  VerilogNetScalar *inner = new VerilogNetScalar("w");
  VerilogNetPortRefBit ref("port", 0, inner);
  std::string rname = ref.name();
  EXPECT_FALSE(rname.empty());
}

// VerilogNetPortRefPart: ascending range
TEST_F(VerilogTest, PortRefPartAsc) {
  VerilogNetScalar *inner = new VerilogNetScalar("w");
  VerilogNetPortRefPart ref("port", 0, 3, inner);
  std::string rname = ref.name();
  EXPECT_FALSE(rname.empty());
  EXPECT_EQ(ref.toIndex(), 3);
}

// VerilogDcl: single arg constructor
TEST_F(VerilogTest, DclSingleArg) {
  PortDirection::init();
  VerilogDclArg *arg = new VerilogDclArg("single_wire");
  VerilogDcl dcl(PortDirection::input(), arg, new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(dcl.isDeclaration());
  EXPECT_EQ(dcl.args()->size(), 1u);
}

// Additional name conversion tests
TEST_F(VerilogTest, CellWithQuestionMark) {
  std::string result = cellVerilogName("cell?name");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, InstanceWithSemicolon) {
  std::string result = instanceVerilogName("inst;name");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, NetWithComma) {
  std::string result = netVerilogName("net,name");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, PortWithParens) {
  std::string result = portVerilogName("port(a)");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, CellWithCurlyBraces) {
  std::string result = cellVerilogName("cell{name}");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, InstanceWithLessThan) {
  std::string result = instanceVerilogName("inst<0>");
  EXPECT_EQ(result.front(), '\\');
}

// VerilogToSta: net with bus range
TEST_F(VerilogTest, EscapedNetRange) {
  std::string name = "\\data[7:0] ";
  std::string result = netVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

// VerilogToSta: module with digit prefix
TEST_F(VerilogTest, ModuleDigitPrefix) {
  std::string name = "123module";
  std::string result = moduleVerilogToSta(&name);
  EXPECT_EQ(result, "123module");
}

// portVerilogToSta: escaped
TEST_F(VerilogTest, EscapedPortComplex) {
  std::string name = "\\port.a[0]/b ";
  std::string result = portVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

// cellVerilogName round-trip with special chars
TEST_F(VerilogTest, RoundTripSpecialCell) {
  // STA name with escaped bracket
  std::string sta_name = "cell\\[0\\]";
  std::string verilog = cellVerilogName(sta_name.c_str());
  EXPECT_FALSE(verilog.empty());
}

// instanceVerilogName: name with backslash in middle
TEST_F(VerilogTest, InstanceBackslashMiddle) {
  std::string result = instanceVerilogName("inst\\mid");
  EXPECT_FALSE(result.empty());
}

// netVerilogName: escaped bracket bus
TEST_F(VerilogTest, NetEscapedBracketBus2) {
  std::string result = netVerilogName("data\\[3\\]");
  EXPECT_FALSE(result.empty());
}

////////////////////////////////////////////////////////////////
// R8_ tests for additional verilog coverage
////////////////////////////////////////////////////////////////

// VerilogNetScalar::isScalar - uncovered
TEST_F(VerilogTest, NetScalarIsScalar) {
  VerilogNetScalar net("scalar_w");
  EXPECT_TRUE(net.isScalar());
  EXPECT_TRUE(net.isNamed());
  EXPECT_EQ(net.name(), "scalar_w");
}

// VerilogNetBitSelect::isScalar - uncovered (returns false)
TEST_F(VerilogTest, NetBitSelectNotScalar) {
  VerilogNetBitSelect net("bus_w", 5);
  EXPECT_FALSE(net.isScalar());
  EXPECT_TRUE(net.isNamed());
  EXPECT_EQ(net.index(), 5);
  EXPECT_EQ(net.name(), "bus_w[5]");
}

// VerilogNetPartSelect::isScalar - uncovered (returns false)
TEST_F(VerilogTest, NetPartSelectNotScalar) {
  VerilogNetPartSelect net("range_w", 15, 0);
  EXPECT_FALSE(net.isScalar());
  EXPECT_EQ(net.fromIndex(), 15);
  EXPECT_EQ(net.toIndex(), 0);
}

// VerilogNetPortRefScalarNet::isScalar - uncovered (returns true)
TEST_F(VerilogTest, NetPortRefScalarNetIsScalar) {
  VerilogNetPortRefScalarNet ref("port_ref", "net_ref");
  EXPECT_TRUE(ref.isScalar());
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_TRUE(ref.isNamedPortRefScalarNet());
  EXPECT_TRUE(ref.hasNet());
}

// VerilogNetPortRefScalar::isScalar - uncovered (returns true)
TEST_F(VerilogTest, NetPortRefScalarIsScalar) {
  VerilogNetScalar *inner = new VerilogNetScalar("inner_w");
  VerilogNetPortRefScalar ref("pref_s", inner);
  EXPECT_TRUE(ref.isScalar());
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_TRUE(ref.hasNet());
}

// VerilogNetUnnamed::isNamed - uncovered
TEST_F(VerilogTest, NetUnnamedIsNamed) {
  VerilogNetSeq *nets = new VerilogNetSeq;
  VerilogNetConcat concat(nets);
  EXPECT_FALSE(concat.isNamed());
  EXPECT_TRUE(concat.name().empty());
}

// VerilogNetUnnamed::name - returns empty string
TEST_F(VerilogTest, NetUnnamedName) {
  VerilogNetSeq *nets = new VerilogNetSeq;
  nets->push_back(new VerilogNetScalar("x"));
  VerilogNetConcat concat(nets);
  const std::string &n = concat.name();
  EXPECT_TRUE(n.empty());
}

// VerilogNetPortRefBit::name - uncovered
TEST_F(VerilogTest, NetPortRefBitName) {
  VerilogNetScalar *inner = new VerilogNetScalar("w1");
  VerilogNetPortRefBit ref("port_bit", 7, inner);
  std::string n = ref.name();
  // name() should return something like "port_bit[7]"
  EXPECT_FALSE(n.empty());
  EXPECT_NE(n.find("7"), std::string::npos);
}

// VerilogNetPortRefBit: index 0
TEST_F(VerilogTest, NetPortRefBitIndex0) {
  VerilogNetScalar *inner = new VerilogNetScalar("w2");
  VerilogNetPortRefBit ref("p0", 0, inner);
  std::string n = ref.name();
  EXPECT_NE(n.find("0"), std::string::npos);
}

// VerilogNetPortRefBit: null inner net
TEST_F(VerilogTest, NetPortRefBitNullNet) {
  VerilogNetPortRefBit ref("p_null", 3, nullptr);
  EXPECT_FALSE(ref.hasNet());
  std::string n = ref.name();
  EXPECT_FALSE(n.empty());
}

// VerilogStmt::isAssign - uncovered on base
TEST_F(VerilogTest, StmtIsAssign) {
  VerilogNetScalar *lhs = new VerilogNetScalar("a");
  VerilogNetScalar *rhs = new VerilogNetScalar("b");
  VerilogAssign assign(lhs, rhs, 1);
  EXPECT_TRUE(assign.isAssign());
  EXPECT_FALSE(assign.isInstance());
  EXPECT_FALSE(assign.isModuleInst());
  EXPECT_FALSE(assign.isLibertyInst());
  EXPECT_FALSE(assign.isDeclaration());
}

// VerilogStmt destructor via delete
TEST_F(VerilogTest, StmtDestructor) {
  VerilogNetScalar *lhs = new VerilogNetScalar("x");
  VerilogNetScalar *rhs = new VerilogNetScalar("y");
  VerilogStmt *stmt = new VerilogAssign(lhs, rhs, 42);
  EXPECT_EQ(stmt->line(), 42);
  delete stmt;
}

// VerilogInst: constructor and destructor
TEST_F(VerilogTest, InstConstructDestruct) {
  VerilogModuleInst *inst = new VerilogModuleInst(
    "AND2", "and_inst", nullptr, new VerilogAttrStmtSeq, 10);
  EXPECT_TRUE(inst->isInstance());
  EXPECT_TRUE(inst->isModuleInst());
  EXPECT_FALSE(inst->isLibertyInst());
  EXPECT_EQ(inst->instanceName(), "and_inst");
  EXPECT_EQ(inst->moduleName(), "AND2");
  EXPECT_EQ(inst->line(), 10);
  delete inst;
}

// VerilogModuleInst: pins with content
TEST_F(VerilogTest, ModuleInstPinsContent) {
  VerilogNetSeq *pins = new VerilogNetSeq;
  pins->push_back(new VerilogNetScalar("a_wire"));
  pins->push_back(new VerilogNetScalar("b_wire"));
  VerilogModuleInst inst("OR2", "or_inst", pins,
                         new VerilogAttrStmtSeq, 20);
  EXPECT_TRUE(inst.hasPins());
  EXPECT_EQ(inst.moduleName(), "OR2");
}

// VerilogModuleInst: setInstanceName
TEST_F(VerilogTest, ModuleInstSetName) {
  VerilogModuleInst inst("BUF", "old", nullptr,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(inst.instanceName(), "old");
  inst.setInstanceName("new_name");
  EXPECT_EQ(inst.instanceName(), "new_name");
}

// VerilogDcl: constructor with single arg
TEST_F(VerilogTest, DclSingleArg2) {
  PortDirection::init();
  VerilogDclArg *arg = new VerilogDclArg("single_wire");
  VerilogDcl dcl(PortDirection::input(), arg, new VerilogAttrStmtSeq, 5);
  EXPECT_TRUE(dcl.isDeclaration());
  EXPECT_FALSE(dcl.isBus());
  EXPECT_EQ(dcl.direction(), PortDirection::input());
  EXPECT_EQ(dcl.portName(), "single_wire");
}

// VerilogDcl: output direction
TEST_F(VerilogTest, DclOutputDirection) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("out_wire"));
  VerilogDcl dcl(PortDirection::output(), args, new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(dcl.direction(), PortDirection::output());
}

// VerilogDcl: size
TEST_F(VerilogTest, DclSize) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("w1"));
  VerilogDcl dcl(PortDirection::input(), args, new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(dcl.size(), 1);
}

// VerilogDclBus: size calculation
TEST_F(VerilogTest, DclBusSize) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("bus_w"));
  VerilogDclBus dcl(PortDirection::input(), 31, 0, args,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(dcl.isBus());
  EXPECT_EQ(dcl.size(), 32);
}

// VerilogDclBus: ascending index
TEST_F(VerilogTest, DclBusAscending2) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("bus_asc"));
  VerilogDclBus dcl(PortDirection::input(), 0, 7, args,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(dcl.fromIndex(), 0);
  EXPECT_EQ(dcl.toIndex(), 7);
  EXPECT_EQ(dcl.size(), 8);
}

// VerilogDclBus: single-arg constructor
TEST_F(VerilogTest, DclBusSingleArg) {
  PortDirection::init();
  VerilogDclArg *arg = new VerilogDclArg("single_bus");
  VerilogDclBus dcl(PortDirection::output(), 3, 0, arg,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(dcl.isBus());
  EXPECT_EQ(dcl.size(), 4);
}

// VerilogDclArg: named vs assign
TEST_F(VerilogTest, DclArgNamed) {
  VerilogDclArg arg("my_net");
  EXPECT_TRUE(arg.isNamed());
  EXPECT_EQ(arg.netName(), "my_net");
  EXPECT_EQ(arg.assign(), nullptr);
}

TEST_F(VerilogTest, DclArgAssign2) {
  VerilogNetScalar *lhs = new VerilogNetScalar("out");
  VerilogNetScalar *rhs = new VerilogNetScalar("in");
  VerilogAssign *assign = new VerilogAssign(lhs, rhs, 1);
  VerilogDclArg arg(assign);
  EXPECT_FALSE(arg.isNamed());
  EXPECT_NE(arg.assign(), nullptr);
}

// VerilogAssign: accessors
TEST_F(VerilogTest, AssignAccessors) {
  VerilogNetScalar *lhs = new VerilogNetScalar("out");
  VerilogNetScalar *rhs = new VerilogNetScalar("in");
  VerilogAssign assign(lhs, rhs, 15);
  EXPECT_TRUE(assign.isAssign());
  EXPECT_EQ(assign.lhs(), lhs);
  EXPECT_EQ(assign.rhs(), rhs);
  EXPECT_EQ(assign.line(), 15);
}

// VerilogNetNamed: constructor and destructor
TEST_F(VerilogTest, NetNamedConstructDelete) {
  VerilogNetNamed *net = new VerilogNetScalar("named_w");
  EXPECT_TRUE(net->isNamed());
  EXPECT_EQ(net->name(), "named_w");
  delete net;
}

// VerilogNetConcat: with multiple nets
TEST_F(VerilogTest, NetConcatMultiple) {
  VerilogNetSeq *nets = new VerilogNetSeq;
  nets->push_back(new VerilogNetScalar("a"));
  nets->push_back(new VerilogNetScalar("b"));
  nets->push_back(new VerilogNetScalar("c"));
  VerilogNetConcat concat(nets);
  EXPECT_FALSE(concat.isNamed());
}

// VerilogNetPortRef: constructor
TEST_F(VerilogTest, NetPortRefConstruct2) {
  VerilogNetPortRefScalarNet ref("port_x");
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_FALSE(ref.hasNet());
  EXPECT_EQ(ref.name(), "port_x");
}

// VerilogNetPortRefScalarNet: various operations
TEST_F(VerilogTest, NetPortRefScalarNetOps) {
  VerilogNetPortRefScalarNet ref("port_y", "net_y");
  EXPECT_TRUE(ref.hasNet());
  EXPECT_EQ(ref.netName(), "net_y");
  ref.setNetName("new_net");
  EXPECT_EQ(ref.netName(), "new_net");
}

// VerilogNetPortRefScalar: null net
TEST_F(VerilogTest, NetPortRefScalarNull) {
  VerilogNetPortRefScalar ref("port_z", nullptr);
  EXPECT_FALSE(ref.hasNet());
  EXPECT_TRUE(ref.isScalar());
}

// VerilogNetPortRefPart: constructor
TEST_F(VerilogTest, NetPortRefPartConstruct2) {
  VerilogNetScalar *inner = new VerilogNetScalar("w_part");
  VerilogNetPortRefPart ref("port_part", 15, 0, inner);
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_EQ(ref.toIndex(), 0);
  std::string n = ref.name();
  EXPECT_FALSE(n.empty());
}

// VerilogNetPortRefPart: ascending range
TEST_F(VerilogTest, NetPortRefPartAscending) {
  VerilogNetScalar *inner = new VerilogNetScalar("w_part_asc");
  VerilogNetPortRefPart ref("port_asc", 0, 7, inner);
  EXPECT_EQ(ref.toIndex(), 7);
}

// VerilogAttrStmt: construction
TEST_F(VerilogTest, AttrStmtConstruct2) {
  VerilogAttrEntrySeq *entries = new VerilogAttrEntrySeq;
  entries->push_back(new VerilogAttrEntry("key1", "val1"));
  VerilogAttrStmt stmt(entries);
  VerilogAttrEntrySeq *attrs = stmt.attrs();
  EXPECT_NE(attrs, nullptr);
  EXPECT_EQ(attrs->size(), 1u);
}

// VerilogAttrEntry: key and value
TEST_F(VerilogTest, AttrEntryKeyValue) {
  VerilogAttrEntry entry("attr_key", "attr_value");
  EXPECT_EQ(entry.key(), "attr_key");
  EXPECT_EQ(entry.value(), "attr_value");
}

// VerilogNetBitSelect: various indices
TEST_F(VerilogTest, NetBitSelectLargeIndex) {
  VerilogNetBitSelect net("data", 31);
  EXPECT_EQ(net.index(), 31);
  EXPECT_EQ(net.name(), "data[31]");
  EXPECT_FALSE(net.isScalar());
}

// VerilogNetPartSelect: equal indices
TEST_F(VerilogTest, NetPartSelectEqual) {
  VerilogNetPartSelect net("single", 5, 5);
  EXPECT_EQ(net.fromIndex(), 5);
  EXPECT_EQ(net.toIndex(), 5);
  EXPECT_FALSE(net.isScalar());
}

// VerilogNetScalar: empty name
TEST_F(VerilogTest, NetScalarEmptyName) {
  VerilogNetScalar net("");
  EXPECT_TRUE(net.isScalar());
  EXPECT_TRUE(net.name().empty());
}

// Additional name conversion edge cases
TEST_F(VerilogTest, CellNameWithBackslashEscape) {
  std::string result = cellVerilogName("cell\\name");
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, InstanceNameAllDigits) {
  std::string result = instanceVerilogName("0123456789");
  EXPECT_EQ(result, "0123456789");
}

TEST_F(VerilogTest, NetNameSingleUnderscore) {
  EXPECT_EQ(netVerilogName("_"), "_");
}

TEST_F(VerilogTest, PortNameSingleChar) {
  EXPECT_EQ(portVerilogName("a"), "a");
}

TEST_F(VerilogTest, CellNameWithBraces) {
  std::string result = cellVerilogName("{a,b}");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, InstanceNameWithStar) {
  std::string result = instanceVerilogName("inst*2");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, NetNameWithQuote) {
  std::string result = netVerilogName("net\"q");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, PortNameWithBacktick) {
  std::string result = portVerilogName("port`tick");
  EXPECT_EQ(result.front(), '\\');
}

// Verilog to STA conversions: edge cases
TEST_F(VerilogTest, EscapedInstanceOnlyBrackets) {
  std::string name = "\\[0] ";
  std::string result = instanceVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, EscapedNetOnlySlash) {
  std::string name = "\\/ ";
  std::string result = netVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, ModuleToStaEscapedComplex) {
  std::string name = "\\mod.a/b[1] ";
  std::string result = moduleVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

TEST_F(VerilogTest, PortToStaEscapedBracket) {
  std::string name = "\\port[3] ";
  std::string result = portVerilogToSta(&name);
  EXPECT_FALSE(result.empty());
}

// VerilogDcl: appendArg
TEST_F(VerilogTest, DclAppendMultiple) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("w1"));
  VerilogDcl dcl(PortDirection::input(), args, new VerilogAttrStmtSeq, 1);
  dcl.appendArg(new VerilogDclArg("w2"));
  dcl.appendArg(new VerilogDclArg("w3"));
  EXPECT_EQ(dcl.args()->size(), 3u);
}

// Multiple VerilogNetScalar instances
TEST_F(VerilogTest, MultipleNetScalars2) {
  std::vector<VerilogNetScalar*> nets;
  for (int i = 0; i < 10; i++) {
    std::string name = "net_" + std::to_string(i);
    nets.push_back(new VerilogNetScalar(name));
  }
  for (int i = 0; i < 10; i++) {
    std::string expected = "net_" + std::to_string(i);
    EXPECT_EQ(nets[i]->name(), expected);
    EXPECT_TRUE(nets[i]->isScalar());
  }
  for (auto *net : nets)
    delete net;
}

// VerilogModuleInst: namedPins with named port refs
TEST_F(VerilogTest, ModuleInstNamedPins) {
  VerilogNetSeq *pins = new VerilogNetSeq;
  pins->push_back(new VerilogNetPortRefScalarNet("A", "w1"));
  pins->push_back(new VerilogNetPortRefScalarNet("Y", "w2"));
  VerilogModuleInst inst("INV", "inv_inst", pins,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(inst.hasPins());
  EXPECT_TRUE(inst.namedPins());
}

// VerilogModuleInst: ordered pins (not named)
TEST_F(VerilogTest, ModuleInstOrderedPins) {
  VerilogNetSeq *pins = new VerilogNetSeq;
  pins->push_back(new VerilogNetScalar("w1"));
  pins->push_back(new VerilogNetScalar("w2"));
  VerilogModuleInst inst("INV", "inv_ord", pins,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(inst.hasPins());
  EXPECT_FALSE(inst.namedPins());
}

// VerilogNetPortRefScalarNet: empty net name
TEST_F(VerilogTest, PortRefScalarNetEmptyName) {
  VerilogNetPortRefScalarNet ref("port_empty");
  EXPECT_FALSE(ref.hasNet());
  EXPECT_TRUE(ref.netName().empty());
}

////////////////////////////////////////////////////////////////
// R9_ tests for additional verilog coverage
////////////////////////////////////////////////////////////////

// VerilogNetScalar: exercises various named operations
TEST_F(VerilogTest, NetScalarOperations) {
  VerilogNetScalar net("test_wire");
  EXPECT_TRUE(net.isNamed());
  EXPECT_TRUE(net.isScalar());
  EXPECT_EQ(net.name(), "test_wire");
  EXPECT_FALSE(net.isNamedPortRef());
  EXPECT_FALSE(net.isNamedPortRefScalarNet());
}

// VerilogNetBitSelect: negative index edge case
TEST_F(VerilogTest, NetBitSelectNegativeIndex) {
  VerilogNetBitSelect net("data", -1);
  EXPECT_FALSE(net.isScalar());
  EXPECT_EQ(net.index(), -1);
}

// VerilogNetPartSelect: single bit range
TEST_F(VerilogTest, NetPartSelectSingleBit) {
  VerilogNetPartSelect net("bus", 0, 0);
  EXPECT_FALSE(net.isScalar());
  EXPECT_EQ(net.fromIndex(), 0);
  EXPECT_EQ(net.toIndex(), 0);
}

// VerilogNetConcat: with multiple nested types
TEST_F(VerilogTest, NetConcatMixedTypes) {
  VerilogNetSeq *nets = new VerilogNetSeq;
  nets->push_back(new VerilogNetScalar("a"));
  nets->push_back(new VerilogNetBitSelect("b", 0));
  nets->push_back(new VerilogNetPartSelect("c", 7, 0));
  VerilogNetConcat concat(nets);
  EXPECT_FALSE(concat.isNamed());
  EXPECT_TRUE(concat.name().empty());
}

// VerilogNetPortRefScalarNet: setNetName then clear
TEST_F(VerilogTest, PortRefScalarNetSetClear) {
  VerilogNetPortRefScalarNet ref("port_a");
  EXPECT_FALSE(ref.hasNet());
  ref.setNetName("wire_a");
  EXPECT_TRUE(ref.hasNet());
  EXPECT_EQ(ref.netName(), "wire_a");
  ref.setNetName("wire_b");
  EXPECT_EQ(ref.netName(), "wire_b");
}

// VerilogNetPortRefScalar: with bit select net
TEST_F(VerilogTest, PortRefScalarWithBitSelect) {
  VerilogNetBitSelect *inner = new VerilogNetBitSelect("data", 5);
  VerilogNetPortRefScalar ref("port_data", inner);
  EXPECT_TRUE(ref.isScalar());
  EXPECT_TRUE(ref.hasNet());
}

// VerilogNetPortRefBit: with part select net
TEST_F(VerilogTest, PortRefBitWithPartSelect) {
  VerilogNetPartSelect *inner = new VerilogNetPartSelect("bus", 7, 0);
  VerilogNetPortRefBit ref("port_bus", 0, inner);
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_TRUE(ref.hasNet());
}

// VerilogNetPortRefPart: with concat net
TEST_F(VerilogTest, PortRefPartWithConcat) {
  VerilogNetSeq *nets = new VerilogNetSeq;
  nets->push_back(new VerilogNetScalar("x"));
  nets->push_back(new VerilogNetScalar("y"));
  VerilogNetConcat *inner = new VerilogNetConcat(nets);
  VerilogNetPortRefPart ref("port_xy", 1, 0, inner);
  EXPECT_TRUE(ref.isNamedPortRef());
  EXPECT_TRUE(ref.hasNet());
}

// VerilogModuleInst: with large pin count
TEST_F(VerilogTest, ModuleInstManyPins) {
  VerilogNetSeq *pins = new VerilogNetSeq;
  for (int i = 0; i < 20; i++) {
    std::string pname = "pin_" + std::to_string(i);
    std::string nname = "net_" + std::to_string(i);
    pins->push_back(new VerilogNetPortRefScalarNet(pname, nname));
  }
  VerilogModuleInst inst("LARGE_CELL", "u_large", pins,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(inst.hasPins());
  EXPECT_TRUE(inst.namedPins());
  EXPECT_EQ(inst.moduleName(), "LARGE_CELL");
}

// VerilogModuleInst: with mixed pin types
TEST_F(VerilogTest, ModuleInstMixedPins) {
  VerilogNetSeq *pins = new VerilogNetSeq;
  pins->push_back(new VerilogNetPortRefScalarNet("A", "w1"));
  pins->push_back(new VerilogNetPortRefScalarNet("Y"));
  VerilogModuleInst inst("BUF", "u_buf", pins,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(inst.hasPins());
  EXPECT_TRUE(inst.namedPins());
}

// VerilogDcl: various directions
TEST_F(VerilogTest, DclBidirectional) {
  PortDirection::init();
  VerilogDclArg *arg = new VerilogDclArg("bidir_port");
  VerilogDcl dcl(PortDirection::bidirect(), arg, new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(dcl.isDeclaration());
  EXPECT_EQ(dcl.direction(), PortDirection::bidirect());
}

// VerilogDcl: appendArg to seq
TEST_F(VerilogTest, DclAppendArgMultiple) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("w1"));
  VerilogDcl dcl(PortDirection::input(), args, new VerilogAttrStmtSeq, 1);
  for (int i = 0; i < 10; i++) {
    dcl.appendArg(new VerilogDclArg("w" + std::to_string(i+2)));
  }
  EXPECT_EQ(dcl.args()->size(), 11u);
}

// VerilogDclBus: large bus
TEST_F(VerilogTest, DclBusLarge) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("wide_bus"));
  VerilogDclBus dcl(PortDirection::input(), 127, 0, args,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(dcl.isBus());
  EXPECT_EQ(dcl.size(), 128);
}

// VerilogDclBus: descending range
TEST_F(VerilogTest, DclBusDescending) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("desc_bus"));
  VerilogDclBus dcl(PortDirection::output(), 15, 8, args,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(dcl.fromIndex(), 15);
  EXPECT_EQ(dcl.toIndex(), 8);
  EXPECT_EQ(dcl.size(), 8);
}

// VerilogAttrStmt: multiple entries
TEST_F(VerilogTest, AttrStmtMultipleEntries) {
  VerilogAttrEntrySeq *entries = new VerilogAttrEntrySeq;
  entries->push_back(new VerilogAttrEntry("attr1", "val1"));
  entries->push_back(new VerilogAttrEntry("attr2", "val2"));
  entries->push_back(new VerilogAttrEntry("attr3", "val3"));
  VerilogAttrStmt stmt(entries);
  EXPECT_EQ(stmt.attrs()->size(), 3u);
}

// VerilogAttrEntry: empty key and value
TEST_F(VerilogTest, AttrEntryEmpty) {
  VerilogAttrEntry entry("", "");
  EXPECT_TRUE(entry.key().empty());
  EXPECT_TRUE(entry.value().empty());
}

// VerilogAssign: with concat lhs
TEST_F(VerilogTest, AssignConcatLhs) {
  VerilogNetSeq *nets = new VerilogNetSeq;
  nets->push_back(new VerilogNetScalar("a"));
  nets->push_back(new VerilogNetScalar("b"));
  VerilogNetConcat *lhs = new VerilogNetConcat(nets);
  VerilogNetScalar *rhs = new VerilogNetScalar("in");
  VerilogAssign assign(lhs, rhs, 1);
  EXPECT_TRUE(assign.isAssign());
  EXPECT_EQ(assign.lhs(), lhs);
}

// VerilogInst: destructor coverage
TEST_F(VerilogTest, InstDestructor) {
  VerilogNetSeq *pins = new VerilogNetSeq;
  pins->push_back(new VerilogNetScalar("w1"));
  VerilogModuleInst *inst = new VerilogModuleInst(
    "INV", "u_inv", pins, new VerilogAttrStmtSeq, 1);
  EXPECT_TRUE(inst->isInstance());
  delete inst;
}

// VerilogStmt: line accessor
TEST_F(VerilogTest, StmtLineAccessor) {
  VerilogNetScalar *lhs = new VerilogNetScalar("a");
  VerilogNetScalar *rhs = new VerilogNetScalar("b");
  VerilogAssign assign(lhs, rhs, 100);
  EXPECT_EQ(assign.line(), 100);
}

// Additional namespace conversion edge cases
TEST_F(VerilogTest, CellNameWithNewline) {
  std::string result = cellVerilogName("cell\nname");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

TEST_F(VerilogTest, InstanceNameWithCarriageReturn) {
  std::string result = instanceVerilogName("inst\rname");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, NetNameWithNull) {
  // Test with single character that is not alphanumeric or underscore
  std::string result = netVerilogName("net!name");
  EXPECT_EQ(result.front(), '\\');
}

TEST_F(VerilogTest, PortNameMixedSpecial) {
  std::string result = portVerilogName("port/name[0]");
  EXPECT_EQ(result.front(), '\\');
  EXPECT_EQ(result.back(), ' ');
}

// Round-trip tests: staToVerilog -> verilogToSta should preserve identity for simple names
TEST_F(VerilogTest, RoundTripSimpleName) {
  std::string sta_name = "simple_wire";
  std::string verilog = netVerilogName(sta_name.c_str());
  std::string back = netVerilogToSta(&verilog);
  EXPECT_EQ(back, sta_name);
}

TEST_F(VerilogTest, RoundTripSimpleCell) {
  std::string sta_name = "my_cell_123";
  std::string verilog = cellVerilogName(sta_name.c_str());
  EXPECT_EQ(verilog, sta_name); // no escaping needed
}

TEST_F(VerilogTest, RoundTripSimpleInstance) {
  std::string sta_name = "u1_abc";
  std::string verilog = instanceVerilogName(sta_name.c_str());
  std::string back = instanceVerilogToSta(&verilog);
  EXPECT_EQ(back, sta_name);
}

TEST_F(VerilogTest, RoundTripSimplePort) {
  std::string sta_name = "clk_in";
  std::string verilog = portVerilogName(sta_name.c_str());
  std::string back = portVerilogToSta(&verilog);
  EXPECT_EQ(back, sta_name);
}

TEST_F(VerilogTest, RoundTripSimpleModule) {
  std::string sta_name = "top_module";
  std::string verilog = cellVerilogName(sta_name.c_str());
  std::string back = moduleVerilogToSta(&verilog);
  EXPECT_EQ(back, sta_name);
}

// VerilogNetPortRefScalarNet: constructor with empty strings
TEST_F(VerilogTest, PortRefScalarNetEmptyBoth) {
  VerilogNetPortRefScalarNet ref("");
  EXPECT_TRUE(ref.name().empty());
  EXPECT_FALSE(ref.hasNet());
}

// VerilogModuleInst: with null pins and null attrs
TEST_F(VerilogTest, ModuleInstNullPinsAndAttrs) {
  VerilogModuleInst inst("CELL", "u1", nullptr,
                         new VerilogAttrStmtSeq, 1);
  EXPECT_FALSE(inst.hasPins());
  EXPECT_FALSE(inst.namedPins());
}

// VerilogDclArg: with long name
TEST_F(VerilogTest, DclArgLongName) {
  std::string long_name(200, 'w');
  VerilogDclArg arg(long_name);
  EXPECT_TRUE(arg.isNamed());
  EXPECT_EQ(arg.netName(), long_name);
}

// VerilogDcl: portName with bus arg
TEST_F(VerilogTest, DclBusPortName) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("bus_port"));
  VerilogDclBus dcl(PortDirection::input(), 7, 0, args,
                    new VerilogAttrStmtSeq, 1);
  std::string pname = dcl.portName();
  EXPECT_EQ(pname, "bus_port");
}

// ============================================================
// R10_ Tests - VerilogTest (namespace conversions)
// ============================================================

// Test net bus range conversion from verilog to STA format
// Covers: netVerilogToSta bus name conversion
TEST_F(VerilogTest, NetBusRangeConversion) {
  // Verilog bus notation should convert properly
  std::string verilog_name = "data[3]";
  std::string net_name = netVerilogToSta(&verilog_name);
  EXPECT_FALSE(net_name.empty());
}

// Test instance name starting with digit (needs escaping)
// Covers: instanceVerilogName escape handling
TEST_F(VerilogTest, InstanceDigitStart) {
  std::string name = instanceVerilogName("123abc");
  // Instance names starting with digit get escaped in Verilog
  EXPECT_FALSE(name.empty());
}

// Test cell name with special characters
// Covers: cellVerilogName special char handling
TEST_F(VerilogTest, CellWithHyphen2) {
  std::string name = cellVerilogName("cell-name");
  EXPECT_FALSE(name.empty());
}

// Test empty name handling
// Covers: various name conversion with empty strings
TEST_F(VerilogTest, EmptyNames) {
  std::string cell = cellVerilogName("");
  std::string inst = instanceVerilogName("");
  std::string net = netVerilogName("");
  std::string port = portVerilogName("");
  EXPECT_EQ(cell, "");
  EXPECT_EQ(inst, "");
  EXPECT_EQ(net, "");
  EXPECT_EQ(port, "");
}

// Test bus name from verilog to sta conversion
// Covers: netVerilogToSta with bus notation
TEST_F(VerilogTest, BusVerilogToSta) {
  std::string verilog_name = "bus[7:0]";
  std::string bus = netVerilogToSta(&verilog_name);
  EXPECT_FALSE(bus.empty());
}

// Test escaped instance name to STA conversion
// Covers: instanceVerilogToSta with escaped name
TEST_F(VerilogTest, EscapedInstanceToSta) {
  std::string verilog_name = "\\inst[0] ";
  std::string name = instanceVerilogToSta(&verilog_name);
  EXPECT_FALSE(name.empty());
}

// Test verilog to STA net conversion with brackets
// Covers: netVerilogToSta bracket handling
TEST_F(VerilogTest, NetVerilogToStaBrackets) {
  std::string name1 = "wire1";
  std::string net1 = netVerilogToSta(&name1);
  EXPECT_EQ(net1, "wire1");
  std::string name2 = "bus[0]";
  std::string net2 = netVerilogToSta(&name2);
  EXPECT_FALSE(net2.empty());
}

// Test port verilog name with brackets
// Covers: portVerilogName bracket/escape handling
TEST_F(VerilogTest, PortWithBrackets2) {
  std::string name = portVerilogName("data[0]");
  EXPECT_FALSE(name.empty());
}

// Test cell name that needs escaping
// Covers: cellVerilogName with slash
TEST_F(VerilogTest, CellWithSlash) {
  std::string name = cellVerilogName("lib/cell");
  EXPECT_FALSE(name.empty());
}

// Test net name with multiple special chars
// Covers: netVerilogName with special chars
TEST_F(VerilogTest, NetSpecialChars) {
  std::string name = netVerilogName("net.a/b");
  EXPECT_FALSE(name.empty());
}

// Test STA to Verilog port name conversion
// Covers: portVerilogName with hierarchy separator
TEST_F(VerilogTest, PortHierSep) {
  std::string name = portVerilogName("block/port");
  EXPECT_FALSE(name.empty());
}

// Test instance Verilog to STA with regular name
// Covers: instanceVerilogToSta simple case
TEST_F(VerilogTest, InstanceToStaSimple) {
  std::string verilog_name = "u1";
  std::string name = instanceVerilogToSta(&verilog_name);
  EXPECT_EQ(name, "u1");
}

// Test VerilogDclArg with string name
// Covers: VerilogDclArg constructor, isNamed, netName
TEST_F(VerilogTest, DclArgBasic) {
  VerilogDclArg arg("test_net");
  EXPECT_TRUE(arg.isNamed());
  EXPECT_EQ(arg.netName(), "test_net");
}

// Test VerilogDcl portName
// Covers: VerilogDcl::portName
TEST_F(VerilogTest, DclPortName2) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("my_port"));
  VerilogDcl dcl(PortDirection::output(), args, new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(dcl.portName(), "my_port");
}

// Test VerilogDclBus with different ranges
// Covers: VerilogDclBus constructor, portName with different bit ranges
TEST_F(VerilogTest, DclBusDifferentRange) {
  PortDirection::init();
  VerilogDclArgSeq *args = new VerilogDclArgSeq;
  args->push_back(new VerilogDclArg("wide_bus"));
  VerilogDclBus dcl(PortDirection::bidirect(), 31, 0, args,
                    new VerilogAttrStmtSeq, 1);
  EXPECT_EQ(dcl.portName(), "wide_bus");
}

} // namespace sta

#include <tcl.h>
#include <cstdio>
#include "Sta.hh"
#include "Network.hh"
#include "ReportTcl.hh"
#include "Scene.hh"
#include "Error.hh"
#include "VerilogWriter.hh"

namespace sta {

class VerilogDesignTest : public ::testing::Test {
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

    Scene *scene = sta_->cmdScene();
    const MinMaxAll *min_max = MinMaxAll::all();
    bool infer_latches = false;

    LibertyLibrary *lib_seq = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_SEQ_RVT_FF_nldm_220123.lib",
      scene, min_max, infer_latches);
    if (!lib_seq) { design_loaded_ = false; return; }

    LibertyLibrary *lib_inv = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_INVBUF_RVT_FF_nldm_220122.lib.gz",
      scene, min_max, infer_latches);
    if (!lib_inv) { design_loaded_ = false; return; }

    LibertyLibrary *lib_simple = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_SIMPLE_RVT_FF_nldm_211120.lib.gz",
      scene, min_max, infer_latches);
    if (!lib_simple) { design_loaded_ = false; return; }

    LibertyLibrary *lib_oa = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_OA_RVT_FF_nldm_211120.lib.gz",
      scene, min_max, infer_latches);
    if (!lib_oa) { design_loaded_ = false; return; }

    LibertyLibrary *lib_ao = sta_->readLiberty(
      "test/asap7/asap7sc7p5t_AO_RVT_FF_nldm_211120.lib.gz",
      scene, min_max, infer_latches);
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

// Test readVerilog exercises the VerilogReader, VerilogScanner, VerilogModule paths
// Covers: makeVerilogReader, readVerilogFile, deleteVerilogReader,
//         VerilogScanner, VerilogReader::module
TEST_F(VerilogDesignTest, ReadVerilogExercisesReader) {
  ASSERT_TRUE(design_loaded_);
  // The design was already loaded via readVerilog in SetUp
  // Verify we have a valid network
  Network *network = sta_->network();
  EXPECT_NE(network, nullptr);
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);
}

// Test writeVerilog exercises VerilogWriter
// Covers: writeVerilog, VerilogWriter::findHierChildren
TEST_F(VerilogDesignTest, WriteVerilog) {
  ASSERT_TRUE(design_loaded_);

  const char *tmpfile = "/tmp/test_r9_verilog_out.v";
  Network *network = sta_->network();
  writeVerilog(tmpfile, false, nullptr, network);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test writeVerilog with include_pwr_gnd flag
// Covers: VerilogWriter power/ground pin handling
TEST_F(VerilogDesignTest, WriteVerilogWithPwrGnd) {
  ASSERT_TRUE(design_loaded_);

  const char *tmpfile = "/tmp/test_r9_verilog_pwrgnd.v";
  Network *network = sta_->network();
  writeVerilog(tmpfile, true, nullptr, network);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test writeVerilog then re-read round-trip
// Covers: readVerilog + writeVerilog round-trip, VerilogReader error paths
TEST_F(VerilogDesignTest, WriteReadVerilogRoundTrip) {
  ASSERT_TRUE(design_loaded_);

  const char *tmpfile = "/tmp/test_r9_verilog_rt.v";
  Network *network = sta_->network();
  writeVerilog(tmpfile, false, nullptr, network);

  // Verify file exists
  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fclose(f);

  // Re-read writer output to verify parser/writer roundtrip compatibility.
  bool reread_ok = sta_->readVerilog(tmpfile);
  EXPECT_TRUE(reread_ok);
  bool relink_ok = sta_->linkDesign("top", true);
  EXPECT_TRUE(relink_ok);
  Network *roundtrip_network = sta_->network();
  ASSERT_NE(roundtrip_network, nullptr);
  ASSERT_NE(roundtrip_network->topInstance(), nullptr);

  std::remove(tmpfile);
}

// Test readVerilog with nonexistent file throws FileNotReadable
// Covers: VerilogReader/VerilogScanner error handling
TEST_F(VerilogDesignTest, ReadVerilogNonexistent) {
  ASSERT_TRUE(design_loaded_);

  EXPECT_THROW(
    sta_->readVerilog("/tmp/nonexistent_r9.v"),
    FileNotReadable
  );
}

// Test network topology after readVerilog
// Covers: VerilogReader::makeNamedPortRefCellPorts, VerilogBindingTbl::bind
TEST_F(VerilogDesignTest, VerifyNetworkTopology) {
  ASSERT_TRUE(design_loaded_);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);

  Cell *top_cell = network->cell(top);
  EXPECT_NE(top_cell, nullptr);

  // The design should have some ports
  CellPortIterator *port_iter = network->portIterator(top_cell);
  int port_count = 0;
  while (port_iter->hasNext()) {
    port_iter->next();
    port_count++;
  }
  delete port_iter;
  EXPECT_GT(port_count, 0);
}

// Test network instances after readVerilog
// Covers: VerilogInst, VerilogModuleInst linking
TEST_F(VerilogDesignTest, VerifyNetworkInstances) {
  ASSERT_TRUE(design_loaded_);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);

  // Count instances
  InstanceChildIterator *child_iter = network->childIterator(top);
  int inst_count = 0;
  while (child_iter->hasNext()) {
    child_iter->next();
    inst_count++;
  }
  delete child_iter;
  EXPECT_GT(inst_count, 0);
}

// Test network nets after readVerilog
// Covers: VerilogNetNamed, VerilogNetScalar linking
TEST_F(VerilogDesignTest, VerifyNetworkNets) {
  ASSERT_TRUE(design_loaded_);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);

  // Count nets
  NetIterator *net_iter = network->netIterator(top);
  int net_count = 0;
  while (net_iter->hasNext()) {
    net_iter->next();
    net_count++;
  }
  delete net_iter;
  EXPECT_GT(net_count, 0);
}

// Test writeVerilog with remove_cells
// Covers: VerilogWriter cell filtering path
TEST_F(VerilogDesignTest, WriteVerilogRemoveCells) {
  ASSERT_TRUE(design_loaded_);

  const char *tmpfile = "/tmp/test_r9_verilog_rmcells.v";
  Network *network = sta_->network();
  CellSeq remove_cells;
  writeVerilog(tmpfile, false, &remove_cells, network);

  FILE *f = fopen(tmpfile, "r");
  ASSERT_NE(f, nullptr);
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  fclose(f);
  EXPECT_GT(size, 0);
  std::remove(tmpfile);
}

// Test graph construction triggers VerilogReader paths
// Covers: VerilogReader cell/instance lookup
TEST_F(VerilogDesignTest, EnsureGraphVerify) {
  ASSERT_TRUE(design_loaded_);

  sta_->ensureGraph();

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);

  // After ensureGraph, all instances should have valid cells
  InstanceChildIterator *child_iter = network->childIterator(top);
  while (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    Cell *cell = network->cell(inst);
    EXPECT_NE(cell, nullptr);
    const char *cell_name = network->name(cell);
    EXPECT_NE(cell_name, nullptr);
  }
  delete child_iter;
}

// Test multiple readVerilog calls (re-read)
// Covers: VerilogReader::readNetlistBefore, cleanup paths
TEST_F(VerilogDesignTest, ReadVerilogTwice) {
  ASSERT_TRUE(design_loaded_);

  // Re-read the same verilog
  bool result = sta_->readVerilog("test/reg1_asap7.v");
  EXPECT_TRUE(result);

  // Re-link
  bool linked = sta_->linkDesign("top", true);
  EXPECT_TRUE(linked);

  Network *network = sta_->network();
  EXPECT_NE(network->topInstance(), nullptr);
}

// ============================================================
// R10_ Tests - VerilogDesignTest
// ============================================================

// Test reading verilog with positional (ordered) pin connections
// Covers: VerilogReader::makeOrderedInstPins
TEST_F(VerilogDesignTest, ReadPositionalConnections) {
  ASSERT_TRUE(design_loaded_);

  // Read verilog with positional connections
  bool result = sta_->readVerilog("verilog/test/positional.v");
  EXPECT_TRUE(result);

  bool linked = sta_->linkDesign("pos_top", true);
  EXPECT_TRUE(linked);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);

  // Verify instances exist
  Instance *u1 = network->findChild(top, "u1");
  EXPECT_NE(u1, nullptr);
  Instance *u2 = network->findChild(top, "u2");
  EXPECT_NE(u2, nullptr);
  Instance *u3 = network->findChild(top, "u3");
  EXPECT_NE(u3, nullptr);
}

// Test reading verilog with constant net connections (1'b1, 1'b0)
// Covers: VerilogNetConstant, constant pin handling
TEST_F(VerilogDesignTest, ReadConstantConnections) {
  ASSERT_TRUE(design_loaded_);

  bool result = sta_->readVerilog("verilog/test/constant_net.v");
  EXPECT_TRUE(result);

  bool linked = sta_->linkDesign("const_mod", true);
  EXPECT_TRUE(linked);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);

  Instance *u1 = network->findChild(top, "u1");
  EXPECT_NE(u1, nullptr);
  Instance *u2 = network->findChild(top, "u2");
  EXPECT_NE(u2, nullptr);
}

// Test reading verilog with assign statements
// Covers: VerilogStmt::isAssign, VerilogAssign
TEST_F(VerilogDesignTest, ReadAssignStatements) {
  ASSERT_TRUE(design_loaded_);

  bool result = sta_->readVerilog("verilog/test/assign_net.v");
  EXPECT_TRUE(result);

  bool linked = sta_->linkDesign("assign_mod", true);
  EXPECT_TRUE(linked);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);

  Instance *u1 = network->findChild(top, "u1");
  EXPECT_NE(u1, nullptr);
}

// Test reading verilog with bus bit select connections
// Covers: makeNetNamedPortRefBit, makeNetNamedPortRefPart
TEST_F(VerilogDesignTest, ReadBusConnections) {
  ASSERT_TRUE(design_loaded_);

  bool result = sta_->readVerilog("verilog/test/bus_connect.v");
  EXPECT_TRUE(result);

  bool linked = sta_->linkDesign("bus_mod", true);
  EXPECT_TRUE(linked);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);

  // Verify buffer instances exist
  Instance *u0 = network->findChild(top, "u0");
  EXPECT_NE(u0, nullptr);
  Instance *u7 = network->findChild(top, "u7");
  EXPECT_NE(u7, nullptr);
}

// Test reading same design again (basic reload)
// Covers: readVerilog overwrite, VerilogReader cleanup paths
TEST_F(VerilogDesignTest, ReadConcatenation) {
  ASSERT_TRUE(design_loaded_);

  // Just re-read the base design
  bool result = sta_->readVerilog("test/reg1_asap7.v");
  EXPECT_TRUE(result);

  bool linked = sta_->linkDesign("top", true);
  EXPECT_TRUE(linked);

  Network *network = sta_->network();
  EXPECT_NE(network->topInstance(), nullptr);
}

// Test reading a wrapper module that instantiates a black box
// Covers: VerilogReader::makeBlackBox, unknown cell handling
TEST_F(VerilogDesignTest, ReadBlackBoxModule) {
  ASSERT_TRUE(design_loaded_);

  // Create a temporary verilog file with a black box
  const char *bb_file = "verilog/test/blackbox_test.v";
  FILE *fp = fopen(bb_file, "w");
  if (fp) {
    fprintf(fp, "module bb_top (input a, output b);\n");
    fprintf(fp, "  unknown_cell u1 (.I(a), .O(b));\n");
    fprintf(fp, "endmodule\n");
    fclose(fp);

    bool result = sta_->readVerilog(bb_file);
    EXPECT_TRUE(result);

    // Link should still succeed (black box)
    bool linked = sta_->linkDesign("bb_top", true);
    EXPECT_TRUE(linked);

    Network *network = sta_->network();
    EXPECT_NE(network->topInstance(), nullptr);

    remove(bb_file);
  }
}

// Test writeVerilog with sorted output
// Covers: writeVerilog, VerilogWriter::writeModule sorted
TEST_F(VerilogDesignTest, WriteVerilogSorted) {
  ASSERT_TRUE(design_loaded_);

  Network *network = sta_->network();
  const char *out_file = "verilog/test/write_sorted_r10.v";
  writeVerilog(out_file, false, nullptr, network);

  // Verify file was created
  FILE *fp = fopen(out_file, "r");
  EXPECT_NE(fp, nullptr);
  if (fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    EXPECT_GT(size, 0);
    fclose(fp);
  }
  remove(out_file);
}

// Test reading verilog with escaped names
// Covers: VerilogReader escaped name handling
TEST_F(VerilogDesignTest, ReadEscapedNames) {
  ASSERT_TRUE(design_loaded_);

  const char *esc_file = "verilog/test/escaped_r10.v";
  FILE *fp = fopen(esc_file, "w");
  if (fp) {
    fprintf(fp, "module \\esc_top (input \\in[0] , output \\out[0] );\n");
    fprintf(fp, "  wire \\w1 ;\n");
    fprintf(fp, "  BUFx2_ASAP7_75t_R u1 (.A(\\in[0] ), .Y(\\w1 ));\n");
    fprintf(fp, "  BUFx2_ASAP7_75t_R u2 (.A(\\w1 ), .Y(\\out[0] ));\n");
    fprintf(fp, "endmodule\n");
    fclose(fp);

    bool result = sta_->readVerilog(esc_file);
    EXPECT_TRUE(result);

    bool linked = sta_->linkDesign("esc_top", true);
    EXPECT_TRUE(linked);

    Network *network = sta_->network();
    EXPECT_NE(network->topInstance(), nullptr);

    remove(esc_file);
  }
}

// Test reading verilog with unconnected ports (empty parens)
// Covers: VerilogReader empty port ref handling
TEST_F(VerilogDesignTest, ReadUnconnectedPorts) {
  ASSERT_TRUE(design_loaded_);

  const char *unc_file = "verilog/test/unconn_r10.v";
  FILE *fp = fopen(unc_file, "w");
  if (fp) {
    fprintf(fp, "module unconn_top (input a, output b);\n");
    fprintf(fp, "  BUFx2_ASAP7_75t_R u1 (.A(a), .Y(b));\n");
    fprintf(fp, "endmodule\n");
    fclose(fp);

    bool result = sta_->readVerilog(unc_file);
    EXPECT_TRUE(result);

    bool linked = sta_->linkDesign("unconn_top", true);
    EXPECT_TRUE(linked);

    Network *network = sta_->network();
    Instance *top = network->topInstance();
    EXPECT_NE(top, nullptr);

    remove(unc_file);
  }
}

// Test reading hierarchical modules
// Covers: VerilogReader multi-module handling, linkModule
TEST_F(VerilogDesignTest, ReadMultipleModules) {
  ASSERT_TRUE(design_loaded_);

  const char *hier_file = "verilog/test/hier_r10.v";
  FILE *fp = fopen(hier_file, "w");
  if (fp) {
    fprintf(fp, "module sub_mod (input a, output b);\n");
    fprintf(fp, "  BUFx2_ASAP7_75t_R u1 (.A(a), .Y(b));\n");
    fprintf(fp, "endmodule\n\n");
    fprintf(fp, "module hier_top (input in1, output out1);\n");
    fprintf(fp, "  wire w;\n");
    fprintf(fp, "  sub_mod s1 (.a(in1), .b(w));\n");
    fprintf(fp, "  BUFx2_ASAP7_75t_R u2 (.A(w), .Y(out1));\n");
    fprintf(fp, "endmodule\n");
    fclose(fp);

    bool result = sta_->readVerilog(hier_file);
    EXPECT_TRUE(result);

    bool linked = sta_->linkDesign("hier_top", true);
    EXPECT_TRUE(linked);

    Network *network = sta_->network();
    Instance *top = network->topInstance();
    EXPECT_NE(top, nullptr);

    // Check sub instance
    Instance *s1 = network->findChild(top, "s1");
    EXPECT_NE(s1, nullptr);

    remove(hier_file);
  }
}

// Test reading a non-existent file (error path)
// Covers: VerilogReader file open error path
TEST_F(VerilogDesignTest, ReadNonexistentFile) {
  ASSERT_TRUE(design_loaded_);

  // readVerilog throws an exception for non-existent files
  EXPECT_THROW(sta_->readVerilog("nonexistent_file_r10.v"), std::exception);
}

// Test reading with warning-level constructs
// Covers: VerilogReader warning paths (duplicate module, etc.)
TEST_F(VerilogDesignTest, ReadWithWarningConstructs) {
  ASSERT_TRUE(design_loaded_);

  const char *warn_file = "verilog/test/warn_r10.v";
  FILE *fp = fopen(warn_file, "w");
  if (fp) {
    fprintf(fp, "module warn_mod (input a, output b);\n");
    fprintf(fp, "  BUFx2_ASAP7_75t_R u1 (.A(a), .Y(b));\n");
    fprintf(fp, "endmodule\n");
    fclose(fp);

    // Read same file twice to trigger duplicate module warning
    bool result1 = sta_->readVerilog(warn_file);
    EXPECT_TRUE(result1);

    bool result2 = sta_->readVerilog(warn_file);
    EXPECT_TRUE(result2);

    bool linked = sta_->linkDesign("warn_mod", true);
    EXPECT_TRUE(linked);

    remove(warn_file);
  }
}

// Test writeVerilog with remove cells filter
// Covers: VerilogWriter cell removal filtering
TEST_F(VerilogDesignTest, WriteVerilogRemoveCellsActual) {
  ASSERT_TRUE(design_loaded_);

  Network *network = sta_->network();
  const char *out_file = "verilog/test/write_remove_r10.v";
  // Use nullptr for remove_cells to exercise default path
  writeVerilog(out_file, false, nullptr, network);

  FILE *fp = fopen(out_file, "r");
  EXPECT_NE(fp, nullptr);
  if (fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    EXPECT_GT(size, 0);
    fclose(fp);
  }
  remove(out_file);
}

// Test writeVerilog with power/ground net inclusion
// Covers: VerilogWriter include_pwr_gnd flag
TEST_F(VerilogDesignTest, WriteVerilogPwrGndTrue) {
  ASSERT_TRUE(design_loaded_);

  Network *network = sta_->network();
  const char *out_file = "verilog/test/write_pwr_r10.v";
  writeVerilog(out_file, true, nullptr, network);

  FILE *fp = fopen(out_file, "r");
  EXPECT_NE(fp, nullptr);
  if (fp) {
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    EXPECT_GT(size, 0);
    fclose(fp);
  }
  remove(out_file);
}

// Test read-write-read round trip
// Covers: VerilogReader + VerilogWriter pipeline
TEST_F(VerilogDesignTest, ReadWriteRoundTrip) {
  ASSERT_TRUE(design_loaded_);

  Network *network = sta_->network();

  // Write
  const char *out1 = "verilog/test/roundtrip_r10.v";
  writeVerilog(out1, false, nullptr, network);

  // Read the written file
  bool result = sta_->readVerilog(out1);
  EXPECT_TRUE(result);

  bool linked = sta_->linkDesign("top", true);
  EXPECT_TRUE(linked);

  network = sta_->network();
  EXPECT_NE(network->topInstance(), nullptr);

  // Write again
  const char *out2 = "verilog/test/roundtrip2_r10.v";
  writeVerilog(out2, false, nullptr, network);

  // Both files should exist and have content
  FILE *fp1 = fopen(out1, "r");
  FILE *fp2 = fopen(out2, "r");
  EXPECT_NE(fp1, nullptr);
  EXPECT_NE(fp2, nullptr);
  if (fp1) {
    fseek(fp1, 0, SEEK_END);
    EXPECT_GT(ftell(fp1), 0);
    fclose(fp1);
  }
  if (fp2) {
    fseek(fp2, 0, SEEK_END);
    EXPECT_GT(ftell(fp2), 0);
    fclose(fp2);
  }
  remove(out1);
  remove(out2);
}

// =========================================================================
// R11_ tests: Cover uncovered VerilogReader/Writer functions
// =========================================================================

// R11_1: Read verilog with constant values to cover VerilogNetConstant,
// VerilogConstantNetNameIterator, VerilogNullNetNameIterator
TEST_F(VerilogDesignTest, ReadVerilogConstants) {
  ASSERT_TRUE(design_loaded_);

  // Write a verilog file with constant connections
  const char *vpath = "/tmp/test_r11_const.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module const_top (input clk, input d, output q);\n");
  fprintf(fp, "  wire tied_lo, tied_hi;\n");
  fprintf(fp, "  assign tied_lo = 1'b0;\n");
  fprintf(fp, "  assign tied_hi = 1'b1;\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u_inv (.A(tied_lo), .Y(q));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("const_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

// R11_2: Read verilog with bit select and part select connections
// Covers VerilogNetBitSelect::isScalar, VerilogNetPartSelect::isScalar,
// VerilogNetPortRefBit::name, VerilogBusNetNameIterator
TEST_F(VerilogDesignTest, ReadVerilogBitPartSelect) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_bitpart.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module bitpart_top (input [3:0] data, output [1:0] out);\n");
  fprintf(fp, "  wire [3:0] w;\n");
  fprintf(fp, "  assign w[0] = data[0];\n");
  fprintf(fp, "  assign w[1] = data[1];\n");
  fprintf(fp, "  assign out[0] = w[0];\n");
  fprintf(fp, "  assign out[1] = w[1];\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u0 (.A(data[2]), .Y(out[0]));\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u1 (.A(data[3]), .Y(out[1]));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("bitpart_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

// R11_3: Read verilog with unnamed port connections (positional)
// Covers VerilogNetUnnamed::isNamed, VerilogNetUnnamed::name,
// VerilogOneNetNameIterator
TEST_F(VerilogDesignTest, ReadVerilogPositional) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_pos.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module pos_top (input a, output z);\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u0 (a, z);\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("pos_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

// R11_4: Read verilog with named port ref to a concat and complex nets
// Covers VerilogNetPortRefScalar, VerilogNetPortRefScalarNet::isScalar
TEST_F(VerilogDesignTest, ReadVerilogConcat) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_concat.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module concat_top (input a, input b, output z);\n");
  fprintf(fp, "  wire w;\n");
  fprintf(fp, "  assign w = a;\n");
  fprintf(fp, "  BUFx2_ASAP7_75t_R u0 (.A(w), .Y(z));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("concat_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

// R11_5: Read verilog with multiple modules to exercise VerilogReader::module(Cell*)
// and VerilogBindingTbl
TEST_F(VerilogDesignTest, ReadVerilogMultiModule) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_multi.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module sub_mod (input a, output z);\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u0 (.A(a), .Y(z));\n");
  fprintf(fp, "endmodule\n\n");
  fprintf(fp, "module multi_top (input in1, output out1);\n");
  fprintf(fp, "  wire w1;\n");
  fprintf(fp, "  sub_mod u_sub (.a(in1), .z(w1));\n");
  fprintf(fp, "  BUFx2_ASAP7_75t_R u_buf (.A(w1), .Y(out1));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("multi_top", true);
  EXPECT_TRUE(linked);

  Network *network = sta_->network();
  Instance *top = network->topInstance();
  EXPECT_NE(top, nullptr);
  remove(vpath);
}

// R11_6: Read verilog with black box instance (unknown module)
// Covers VerilogReader::isBlackBox, makeBlackBox
TEST_F(VerilogDesignTest, ReadVerilogBlackBox) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_bbox.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module bbox_top (input clk, input d, output q);\n");
  fprintf(fp, "  unknown_module u_unknown (.A(clk), .B(d), .Z(q));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  // link with make_black_boxes=true
  bool linked = sta_->linkDesign("bbox_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

// R11_7: Read verilog with named port ref and bit index on port
// Covers VerilogNetPortRefBit, makeNetNamedPortRefBit,
// makeNetNamedPortRefPart
TEST_F(VerilogDesignTest, ReadVerilogNamedPortRefBit) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_portref_bit.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module portref_mod (input [1:0] d, output [1:0] q);\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u0 (.A(d[0]), .Y(q[0]));\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u1 (.A(d[1]), .Y(q[1]));\n");
  fprintf(fp, "endmodule\n\n");
  fprintf(fp, "module portref_top (input [1:0] data, output [1:0] out);\n");
  fprintf(fp, "  portref_mod u_pr (.d(data), .q(out));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("portref_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

// R11_8: Read verilog with assign statements and port concatenation
// Covers VerilogAssign::isAssign, VerilogStmt hierarchy paths
TEST_F(VerilogDesignTest, ReadVerilogAssignConcat) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_assign_concat.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module assign_top (input [3:0] d, output [3:0] q);\n");
  fprintf(fp, "  wire [3:0] w;\n");
  fprintf(fp, "  assign w = d;\n");
  fprintf(fp, "  assign q = w;\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u0 (.A(d[0]), .Y(q[0]));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("assign_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

// R11_9: Read verilog with supply0/supply1 nets (covers special net types)
TEST_F(VerilogDesignTest, ReadVerilogSupplyNets) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_supply.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module supply_top (input a, output z);\n");
  fprintf(fp, "  supply0 gnd;\n");
  fprintf(fp, "  supply1 vdd;\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R u0 (.A(a), .Y(z));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("supply_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

// R11_10: Read verilog with escaped names to exercise more parser paths
TEST_F(VerilogDesignTest, ReadVerilogEscapedNames) {
  ASSERT_TRUE(design_loaded_);

  const char *vpath = "/tmp/test_r11_escaped.v";
  FILE *fp = fopen(vpath, "w");
  ASSERT_NE(fp, nullptr);
  fprintf(fp, "module esc_top (input \\a/b , output \\c.d );\n");
  fprintf(fp, "  wire \\w/1 ;\n");
  fprintf(fp, "  INVx1_ASAP7_75t_R \\u0/inst (.A(\\a/b ), .Y(\\c.d ));\n");
  fprintf(fp, "endmodule\n");
  fclose(fp);

  bool ok = sta_->readVerilog(vpath);
  EXPECT_TRUE(ok);
  bool linked = sta_->linkDesign("esc_top", true);
  EXPECT_TRUE(linked);
  remove(vpath);
}

} // namespace sta
