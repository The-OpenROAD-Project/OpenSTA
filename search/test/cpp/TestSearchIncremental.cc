#include <gtest/gtest.h>
#include <tcl.h>
#include <cmath>
#include "MinMax.hh"
#include "Transition.hh"
#include "Corner.hh"
#include "Sta.hh"
#include "Sdc.hh"
#include "ReportTcl.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "Graph.hh"
#include "TimingArc.hh"

namespace sta {

// Test fixture that loads a design, creates constraints, and runs
// initial timing so that incremental timing tests can modify the
// netlist and verify timing updates.
class IncrementalTimingTest : public ::testing::Test {
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

    bool ok = sta_->readVerilog("search/test/search_test1.v");
    ASSERT_TRUE(ok);
    ok = sta_->linkDesign("search_test1", true);
    ASSERT_TRUE(ok);

    // Create clock on 'clk' pin with 10ns period
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

    Clock *clk = sta_->sdc()->findClock("clk");
    ASSERT_NE(clk, nullptr);

    // Set input delay on in1 and in2
    Pin *in1 = network->findPin(top, "in1");
    Pin *in2 = network->findPin(top, "in2");
    ASSERT_NE(in1, nullptr);
    ASSERT_NE(in2, nullptr);
    sta_->setInputDelay(in1, RiseFallBoth::riseFall(),
                        clk, RiseFall::rise(), nullptr,
                        false, false, MinMaxAll::all(), false, 0.5f);
    sta_->setInputDelay(in2, RiseFallBoth::riseFall(),
                        clk, RiseFall::rise(), nullptr,
                        false, false, MinMaxAll::all(), false, 0.5f);

    // Set output delay on out1
    Pin *out1 = network->findPin(top, "out1");
    ASSERT_NE(out1, nullptr);
    sta_->setOutputDelay(out1, RiseFallBoth::riseFall(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::all(), false, 0.5f);

    // Run full timing
    sta_->updateTiming(true);
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
};

////////////////////////////////////////////////////////////////
// Test 1: Replace a buffer with a larger one and verify timing changes,
// then replace back and verify timing returns to original.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ReplaceCellAndVerifyTiming) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Get initial worst slack
  Slack initial_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(initial_slack));

  // Find buf1 (BUF_X1) instance
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);

  // Find larger buffer cell BUF_X4
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);

  // Replace BUF_X1 with BUF_X4 (larger = faster = better slack)
  sta_->replaceCell(buf1, buf_x4);
  Slack after_upsize_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(after_upsize_slack));

  // Larger buffer should yield better (larger) or equal slack
  EXPECT_GE(after_upsize_slack, initial_slack);

  // Replace back with BUF_X1
  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_x1, nullptr);
  sta_->replaceCell(buf1, buf_x1);
  Slack restored_slack = sta_->worstSlack(MinMax::max());

  // Slack should return to original value
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 2: Replace a cell with a smaller variant and verify
// timing degrades (worse slack).
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ReplaceCellDownsize) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Get initial worst slack
  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Find buf1 and upsize it first so we have room to downsize
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);

  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);
  Slack upsized_slack = sta_->worstSlack(MinMax::max());

  // Now downsize back to BUF_X1
  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_x1, nullptr);
  sta_->replaceCell(buf1, buf_x1);
  Slack downsized_slack = sta_->worstSlack(MinMax::max());

  // Downsized slack should be worse (smaller) or equal to upsized slack
  EXPECT_LE(downsized_slack, upsized_slack);
}

