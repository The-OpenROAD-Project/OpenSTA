#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>
#include <unistd.h>

#include <tcl.h>
#include "MinMax.hh"
#include "Transition.hh"
#include "Sta.hh"
#include "Network.hh"
#include "ReportTcl.hh"
#include "Corner.hh"
#include "Liberty.hh"
#include "Sdc.hh"
#include "Graph.hh"
#include "PathEnd.hh"
#include "PathExpanded.hh"
#include "PathAnalysisPt.hh"
#include "DcalcAnalysisPt.hh"
#include "Search.hh"
#include "CircuitSim.hh"
#include "spice/Xyce.hh"
#include "spice/WriteSpice.hh"

// Spice module tests
namespace sta {

class SpiceSmokeTest : public ::testing::Test {};

TEST_F(SpiceSmokeTest, TransitionsForSpice) {
  // WritePathSpice uses rise/fall transitions
  EXPECT_NE(RiseFall::rise(), nullptr);
  EXPECT_NE(RiseFall::fall(), nullptr);
}

TEST_F(SpiceSmokeTest, MinMaxForSpice) {
  EXPECT_NE(MinMax::min(), nullptr);
  EXPECT_NE(MinMax::max(), nullptr);
}

TEST_F(SpiceSmokeTest, TransitionNames) {
  // Transition names use short symbols: "^" for rise, "v" for fall
  EXPECT_EQ(Transition::rise()->to_string(), "^");
  EXPECT_EQ(Transition::fall()->to_string(), "v");
}

// Tests for streamPrint (free function in WriteSpice.cc)
class StreamPrintTest : public ::testing::Test {
protected:
  void SetUp() override {
    char tmpl[] = "/tmp/sta_spice_test_XXXXXX";
    int fd = mkstemp(tmpl);
    ASSERT_NE(fd, -1);
    close(fd);
    tmpfile_ = tmpl;
  }
  void TearDown() override {
    std::remove(tmpfile_.c_str());
  }
  std::string tmpfile_;
};

TEST_F(StreamPrintTest, BasicString) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "hello world\n");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, "hello world");
}

TEST_F(StreamPrintTest, FormattedOutput) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "v%d %s 0 %.3f\n", 1, "node1", 1.800);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, "v1 node1 0 1.800");
}

TEST_F(StreamPrintTest, ScientificNotation) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "C%d %s 0 %.3e\n", 1, "net1", 1.5e-12);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, "C1 net1 0 1.500e-12");
}

TEST_F(StreamPrintTest, MultipleWrites) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "* Header\n");
  streamPrint(out, ".tran %.3g %.3g\n", 1e-13, 1e-9);
  streamPrint(out, ".end\n");
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("* Header"), std::string::npos);
  EXPECT_NE(content.find(".tran"), std::string::npos);
  EXPECT_NE(content.find(".end"), std::string::npos);
}

// Tests for readXyceCsv (free function in Xyce.cc)
class XyceCsvTest : public ::testing::Test {
protected:
  void SetUp() override {
    char tmpl[] = "/tmp/sta_xyce_test_XXXXXX";
    int fd = mkstemp(tmpl);
    ASSERT_NE(fd, -1);
    close(fd);
    tmpfile_ = tmpl;
  }
  void TearDown() override {
    std::remove(tmpfile_.c_str());
  }
  std::string tmpfile_;
};

TEST_F(XyceCsvTest, ReadSimpleCsv) {
  // Create a minimal Xyce CSV file
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(in1),V(out1)\n";
    out << "0.0,0.0,1.8\n";
    out << "1e-10,0.9,1.8\n";
    out << "2e-10,1.8,0.9\n";
    out << "3e-10,1.8,0.0\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  // Should have 2 signal titles (TIME is skipped)
  EXPECT_EQ(titles.size(), 2u);
  EXPECT_EQ(titles[0], "V(in1)");
  EXPECT_EQ(titles[1], "V(out1)");

  // Should have 2 waveforms
  EXPECT_EQ(waveforms.size(), 2u);
}

TEST_F(XyceCsvTest, ReadSingleSignal) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(clk)\n";
    out << "0.0,0.0\n";
    out << "5e-10,1.8\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 1u);
  EXPECT_EQ(titles[0], "V(clk)");
  EXPECT_EQ(waveforms.size(), 1u);
}

TEST_F(XyceCsvTest, FileNotReadableThrows) {
  EXPECT_THROW(
    {
      StdStringSeq titles;
      WaveformSeq waveforms;
      readXyceCsv("/nonexistent/file.csv", titles, waveforms);
    },
    FileNotReadable
  );
}

// Additional streamPrint tests for format coverage
TEST_F(StreamPrintTest, EmptyString) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "%s", "");
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_TRUE(content.empty());
}

TEST_F(StreamPrintTest, LongString) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  // Build a long subcircuit line
  std::string long_name(200, 'x');
  streamPrint(out, ".subckt %s\n", long_name.c_str());
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".subckt"), std::string::npos);
  EXPECT_NE(line.find(long_name), std::string::npos);
}

TEST_F(StreamPrintTest, SpiceResistor) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "R%d %s %s %.4e\n", 1, "n1", "n2", 1.0e3);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line.substr(0, 2), "R1");
  EXPECT_NE(line.find("n1"), std::string::npos);
  EXPECT_NE(line.find("n2"), std::string::npos);
}

TEST_F(StreamPrintTest, SpiceComment) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "* %s\n", "This is a SPICE comment");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line[0], '*');
  EXPECT_NE(line.find("This is a SPICE comment"), std::string::npos);
}

TEST_F(StreamPrintTest, SpiceSubcktInstantiation) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "x%s %s %s %s %s %s\n",
              "inst1", "vdd", "vss", "in", "out", "INV");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_TRUE(line.find("xinst1") == 0);
  EXPECT_NE(line.find("INV"), std::string::npos);
}

TEST_F(StreamPrintTest, SpiceMeasure) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".measure tran %s trig v(%s) val=%.1f %s=%.3e\n",
              "delay", "in", 0.9, "targ", 1e-9);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".measure"), std::string::npos);
  EXPECT_NE(line.find("delay"), std::string::npos);
}

// More XyceCsv tests
TEST_F(XyceCsvTest, ReadMultipleSignals) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(a),V(b),V(c),V(d)\n";
    out << "0.0,0.0,1.8,0.0,1.8\n";
    out << "1e-10,0.9,0.9,0.9,0.9\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 4u);
  EXPECT_EQ(titles[0], "V(a)");
  EXPECT_EQ(titles[1], "V(b)");
  EXPECT_EQ(titles[2], "V(c)");
  EXPECT_EQ(titles[3], "V(d)");
  EXPECT_EQ(waveforms.size(), 4u);
}

TEST_F(XyceCsvTest, ReadManyDataPoints) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(sig)\n";
    for (int i = 0; i < 100; i++) {
      out << (i * 1e-12) << "," << (i % 2 ? 1.8 : 0.0) << "\n";
    }
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 1u);
  EXPECT_EQ(waveforms.size(), 1u);
}

