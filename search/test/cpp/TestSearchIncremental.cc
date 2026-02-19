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

} // namespace sta