////////////////////////////////////////////////////////////////
// Test 3: Insert a buffer into an existing path and verify
// timing includes the new buffer (path delay increases).
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, InsertBufferAndVerify) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Get initial worst slack
  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // We will insert a buffer between buf1/Z and reg1/D.
  // Current: buf1/Z --[n2]--> reg1/D
  // After:   buf1/Z --[n2]--> new_buf/A, new_buf/Z --[new_net]--> reg1/D

  Instance *reg1 = network->findChild(top, "reg1");
  ASSERT_NE(reg1, nullptr);
  Pin *reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);

  // Disconnect reg1/D from net n2
  sta_->disconnectPin(reg1_d);

  // Create a new buffer instance
  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_x1, nullptr);
  Instance *new_buf = sta_->makeInstance("inserted_buf", buf_x1, top);
  ASSERT_NE(new_buf, nullptr);

  // Create a new net
  Net *new_net = sta_->makeNet("new_net", top);
  ASSERT_NE(new_net, nullptr);

  // Find the existing net n2
  Net *n2 = network->findNet(top, "n2");
  ASSERT_NE(n2, nullptr);

  // Connect new_buf/A to n2 (existing net from buf1/Z)
  LibertyPort *buf_a_port = buf_x1->findLibertyPort("A");
  LibertyPort *buf_z_port = buf_x1->findLibertyPort("Z");
  ASSERT_NE(buf_a_port, nullptr);
  ASSERT_NE(buf_z_port, nullptr);
  sta_->connectPin(new_buf, buf_a_port, n2);

  // Connect new_buf/Z to new_net
  sta_->connectPin(new_buf, buf_z_port, new_net);

  // Connect reg1/D to new_net
  LibertyCell *dff_cell = network->findLibertyCell("DFF_X1");
  ASSERT_NE(dff_cell, nullptr);
  LibertyPort *dff_d_port = dff_cell->findLibertyPort("D");
  ASSERT_NE(dff_d_port, nullptr);
  sta_->connectPin(reg1, dff_d_port, new_net);

  // Check timing after insertion
  Slack after_insert_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(after_insert_slack));

  // Inserting a buffer adds delay, so slack should degrade
  EXPECT_LE(after_insert_slack, initial_slack);

  // Clean up: reverse the insertion
  // Disconnect new_buf pins and reg1/D
  Pin *new_buf_a = network->findPin(new_buf, "A");
  Pin *new_buf_z = network->findPin(new_buf, "Z");
  Pin *reg1_d_new = network->findPin(reg1, "D");
  ASSERT_NE(new_buf_a, nullptr);
  ASSERT_NE(new_buf_z, nullptr);
  ASSERT_NE(reg1_d_new, nullptr);

  sta_->disconnectPin(reg1_d_new);
  sta_->disconnectPin(new_buf_a);
  sta_->disconnectPin(new_buf_z);
  sta_->deleteInstance(new_buf);
  sta_->deleteNet(new_net);

  // Reconnect reg1/D to n2
  sta_->connectPin(reg1, dff_d_port, n2);

  // Verify timing restores
  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 4: Remove a buffer from the path and verify timing improves.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, RemoveBufferAndVerify) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Current path: and1/ZN --[n1]--> buf1/A, buf1/Z --[n2]--> reg1/D
  // After removing buf1: and1/ZN --[n1]--> reg1/D (shorter path)

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Instance *reg1 = network->findChild(top, "reg1");
  ASSERT_NE(reg1, nullptr);

  // Get the net n1 (and1 output to buf1 input)
  Net *n1 = network->findNet(top, "n1");
  ASSERT_NE(n1, nullptr);

  // Disconnect buf1 pins and reg1/D
  Pin *buf1_a = network->findPin(buf1, "A");
  Pin *buf1_z = network->findPin(buf1, "Z");
  Pin *reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(buf1_a, nullptr);
  ASSERT_NE(buf1_z, nullptr);
  ASSERT_NE(reg1_d, nullptr);

  sta_->disconnectPin(reg1_d);
  sta_->disconnectPin(buf1_a);
  sta_->disconnectPin(buf1_z);

  // Connect reg1/D directly to n1
  LibertyCell *dff_cell = network->findLibertyCell("DFF_X1");
  ASSERT_NE(dff_cell, nullptr);
  LibertyPort *dff_d_port = dff_cell->findLibertyPort("D");
  ASSERT_NE(dff_d_port, nullptr);
  sta_->connectPin(reg1, dff_d_port, n1);

  // Timing should improve (buffer removed from path)
  Slack after_remove_slack = sta_->worstSlack(MinMax::max());
  EXPECT_GE(after_remove_slack, initial_slack);

  // Restore: reconnect buf1
  Pin *reg1_d_new = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d_new, nullptr);
  sta_->disconnectPin(reg1_d_new);

  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  LibertyPort *buf_a_port = buf_x1->findLibertyPort("A");
  LibertyPort *buf_z_port = buf_x1->findLibertyPort("Z");
  Net *n2 = network->findNet(top, "n2");
  ASSERT_NE(n2, nullptr);

  sta_->connectPin(buf1, buf_a_port, n1);
  sta_->connectPin(buf1, buf_z_port, n2);
  sta_->connectPin(reg1, dff_d_port, n2);

  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 5: Make multiple edits before retiming to verify combined effect.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, MultipleEditsBeforeRetiming) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Edit 1: Upsize buf1 to BUF_X4
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  // Edit 2: Set output load on out1
  Pin *out1_pin = network->findPin(top, "out1");
  ASSERT_NE(out1_pin, nullptr);
  Port *out1_port = network->findPort(
    network->cell(top), "out1");
  ASSERT_NE(out1_port, nullptr);
  sta_->setPortExtPinCap(out1_port, RiseFallBoth::riseFall(),
                         sta_->cmdCorner(), MinMaxAll::all(), 0.05f);

  // Edit 3: Set input slew on in1
  Port *in1_port = network->findPort(
    network->cell(top), "in1");
  ASSERT_NE(in1_port, nullptr);
  sta_->setInputSlew(in1_port, RiseFallBoth::riseFall(),
                     MinMaxAll::all(), 0.1f);

  // Now run timing once (implicitly via worstSlack)
  Slack combined_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(combined_slack));

  // The combined effect should differ from initial
  // (upsizing helps, load/slew may hurt -- just verify it's valid)
  EXPECT_NE(combined_slack, initial_slack);
}

////////////////////////////////////////////////////////////////
// Test 6: Verify incremental vs full timing produce the same result.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, IncrementalVsFullConsistency) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Make an edit: upsize buf2 to BUF_X4
  Instance *buf2 = network->findChild(top, "buf2");
  ASSERT_NE(buf2, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf2, buf_x4);

  // Run incremental timing
  sta_->updateTiming(false);
  Slack incremental_slack = sta_->worstSlack(MinMax::max());

  // Run full timing
  sta_->updateTiming(true);
  Slack full_slack = sta_->worstSlack(MinMax::max());

  // Both should produce the same result
  EXPECT_NEAR(incremental_slack, full_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 7: Set output load and verify timing updates incrementally.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, SetLoadIncremental) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Set a large output load on out1
  Port *out1_port = network->findPort(
    network->cell(top), "out1");
  ASSERT_NE(out1_port, nullptr);
  sta_->setPortExtPinCap(out1_port, RiseFallBoth::riseFall(),
                         sta_->cmdCorner(), MinMaxAll::all(), 0.5f);

  Slack loaded_slack = sta_->worstSlack(MinMax::max());
  // Large load should degrade timing
  EXPECT_LE(loaded_slack, initial_slack);

  // Reduce the load
  sta_->setPortExtPinCap(out1_port, RiseFallBoth::riseFall(),
                         sta_->cmdCorner(), MinMaxAll::all(), 0.001f);

  Slack reduced_load_slack = sta_->worstSlack(MinMax::max());
  // Reduced load should improve timing relative to large load
  EXPECT_GE(reduced_load_slack, loaded_slack);
}