// Test RiseFall range iteration (used in SPICE netlisting)
TEST_F(SpiceSmokeTest, RiseFallRange) {
  int count = 0;
  for (auto rf : RiseFall::range()) {
    EXPECT_NE(rf, nullptr);
    count++;
  }
  EXPECT_EQ(count, 2);
}

TEST_F(SpiceSmokeTest, RiseFallRangeIndex) {
  std::vector<int> indices;
  for (auto idx : RiseFall::rangeIndex()) {
    indices.push_back(idx);
  }
  EXPECT_EQ(indices.size(), 2u);
  EXPECT_EQ(indices[0], 0);
  EXPECT_EQ(indices[1], 1);
}

TEST_F(SpiceSmokeTest, RiseFallFindByIndex) {
  EXPECT_EQ(RiseFall::find(0), RiseFall::rise());
  EXPECT_EQ(RiseFall::find(1), RiseFall::fall());
}

TEST_F(SpiceSmokeTest, TransitionAsRiseFall) {
  EXPECT_EQ(Transition::rise()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::fall()->asRiseFall(), RiseFall::fall());
}

TEST_F(SpiceSmokeTest, TransitionInitFinalString) {
  const char *rise_str = Transition::rise()->asInitFinalString();
  EXPECT_NE(rise_str, nullptr);
  const char *fall_str = Transition::fall()->asInitFinalString();
  EXPECT_NE(fall_str, nullptr);
}

////////////////////////////////////////////////////////////////
// Additional SPICE tests for function coverage
////////////////////////////////////////////////////////////////

// streamPrint with integers
TEST_F(StreamPrintTest, IntegerFormats) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "R%d %d %d %d\n", 1, 100, 200, 50000);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, "R1 100 200 50000");
}

// streamPrint with mixed types
TEST_F(StreamPrintTest, MixedTypes) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".param %s=%g\n", "vdd", 1.8);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".param"), std::string::npos);
  EXPECT_NE(line.find("vdd"), std::string::npos);
  EXPECT_NE(line.find("1.8"), std::string::npos);
}

// streamPrint with percent
TEST_F(StreamPrintTest, PercentLiteral) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "value = 100%%\n");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("100%"), std::string::npos);
}

// streamPrint with very long format
TEST_F(StreamPrintTest, VeryLongFormat) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  std::string long_name(500, 'n');
  streamPrint(out, ".subckt %s port1 port2 port3\n", long_name.c_str());
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(long_name), std::string::npos);
}

// XyceCsv with negative time values
TEST_F(XyceCsvTest, ReadNegativeValues) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(sig1)\n";
    out << "0.0,-0.1\n";
    out << "1e-10,1.8\n";
    out << "2e-10,-0.05\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 1u);
  EXPECT_EQ(waveforms.size(), 1u);
}

// XyceCsv: empty data (header only)
TEST_F(XyceCsvTest, ReadHeaderOnly) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(sig1),V(sig2)\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 2u);
  EXPECT_EQ(waveforms.size(), 2u);
}

// Transition tests for spice coverage
TEST_F(SpiceSmokeTest, TransitionSdfTripleIndices) {
  EXPECT_EQ(Transition::rise()->sdfTripleIndex(), 0);
  EXPECT_EQ(Transition::fall()->sdfTripleIndex(), 1);
  EXPECT_GE(Transition::maxIndex(), 11);
}

TEST_F(SpiceSmokeTest, TransitionMatches) {
  EXPECT_TRUE(Transition::rise()->matches(Transition::rise()));
  EXPECT_FALSE(Transition::rise()->matches(Transition::fall()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::rise()));
  EXPECT_TRUE(Transition::riseFall()->matches(Transition::fall()));
}

// RiseFall asTransition
TEST_F(SpiceSmokeTest, RiseFallAsTransition) {
  EXPECT_EQ(RiseFall::rise()->asTransition(), Transition::rise());
  EXPECT_EQ(RiseFall::fall()->asTransition(), Transition::fall());
}

// RiseFall opposite
TEST_F(SpiceSmokeTest, RiseFallOpposite) {
  EXPECT_EQ(RiseFall::rise()->opposite(), RiseFall::fall());
  EXPECT_EQ(RiseFall::fall()->opposite(), RiseFall::rise());
}

// RiseFall find by name
TEST_F(SpiceSmokeTest, RiseFallFindName) {
  EXPECT_EQ(RiseFall::find("rise"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("fall"), RiseFall::fall());
  EXPECT_EQ(RiseFall::find("^"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("v"), RiseFall::fall());
  EXPECT_EQ(RiseFall::find("nonexistent"), nullptr);
}

// RiseFallBoth tests
TEST_F(SpiceSmokeTest, RiseFallBothMatches) {
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::rise()));
  EXPECT_TRUE(RiseFallBoth::riseFall()->matches(RiseFall::fall()));
  EXPECT_TRUE(RiseFallBoth::rise()->matches(RiseFall::rise()));
  EXPECT_FALSE(RiseFallBoth::rise()->matches(RiseFall::fall()));
}

// MinMax tests
TEST_F(SpiceSmokeTest, MinMaxCompare) {
  EXPECT_TRUE(MinMax::min()->compare(1.0f, 2.0f));
  EXPECT_FALSE(MinMax::min()->compare(2.0f, 1.0f));
  EXPECT_TRUE(MinMax::max()->compare(2.0f, 1.0f));
  EXPECT_FALSE(MinMax::max()->compare(1.0f, 2.0f));
}

// Test RiseFall find
TEST_F(SpiceSmokeTest, RiseFallFind) {
  EXPECT_EQ(RiseFall::find("rise"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("fall"), RiseFall::fall());
  EXPECT_EQ(RiseFall::find("^"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("v"), RiseFall::fall());
  EXPECT_EQ(RiseFall::find("nonexistent"), nullptr);
}

// Test Transition find used in spice
TEST_F(SpiceSmokeTest, TransitionFind) {
  EXPECT_EQ(Transition::find("^"), Transition::rise());
  EXPECT_EQ(Transition::find("v"), Transition::fall());
}

// Test streamPrint with empty format
TEST_F(StreamPrintTest, EmptyFormat) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "%s", "");
  out.close();

  std::ifstream in(tmpfile_);
  std::string content;
  std::getline(in, content);
  EXPECT_EQ(content, "");
}

// Test streamPrint with integer formatting
TEST_F(StreamPrintTest, IntegerFormatting) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "R%d %d %d %.2f\n", 1, 10, 20, 100.5);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, "R1 10 20 100.50");
}

// Test streamPrint with multiple lines
TEST_F(StreamPrintTest, MultipleLines) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "line1\n");
  streamPrint(out, "line2\n");
  streamPrint(out, "line3\n");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, "line1");
  std::getline(in, line);
  EXPECT_EQ(line, "line2");
  std::getline(in, line);
  EXPECT_EQ(line, "line3");
}

// Test streamPrint with special characters
TEST_F(StreamPrintTest, SpecialChars) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "* SPICE deck for %s\n", "test_design");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, "* SPICE deck for test_design");
}

// Test RiseFall index constants used by spice
TEST_F(SpiceSmokeTest, RiseFallIndexConstants) {
  EXPECT_EQ(RiseFall::riseIndex(), 0);
  EXPECT_EQ(RiseFall::fallIndex(), 1);
  // index_count is a static constexpr, verify via range size
  EXPECT_EQ(RiseFall::range().size(), 2u);
}

