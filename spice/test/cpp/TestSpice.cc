#include <gtest/gtest.h>
#include <fstream>
#include <cstdio>

#include "MinMax.hh"
#include "Transition.hh"
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
    tmpfile_ = std::tmpnam(nullptr);
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
    tmpfile_ = std::tmpnam(nullptr);
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
TEST_F(SpiceSmokeTest, R5_RiseFallFind) {
  EXPECT_EQ(RiseFall::find("rise"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("fall"), RiseFall::fall());
  EXPECT_EQ(RiseFall::find("^"), RiseFall::rise());
  EXPECT_EQ(RiseFall::find("v"), RiseFall::fall());
  EXPECT_EQ(RiseFall::find("nonexistent"), nullptr);
}

// Test Transition find used in spice
TEST_F(SpiceSmokeTest, R5_TransitionFind) {
  EXPECT_EQ(Transition::find("^"), Transition::rise());
  EXPECT_EQ(Transition::find("v"), Transition::fall());
}

// Test streamPrint with empty format
TEST_F(StreamPrintTest, R5_EmptyFormat) {
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
TEST_F(StreamPrintTest, R5_IntegerFormatting) {
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
TEST_F(StreamPrintTest, R5_MultipleLines) {
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
TEST_F(StreamPrintTest, R5_SpecialChars) {
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
TEST_F(SpiceSmokeTest, R5_RiseFallIndexConstants) {
  EXPECT_EQ(RiseFall::riseIndex(), 0);
  EXPECT_EQ(RiseFall::fallIndex(), 1);
  // index_count is a static constexpr, verify via range size
  EXPECT_EQ(RiseFall::range().size(), 2u);
}

// Test RiseFall range iteration used in spice
TEST_F(SpiceSmokeTest, R5_RiseFallRange) {
  int count = 0;
  for (auto rf : RiseFall::range()) {
    EXPECT_NE(rf, nullptr);
    count++;
  }
  EXPECT_EQ(count, 2);
}

// Test RiseFallBoth range
TEST_F(SpiceSmokeTest, R5_RiseFallBothRange) {
  EXPECT_NE(RiseFallBoth::rise(), nullptr);
  EXPECT_NE(RiseFallBoth::fall(), nullptr);
  EXPECT_NE(RiseFallBoth::riseFall(), nullptr);
}

// Test Transition init strings used in WriteSpice
TEST_F(SpiceSmokeTest, R5_TransitionInitFinalStrings) {
  EXPECT_NE(Transition::rise()->asInitFinalString(), nullptr);
  EXPECT_NE(Transition::fall()->asInitFinalString(), nullptr);
}

// Test MinMax initValue used in spice
TEST_F(SpiceSmokeTest, R5_MinMaxInitValue) {
  float min_init = MinMax::min()->initValue();
  float max_init = MinMax::max()->initValue();
  EXPECT_GT(min_init, 0.0f);
  EXPECT_LT(max_init, 0.0f);
}

// Test MinMax opposite used in spice
TEST_F(SpiceSmokeTest, R5_MinMaxOpposite) {
  EXPECT_EQ(MinMax::min()->opposite(), MinMax::max());
  EXPECT_EQ(MinMax::max()->opposite(), MinMax::min());
}

////////////////////////////////////////////////////////////////
// R6_ tests for Spice function coverage
////////////////////////////////////////////////////////////////

// Test streamPrint with wide variety of format specifiers
// Covers: streamPrint with many format types
TEST_F(StreamPrintTest, R6_FormatSpecifiers) {
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
TEST_F(StreamPrintTest, R6_SpiceNodeNaming) {
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
TEST_F(StreamPrintTest, R6_SpiceIncludeDirective) {
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
TEST_F(StreamPrintTest, R6_SpiceVoltageSource) {
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
TEST_F(StreamPrintTest, R6_SpiceTransAnalysis) {
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
TEST_F(StreamPrintTest, R6_SpicePWLSource) {
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
TEST_F(XyceCsvTest, R6_ReadPrecisionValues) {
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
TEST_F(XyceCsvTest, R6_ReadManySignals) {
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
TEST_F(SpiceSmokeTest, R6_TransitionAsRiseFallMapping) {
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
TEST_F(SpiceSmokeTest, R6_MinMaxCompareExhaustive) {
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
TEST_F(SpiceSmokeTest, R6_MinMaxFindByName) {
  EXPECT_EQ(MinMax::find("min"), MinMax::min());
  EXPECT_EQ(MinMax::find("max"), MinMax::max());
  EXPECT_EQ(MinMax::find("unknown"), nullptr);
}

// Test MinMax to_string
// Covers: MinMax::to_string
TEST_F(SpiceSmokeTest, R6_MinMaxToString) {
  EXPECT_EQ(MinMax::min()->to_string(), "min");
  EXPECT_EQ(MinMax::max()->to_string(), "max");
}

// Test RiseFall shortName
// Covers: RiseFall::shortName
TEST_F(SpiceSmokeTest, R6_RiseFallShortName) {
  EXPECT_STREQ(RiseFall::rise()->shortName(), "^");
  EXPECT_STREQ(RiseFall::fall()->shortName(), "v");
}

////////////////////////////////////////////////////////////////
// R8_ tests for SPICE module coverage improvement
////////////////////////////////////////////////////////////////

// Test streamPrint with SPICE transistor format (used in writeParasiticNetwork)
// Covers: streamPrint paths used by WriteSpice
TEST_F(StreamPrintTest, R8_SpiceTransistorFormat) {
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
TEST_F(StreamPrintTest, R8_SpiceCapacitorFormat) {
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
TEST_F(StreamPrintTest, R8_SpiceVoltageSource) {
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
TEST_F(StreamPrintTest, R8_SpiceWaveformFormat) {
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
TEST_F(StreamPrintTest, R8_SpiceMeasureRiseFall) {
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
TEST_F(XyceCsvTest, R8_ReadCsvWithZeroValues) {
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
TEST_F(XyceCsvTest, R8_ReadCsvManySignals) {
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
TEST_F(StreamPrintTest, R9_SpiceSubcktDefinition) {
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
TEST_F(StreamPrintTest, R9_SpiceResistorNetwork) {
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
TEST_F(StreamPrintTest, R9_SpiceCapacitorNetwork) {
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
TEST_F(StreamPrintTest, R9_SpiceLibDirective) {
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
TEST_F(StreamPrintTest, R9_SpiceOptionDirective) {
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
TEST_F(StreamPrintTest, R9_SpicePrintDirective) {
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
TEST_F(StreamPrintTest, R9_SpicePulseSource) {
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
TEST_F(StreamPrintTest, R9_SpiceMutualInductance) {
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
TEST_F(StreamPrintTest, R9_SpiceProbeStatement) {
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
TEST_F(StreamPrintTest, R9_SpiceEscapedChars) {
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
TEST_F(StreamPrintTest, R9_SpiceFullDeck) {
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
TEST_F(XyceCsvTest, R9_ReadCsvSmallValues) {
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
TEST_F(XyceCsvTest, R9_ReadCsvLargeValues) {
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
TEST_F(XyceCsvTest, R9_ReadCsv100TimeSteps) {
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
TEST_F(XyceCsvTest, R9_ReadCsvSpecialSignalNames) {
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
TEST_F(XyceCsvTest, R9_ReadCsvCurrentProbes) {
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
TEST_F(SpiceSmokeTest, R9_TransitionAsRiseFallBoth) {
  // Rise-like transitions have valid asRiseFallBoth
  EXPECT_NE(Transition::rise()->asRiseFallBoth(), nullptr);
  EXPECT_NE(Transition::fall()->asRiseFallBoth(), nullptr);
  EXPECT_NE(Transition::tr0Z()->asRiseFallBoth(), nullptr);
  EXPECT_NE(Transition::trZ1()->asRiseFallBoth(), nullptr);
}

TEST_F(SpiceSmokeTest, R9_TransitionIndex) {
  EXPECT_GE(Transition::rise()->index(), 0);
  EXPECT_GE(Transition::fall()->index(), 0);
  EXPECT_NE(Transition::rise()->index(), Transition::fall()->index());
}

TEST_F(SpiceSmokeTest, R9_RiseFallBothIndex) {
  EXPECT_GE(RiseFallBoth::rise()->index(), 0);
  EXPECT_GE(RiseFallBoth::fall()->index(), 0);
  EXPECT_GE(RiseFallBoth::riseFall()->index(), 0);
}

TEST_F(SpiceSmokeTest, R9_RiseFallBothToString) {
  EXPECT_EQ(RiseFallBoth::rise()->to_string(), "^");
  EXPECT_EQ(RiseFallBoth::fall()->to_string(), "v");
  EXPECT_FALSE(RiseFallBoth::riseFall()->to_string().empty());
}

TEST_F(SpiceSmokeTest, R9_MinMaxAllForSpice) {
  // MinMaxAll range used in SPICE for iteration
  int count = 0;
  for (auto mm : MinMaxAll::all()->range()) {
    EXPECT_NE(mm, nullptr);
    count++;
  }
  EXPECT_EQ(count, 2);
}

TEST_F(SpiceSmokeTest, R9_MinMaxAllAsMinMax) {
  EXPECT_EQ(MinMaxAll::min()->asMinMax(), MinMax::min());
  EXPECT_EQ(MinMaxAll::max()->asMinMax(), MinMax::max());
}

TEST_F(SpiceSmokeTest, R9_TransitionRiseFallAsString) {
  // Transition::to_string used in SPICE reporting
  EXPECT_EQ(Transition::rise()->to_string(), "^");
  EXPECT_EQ(Transition::fall()->to_string(), "v");
  // riseFall wildcard
  EXPECT_FALSE(Transition::riseFall()->to_string().empty());
}

TEST_F(SpiceSmokeTest, R9_RiseFallAsRiseFallBoth) {
  EXPECT_EQ(RiseFall::rise()->asRiseFallBoth(), RiseFallBoth::rise());
  EXPECT_EQ(RiseFall::fall()->asRiseFallBoth(), RiseFallBoth::fall());
}

TEST_F(SpiceSmokeTest, R9_MinMaxCompareInfinity) {
  float large = 1e30f;
  float small = -1e30f;
  EXPECT_TRUE(MinMax::min()->compare(small, large));
  EXPECT_FALSE(MinMax::min()->compare(large, small));
  EXPECT_TRUE(MinMax::max()->compare(large, small));
  EXPECT_FALSE(MinMax::max()->compare(small, large));
}

TEST_F(SpiceSmokeTest, R9_RiseFallRangeValues) {
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
TEST_F(XyceCsvTest, R9_ReadCsvSingleRow) {
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
TEST_F(XyceCsvTest, R9_ReadCsvAlternatingSign) {
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
TEST_F(StreamPrintTest, R9_SpiceEndDirective) {
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
TEST_F(XyceCsvTest, R9_ReadCsv50Signals) {
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

} // namespace sta