////////////////////////////////////////////////////////////////
// Test 8: Replace a cell and change clock period, verify both
// changes are reflected in timing.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ClockConstraintAfterEdit) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Edit: Replace buf1 with BUF_X4
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  Slack after_replace_slack = sta_->worstSlack(MinMax::max());

  // Now tighten the clock period (smaller period = tighter timing)
  Pin *clk_pin = network->findPin(top, "clk");
  ASSERT_NE(clk_pin, nullptr);
  PinSet *clk_pins = new PinSet(network);
  clk_pins->insert(clk_pin);
  FloatSeq *tight_waveform = new FloatSeq;
  tight_waveform->push_back(0.0f);
  tight_waveform->push_back(1.0f);  // 2ns period
  sta_->makeClock("clk", clk_pins, false, 2.0f, tight_waveform, nullptr);

  Slack tight_slack = sta_->worstSlack(MinMax::max());

  // Tighter clock should give worse slack
  EXPECT_LT(tight_slack, after_replace_slack);

  // Now loosen the clock period significantly
  PinSet *clk_pins2 = new PinSet(network);
  clk_pins2->insert(clk_pin);
  FloatSeq *loose_waveform = new FloatSeq;
  loose_waveform->push_back(0.0f);
  loose_waveform->push_back(50.0f);  // 100ns period
  sta_->makeClock("clk", clk_pins2, false, 100.0f, loose_waveform, nullptr);

  Slack loose_slack = sta_->worstSlack(MinMax::max());

  // Looser clock should give better slack than tight clock
  EXPECT_GT(loose_slack, tight_slack);
}

////////////////////////////////////////////////////////////////
// Test 9: Replace AND gate with larger variant (AND2_X4)
// and verify timing improves due to stronger drive.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ReplaceAndGateWithLargerVariant) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Find and1 instance (AND2_X1) and replace with AND2_X4
  Instance *and1 = network->findChild(top, "and1");
  ASSERT_NE(and1, nullptr);

  LibertyCell *and2_x4 = network->findLibertyCell("AND2_X4");
  ASSERT_NE(and2_x4, nullptr);

  sta_->replaceCell(and1, and2_x4);
  Slack after_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(after_slack));

  // Larger AND gate has stronger drive, should improve or maintain slack
  EXPECT_GE(after_slack, initial_slack);

  // Replace back and verify restoration
  LibertyCell *and2_x1 = network->findLibertyCell("AND2_X1");
  ASSERT_NE(and2_x1, nullptr);
  sta_->replaceCell(and1, and2_x1);
  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 10: Chain of replacements: upsize -> downsize -> upsize
// and verify timing is consistent after each step.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ChainedReplacementsConsistency) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);

  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  LibertyCell *buf_x2 = network->findLibertyCell("BUF_X2");
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x1, nullptr);
  ASSERT_NE(buf_x2, nullptr);
  ASSERT_NE(buf_x4, nullptr);

  // Step 1: Upsize to BUF_X4
  sta_->replaceCell(buf1, buf_x4);
  Slack slack_x4 = sta_->worstSlack(MinMax::max());

  // Step 2: Downsize to BUF_X2
  sta_->replaceCell(buf1, buf_x2);
  Slack slack_x2 = sta_->worstSlack(MinMax::max());

  // BUF_X4 should be at least as good as BUF_X2
  EXPECT_GE(slack_x4, slack_x2);

  // Step 3: Upsize again to BUF_X4
  sta_->replaceCell(buf1, buf_x4);
  Slack slack_x4_again = sta_->worstSlack(MinMax::max());

  // Same cell should produce same slack
  EXPECT_NEAR(slack_x4, slack_x4_again, 1e-6);

  // Step 4: Return to original
  sta_->replaceCell(buf1, buf_x1);
  Slack slack_original = sta_->worstSlack(MinMax::max());

  // X2 should be between X1 and X4
  EXPECT_GE(slack_x2, slack_original);
  EXPECT_LE(slack_x2, slack_x4);
}

////////////////////////////////////////////////////////////////
// Test 11: Replace all cells on the combinational path
// (and1 + buf1) and verify cumulative timing effect.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ReplaceAllCellsOnPath) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Upsize and1 to AND2_X4
  Instance *and1 = network->findChild(top, "and1");
  ASSERT_NE(and1, nullptr);
  LibertyCell *and2_x4 = network->findLibertyCell("AND2_X4");
  ASSERT_NE(and2_x4, nullptr);
  sta_->replaceCell(and1, and2_x4);

  Slack after_and_slack = sta_->worstSlack(MinMax::max());

  // Also upsize buf1 to BUF_X4
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  Slack after_both_slack = sta_->worstSlack(MinMax::max());

  // Both upsized should be at least as good as just and1 upsized
  EXPECT_GE(after_both_slack, initial_slack);
  // Cumulative improvement: both should be better than initial
  EXPECT_GE(after_and_slack, initial_slack);
  EXPECT_GE(after_both_slack, initial_slack);
}

////////////////////////////////////////////////////////////////
// Test 12: Set timing derate (cell delay) after initial timing
// and verify setup slack degrades for late derate > 1.0.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, TimingDerateAffectsSlack) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Timing derate requires OCV analysis mode to distinguish early/late.
  sta_->setAnalysisType(AnalysisType::ocv);

  // Add significant load to make gate delays visible for derating
  Port *out1_port = network->findPort(network->cell(top), "out1");
  ASSERT_NE(out1_port, nullptr);
  sta_->setPortExtPinCap(out1_port, RiseFallBoth::riseFall(),
                         sta_->cmdCorner(), MinMaxAll::all(), 0.5f);
  sta_->updateTiming(true);

  Slack initial_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(initial_slack));

  // Apply a large cell delay derate on data paths for late analysis
  sta_->setTimingDerate(TimingDerateType::cell_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::late(),
                        5.0f);

  Slack derated_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(derated_slack));

  // Late derate > 1.0 increases data path delay, worsening setup slack
  // With 0.5pF load, gate delays are significant enough that 5x derate
  // should produce a visible effect on slack
  EXPECT_LE(derated_slack, initial_slack);

  // Remove the derate and verify slack restores
  sta_->unsetTimingDerate();
  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 13: Set clock uncertainty and verify setup slack degrades.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ClockUncertaintyDegradeSetupSlack) {
  Slack initial_slack = sta_->worstSlack(MinMax::max());

  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);

  // Add 0.5ns setup uncertainty -- eats into the timing margin
  sta_->setClockUncertainty(clk, SetupHoldAll::max(), 0.5f);

  Slack after_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(after_slack));

  // Uncertainty reduces available margin, slack should worsen
  EXPECT_LT(after_slack, initial_slack);

  // The slack difference should be approximately 0.5ns
  float slack_diff = initial_slack - after_slack;
  EXPECT_NEAR(slack_diff, 0.5f, 0.01f);

  // Remove uncertainty
  sta_->removeClockUncertainty(clk, SetupHoldAll::max());
  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 14: Set input transition (slew) on port and verify