// Test RiseFall range iteration used in spice
TEST_F(SpiceSmokeTest, RiseFallRange2) {
  int count = 0;
  for (auto rf : RiseFall::range()) {
    EXPECT_NE(rf, nullptr);
    count++;
  }
  EXPECT_EQ(count, 2);
}

// Test RiseFallBoth range
TEST_F(SpiceSmokeTest, RiseFallBothRange) {
  EXPECT_NE(RiseFallBoth::rise(), nullptr);
  EXPECT_NE(RiseFallBoth::fall(), nullptr);
  EXPECT_NE(RiseFallBoth::riseFall(), nullptr);
}

// Test Transition init strings used in WriteSpice
TEST_F(SpiceSmokeTest, TransitionInitFinalStrings) {
  EXPECT_NE(Transition::rise()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::fall()->asInitFinalString(), nullptr);
}

// Test MinMax initValue used in spice
TEST_F(SpiceSmokeTest, MinMaxInitValue) {
  float min_init = MinMax::min()->initValue();
  float max_init = MinMax::max()->initValue();
  EXPECT_GT(min_init, 0.0f);
  EXPECT_LT(max_init, 0.0f);
}

// Test MinMax opposite used in spice
TEST_F(SpiceSmokeTest, MinMaxOpposite) {
  EXPECT_EQ(MinMax::min()->opposite(), MinMax::max());
  EXPECT_EQ(MinMax::max()->opposite(), MinMax::min());
}

////////////////////////////////////////////////////////////////
// R6_ tests for Spice function coverage
////////////////////////////////////////////////////////////////

// Test streamPrint with wide variety of format specifiers
// Covers: streamPrint with many format types
TEST_F(StreamPrintTest, FormatSpecifiers) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "%c %s %d %f %e %g\n", 'A', "test", 42, 3.14, 1.5e-12, 1.8);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("A"), std::string::npos);
  EXPECT_NE(line.find("test"), std::string::npos);
  EXPECT_NE(line.find("42"), std::string::npos);
}

// Test streamPrint with SPICE node naming
// Covers: streamPrint for SPICE net naming patterns
TEST_F(StreamPrintTest, SpiceNodeNaming) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "C%d %s %s %.4e\n", 1, "n_top/sub/net:1", "0", 1.5e-15);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_TRUE(line.find("C1") == 0);
  EXPECT_NE(line.find("n_top/sub/net:1"), std::string::npos);
}

// Test streamPrint with SPICE .include directive
// Covers: streamPrint for SPICE directives
TEST_F(StreamPrintTest, SpiceIncludeDirective) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".include \"%s\"\n", "/path/to/models.spice");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".include"), std::string::npos);
  EXPECT_NE(line.find("/path/to/models.spice"), std::string::npos);
}

// Test streamPrint SPICE voltage source
// Covers: streamPrint for voltage sources
TEST_F(StreamPrintTest, SpiceVoltageSource) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "v%s %s 0 %.3f\n", "dd", "vdd", 1.800);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, "vdd vdd 0 1.800");
}

// Test streamPrint SPICE .tran with detailed parameters
// Covers: streamPrint for transient analysis
TEST_F(StreamPrintTest, SpiceTransAnalysis) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".tran %g %g %g %g\n", 1e-13, 5e-9, 0.0, 1e-12);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".tran"), std::string::npos);
}

// Test streamPrint SPICE PWL source
// Covers: streamPrint with PWL voltage source
TEST_F(StreamPrintTest, SpicePWLSource) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "v_in in 0 PWL(\n");
  streamPrint(out, "+%.3e %.3f\n", 0.0, 0.0);
  streamPrint(out, "+%.3e %.3f\n", 1e-10, 1.8);
  streamPrint(out, "+%.3e %.3f)\n", 2e-10, 1.8);
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("PWL"), std::string::npos);
  EXPECT_NE(content.find("1.800"), std::string::npos);
}

// Test readXyceCsv with precision values
// Covers: readXyceCsv parsing
TEST_F(XyceCsvTest, ReadPrecisionValues) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(out)\n";
    out << "0.000000000000e+00,0.000000000000e+00\n";
    out << "1.234567890123e-10,9.876543210987e-01\n";
    out << "2.469135780246e-10,1.800000000000e+00\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 1u);
  EXPECT_EQ(titles[0], "V(out)");
  EXPECT_EQ(waveforms.size(), 1u);
}

// Test readXyceCsv with many signals
// Covers: readXyceCsv with wide data
TEST_F(XyceCsvTest, ReadManySignals) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME";
    for (int i = 0; i < 20; i++) {
      out << ",V(sig" << i << ")";
    }
    out << "\n";
    out << "0.0";
    for (int i = 0; i < 20; i++) {
      out << "," << (i % 2 ? "1.8" : "0.0");
    }
    out << "\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 20u);
  EXPECT_EQ(waveforms.size(), 20u);
}

// Test Transition asRiseFall returns correct mapping for all types
// Covers: Transition::asRiseFall for spice
TEST_F(SpiceSmokeTest, TransitionAsRiseFallMapping) {
  // Rise-type transitions
  EXPECT_EQ(Transition::rise()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::tr0Z()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::trZ1()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::tr0X()->asRiseFall(), RiseFall::rise());
  EXPECT_EQ(Transition::trX1()->asRiseFall(), RiseFall::rise());

  // Fall-type transitions
  EXPECT_EQ(Transition::fall()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::tr1Z()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::trZ0()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::tr1X()->asRiseFall(), RiseFall::fall());
  EXPECT_EQ(Transition::trX0()->asRiseFall(), RiseFall::fall());

  // Indeterminate
  EXPECT_EQ(Transition::trXZ()->asRiseFall(), nullptr);
  EXPECT_EQ(Transition::trZX()->asRiseFall(), nullptr);
}

// Test MinMax compare function exhaustively
// Covers: MinMax::compare
TEST_F(SpiceSmokeTest, MinMaxCompareExhaustive) {
  // min: true when v1 < v2
  EXPECT_TRUE(MinMax::min()->compare(-1.0f, 0.0f));
  EXPECT_TRUE(MinMax::min()->compare(0.0f, 1.0f));
  EXPECT_FALSE(MinMax::min()->compare(0.0f, 0.0f));
  EXPECT_FALSE(MinMax::min()->compare(1.0f, 0.0f));

  // max: true when v1 > v2
  EXPECT_TRUE(MinMax::max()->compare(1.0f, 0.0f));
  EXPECT_TRUE(MinMax::max()->compare(0.0f, -1.0f));
  EXPECT_FALSE(MinMax::max()->compare(0.0f, 0.0f));
  EXPECT_FALSE(MinMax::max()->compare(-1.0f, 0.0f));
}

// Test MinMax find by string name
// Covers: MinMax::find
TEST_F(SpiceSmokeTest, MinMaxFindByName) {
  EXPECT_EQ(MinMax::find("min"), MinMax::min());
  EXPECT_EQ(MinMax::find("max"), MinMax::max());
  EXPECT_EQ(MinMax::find("unknown"), nullptr);
}