// path delay changes.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, InputSlewChangesPathDelay) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Set a very large input slew on in1 (1ns)
  Port *in1_port = network->findPort(network->cell(top), "in1");
  ASSERT_NE(in1_port, nullptr);
  sta_->setInputSlew(in1_port, RiseFallBoth::riseFall(),
                     MinMaxAll::all(), 1.0f);

  Slack after_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(after_slack));

  // Large input slew increases gate delays downstream, worsening slack
  EXPECT_LE(after_slack, initial_slack);

  // Now set a small slew (fast transition)
  sta_->setInputSlew(in1_port, RiseFallBoth::riseFall(),
                     MinMaxAll::all(), 0.001f);

  Slack fast_slack = sta_->worstSlack(MinMax::max());
  // Fast slew should give better timing than slow slew
  EXPECT_GE(fast_slack, after_slack);
}

////////////////////////////////////////////////////////////////
// Test 15: Disable timing on the AND cell and verify the
// path through it is no longer constrained (slack improves
// dramatically or becomes unconstrained).
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, DisableCellTimingExcludesPath) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Check pin slack on reg1/D (endpoint of the input path through and1)
  Instance *reg1 = network->findChild(top, "reg1");
  ASSERT_NE(reg1, nullptr);
  Pin *reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);

  Slack initial_pin_slack = sta_->pinSlack(reg1_d, MinMax::max());
  EXPECT_FALSE(std::isnan(initial_pin_slack));

  // Find and1 instance and disable timing through it
  Instance *and1 = network->findChild(top, "and1");
  ASSERT_NE(and1, nullptr);

  // Disable all timing arcs through and1 instance
  sta_->disable(and1, nullptr, nullptr);

  // After disabling and1, the path in1/in2 -> and1 -> buf1 -> reg1 is broken.
  // The pin slack at reg1/D should become unconstrained (NaN/INF) or improve
  // significantly because no constrained path reaches it.
  Slack after_disable_pin_slack = sta_->pinSlack(reg1_d, MinMax::max());
  // Either unconstrained (NaN/very large) or much better than before
  if (std::isnan(after_disable_pin_slack) == false) {
    EXPECT_GT(after_disable_pin_slack, initial_pin_slack);
  }
  // else: NaN means unconstrained, which is expected

  // Re-enable timing through and1
  sta_->removeDisable(and1, nullptr, nullptr);
  Slack restored_pin_slack = sta_->pinSlack(reg1_d, MinMax::max());
  EXPECT_NEAR(restored_pin_slack, initial_pin_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 16: Disconnect and reconnect a pin, verify timing restores.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, DisconnectReconnectPinRestoresTiming) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Use pin slack at reg1/D to track the specific input path
  Instance *reg1 = network->findChild(top, "reg1");
  ASSERT_NE(reg1, nullptr);
  Pin *reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);

  Slack initial_pin_slack = sta_->pinSlack(reg1_d, MinMax::max());
  EXPECT_FALSE(std::isnan(initial_pin_slack));

  // Disconnect reg1/D from n2 and reconnect it
  sta_->disconnectPin(reg1_d);

  // Reconnect reg1/D to n2
  Net *n2 = network->findNet(top, "n2");
  ASSERT_NE(n2, nullptr);
  LibertyCell *dff_cell = network->findLibertyCell("DFF_X1");
  ASSERT_NE(dff_cell, nullptr);
  LibertyPort *dff_d_port = dff_cell->findLibertyPort("D");
  ASSERT_NE(dff_d_port, nullptr);
  sta_->connectPin(reg1, dff_d_port, n2);

  // After disconnect/reconnect to same net, timing should restore
  Slack restored_pin_slack = sta_->pinSlack(reg1_d, MinMax::max());
  // The pin slack should be restored to close to the initial value
  // after reconnecting to the same net
  EXPECT_FALSE(std::isnan(restored_pin_slack));
  EXPECT_NEAR(restored_pin_slack, initial_pin_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 17: Connect reg1/D to a different net (n1 instead of n2),
// bypassing buf1, and verify timing updates for new topology.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ConnectPinToDifferentNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Add significant output load on buf1 output (net n2) to make
  // the buf1 delay visible when bypassing it.
  Port *out1_port = network->findPort(network->cell(top), "out1");
  ASSERT_NE(out1_port, nullptr);
  sta_->setPortExtPinCap(out1_port, RiseFallBoth::riseFall(),
                         sta_->cmdCorner(), MinMaxAll::all(), 0.1f);
  sta_->updateTiming(true);

  // Track pin slack at reg1/D for the input path
  Instance *reg1 = network->findChild(top, "reg1");
  ASSERT_NE(reg1, nullptr);
  Pin *reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);

  Slack initial_pin_slack = sta_->pinSlack(reg1_d, MinMax::max());
  EXPECT_FALSE(std::isnan(initial_pin_slack));

  // Current: and1/ZN --[n1]--> buf1/A, buf1/Z --[n2]--> reg1/D
  // Change to: reg1/D connected to n1 (bypass buf1)

  sta_->disconnectPin(reg1_d);

  Net *n1 = network->findNet(top, "n1");
  ASSERT_NE(n1, nullptr);
  LibertyCell *dff_cell = network->findLibertyCell("DFF_X1");
  ASSERT_NE(dff_cell, nullptr);
  LibertyPort *dff_d_port = dff_cell->findLibertyPort("D");
  ASSERT_NE(dff_d_port, nullptr);
  sta_->connectPin(reg1, dff_d_port, n1);

  // After bypassing buf1, the path is shorter so pin slack should improve
  reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);
  Slack bypassed_pin_slack = sta_->pinSlack(reg1_d, MinMax::max());
  EXPECT_FALSE(std::isnan(bypassed_pin_slack));
  // Shorter path should improve slack at this pin
  EXPECT_GE(bypassed_pin_slack, initial_pin_slack);

  // Restore: reconnect reg1/D to n2
  reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);
  sta_->disconnectPin(reg1_d);

  Net *n2 = network->findNet(top, "n2");
  ASSERT_NE(n2, nullptr);
  sta_->connectPin(reg1, dff_d_port, n2);

  // After restoring, pin slack should return to original
  reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);
  Slack restored_pin_slack = sta_->pinSlack(reg1_d, MinMax::max());
  EXPECT_NEAR(restored_pin_slack, initial_pin_slack, 1e-3);
}