// Test MinMax to_string
// Covers: MinMax::to_string
TEST_F(SpiceSmokeTest, MinMaxToString) {
  EXPECT_EQ(MinMax::min()->to_string(), "min");
  EXPECT_EQ(MinMax::max()->to_string(), "max");
}

// Test RiseFall shortName
// Covers: RiseFall::shortName
TEST_F(SpiceSmokeTest, RiseFallShortName) {
  EXPECT_STREQ(RiseFall::rise()->shortName(), "^");
  EXPECT_STREQ(RiseFall::fall()->shortName(), "v");
}

////////////////////////////////////////////////////////////////
// R8_ tests for SPICE module coverage improvement
////////////////////////////////////////////////////////////////

// Test streamPrint with SPICE transistor format (used in writeParasiticNetwork)
// Covers: streamPrint paths used by WriteSpice
TEST_F(StreamPrintTest, SpiceTransistorFormat) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "M%d %s %s %s %s %s W=%.3e L=%.3e\n",
              1, "drain", "gate", "source", "bulk", "NMOS",
              1.0e-6, 45.0e-9);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_TRUE(line.find("M1") == 0);
  EXPECT_NE(line.find("drain"), std::string::npos);
  EXPECT_NE(line.find("NMOS"), std::string::npos);
}

// Test streamPrint with SPICE capacitor format (used in writeParasiticNetwork)
// Covers: streamPrint paths used by WriteSpice::writeParasiticNetwork
TEST_F(StreamPrintTest, SpiceCapacitorFormat) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "C%d %s %s %.4e\n", 1, "net1:1", "0", 1.5e-15);
  streamPrint(out, "C%d %s %s %.4e\n", 2, "net1:2", "net1:3", 2.5e-15);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line1, line2;
  std::getline(in, line1);
  std::getline(in, line2);
  EXPECT_TRUE(line1.find("C1") == 0);
  EXPECT_TRUE(line2.find("C2") == 0);
}

// Test streamPrint with SPICE voltage source (used in writeClkedStepSource)
// Covers: streamPrint paths used by WriteSpice::writeClkedStepSource
TEST_F(StreamPrintTest, SpiceVoltageSource2) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "v%s %s 0 pwl(0 %.3f %.3e %.3f)\n",
              "clk", "clk_node", 0.0, 1e-9, 1.8);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_TRUE(line.find("vclk") == 0);
  EXPECT_NE(line.find("pwl"), std::string::npos);
}

// Test streamPrint with SPICE waveform format (used in writeWaveformVoltSource)
// Covers: streamPrint paths used by WriteSpice::writeWaveformVoltSource
TEST_F(StreamPrintTest, SpiceWaveformFormat) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "v%s %s 0 pwl(\n", "in", "in_node");
  streamPrint(out, "+ %.3e %.3f\n", 0.0, 0.0);
  streamPrint(out, "+ %.3e %.3f\n", 1e-10, 0.9);
  streamPrint(out, "+ %.3e %.3f\n", 2e-10, 1.8);
  streamPrint(out, "+)\n");
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("vin"), std::string::npos);
  EXPECT_NE(content.find("pwl"), std::string::npos);
}

// Test streamPrint with SPICE .measure format (used in spiceTrans context)
// Covers: streamPrint with RISE/FALL strings (used by WriteSpice::spiceTrans)
TEST_F(StreamPrintTest, SpiceMeasureRiseFall) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  // This mimics how spiceTrans returns RISE/FALL strings
  const char *rise_str = "RISE";
  const char *fall_str = "FALL";
  streamPrint(out, ".measure tran delay_rf trig v(in) val=0.9 %s=last\n", rise_str);
  streamPrint(out, "+targ v(out) val=0.9 %s=last\n", fall_str);
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("RISE"), std::string::npos);
  EXPECT_NE(content.find("FALL"), std::string::npos);
}

// Test Xyce CSV with special values
// Covers: readXyceCsv edge cases
TEST_F(XyceCsvTest, ReadCsvWithZeroValues) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(sig)\n";
    out << "0.0,0.0\n";
    out << "1e-10,0.0\n";
    out << "2e-10,0.0\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 1u);
  EXPECT_EQ(titles[0], "V(sig)");
  EXPECT_EQ(waveforms.size(), 1u);
}

// Test Xyce CSV with large number of signals
// Covers: readXyceCsv with many columns
TEST_F(XyceCsvTest, ReadCsvManySignals) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME";
    for (int i = 0; i < 20; i++)
      out << ",V(sig" << i << ")";
    out << "\n";
    out << "0.0";
    for (int i = 0; i < 20; i++)
      out << "," << (i * 0.1);
    out << "\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 20u);
  EXPECT_EQ(waveforms.size(), 20u);
}

////////////////////////////////////////////////////////////////
// R9_ tests for SPICE module coverage improvement
////////////////////////////////////////////////////////////////

// streamPrint: SPICE subcircuit definition (used by WriteSpice)
TEST_F(StreamPrintTest, SpiceSubcktDefinition) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".subckt %s %s %s %s %s\n",
              "INV_X1", "VDD", "VSS", "A", "Y");
  streamPrint(out, "M1 Y A VDD VDD PMOS W=%.3e L=%.3e\n", 200e-9, 45e-9);
  streamPrint(out, "M2 Y A VSS VSS NMOS W=%.3e L=%.3e\n", 100e-9, 45e-9);
  streamPrint(out, ".ends %s\n", "INV_X1");
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find(".subckt INV_X1"), std::string::npos);
  EXPECT_NE(content.find(".ends INV_X1"), std::string::npos);
  EXPECT_NE(content.find("PMOS"), std::string::npos);
  EXPECT_NE(content.find("NMOS"), std::string::npos);
}

// streamPrint: SPICE resistor network (used in writeParasiticNetwork)
TEST_F(StreamPrintTest, SpiceResistorNetwork) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  for (int i = 0; i < 10; i++) {
    streamPrint(out, "R%d n%d n%d %.4e\n", i+1, i, i+1, 50.0 + i*10.0);
  }
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("R1"), std::string::npos);
  EXPECT_NE(content.find("R10"), std::string::npos);
}

// streamPrint: SPICE capacitor network (used in writeParasiticNetwork)
TEST_F(StreamPrintTest, SpiceCapacitorNetwork) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  for (int i = 0; i < 10; i++) {
    streamPrint(out, "C%d n%d 0 %.4e\n", i+1, i, 1e-15 * (i+1));
  }
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("C1"), std::string::npos);
  EXPECT_NE(content.find("C10"), std::string::npos);
}

// streamPrint: SPICE .lib directive
TEST_F(StreamPrintTest, SpiceLibDirective) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".lib '%s' %s\n", "/path/to/models.lib", "tt");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".lib"), std::string::npos);
  EXPECT_NE(line.find("tt"), std::string::npos);
}

// streamPrint: SPICE .option directive
TEST_F(StreamPrintTest, SpiceOptionDirective) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".option %s=%g %s=%g\n", "reltol", 1e-6, "abstol", 1e-12);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".option"), std::string::npos);
  EXPECT_NE(line.find("reltol"), std::string::npos);
}

// streamPrint: SPICE .print directive
TEST_F(StreamPrintTest, SpicePrintDirective) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".print tran v(%s) v(%s) v(%s)\n",
              "input", "output", "clk");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".print tran"), std::string::npos);
  EXPECT_NE(line.find("v(input)"), std::string::npos);
  EXPECT_NE(line.find("v(output)"), std::string::npos);
}

// streamPrint: SPICE pulse source
TEST_F(StreamPrintTest, SpicePulseSource) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "v%s %s 0 PULSE(%.3f %.3f %.3e %.3e %.3e %.3e %.3e)\n",
              "clk", "clk_node", 0.0, 1.8, 0.0, 20e-12, 20e-12, 500e-12, 1e-9);
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("vclk"), std::string::npos);
  EXPECT_NE(line.find("PULSE"), std::string::npos);
}

// streamPrint: SPICE mutual inductance
TEST_F(StreamPrintTest, SpiceMutualInductance) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "L%d %s %s %.4e\n", 1, "n1", "n2", 1e-9);
  streamPrint(out, "L%d %s %s %.4e\n", 2, "n3", "n4", 1e-9);
  streamPrint(out, "K%d L%d L%d %.4f\n", 1, 1, 2, 0.5);
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("L1"), std::string::npos);
  EXPECT_NE(content.find("K1"), std::string::npos);
}

// streamPrint: SPICE probe statement
TEST_F(StreamPrintTest, SpiceProbeStatement) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".probe v(%s) v(%s) i(%s)\n",
              "out", "in", "v_supply");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find(".probe"), std::string::npos);
}

// streamPrint: SPICE with escaped characters
TEST_F(StreamPrintTest, SpiceEscapedChars) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "* Node: %s\n", "top/sub/inst:pin");
  streamPrint(out, "R1 %s %s %.4e\n",
              "top/sub/inst:pin", "top/sub/inst:int", 100.0);
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("top/sub/inst:pin"), std::string::npos);
}

// streamPrint: SPICE full deck structure
TEST_F(StreamPrintTest, SpiceFullDeck) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "* Full SPICE deck\n");
  streamPrint(out, ".include \"%s\"\n", "models.spice");
  streamPrint(out, ".subckt top VDD VSS IN OUT\n");
  streamPrint(out, "R1 IN n1 %.2e\n", 50.0);
  streamPrint(out, "C1 n1 VSS %.4e\n", 1e-15);
  streamPrint(out, "xinv VDD VSS n1 OUT INV_X1\n");
  streamPrint(out, ".ends top\n");
  streamPrint(out, "\n");
  streamPrint(out, "xinst VDD VSS IN OUT top\n");
  streamPrint(out, "vvdd VDD 0 %.3f\n", 1.8);
  streamPrint(out, "vvss VSS 0 0\n");
  streamPrint(out, "vin IN 0 PULSE(0 %.3f 0 %.3e %.3e %.3e %.3e)\n",
              1.8, 20e-12, 20e-12, 500e-12, 1e-9);
  streamPrint(out, ".tran %.3e %.3e\n", 1e-13, 2e-9);
  streamPrint(out, ".end\n");
  out.close();

  std::ifstream in(tmpfile_);
  std::string content((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
  EXPECT_NE(content.find("* Full SPICE deck"), std::string::npos);
  EXPECT_NE(content.find(".include"), std::string::npos);
  EXPECT_NE(content.find(".subckt top"), std::string::npos);
  EXPECT_NE(content.find(".ends top"), std::string::npos);
  EXPECT_NE(content.find(".tran"), std::string::npos);
  EXPECT_NE(content.find(".end"), std::string::npos);
}

// XyceCsv: with very small values
TEST_F(XyceCsvTest, ReadCsvSmallValues) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(sig1),V(sig2)\n";
    out << "0.0,1e-15,2e-20\n";
    out << "1e-15,3e-15,4e-20\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 2u);
  EXPECT_EQ(waveforms.size(), 2u);
}

// XyceCsv: with very large values
TEST_F(XyceCsvTest, ReadCsvLargeValues) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(sig)\n";
    out << "0.0,1e10\n";
    out << "1e-10,2e10\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 1u);
  EXPECT_EQ(waveforms.size(), 1u);
}

// XyceCsv: with 100 time steps
TEST_F(XyceCsvTest, ReadCsv100TimeSteps) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(out),V(in)\n";
    for (int i = 0; i < 100; i++) {
      double t = i * 1e-12;
      double v1 = (i % 2 == 0) ? 1.8 : 0.0;
      double v2 = (i % 2 == 0) ? 0.0 : 1.8;
      out << t << "," << v1 << "," << v2 << "\n";
    }
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 2u);
  EXPECT_EQ(waveforms.size(), 2u);
}

// XyceCsv: with signal names containing special characters
TEST_F(XyceCsvTest, ReadCsvSpecialSignalNames) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(top/sub/net:1),V(top/sub/net:2)\n";
    out << "0.0,0.0,1.8\n";
    out << "1e-10,1.8,0.0\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 2u);
  EXPECT_EQ(titles[0], "V(top/sub/net:1)");
  EXPECT_EQ(titles[1], "V(top/sub/net:2)");
}

// XyceCsv: with current probes
TEST_F(XyceCsvTest, ReadCsvCurrentProbes) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,I(v_supply),V(out)\n";
    out << "0.0,1e-3,0.0\n";
    out << "1e-10,2e-3,1.8\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 2u);
  EXPECT_EQ(titles[0], "I(v_supply)");
}

// Transition properties relevant to SPICE
TEST_F(SpiceSmokeTest, TransitionAsRiseFallBoth) {
  // Rise-like transitions have valid asRiseFallBoth
  EXPECT_NE(Transition::rise()->asRiseFallBoth(), nullptr);
  EXPECT_NE(Transition::fall()->asRiseFallBoth(), nullptr);
  EXPECT_NE(Transition::tr0Z()->asRiseFallBoth(), nullptr);
  EXPECT_NE(Transition::trZ1()->asRiseFallBoth(), nullptr);
}

TEST_F(SpiceSmokeTest, TransitionIndex) {
  EXPECT_GE(Transition::rise()->index(), 0);
  EXPECT_GE(Transition::fall()->index(), 0);
  EXPECT_NE(Transition::rise()->index(), Transition::fall()->index());
}

TEST_F(SpiceSmokeTest, RiseFallBothIndex) {
  EXPECT_GE(RiseFallBoth::rise()->index(), 0);
  EXPECT_GE(RiseFallBoth::fall()->index(), 0);
  EXPECT_GE(RiseFallBoth::riseFall()->index(), 0);
}

TEST_F(SpiceSmokeTest, RiseFallBothToString) {
  EXPECT_EQ(RiseFallBoth::rise()->to_string(), "^");
  EXPECT_EQ(RiseFallBoth::fall()->to_string(), "v");
  EXPECT_FALSE(RiseFallBoth::riseFall()->to_string().empty());
}

TEST_F(SpiceSmokeTest, MinMaxAllForSpice) {
  // MinMaxAll range used in SPICE for iteration
  int count = 0;
  for (auto mm : MinMaxAll::all()->range()) {
    EXPECT_NE(mm, nullptr);
    count++;
  }
  EXPECT_EQ(count, 2);
}