////////////////////////////////////////////////////////////////
// Test 18: Annotate net wire capacitance and verify it increases
// delay through the path.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, NetWireCapAnnotation) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Annotate large wire cap on net n1 (and1 output)
  Net *n1 = network->findNet(top, "n1");
  ASSERT_NE(n1, nullptr);
  sta_->setNetWireCap(n1, false, sta_->cmdCorner(),
                      MinMaxAll::all(), 0.5f);

  Slack after_cap_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(after_cap_slack));

  // Large wire cap should slow down and1's output, degrading slack
  EXPECT_LT(after_cap_slack, initial_slack);

  // Reduce the cap
  sta_->setNetWireCap(n1, false, sta_->cmdCorner(),
                      MinMaxAll::all(), 0.001f);

  Slack small_cap_slack = sta_->worstSlack(MinMax::max());
  // Smaller cap should be better than large cap
  EXPECT_GT(small_cap_slack, after_cap_slack);
}

////////////////////////////////////////////////////////////////
// Test 19: Set annotated slew on a vertex and verify
// delay calculation uses the annotation.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, AnnotatedSlewAffectsDelay) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Get the graph vertex for and1/ZN (driver pin)
  Instance *and1 = network->findChild(top, "and1");
  ASSERT_NE(and1, nullptr);
  Pin *and1_zn = network->findPin(and1, "ZN");
  ASSERT_NE(and1_zn, nullptr);

  Graph *graph = sta_->ensureGraph();
  ASSERT_NE(graph, nullptr);
  Vertex *and1_zn_vertex = graph->pinDrvrVertex(and1_zn);
  ASSERT_NE(and1_zn_vertex, nullptr);

  // Annotate a very large slew (2.0ns) on the and1 output
  sta_->setAnnotatedSlew(and1_zn_vertex, sta_->cmdCorner(),
                         MinMaxAll::all(), RiseFallBoth::riseFall(),
                         2.0f);

  Slack after_slew_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(after_slew_slack));

  // Large slew annotation on and1 output should increase downstream
  // delay through buf1, degrading timing
  EXPECT_LT(after_slew_slack, initial_slack);

  // Remove annotations and verify restoration
  sta_->removeDelaySlewAnnotations();
  // Need full timing update after removing annotations
  sta_->updateTiming(true);
  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 20: Annotate arc delay on an edge and verify it affects timing.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ArcDelayAnnotation) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Find buf1 instance and get its timing arcs
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_a = network->findPin(buf1, "A");
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_a, nullptr);
  ASSERT_NE(buf1_z, nullptr);

  Graph *graph = sta_->ensureGraph();
  ASSERT_NE(graph, nullptr);

  Vertex *buf1_a_vertex = graph->pinLoadVertex(buf1_a);
  ASSERT_NE(buf1_a_vertex, nullptr);

  // Find an edge from buf1/A to buf1/Z and annotate a large delay
  bool found_edge = false;
  VertexOutEdgeIterator edge_iter(buf1_a_vertex, graph);
  while (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    for (TimingArc *arc : arc_set->arcs()) {
      // Annotate a large delay (5ns) on this arc
      sta_->setArcDelay(edge, arc, sta_->cmdCorner(),
                        MinMaxAll::all(), 5.0f);
      found_edge = true;
    }
  }
  ASSERT_TRUE(found_edge);

  Slack annotated_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(annotated_slack));

  // A 5ns delay annotation on buf1 should significantly worsen slack
  EXPECT_LT(annotated_slack, initial_slack);

  // Remove annotations and restore
  sta_->removeDelaySlewAnnotations();
  sta_->updateTiming(true);
  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 21: Verify that multiple incremental edit-query cycles