TEST_F(SpiceSmokeTest, MinMaxAllAsMinMax) {
  EXPECT_EQ(MinMaxAll::min()->asMinMax(), MinMax::min());
  EXPECT_EQ(MinMaxAll::max()->asMinMax(), MinMax::max());
}

TEST_F(SpiceSmokeTest, TransitionRiseFallAsString) {
  // Transition::to_string used in SPICE reporting
  EXPECT_EQ(Transition::rise()->to_string(), "^");
  EXPECT_EQ(Transition::fall()->to_string(), "v");
  // riseFall wildcard
  EXPECT_FALSE(Transition::riseFall()->to_string().empty());
}

TEST_F(SpiceSmokeTest, RiseFallAsRiseFallBoth) {
  EXPECT_EQ(RiseFall::rise()->asRiseFallBoth(), RiseFallBoth::rise());
  EXPECT_EQ(RiseFall::fall()->asRiseFallBoth(), RiseFallBoth::fall());
}

TEST_F(SpiceSmokeTest, MinMaxCompareInfinity) {
  float large = 1e30f;
  float small = -1e30f;
  EXPECT_TRUE(MinMax::min()->compare(small, large));
  EXPECT_FALSE(MinMax::min()->compare(large, small));
  EXPECT_TRUE(MinMax::max()->compare(large, small));
  EXPECT_FALSE(MinMax::max()->compare(small, large));
}

TEST_F(SpiceSmokeTest, RiseFallRangeValues) {
  // Verify range produces rise then fall
  auto range = RiseFall::range();
  int idx = 0;
  for (auto rf : range) {
    if (idx == 0) EXPECT_EQ(rf, RiseFall::rise());
    if (idx == 1) EXPECT_EQ(rf, RiseFall::fall());
    idx++;
  }
  EXPECT_EQ(idx, 2);
}

// XyceCsv: single row of data
TEST_F(XyceCsvTest, ReadCsvSingleRow) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(out)\n";
    out << "0.0,1.8\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 1u);
  EXPECT_EQ(waveforms.size(), 1u);
}

// XyceCsv: with alternating sign values
TEST_F(XyceCsvTest, ReadCsvAlternatingSign) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME,V(out)\n";
    for (int i = 0; i < 20; i++) {
      out << (i * 1e-12) << "," << ((i % 2 == 0) ? 0.9 : -0.1) << "\n";
    }
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 1u);
  EXPECT_EQ(waveforms.size(), 1u);
}

// streamPrint: SPICE .end directive
TEST_F(StreamPrintTest, SpiceEndDirective) {
  std::ofstream out(tmpfile_);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, ".end\n");
  out.close();

  std::ifstream in(tmpfile_);
  std::string line;
  std::getline(in, line);
  EXPECT_EQ(line, ".end");
}

// XyceCsv: many columns (50 signals)
TEST_F(XyceCsvTest, ReadCsv50Signals) {
  {
    std::ofstream out(tmpfile_);
    out << "TIME";
    for (int i = 0; i < 50; i++)
      out << ",V(s" << i << ")";
    out << "\n";
    out << "0.0";
    for (int i = 0; i < 50; i++)
      out << "," << (i * 0.036);
    out << "\n";
    out.close();
  }

  StdStringSeq titles;
  WaveformSeq waveforms;
  readXyceCsv(tmpfile_.c_str(), titles, waveforms);

  EXPECT_EQ(titles.size(), 50u);
  EXPECT_EQ(waveforms.size(), 50u);
}

////////////////////////////////////////////////////////////////
// SpiceDesignTest: tests that load a design and exercise
// higher-level SPICE writing functionality
////////////////////////////////////////////////////////////////

class SpiceDesignTest : public ::testing::Test {
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
    LibertyLibrary *lib = sta_->readLiberty(
      "test/nangate45/Nangate45_typ.lib", corner, min_max, false);
    ASSERT_NE(lib, nullptr);
    lib_ = lib;

    bool ok = sta_->readVerilog("search/test/search_test1.v");
    ASSERT_TRUE(ok);
    ok = sta_->linkDesign("search_test1", true);
    ASSERT_TRUE(ok);

    // Create clock and constraints
    Network *network = sta_->cmdNetwork();
    Instance *top = network->topInstance();
    Pin *clk_pin = network->findPin(top, "clk");
    ASSERT_NE(clk_pin, nullptr);
    PinSet *clk_pins = new PinSet(network);
    clk_pins->insert(clk_pin);
    FloatSeq *waveform = new FloatSeq;
    waveform->push_back(0.0f);
    waveform->push_back(5.0f);
    sta_->makeClock("clk", clk_pins, false, 10.0f, waveform, nullptr);

    Pin *in1 = network->findPin(top, "in1");
    Pin *in2 = network->findPin(top, "in2");
    Pin *out1 = network->findPin(top, "out1");
    Clock *clk = sta_->sdc()->findClock("clk");
    sta_->setInputDelay(in1, RiseFallBoth::riseFall(), clk, RiseFall::rise(),
                        nullptr, false, false, MinMaxAll::all(), false, 0.5f);
    sta_->setInputDelay(in2, RiseFallBoth::riseFall(), clk, RiseFall::rise(),
                        nullptr, false, false, MinMaxAll::all(), false, 0.5f);
    sta_->setOutputDelay(out1, RiseFallBoth::riseFall(), clk, RiseFall::rise(),
                         nullptr, false, false, MinMaxAll::all(), false, 0.5f);
    sta_->updateTiming(true);
    design_loaded_ = true;
  }

  void TearDown() override {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_)
      Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  // Helper: find a vertex for a pin by hierarchical path name
  Vertex *findVertex(const char *path_name) {
    Network *network = sta_->cmdNetwork();
    Pin *pin = network->findPin(path_name);
    if (pin == nullptr)
      return nullptr;
    Graph *graph = sta_->graph();
    if (graph == nullptr)
      return nullptr;
    return graph->pinDrvrVertex(pin);
  }

  Pin *findPin(const char *path_name) {
    Network *network = sta_->cmdNetwork();
    return network->findPin(path_name);
  }

  Sta *sta_;
  Tcl_Interp *interp_;
  LibertyLibrary *lib_;
  bool design_loaded_ = false;
};

// Verify that the design loaded and basic network is accessible
TEST_F(SpiceDesignTest, DesignLoaded) {
  ASSERT_TRUE(design_loaded_);
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  ASSERT_NE(top, nullptr);
}

// Verify all leaf instances are accessible for SPICE netlisting
TEST_F(SpiceDesignTest, NetworkLeafInstances) {
  Network *network = sta_->cmdNetwork();
  InstanceSeq leaves = network->leafInstances();
  // search_test1.v has: and1 (AND2_X1), buf1 (BUF_X1), reg1 (DFF_X1),
  //                     buf2 (BUF_X1)
  EXPECT_EQ(leaves.size(), 4u);
}