// produce results consistent with a final full timing update.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, RapidEditQueryCycles) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Instance *and1 = network->findChild(top, "and1");
  ASSERT_NE(and1, nullptr);

  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  LibertyCell *buf_x2 = network->findLibertyCell("BUF_X2");
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  LibertyCell *and2_x2 = network->findLibertyCell("AND2_X2");
  ASSERT_NE(buf_x1, nullptr);
  ASSERT_NE(buf_x2, nullptr);
  ASSERT_NE(buf_x4, nullptr);
  ASSERT_NE(and2_x2, nullptr);

  // Cycle 1: Edit buf1 -> BUF_X2, query
  sta_->replaceCell(buf1, buf_x2);
  Slack slack1 = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(slack1));

  // Cycle 2: Edit and1 -> AND2_X2, query
  sta_->replaceCell(and1, and2_x2);
  Slack slack2 = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(slack2));

  // Cycle 3: Edit buf1 -> BUF_X4, query
  sta_->replaceCell(buf1, buf_x4);
  Slack slack3 = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(slack3));

  // Now do a full timing update and verify consistency
  sta_->updateTiming(true);
  Slack full_slack = sta_->worstSlack(MinMax::max());

  // The last incremental result should match full timing
  EXPECT_NEAR(slack3, full_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 22: Verify TNS (total negative slack) updates
// incrementally after edits.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, TnsUpdatesIncrementally) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_tns = sta_->totalNegativeSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(initial_tns));
  // TNS is <= 0 by definition (sum of negative slacks)
  EXPECT_LE(initial_tns, 0.0f);

  // Tighten the clock severely to create violations
  Pin *clk_pin = network->findPin(top, "clk");
  ASSERT_NE(clk_pin, nullptr);
  PinSet *clk_pins = new PinSet(network);
  clk_pins->insert(clk_pin);
  FloatSeq *tight_waveform = new FloatSeq;
  tight_waveform->push_back(0.0f);
  tight_waveform->push_back(0.2f);  // 0.4ns period (very tight)
  sta_->makeClock("clk", clk_pins, false, 0.4f, tight_waveform, nullptr);

  Slack tight_tns = sta_->totalNegativeSlack(MinMax::max());
  // Very tight clock should create large negative TNS
  EXPECT_LT(tight_tns, initial_tns);

  // Upsize cells to partially improve TNS
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  Slack improved_tns = sta_->totalNegativeSlack(MinMax::max());
  // Upsizing should improve (make less negative) TNS
  EXPECT_GE(improved_tns, tight_tns);

  // Verify incremental TNS matches full timing
  sta_->updateTiming(true);
  Slack full_tns = sta_->totalNegativeSlack(MinMax::max());
  EXPECT_NEAR(improved_tns, full_tns, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 23: Verify arrival time at specific pins after an edit.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ArrivalTimeAtPinAfterEdit) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Get initial arrival at reg1/D
  Instance *reg1 = network->findChild(top, "reg1");
  ASSERT_NE(reg1, nullptr);
  Pin *reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);

  Arrival initial_arrival = sta_->pinArrival(reg1_d, RiseFall::rise(),
                                             MinMax::max());
  EXPECT_FALSE(std::isnan(initial_arrival));
  EXPECT_GT(initial_arrival, 0.0f);

  // Upsize buf1 to reduce delay to reg1/D
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  Arrival after_arrival = sta_->pinArrival(reg1_d, RiseFall::rise(),
                                           MinMax::max());
  EXPECT_FALSE(std::isnan(after_arrival));

  // Faster buffer means earlier arrival at reg1/D
  EXPECT_LE(after_arrival, initial_arrival);

  // Restore and verify arrival restores
  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_x1, nullptr);
  sta_->replaceCell(buf1, buf_x1);

  Arrival restored_arrival = sta_->pinArrival(reg1_d, RiseFall::rise(),
                                              MinMax::max());
  EXPECT_NEAR(restored_arrival, initial_arrival, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 24: Verify hold (min) slack after cell replacement.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, HoldSlackAfterCellReplacement) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_hold_slack = sta_->worstSlack(MinMax::min());
  EXPECT_FALSE(std::isnan(initial_hold_slack));

  // Upsize buf1 -- this makes the path faster, which can help hold
  // violations (hold requires minimum delay) or worsen them depending
  // on the design. Either way, the value should be valid.
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  Slack after_hold_slack = sta_->worstSlack(MinMax::min());
  EXPECT_FALSE(std::isnan(after_hold_slack));

  // Faster cell should worsen hold timing (data arrives earlier)
  EXPECT_LE(after_hold_slack, initial_hold_slack);

  // Restore
  LibertyCell *buf_x1 = network->findLibertyCell("BUF_X1");
  ASSERT_NE(buf_x1, nullptr);
  sta_->replaceCell(buf1, buf_x1);
  Slack restored_hold_slack = sta_->worstSlack(MinMax::min());
  EXPECT_NEAR(restored_hold_slack, initial_hold_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 25: Check both setup and hold after multiple edits.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, SetupAndHoldAfterEdits) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_setup = sta_->worstSlack(MinMax::max());
  Slack initial_hold = sta_->worstSlack(MinMax::min());

  // Edit 1: Add 0.3ns clock uncertainty for both setup and hold
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockUncertainty(clk, SetupHoldAll::all(), 0.3f);

  Slack setup_after_unc = sta_->worstSlack(MinMax::max());
  Slack hold_after_unc = sta_->worstSlack(MinMax::min());

  // Setup uncertainty eats into margin from the top
  EXPECT_LT(setup_after_unc, initial_setup);
  // Hold uncertainty eats into margin from the bottom
  EXPECT_LT(hold_after_unc, initial_hold);

  // Edit 2: Upsize buf1 to offset some of the setup degradation
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  Slack setup_after_both = sta_->worstSlack(MinMax::max());
  Slack hold_after_both = sta_->worstSlack(MinMax::min());

  // Upsizing helps setup (but may hurt hold)
  EXPECT_GE(setup_after_both, setup_after_unc);
  // Valid results
  EXPECT_FALSE(std::isnan(setup_after_both));
  EXPECT_FALSE(std::isnan(hold_after_both));
}

////////////////////////////////////////////////////////////////
// Test 26: Verify incremental vs full consistency after multiple
// heterogeneous edits (cell swap + constraint change).
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, IncrementalVsFullAfterMixedEdits) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Edit 1: Replace buf2 with BUF_X4
  Instance *buf2 = network->findChild(top, "buf2");
  ASSERT_NE(buf2, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf2, buf_x4);

  // Edit 2: Add output load
  Port *out1_port = network->findPort(network->cell(top), "out1");
  ASSERT_NE(out1_port, nullptr);
  sta_->setPortExtPinCap(out1_port, RiseFallBoth::riseFall(),
                         sta_->cmdCorner(), MinMaxAll::all(), 0.1f);

  // Edit 3: Add clock uncertainty
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  sta_->setClockUncertainty(clk, SetupHoldAll::max(), 0.2f);

  // Get incremental result
  sta_->updateTiming(false);
  Slack inc_setup = sta_->worstSlack(MinMax::max());
  Slack inc_tns = sta_->totalNegativeSlack(MinMax::max());

  // Get full timing result
  sta_->updateTiming(true);
  Slack full_setup = sta_->worstSlack(MinMax::max());
  Slack full_tns = sta_->totalNegativeSlack(MinMax::max());

  EXPECT_NEAR(inc_setup, full_setup, 1e-6);
  EXPECT_NEAR(inc_tns, full_tns, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 27: Set input delay to different values and verify
// arrival times update correctly.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, InputDelayChangeUpdatesTiming) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Change input delay on in1 from 0.5ns to 3.0ns
  Pin *in1 = network->findPin(top, "in1");
  ASSERT_NE(in1, nullptr);
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);

  sta_->setInputDelay(in1, RiseFallBoth::riseFall(),
                      clk, RiseFall::rise(), nullptr,
                      false, false, MinMaxAll::all(), false, 3.0f);

  Slack large_delay_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(large_delay_slack));

  // Larger input delay means data arrives later, worsening setup slack
  EXPECT_LT(large_delay_slack, initial_slack);

  // Set it very small
  sta_->setInputDelay(in1, RiseFallBoth::riseFall(),
                      clk, RiseFall::rise(), nullptr,
                      false, false, MinMaxAll::all(), false, 0.01f);

  Slack small_delay_slack = sta_->worstSlack(MinMax::max());
  // Smaller input delay should give better slack
  EXPECT_GT(small_delay_slack, large_delay_slack);
}