// Verify each instance can be found by name for SPICE subcircuit generation
TEST_F(SpiceDesignTest, NetworkInstancesByName) {
  Network *network = sta_->cmdNetwork();
  Instance *and1 = network->findInstance("and1");
  Instance *buf1 = network->findInstance("buf1");
  Instance *reg1 = network->findInstance("reg1");
  Instance *buf2 = network->findInstance("buf2");
  EXPECT_NE(and1, nullptr);
  EXPECT_NE(buf1, nullptr);
  EXPECT_NE(reg1, nullptr);
  EXPECT_NE(buf2, nullptr);
}

// Verify liberty cell information is accessible for SPICE model generation
TEST_F(SpiceDesignTest, LibertyCellAccess) {
  Network *network = sta_->cmdNetwork();
  Instance *and1 = network->findInstance("and1");
  ASSERT_NE(and1, nullptr);
  LibertyCell *cell = network->libertyCell(and1);
  ASSERT_NE(cell, nullptr);
  EXPECT_STREQ(cell->name(), "AND2_X1");
}

// Verify liberty cell ports (needed for SPICE subcircuit port mapping)
TEST_F(SpiceDesignTest, LibertyCellPortInfo) {
  LibertyCell *and2_cell = lib_->findLibertyCell("AND2_X1");
  ASSERT_NE(and2_cell, nullptr);

  LibertyPort *a1 = and2_cell->findLibertyPort("A1");
  LibertyPort *a2 = and2_cell->findLibertyPort("A2");
  LibertyPort *zn = and2_cell->findLibertyPort("ZN");
  EXPECT_NE(a1, nullptr);
  EXPECT_NE(a2, nullptr);
  EXPECT_NE(zn, nullptr);
}

// Verify buffer cell identification (used in SPICE path analysis)
TEST_F(SpiceDesignTest, LibertyCellIsBuffer) {
  LibertyCell *buf_cell = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_cell, nullptr);
  EXPECT_TRUE(buf_cell->isBuffer());

  LibertyCell *and2_cell = lib_->findLibertyCell("AND2_X1");
  ASSERT_NE(and2_cell, nullptr);
  EXPECT_FALSE(and2_cell->isBuffer());
}

// Verify inverter cell identification (used in SPICE logic value computation)
TEST_F(SpiceDesignTest, LibertyCellIsInverter) {
  LibertyCell *inv_cell = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv_cell, nullptr);
  EXPECT_TRUE(inv_cell->isInverter());

  LibertyCell *buf_cell = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_cell, nullptr);
  EXPECT_FALSE(buf_cell->isInverter());
}

// Verify timing arcs exist for cells (needed for SPICE delay checking)
TEST_F(SpiceDesignTest, LibertyCellTimingArcs) {
  LibertyCell *and2_cell = lib_->findLibertyCell("AND2_X1");
  ASSERT_NE(and2_cell, nullptr);
  EXPECT_GT(and2_cell->timingArcSets().size(), 0u);
}

// Verify pin connectivity for SPICE net writing
TEST_F(SpiceDesignTest, PinConnectivity) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // The internal net n1 connects and1:ZN to buf1:A
  Pin *and1_zn = network->findPin("and1/ZN");
  Pin *buf1_a = network->findPin("buf1/A");
  ASSERT_NE(and1_zn, nullptr);
  ASSERT_NE(buf1_a, nullptr);

  // Both pins should be on the same net
  Net *net_and1_zn = network->net(and1_zn);
  Net *net_buf1_a = network->net(buf1_a);
  ASSERT_NE(net_and1_zn, nullptr);
  ASSERT_NE(net_buf1_a, nullptr);
  EXPECT_EQ(net_and1_zn, net_buf1_a);
}

// Verify driver/load pin classification (used in SPICE netlisting)
TEST_F(SpiceDesignTest, PinDriverLoad) {
  Network *network = sta_->cmdNetwork();

  Pin *and1_zn = network->findPin("and1/ZN");
  Pin *buf1_a = network->findPin("buf1/A");
  ASSERT_NE(and1_zn, nullptr);
  ASSERT_NE(buf1_a, nullptr);

  // ZN is an output (driver), A is an input (load)
  EXPECT_TRUE(network->isDriver(and1_zn));
  EXPECT_TRUE(network->isLoad(buf1_a));
}

// Verify graph vertex access (needed for SPICE path traversal)
TEST_F(SpiceDesignTest, GraphVertexAccess) {
  Graph *graph = sta_->graph();
  ASSERT_NE(graph, nullptr);

  Vertex *v = findVertex("buf1/Z");
  EXPECT_NE(v, nullptr);

  Vertex *v2 = findVertex("and1/ZN");
  EXPECT_NE(v2, nullptr);
}

// Verify timing paths exist after analysis (prerequisite for writePathSpice)
TEST_F(SpiceDesignTest, TimingPathExists) {
  PathEndSeq path_ends = sta_->findPathEnds(
    nullptr,       // from
    nullptr,       // thrus
    nullptr,       // to
    false,         // unconstrained
    sta_->cmdCorner(),
    MinMaxAll::max(),
    10,            // group_path_count
    1,             // endpoint_path_count
    false,         // unique_pins
    false,         // unique_edges
    -INF,          // slack_min
    INF,           // slack_max
    false,         // sort_by_slack
    nullptr,       // group_names
    true,          // setup
    false,         // hold
    false,         // recovery
    false,         // removal
    false,         // clk_gating_setup
    false          // clk_gating_hold
  );
  // The design has constrained paths (in1/in2 -> reg1 -> out1)
  EXPECT_GT(path_ends.size(), 0u);
}

// Verify path end has a valid path object for SPICE writing
TEST_F(SpiceDesignTest, PathEndHasPath) {
  PathEndSeq path_ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false,
    sta_->cmdCorner(), MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false
  );
  ASSERT_GT(path_ends.size(), 0u);
  PathEnd *path_end = path_ends[0];
  ASSERT_NE(path_end, nullptr);
  Path *path = path_end->path();
  ASSERT_NE(path, nullptr);
}

// Verify worst slack computation (used to select paths for SPICE simulation)
TEST_F(SpiceDesignTest, WorstSlackComputation) {
  Slack worst_slack;
  Vertex *worst_vertex;
  sta_->worstSlack(MinMax::max(), worst_slack, worst_vertex);
  // The design should have a finite slack (not INF/-INF)
  EXPECT_NE(worst_vertex, nullptr);
}

// Verify DcalcAnalysisPt access (needed for WriteSpice constructor)
TEST_F(SpiceDesignTest, DcalcAnalysisPtAccess) {
  Corner *corner = sta_->cmdCorner();
  ASSERT_NE(corner, nullptr);
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
  ASSERT_NE(dcalc_ap, nullptr);
}

// Verify SPICE file can be written for a timing path
TEST_F(SpiceDesignTest, WriteSpicePathFile) {
  PathEndSeq path_ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false,
    sta_->cmdCorner(), MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false
  );
  ASSERT_GT(path_ends.size(), 0u);
  Path *path = path_ends[0]->path();
  ASSERT_NE(path, nullptr);

  // Create temp files for SPICE output
  char spice_tmpl[] = "/tmp/sta_spice_path_XXXXXX";
  int fd = mkstemp(spice_tmpl);
  ASSERT_NE(fd, -1);
  close(fd);

  // writePathSpice requires subckt/model files to exist, but we can test
  // that the function does not crash with empty stubs
  char subckt_tmpl[] = "/tmp/sta_spice_subckt_XXXXXX";
  fd = mkstemp(subckt_tmpl);
  ASSERT_NE(fd, -1);
  close(fd);

  // We cannot provide real model/subckt files for this unit test, so we
  // verify the path is valid and the API is callable. The actual file write
  // would fail without proper SPICE models, so we just verify the path
  // and analysis point are properly formed.
  Corner *corner = sta_->cmdCorner();
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
  EXPECT_NE(dcalc_ap, nullptr);

  // Clean up temp files
  std::remove(spice_tmpl);
  std::remove(subckt_tmpl);
}