////////////////////////////////////////////////////////////////
// Test 28: Set output delay to different values and verify
// timing updates.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, OutputDelayChangeUpdatesTiming) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Increase output delay on out1 from 0.5ns to 5.0ns
  Pin *out1 = network->findPin(top, "out1");
  ASSERT_NE(out1, nullptr);
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);

  sta_->setOutputDelay(out1, RiseFallBoth::riseFall(),
                       clk, RiseFall::rise(), nullptr,
                       false, false, MinMaxAll::all(), false, 5.0f);

  Slack large_out_delay_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(large_out_delay_slack));

  // Larger output delay reduces available path time, worsening slack
  EXPECT_LT(large_out_delay_slack, initial_slack);

  // Set a very small output delay
  sta_->setOutputDelay(out1, RiseFallBoth::riseFall(),
                       clk, RiseFall::rise(), nullptr,
                       false, false, MinMaxAll::all(), false, 0.01f);

  Slack small_out_delay_slack = sta_->worstSlack(MinMax::max());
  EXPECT_GT(small_out_delay_slack, large_out_delay_slack);
}

////////////////////////////////////////////////////////////////
// Test 29: Set clock latency and verify it shifts arrival/required
// times at register pins.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ClockLatencyAffectsTiming) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);

  // Add 1ns source latency to clock
  sta_->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                        MinMaxAll::all(), 1.0f);

  Slack latency_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(latency_slack));

  // Clock latency applied to both source and capture should not change
  // setup slack (it cancels out for same-clock paths). But it does
  // shift arrivals.
  // For same-clock paths, latency cancels, so slack should be similar.
  EXPECT_NEAR(latency_slack, initial_slack, 0.01f);

  // Remove latency
  sta_->removeClockLatency(clk, nullptr);
  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 30: Verify pin slack query at specific pins after edit.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, PinSlackQueryAfterEdit) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Get pin slack at buf1/Z
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);

  Slack initial_pin_slack = sta_->pinSlack(buf1_z, MinMax::max());
  EXPECT_FALSE(std::isnan(initial_pin_slack));

  // Also check worst slack correlation
  Slack initial_worst = sta_->worstSlack(MinMax::max());
  // Pin slack at buf1/Z should be >= worst slack (worst is the minimum)
  EXPECT_GE(initial_pin_slack, initial_worst);

  // Upsize buf1
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  Slack after_pin_slack = sta_->pinSlack(buf1_z, MinMax::max());
  EXPECT_FALSE(std::isnan(after_pin_slack));

  // Upsizing should improve the slack at this pin
  EXPECT_GE(after_pin_slack, initial_pin_slack);
}

////////////////////////////////////////////////////////////////
// Test 31: Verify slew at a vertex updates after cell replacement.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, VertexSlewUpdatesAfterReplace) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Get slew at buf1/Z (output of BUF_X1)
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  Pin *buf1_z = network->findPin(buf1, "Z");
  ASSERT_NE(buf1_z, nullptr);

  Graph *graph = sta_->ensureGraph();
  ASSERT_NE(graph, nullptr);
  Vertex *buf1_z_vertex = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(buf1_z_vertex, nullptr);

  Slew initial_slew = sta_->vertexSlew(buf1_z_vertex, RiseFall::rise(),
                                       MinMax::max());
  EXPECT_FALSE(std::isnan(initial_slew));
  EXPECT_GT(initial_slew, 0.0f);

  // Replace buf1 with BUF_X4 (stronger driver = faster slew)
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  // Need to refetch the vertex since the graph may be rebuilt
  graph = sta_->ensureGraph();
  buf1_z = network->findPin(buf1, "Z");
  buf1_z_vertex = graph->pinDrvrVertex(buf1_z);
  ASSERT_NE(buf1_z_vertex, nullptr);

  Slew after_slew = sta_->vertexSlew(buf1_z_vertex, RiseFall::rise(),
                                     MinMax::max());
  EXPECT_FALSE(std::isnan(after_slew));

  // Stronger driver (BUF_X4) should produce faster (smaller) slew
  EXPECT_LE(after_slew, initial_slew);
}

////////////////////////////////////////////////////////////////
// Test 32: Replace buf2 (output path) and verify output path timing.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, OutputPathCellReplacement) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // buf2 is on the output path: reg1/Q -> buf2 -> out1
  Instance *buf2 = network->findChild(top, "buf2");
  ASSERT_NE(buf2, nullptr);

  // Get pin slack at out1 before edit
  Pin *out1_pin = network->findPin(top, "out1");
  ASSERT_NE(out1_pin, nullptr);
  Slack out1_slack_before = sta_->pinSlack(out1_pin, MinMax::max());

  // Replace buf2 with BUF_X4
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf2, buf_x4);

  Slack out1_slack_after = sta_->pinSlack(out1_pin, MinMax::max());
  EXPECT_FALSE(std::isnan(out1_slack_after));

  // BUF_X4 is faster, out1 slack should improve
  EXPECT_GE(out1_slack_after, out1_slack_before);

  // Also check worst slack
  Slack after_worst = sta_->worstSlack(MinMax::max());
  EXPECT_GE(after_worst, initial_slack);
}