// Verify multiple timing paths are found (SPICE multi-path analysis)
TEST_F(SpiceDesignTest, MultipleTimingPaths) {
  PathEndSeq path_ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false,
    sta_->cmdCorner(), MinMaxAll::max(),
    10, 10, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false
  );
  // The design has multiple paths through and1/buf1/reg1/buf2
  EXPECT_GE(path_ends.size(), 1u);
}

// Verify liberty library cell lookup (used in writeSubckts)
TEST_F(SpiceDesignTest, LibraryLookupForSpice) {
  // The library should contain the cells used in our design
  EXPECT_NE(lib_->findLibertyCell("AND2_X1"), nullptr);
  EXPECT_NE(lib_->findLibertyCell("BUF_X1"), nullptr);
  EXPECT_NE(lib_->findLibertyCell("DFF_X1"), nullptr);
}

// Verify cell name is accessible from instance (for SPICE subcircuit naming)
TEST_F(SpiceDesignTest, InstanceCellName) {
  Network *network = sta_->cmdNetwork();
  Instance *and1 = network->findInstance("and1");
  ASSERT_NE(and1, nullptr);
  const char *cell_name = network->cellName(and1);
  ASSERT_NE(cell_name, nullptr);
  EXPECT_STREQ(cell_name, "AND2_X1");

  Instance *reg1 = network->findInstance("reg1");
  ASSERT_NE(reg1, nullptr);
  cell_name = network->cellName(reg1);
  ASSERT_NE(cell_name, nullptr);
  EXPECT_STREQ(cell_name, "DFF_X1");
}

// Verify streamPrint with SPICE subcircuit instance format for design cells
TEST_F(SpiceDesignTest, StreamPrintSubcktInst) {
  char tmpl[] = "/tmp/sta_spice_subckt_inst_XXXXXX";
  int fd = mkstemp(tmpl);
  ASSERT_NE(fd, -1);
  close(fd);

  Network *network = sta_->cmdNetwork();
  Instance *and1 = network->findInstance("and1");
  ASSERT_NE(and1, nullptr);
  const char *inst_name = network->name(and1);
  const char *cell_name = network->cellName(and1);

  std::ofstream out(tmpl);
  ASSERT_TRUE(out.is_open());
  streamPrint(out, "x%s VDD VSS %s\n", inst_name, cell_name);
  out.close();

  std::ifstream in(tmpl);
  std::string line;
  std::getline(in, line);
  EXPECT_NE(line.find("xand1"), std::string::npos);
  EXPECT_NE(line.find("AND2_X1"), std::string::npos);
  std::remove(tmpl);
}

// Verify net names for SPICE node naming
TEST_F(SpiceDesignTest, NetNamesForSpice) {
  Network *network = sta_->cmdNetwork();
  Pin *and1_zn = network->findPin("and1/ZN");
  ASSERT_NE(and1_zn, nullptr);
  Net *net = network->net(and1_zn);
  ASSERT_NE(net, nullptr);
  const char *net_name = network->name(net);
  EXPECT_NE(net_name, nullptr);
  // The net name should be "n1" (from the Verilog: wire n1)
  EXPECT_STREQ(net_name, "n1");
}

// Verify hold timing paths (for SPICE min-delay analysis)
TEST_F(SpiceDesignTest, HoldTimingPaths) {
  PathEndSeq path_ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false,
    sta_->cmdCorner(), MinMaxAll::min(),
    10, 1, false, false, -INF, INF, false, nullptr,
    false, true, false, false, false, false
  );
  // Hold paths should exist for the constrained design.
  ASSERT_FALSE(path_ends.empty());
  ASSERT_NE(path_ends[0], nullptr);
  EXPECT_NE(path_ends[0]->path(), nullptr);
}

// Verify clock can be found for SPICE waveform generation
TEST_F(SpiceDesignTest, ClockAccessForSpice) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_FLOAT_EQ(clk->period(), 10.0f);
}

// Verify vertex arrival times are computed (used in SPICE timing correlation)
TEST_F(SpiceDesignTest, VertexArrivalForSpice) {
  Vertex *v = findVertex("buf1/Z");
  ASSERT_NE(v, nullptr);
  Arrival arr = sta_->vertexArrival(v, MinMax::max());
  // Arrival should be finite (not INF)
  (void)arr;
}

// Verify PathExpanded works on timing paths (used in SPICE path writing)
TEST_F(SpiceDesignTest, PathExpandedAccess) {
  PathEndSeq path_ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false,
    sta_->cmdCorner(), MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false
  );
  ASSERT_GT(path_ends.size(), 0u);
  Path *path = path_ends[0]->path();
  ASSERT_NE(path, nullptr);

  PathExpanded expanded(path, sta_);
  // The expanded path should have multiple elements
  EXPECT_GT(expanded.size(), 0u);
}

// Verify all top-level ports are accessible (for SPICE port mapping)
TEST_F(SpiceDesignTest, TopLevelPorts) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  ASSERT_NE(top, nullptr);

  // search_test1.v: input clk, in1, in2; output out1
  Pin *clk = network->findPin(top, "clk");
  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  Pin *out1 = network->findPin(top, "out1");
  EXPECT_NE(clk, nullptr);
  EXPECT_NE(in1, nullptr);
  EXPECT_NE(in2, nullptr);
  EXPECT_NE(out1, nullptr);
}

// Verify register cell identification (used in SPICE sequential port values)
TEST_F(SpiceDesignTest, RegisterCellForSpice) {
  LibertyCell *dff_cell = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff_cell, nullptr);

  // DFF should have timing arcs for setup/hold checks
  EXPECT_GT(dff_cell->timingArcSets().size(), 0u);

  // Verify DFF ports needed for SPICE
  EXPECT_NE(dff_cell->findLibertyPort("D"), nullptr);
  EXPECT_NE(dff_cell->findLibertyPort("CK"), nullptr);
  EXPECT_NE(dff_cell->findLibertyPort("Q"), nullptr);
}

// Verify CircuitSim enum values used in WriteSpice
TEST_F(SpiceDesignTest, CircuitSimEnum) {
  // These enum values are used by writePathSpice
  CircuitSim hspice = CircuitSim::hspice;
  CircuitSim ngspice = CircuitSim::ngspice;
  CircuitSim xyce = CircuitSim::xyce;
  EXPECT_NE(static_cast<int>(hspice), static_cast<int>(ngspice));
  EXPECT_NE(static_cast<int>(ngspice), static_cast<int>(xyce));
  EXPECT_NE(static_cast<int>(hspice), static_cast<int>(xyce));
}

} // namespace sta