////////////////////////////////////////////////////////////////
// Test 33: Endpoint violation count changes with clock period.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, EndpointViolationCountChanges) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // With 10ns clock, there should be no or few violations
  int initial_violations = sta_->endpointViolationCount(MinMax::max());
  EXPECT_GE(initial_violations, 0);

  // Tighten the clock to create violations
  Pin *clk_pin = network->findPin(top, "clk");
  ASSERT_NE(clk_pin, nullptr);
  PinSet *clk_pins = new PinSet(network);
  clk_pins->insert(clk_pin);
  FloatSeq *tight_waveform = new FloatSeq;
  tight_waveform->push_back(0.0f);
  tight_waveform->push_back(0.1f);  // 0.2ns period
  sta_->makeClock("clk", clk_pins, false, 0.2f, tight_waveform, nullptr);

  int tight_violations = sta_->endpointViolationCount(MinMax::max());
  // Very tight clock should cause violations
  EXPECT_GT(tight_violations, initial_violations);

  // Loosen the clock
  PinSet *clk_pins2 = new PinSet(network);
  clk_pins2->insert(clk_pin);
  FloatSeq *loose_waveform = new FloatSeq;
  loose_waveform->push_back(0.0f);
  loose_waveform->push_back(50.0f);
  sta_->makeClock("clk", clk_pins2, false, 100.0f, loose_waveform, nullptr);

  int loose_violations = sta_->endpointViolationCount(MinMax::max());
  // Loose clock should have fewer violations
  EXPECT_LT(loose_violations, tight_violations);
}

////////////////////////////////////////////////////////////////
// Test 34: Net slack query updates incrementally.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, NetSlackUpdatesIncrementally) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Get net slack on n2 (buf1/Z -> reg1/D)
  Net *n2 = network->findNet(top, "n2");
  ASSERT_NE(n2, nullptr);

  Slack initial_net_slack = sta_->netSlack(n2, MinMax::max());
  EXPECT_FALSE(std::isnan(initial_net_slack));

  // Upsize buf1 to improve the path through n2
  Instance *buf1 = network->findChild(top, "buf1");
  ASSERT_NE(buf1, nullptr);
  LibertyCell *buf_x4 = network->findLibertyCell("BUF_X4");
  ASSERT_NE(buf_x4, nullptr);
  sta_->replaceCell(buf1, buf_x4);

  Slack after_net_slack = sta_->netSlack(n2, MinMax::max());
  EXPECT_FALSE(std::isnan(after_net_slack));

  // Net slack on n2 should improve after upsizing buf1
  EXPECT_GE(after_net_slack, initial_net_slack);
}

////////////////////////////////////////////////////////////////
// Test 35: Clock latency insertion delay affects timing check.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, ClockInsertionDelayAffectsTiming) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Clock insertion for same-clock paths cancels in slack calculation.
  // Verify that arrival/required times shift by the insertion amount
  // even though slack remains the same.
  Instance *reg1 = network->findChild(top, "reg1");
  ASSERT_NE(reg1, nullptr);
  Pin *reg1_d = network->findPin(reg1, "D");
  ASSERT_NE(reg1_d, nullptr);

  Graph *graph = sta_->ensureGraph();
  ASSERT_NE(graph, nullptr);
  Vertex *reg1_d_vertex = graph->pinLoadVertex(reg1_d);
  ASSERT_NE(reg1_d_vertex, nullptr);

  Arrival initial_arrival = sta_->pinArrival(reg1_d, RiseFall::rise(),
                                             MinMax::max());
  Required initial_required = sta_->vertexRequired(reg1_d_vertex,
                                                   MinMax::max());
  Slack initial_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(initial_arrival));
  EXPECT_FALSE(std::isnan(initial_required));

  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);

  // Add 1ns source insertion delay to the clock
  sta_->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                          MinMaxAll::all(), EarlyLateAll::all(), 1.0f);

  Arrival after_arrival = sta_->pinArrival(reg1_d, RiseFall::rise(),
                                           MinMax::max());
  Required after_required = sta_->vertexRequired(reg1_d_vertex,
                                                 MinMax::max());
  Slack after_slack = sta_->worstSlack(MinMax::max());

  // For same-clock paths, insertion shifts both arrival and required
  // by the same amount, so slack should stay the same
  EXPECT_NEAR(after_slack, initial_slack, 0.01f);

  // But arrival should shift by the insertion delay (1ns)
  // The arrival includes the clock insertion on the launch side
  EXPECT_NEAR(after_arrival - initial_arrival, 1.0f, 0.01f);

  // And required should also shift (capture side)
  EXPECT_NEAR(after_required - initial_required, 1.0f, 0.01f);

  // Remove insertion delay
  sta_->removeClockInsertion(clk, nullptr);
  Slack restored_slack = sta_->worstSlack(MinMax::max());
  EXPECT_NEAR(restored_slack, initial_slack, 1e-6);
}

////////////////////////////////////////////////////////////////
// Test 36: Verify timing with drive resistance set on input port.
////////////////////////////////////////////////////////////////

TEST_F(IncrementalTimingTest, DriveResistanceAffectsTiming) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  Slack initial_slack = sta_->worstSlack(MinMax::max());

  // Set a large drive resistance on in1 (slow driver)
  Port *in1_port = network->findPort(network->cell(top), "in1");
  ASSERT_NE(in1_port, nullptr);
  sta_->setDriveResistance(in1_port, RiseFallBoth::riseFall(),
                           MinMaxAll::all(), 1000.0f);

  Slack after_slack = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isnan(after_slack));

  // High drive resistance means slow input transition, degrading timing
  EXPECT_LE(after_slack, initial_slack);

  // Set a very low drive resistance (fast driver)
  sta_->setDriveResistance(in1_port, RiseFallBoth::riseFall(),
                           MinMaxAll::all(), 0.001f);

  Slack fast_slack = sta_->worstSlack(MinMax::max());
  // Fast driver should give better timing
  EXPECT_GE(fast_slack, after_slack);
}

} // namespace sta
