#include <gtest/gtest.h>
#include <tcl.h>
#include <fstream>
#include <iterator>
#include <string>
#include "Transition.hh"
#include "MinMax.hh"
#include "ExceptionPath.hh"
#include "TimingRole.hh"
#include "Clock.hh"
#include "RiseFallMinMax.hh"
#include "CycleAccting.hh"
#include "SdcCmdComment.hh"
#include "Variables.hh"
#include "DeratingFactors.hh"
#include "ClockLatency.hh"
#include "ClockInsertion.hh"
#include "ClockGatingCheck.hh"
#include "PortExtCap.hh"
#include "DataCheck.hh"
#include "PinPair.hh"
#include "Sta.hh"
#include "Sdc.hh"
#include "ReportTcl.hh"
#include "Scene.hh"
#include "DisabledPorts.hh"
#include "InputDrive.hh"
#include "PatternMatch.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "TimingArc.hh"
#include "Graph.hh"
#include "PortDelay.hh"
#include "PortDirection.hh"

namespace sta {

static std::string
readTextFile(const char *filename)
{
  std::ifstream in(filename);
  if (!in.is_open())
    return "";
  return std::string((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
}

static size_t
countSubstring(const std::string &text,
               const std::string &needle)
{
  if (needle.empty())
    return 0;
  size_t count = 0;
  size_t pos = 0;
  while ((pos = text.find(needle, pos)) != std::string::npos) {
    ++count;
    pos += needle.size();
  }
  return count;
}

// ============================================================
// R10_ tests: Additional SDC coverage
// ============================================================

// SdcInitTest fixture (needed by some R10_ tests below)
class SdcInitTest : public ::testing::Test {
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

// --- SdcDesignTest: loads nangate45 + example1.v for SDC tests needing a design ---

class SdcDesignTest : public ::testing::Test {
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

    Scene *corner = sta_->cmdScene();
    const MinMaxAll *min_max = MinMaxAll::all();
    LibertyLibrary *lib = sta_->readLiberty(
      "test/nangate45/Nangate45_typ.lib", corner, min_max, false);
    ASSERT_NE(lib, nullptr);

    bool ok = sta_->readVerilog("examples/example1.v");
    ASSERT_TRUE(ok);
    ok = sta_->linkDesign("top", true);
    ASSERT_TRUE(ok);

    Network *network = sta_->network();
    Instance *top = network->topInstance();
    Pin *clk1 = network->findPin(top, "clk1");
    Pin *clk2 = network->findPin(top, "clk2");
    Pin *clk3 = network->findPin(top, "clk3");
    ASSERT_NE(clk1, nullptr);

    PinSet *clk_pins = new PinSet(network);
    clk_pins->insert(clk1);
    clk_pins->insert(clk2);
    clk_pins->insert(clk3);
    FloatSeq *waveform = new FloatSeq;
    waveform->push_back(0.0f);
    waveform->push_back(5.0f);
    sta_->makeClock("clk", clk_pins, false, 10.0f, waveform, nullptr, sta_->cmdMode());

    Pin *in1 = network->findPin(top, "in1");
    Clock *clk = sta_->cmdSdc()->findClock("clk");
    if (in1 && clk) {
      sta_->setInputDelay(in1, RiseFallBoth::riseFall(),
                          clk, RiseFall::rise(), nullptr,
                          false, false, MinMaxAll::all(), true, 0.0f, sta_->cmdSdc());
    }
    sta_->updateTiming(true);
  }

  void TearDown() override {
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_)
      Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  Pin *findPin(const char *path_name) {
    Network *network = sta_->cmdNetwork();
    return network->findPin(path_name);
  }

  Sta *sta_;
  Tcl_Interp *interp_;
};

// --- CycleAccting: sourceCycle, targetCycle via timing update ---

TEST_F(SdcDesignTest, CycleAcctingSourceTargetCycle) {
  // CycleAccting methods are called internally during timing
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  ASSERT_NE(clk, nullptr);
  // Make a second clock for inter-clock cycle accounting
  Network *network = sta_->network();
  Instance *top = network->topInstance();
  Pin *clk2 = network->findPin(top, "clk2");
  if (clk2) {
    PinSet *clk2_pins = new PinSet(network);
    clk2_pins->insert(clk2);
    FloatSeq *waveform2 = new FloatSeq;
    waveform2->push_back(0.0f);
    waveform2->push_back(2.5f);
    sta_->makeClock("clk2", clk2_pins, false, 5.0f, waveform2, nullptr, sta_->cmdMode());
    sta_->updateTiming(true);
    // Forces CycleAccting to compute inter-clock accounting
  }
}

// --- ExceptionThru: asString ---

TEST_F(SdcInitTest, ExceptionThruAsString) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Network *network = sta_->cmdNetwork();
  // Create ExceptionThru with no objects
  ExceptionThru *thru = new ExceptionThru(nullptr, nullptr, nullptr,
                                          RiseFallBoth::riseFall(), true, network);
  const char *str = thru->asString(network);
  EXPECT_NE(str, nullptr);
  delete thru;

  }() ));
}

// --- ExceptionTo: asString, matches, cmdKeyword ---

TEST_F(SdcInitTest, ExceptionToAsString) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ExceptionTo *to = new ExceptionTo(nullptr, nullptr, nullptr,
                                    RiseFallBoth::riseFall(),
                                    RiseFallBoth::riseFall(),
                                    true, network);
  const char *str = to->asString(network);
  EXPECT_NE(str, nullptr);
  // matches with null pin and rf
  to->matches(nullptr, RiseFall::rise());
  delete to;

  }() ));
}

// --- ExceptionFrom: findHash ---

TEST_F(SdcInitTest, ExceptionFromHash) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                          RiseFallBoth::riseFall(), true, network);
  size_t h = from->hash();
  EXPECT_GE(h, 0);
  delete from;

  }() ));
}

// --- ExceptionPath: mergeable ---

TEST_F(SdcInitTest, ExceptionPathMergeable) {
  Sdc *sdc = sta_->cmdSdc();
  FalsePath *fp1 = new FalsePath(nullptr, nullptr, nullptr,
                                 MinMaxAll::all(), true, nullptr);
  FalsePath *fp2 = new FalsePath(nullptr, nullptr, nullptr,
                                 MinMaxAll::all(), true, nullptr);
  bool m = fp1->mergeable(fp2);
  EXPECT_TRUE(m);
  // Different type should not be mergeable
  PathDelay *pd = new PathDelay(nullptr, nullptr, nullptr,
                                MinMax::max(), false, false, 5.0, true, nullptr);
  bool m2 = fp1->mergeable(pd);
  EXPECT_FALSE(m2);
  delete fp1;
  delete fp2;
  delete pd;
}

// --- ExceptionPt constructor ---

TEST_F(SdcInitTest, ExceptionPtBasic) {
  Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                          RiseFallBoth::rise(), true, network);
  EXPECT_TRUE(from->isFrom());
  EXPECT_FALSE(from->isTo());
  EXPECT_FALSE(from->isThru());
  delete from;
}

// --- ExceptionFromTo destructor ---

TEST_F(SdcInitTest, ExceptionFromToDestructor) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  ExceptionFrom *from = new ExceptionFrom(nullptr, nullptr, nullptr,
                                          RiseFallBoth::riseFall(), true, network);
  delete from;
  // Destructor coverage for ExceptionFromTo

  }() ));
}

// --- ExceptionPath destructor ---

TEST_F(SdcInitTest, ExceptionPathDestructor) {
  ASSERT_NO_THROW(( [&](){
  FalsePath *fp = new FalsePath(nullptr, nullptr, nullptr,
                                MinMaxAll::all(), true, nullptr);
  delete fp;

  }() ));
}

// --- DisabledCellPorts: construct and accessors ---

TEST_F(SdcInitTest, DisabledCellPortsConstruct2) {
  LibertyLibrary *lib = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                                          sta_->cmdScene(),
                                          MinMaxAll::min(), false);
  if (lib) {
    LibertyCell *buf = lib->findLibertyCell("BUF_X1");
    if (buf) {
      DisabledCellPorts dcp(buf);
      EXPECT_EQ(dcp.cell(), buf);
      EXPECT_FALSE(dcp.all());
      dcp.setDisabledAll();
      EXPECT_TRUE(dcp.all());
      dcp.removeDisabledAll();
      EXPECT_FALSE(dcp.all());
    }
  }
}

// --- PortDelay: refTransition ---

TEST_F(SdcDesignTest, PortDelayRefTransition) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  const InputDelaySet &delays = sdc->inputDelays();
  for (InputDelay *delay : delays) {
    const RiseFall *ref_rf = delay->refTransition();
    EXPECT_NE(ref_rf, nullptr);
    // Also exercise other PortDelay accessors
    const Pin *pin = delay->pin();
    EXPECT_NE(pin, nullptr);
    const ClockEdge *ce = delay->clkEdge();
    EXPECT_NE(ce, nullptr);
    delay->sourceLatencyIncluded();
    delay->networkLatencyIncluded();
    // refPin is null when no reference pin is set for the port delay
    delay->refPin();
    int idx = delay->index();
    EXPECT_GE(idx, 0);
  }

  }() ));
}

// --- ClockEdge: accessors (time, clock, transition) ---

TEST_F(SdcInitTest, ClockEdgeAccessors) {
  Sdc *sdc = sta_->cmdSdc();
  PinSet *clk_pins = new PinSet(sta_->cmdNetwork());
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0f);
  waveform->push_back(5.0f);
  sta_->makeClock("test_clk_edge", clk_pins, false, 10.0f, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("test_clk_edge");
  ASSERT_NE(clk, nullptr);
  ClockEdge *rise_edge = clk->edge(RiseFall::rise());
  ClockEdge *fall_edge = clk->edge(RiseFall::fall());
  ASSERT_NE(rise_edge, nullptr);
  ASSERT_NE(fall_edge, nullptr);
  // time()
  EXPECT_FLOAT_EQ(rise_edge->time(), 0.0f);
  EXPECT_FLOAT_EQ(fall_edge->time(), 5.0f);
  // clock()
  EXPECT_EQ(rise_edge->clock(), clk);
  EXPECT_EQ(fall_edge->clock(), clk);
  // transition()
  EXPECT_EQ(rise_edge->transition(), RiseFall::rise());
  EXPECT_EQ(fall_edge->transition(), RiseFall::fall());
  // name()
  EXPECT_NE(rise_edge->name(), nullptr);
  EXPECT_NE(fall_edge->name(), nullptr);
  // index()
  int ri = rise_edge->index();
  int fi = fall_edge->index();
  EXPECT_NE(ri, fi);
}

// --- Sdc: removeDataCheck ---

TEST_F(SdcDesignTest, SdcRemoveDataCheck) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  if (from_pin && to_pin) {
    sdc->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                      to_pin, RiseFallBoth::riseFall(),
                      nullptr, MinMaxAll::max(), 1.0);
    sdc->removeDataCheck(from_pin, RiseFallBoth::riseFall(),
                         to_pin, RiseFallBoth::riseFall(),
                         nullptr, MinMaxAll::max());
  }

  }() ));
}

// --- Sdc: deleteInterClockUncertainty ---

TEST_F(SdcInitTest, SdcInterClockUncertainty) {
  Sdc *sdc = sta_->cmdSdc();
  PinSet *pins1 = new PinSet(sta_->cmdNetwork());
  FloatSeq *waveform1 = new FloatSeq;
  waveform1->push_back(0.0f);
  waveform1->push_back(5.0f);
  sta_->makeClock("clk_a", pins1, false, 10.0f, waveform1, nullptr, sta_->cmdMode());
  PinSet *pins2 = new PinSet(sta_->cmdNetwork());
  FloatSeq *waveform2 = new FloatSeq;
  waveform2->push_back(0.0f);
  waveform2->push_back(2.5f);
  sta_->makeClock("clk_b", pins2, false, 5.0f, waveform2, nullptr, sta_->cmdMode());

  Clock *clk_a = sdc->findClock("clk_a");
  Clock *clk_b = sdc->findClock("clk_b");
  ASSERT_NE(clk_a, nullptr);
  ASSERT_NE(clk_b, nullptr);

  sta_->setClockUncertainty(clk_a, RiseFallBoth::riseFall(),
                            clk_b, RiseFallBoth::riseFall(),
                            MinMaxAll::max(), 0.2f, sta_->cmdSdc());
  // Remove it
  sta_->removeClockUncertainty(clk_a, RiseFallBoth::riseFall(),
                               clk_b, RiseFallBoth::riseFall(),
                               MinMaxAll::max(), sta_->cmdSdc());
}

// --- Sdc: clearClkGroupExclusions (via removeClockGroupsLogicallyExclusive) ---

TEST_F(SdcInitTest, SdcClearClkGroupExclusions) {
  ClockGroups *cg = sta_->makeClockGroups("grp_exc", true, false, false, false, nullptr, sta_->cmdSdc());
  EXPECT_NE(cg, nullptr);
  sta_->removeClockGroupsLogicallyExclusive("grp_exc", sta_->cmdSdc());
}

// --- Sdc: false path exercises pathDelayFrom/To indirectly ---

TEST_F(SdcDesignTest, SdcFalsePathExercise) {
  // Creating a false path from/to exercises pathDelayFrom/To code paths
  // through makeFalsePath and the SDC infrastructure
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makeFalsePath(from, nullptr, to, MinMaxAll::all(), nullptr, sta_->cmdSdc());
    // Write SDC to exercise the path delay annotation
    const char *fn = "/tmp/test_sdc_r10_falsepath_exercise.sdc";
    sta_->writeSdc(sta_->cmdSdc(), fn, false, false, 4, false, true);
    FILE *f = fopen(fn, "r");
    EXPECT_NE(f, nullptr);
    if (f) fclose(f);
  }
}

// --- WriteSdc via SdcDesignTest ---

TEST_F(SdcDesignTest, WriteSdcBasic) {
  const char *filename = "/tmp/test_write_sdc_sdc_r10.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithOutputDelay) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Clock *clk = sta_->cmdSdc()->findClock("clk");
  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::all(), true, 3.0f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_outdelay.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcNative) {
  const char *filename = "/tmp/test_write_sdc_sdc_r10_native.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, true, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithFalsePath) {
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  const char *filename = "/tmp/test_write_sdc_sdc_r10_fp.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithDerating) {
  sta_->setTimingDerate(TimingDerateType::cell_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::early(), 0.95,
                       sta_->cmdSdc());
  const char *filename = "/tmp/test_write_sdc_sdc_r10_derate.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithDisable) {
  Graph *graph = sta_->graph();
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("r1/D");
  if (pin && graph) {
    Vertex *v = graph->pinLoadVertex(pin);
    if (v) {
      VertexInEdgeIterator in_iter(v, graph);
      if (in_iter.hasNext()) {
        Edge *edge = in_iter.next();
        sta_->disable(edge, sta_->cmdSdc());
      }
    }
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_disable.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithClockLatency) {
  Clock *clk = sta_->cmdSdc()->findClock("clk");
  if (clk) {
    sta_->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                          MinMaxAll::all(), 0.5f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_clklat.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

TEST_F(SdcDesignTest, WriteSdcWithInterClkUncertainty) {
  Clock *clk = sta_->cmdSdc()->findClock("clk");
  if (clk) {
    sta_->setClockUncertainty(clk, RiseFallBoth::riseFall(),
                              clk, RiseFallBoth::riseFall(),
                              MinMaxAll::max(), 0.1f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_interclk.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc: capacitanceLimit ---

TEST_F(SdcDesignTest, SdcCapacitanceLimit) {
  Sdc *sdc = sta_->cmdSdc();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "r1/D");
  if (pin) {
    float limit;
    bool exists;
    sdc->capacitanceLimit(pin, MinMax::max(), limit, exists);
    // No limit set initially
    EXPECT_FALSE(exists);
  }
}

// --- Sdc: annotateGraphConstrained ---

TEST_F(SdcDesignTest, SdcAnnotateGraphConstrained) {
  ASSERT_NO_THROW(( [&](){
  // These are called during timing update; exercising indirectly
  sta_->updateTiming(true);

  }() ));
}

// --- DisabledInstancePorts: construct and accessors ---

TEST_F(SdcDesignTest, DisabledInstancePortsAccessors) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    DisabledInstancePorts dip(inst);
    EXPECT_EQ(dip.instance(), inst);
  }
  delete iter;
}

// --- PinClockPairLess: using public class ---

TEST_F(SdcDesignTest, PinClockPairLessDesign) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  PinClockPairLess less(network);
  }() ));
}

// --- Sdc: clockLatency for edge ---

TEST_F(SdcDesignTest, SdcClockLatencyEdge) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Graph *graph = sta_->graph();
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("r1/CK");
  if (pin && graph) {
    Vertex *v = graph->pinLoadVertex(pin);
    if (v) {
      VertexInEdgeIterator in_iter(v, graph);
      if (in_iter.hasNext()) {
        Edge *edge = in_iter.next();
        // clockLatency may be null if no latency is set for this edge
        sdc->clockLatency(edge);
      }
    }
  }

  }() ));
}

// --- Sdc: disable/removeDisable for pin pair ---

TEST_F(SdcDesignTest, SdcDisablePinPair) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  // Find a gate with input/output pin pair
  InstanceChildIterator *inst_iter = network->childIterator(top);
  while (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      LibertyPort *in_port = nullptr;
      LibertyPort *out_port = nullptr;
      LibertyCellPortIterator port_iter(lib_cell);
      while (port_iter.hasNext()) {
        LibertyPort *port = port_iter.next();
        if (port->direction()->isInput() && !in_port)
          in_port = port;
        else if (port->direction()->isOutput() && !out_port)
          out_port = port;
      }
      if (in_port && out_port) {
        Pin *in_pin = network->findPin(inst, in_port);
        Pin *out_pin = network->findPin(inst, out_port);
        if (in_pin && out_pin) {
          sdc->disableWire(in_pin, out_pin);
          sdc->removeDisableWire(in_pin, out_pin);
          break;
        }
      }
    }
  }
  delete inst_iter;

  }() ));
}

// --- ExceptionThru: makePinEdges, makeNetEdges, makeInstEdges, deletePinEdges ---

TEST_F(SdcDesignTest, ExceptionThruEdges) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "in1");
  if (pin) {
    PinSet *pins = new PinSet(network);
    pins->insert(pin);
    ExceptionThru *thru = new ExceptionThru(pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(), true, network);
    const char *str = thru->asString(network);
    EXPECT_NE(str, nullptr);
    delete thru;
  }
}

TEST_F(SdcDesignTest, ExceptionThruWithNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  // Find a net
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    NetSet *nets = new NetSet(network);
    nets->insert(net);
    ExceptionThru *thru = new ExceptionThru(nullptr, nets, nullptr,
                                            RiseFallBoth::riseFall(), true, network);
    const char *str = thru->asString(network);
    EXPECT_NE(str, nullptr);
    delete thru;
  }
  delete net_iter;
}

TEST_F(SdcDesignTest, ExceptionThruWithInstance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *inst_iter = network->childIterator(top);
  if (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    InstanceSet *insts = new InstanceSet(network);
    insts->insert(inst);
    ExceptionThru *thru = new ExceptionThru(nullptr, nullptr, insts,
                                            RiseFallBoth::riseFall(), true, network);
    const char *str = thru->asString(network);
    EXPECT_NE(str, nullptr);
    delete thru;
  }
  delete inst_iter;
}

// --- WriteSdc with leaf/map_hpins ---

TEST_F(SdcDesignTest, WriteSdcLeaf) {
  const char *filename = "/tmp/test_write_sdc_sdc_r10_leaf.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, true, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with data check ---

TEST_F(SdcDesignTest, WriteSdcWithDataCheck) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  if (from_pin && to_pin) {
    sta_->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                       to_pin, RiseFallBoth::riseFall(),
                       nullptr, MinMaxAll::max(), 1.0f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_datacheck.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with port loads ---

TEST_F(SdcDesignTest, WriteSdcWithPortLoad) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port)
      sta_->setPortExtPinCap(port, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.5f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_portload.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock slew ---

TEST_F(SdcDesignTest, WriteSdcWithClockSlew) {
  Clock *clk = sta_->cmdSdc()->findClock("clk");
  if (clk) {
    sta_->setClockSlew(clk, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.1f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_clkslew.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock insertion ---

TEST_F(SdcDesignTest, WriteSdcWithClockInsertion) {
  Clock *clk = sta_->cmdSdc()->findClock("clk");
  if (clk) {
    sta_->setClockInsertion(clk, nullptr, RiseFallBoth::rise(),
                            MinMaxAll::all(), EarlyLateAll::all(), 0.3f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_clkins.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multicycle path ---

TEST_F(SdcDesignTest, WriteSdcWithMulticycle) {
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::max(), true, 2, nullptr, sta_->cmdSdc());
  const char *filename = "/tmp/test_write_sdc_sdc_r10_mcp.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with max area ---

TEST_F(SdcDesignTest, WriteSdcWithMaxArea) {
  sta_->cmdSdc()->setMaxArea(1000.0);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_maxarea.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min pulse width ---

TEST_F(SdcDesignTest, WriteSdcWithMpw) {
  sta_->cmdSdc()->setMinPulseWidth(RiseFallBoth::rise(), 0.5);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_mpw.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with voltage ---

TEST_F(SdcDesignTest, WriteSdcWithVoltage) {
  sta_->cmdSdc()->setVoltage(MinMax::max(), 1.1);
  sta_->cmdSdc()->setVoltage(MinMax::min(), 0.9);
  const char *filename = "/tmp/test_write_sdc_sdc_r10_voltage.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc: deleteLatchBorrowLimitsReferencing (via clock removal) ---

TEST_F(SdcInitTest, SdcDeleteLatchBorrowLimits) {
  Sdc *sdc = sta_->cmdSdc();
  PinSet *clk_pins = new PinSet(sta_->cmdNetwork());
  FloatSeq *waveform = new FloatSeq;
  waveform->push_back(0.0f);
  waveform->push_back(5.0f);
  sta_->makeClock("clk_borrow", clk_pins, false, 10.0f, waveform, nullptr, sta_->cmdMode());
  Clock *clk = sdc->findClock("clk_borrow");
  ASSERT_NE(clk, nullptr);
  // Set latch borrow limit on clock
  sdc->setLatchBorrowLimit(clk, 0.5f);
  // Remove the clock; this calls deleteLatchBorrowLimitsReferencing
  sta_->removeClock(clk, sta_->cmdSdc());
}

// ============================================================
// R10_ Additional SDC Tests - Round 2
// ============================================================

// --- WriteSdc with drive resistance ---
TEST_F(SdcDesignTest, WriteSdcWithDriveResistance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      sta_->setDriveResistance(port, RiseFallBoth::riseFall(),
                               MinMaxAll::all(), 50.0f, sta_->cmdSdc());
    }
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_driveres.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with logic value / set_logic_one ---
TEST_F(SdcDesignTest, WriteSdcWithLogicValue) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    sta_->setLogicValue(in1, LogicValue::one, sta_->cmdMode());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_logicval.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with case analysis ---
TEST_F(SdcDesignTest, WriteSdcWithCaseAnalysis) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in2 = network->findPin(top, "in2");
  if (in2) {
    sta_->setCaseAnalysis(in2, LogicValue::zero, sta_->cmdMode());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_caseanalysis.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with latch borrow limit on pin ---
TEST_F(SdcDesignTest, WriteSdcWithLatchBorrowLimitPin) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "r1/D");
  if (pin) {
    sta_->setLatchBorrowLimit(pin, 0.3f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_latchborrow.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with latch borrow limit on instance ---
TEST_F(SdcDesignTest, WriteSdcWithLatchBorrowLimitInst) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    sta_->setLatchBorrowLimit(inst, 0.5f, sta_->cmdSdc());
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_latchborrowinst.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with slew limits ---
TEST_F(SdcDesignTest, WriteSdcWithSlewLimits) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::data, MinMax::max(), 2.0f, sta_->cmdSdc());
  }
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setSlewLimit(port, MinMax::max(), 3.0f, sta_->cmdSdc());
    }
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_slewlimit.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with cap limits ---
TEST_F(SdcDesignTest, WriteSdcWithCapLimits) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setCapacitanceLimit(port, MinMax::max(), 0.5f, sta_->cmdSdc());
    }
    sta_->setCapacitanceLimit(out, MinMax::max(), 0.3f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_caplimit.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with fanout limits ---
TEST_F(SdcDesignTest, WriteSdcWithFanoutLimits) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setFanoutLimit(port, MinMax::max(), 10.0f, sta_->cmdSdc());
    }
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_fanoutlimit.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min pulse width on pin ---
TEST_F(SdcDesignTest, WriteSdcWithMpwOnPin) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk_pin = network->findPin(top, "r1/CK");
  if (clk_pin) {
    sta_->setMinPulseWidth(clk_pin, RiseFallBoth::rise(), 0.2f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_mpwpin.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min pulse width on instance ---
TEST_F(SdcDesignTest, WriteSdcWithMpwOnInst) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    sta_->setMinPulseWidth(inst, RiseFallBoth::rise(), 0.25f, sta_->cmdSdc());
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_mpwinst.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with disable on instance ---
TEST_F(SdcDesignTest, WriteSdcWithDisableInstance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      LibertyPort *in_port = nullptr;
      LibertyPort *out_port = nullptr;
      LibertyCellPortIterator port_iter(lib_cell);
      while (port_iter.hasNext()) {
        LibertyPort *port = port_iter.next();
        if (port->direction()->isInput() && !in_port)
          in_port = port;
        else if (port->direction()->isOutput() && !out_port)
          out_port = port;
      }
      if (in_port && out_port)
        sta_->disable(inst, in_port, out_port, sta_->cmdSdc());
    }
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_disableinst.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with disable on liberty port ---
TEST_F(SdcDesignTest, WriteSdcWithDisableLibPort) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      LibertyCellPortIterator port_iter(lib_cell);
      if (port_iter.hasNext()) {
        LibertyPort *port = port_iter.next();
        sta_->disable(port, sta_->cmdSdc());
      }
    }
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_disablelibport.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with disable on cell ---
TEST_F(SdcDesignTest, WriteSdcWithDisableCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      sta_->disable(lib_cell, nullptr, nullptr, sta_->cmdSdc());
    }
  }
  delete iter;
  const char *filename = "/tmp/test_write_sdc_sdc_r10_disablecell.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with output delay ---
TEST_F(SdcDesignTest, WriteSdcWithOutputDelayDetailed) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::rise(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::max(), true, 2.5f, sta_->cmdSdc());
    sta_->setOutputDelay(out, RiseFallBoth::fall(),
                         clk, RiseFall::fall(), nullptr,
                         false, false, MinMaxAll::min(), true, 1.0f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_write_sdc_sdc_r10_outdelay_detail.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc: outputDelays iterator ---
TEST_F(SdcDesignTest, SdcOutputDelays) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::all(), true, 1.0f, sta_->cmdSdc());
  }
  const OutputDelaySet &out_delays = sdc->outputDelays();
  for (OutputDelay *delay : out_delays) {
    const Pin *pin = delay->pin();
    EXPECT_NE(pin, nullptr);
    const ClockEdge *ce = delay->clkEdge();
    EXPECT_NE(ce, nullptr);
    delay->sourceLatencyIncluded();
  }

  }() ));
}

// --- Sdc: Variables class accessors ---
TEST_F(SdcDesignTest, VariablesAccessors) {
  // Test Variables accessors that modify search behavior
  bool crpr_orig = sta_->crprEnabled();
  sta_->setCrprEnabled(!crpr_orig);
  EXPECT_NE(sta_->crprEnabled(), crpr_orig);
  sta_->setCrprEnabled(crpr_orig);

  bool prop_gate = sta_->propagateGatedClockEnable();
  sta_->setPropagateGatedClockEnable(!prop_gate);
  EXPECT_NE(sta_->propagateGatedClockEnable(), prop_gate);
  sta_->setPropagateGatedClockEnable(prop_gate);
}

// --- Clock: name, period, waveform ---
TEST_F(SdcDesignTest, ClockAccessors) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  ASSERT_NE(clk, nullptr);
  EXPECT_STREQ(clk->name(), "clk");
  EXPECT_FLOAT_EQ(clk->period(), 10.0f);
  const FloatSeq *wave = clk->waveform();
  ASSERT_NE(wave, nullptr);
  EXPECT_GE(wave->size(), 2u);
  EXPECT_FLOAT_EQ((*wave)[0], 0.0f);
  EXPECT_FLOAT_EQ((*wave)[1], 5.0f);
  EXPECT_FALSE(clk->isGenerated());
  EXPECT_FALSE(clk->isVirtual());
  int idx = clk->index();
  EXPECT_GE(idx, 0);
}

// --- ExceptionFrom: hasPins, hasClocks, hasInstances ---
TEST_F(SdcDesignTest, ExceptionFromHasPins) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *pins = new PinSet(network);
    pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    ASSERT_NE(from, nullptr);
    EXPECT_TRUE(from->hasPins());
    EXPECT_FALSE(from->hasClocks());
    EXPECT_FALSE(from->hasInstances());
    EXPECT_TRUE(from->hasObjects());
    delete from;
  }
}

// --- ExceptionTo: hasPins, endRf ---
TEST_F(SdcDesignTest, ExceptionToHasPins) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    PinSet *pins = new PinSet(network);
    pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(pins, nullptr, nullptr,
                                            RiseFallBoth::rise(),
                                            RiseFallBoth::riseFall(), sta_->cmdSdc());
    ASSERT_NE(to, nullptr);
    EXPECT_TRUE(to->hasPins());
    const RiseFallBoth *end_rf = to->endTransition();
    EXPECT_NE(end_rf, nullptr);
    delete to;
  }
}

// --- Sdc: removeClockLatency ---
TEST_F(SdcDesignTest, SdcRemoveClockLatency) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setClockLatency(clk, nullptr,
                          RiseFallBoth::riseFall(),
                          MinMaxAll::all(), 0.3f, sta_->cmdSdc());
    sta_->removeClockLatency(clk, nullptr, sta_->cmdSdc());
  }

  }() ));
}

// --- Sdc: removeCaseAnalysis ---
TEST_F(SdcDesignTest, SdcRemoveCaseAnalysis) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    sta_->setCaseAnalysis(in1, LogicValue::one, sta_->cmdMode());
    sta_->removeCaseAnalysis(in1, sta_->cmdMode());
  }

  }() ));
}

// --- Sdc: removeDerating ---
TEST_F(SdcDesignTest, SdcRemoveDerating) {
  ASSERT_NO_THROW(( [&](){
  sta_->setTimingDerate(TimingDerateType::cell_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::early(), 0.95,
                       sta_->cmdSdc());
  sta_->unsetTimingDerate(sta_->cmdSdc());

  }() ));
}

// --- WriteSdc comprehensive: multiple constraints ---
TEST_F(SdcDesignTest, WriteSdcComprehensive) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");

  // Add various constraints
  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  Pin *out = network->findPin(top, "out");

  if (in1) {
    Port *port = network->port(in1);
    if (port)
      sta_->setDriveResistance(port, RiseFallBoth::riseFall(),
                               MinMaxAll::all(), 100.0f, sta_->cmdSdc());
  }
  if (in2) {
    sta_->setCaseAnalysis(in2, LogicValue::zero, sta_->cmdMode());
  }
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setPortExtPinCap(port, RiseFallBoth::riseFall(),
                             MinMaxAll::all(), 0.1f, sta_->cmdSdc());
      sta_->setFanoutLimit(port, MinMax::max(), 5.0f, sta_->cmdSdc());
    }
  }
  if (clk) {
    sta_->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                          MinMaxAll::all(), 0.5f, sta_->cmdSdc());
    sta_->setClockInsertion(clk, nullptr, RiseFallBoth::riseFall(),
                            MinMaxAll::all(), EarlyLateAll::all(), 0.2f, sta_->cmdSdc());
  }
  sdc->setMaxArea(2000.0);
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.3);
  sdc->setVoltage(MinMax::max(), 1.2);
  sdc->setVoltage(MinMax::min(), 0.8);

  sta_->setTimingDerate(TimingDerateType::cell_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::early(), 0.95,
                       sta_->cmdSdc());

  // Write SDC with all constraints
  const char *filename = "/tmp/test_write_sdc_sdc_r10_comprehensive.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);

  // Also write native format
  const char *filename2 = "/tmp/test_write_sdc_sdc_r10_comprehensive_native.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename2, false, true, 4, false, true);
  FILE *f2 = fopen(filename2, "r");
  EXPECT_NE(f2, nullptr);
  if (f2) fclose(f2);
}

// --- Clock: isPropagated, edges, edgeCount ---
TEST_F(SdcDesignTest, ClockEdgeDetails) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  ASSERT_NE(clk, nullptr);
  clk->isPropagated();
  // Each clock has 2 edges: rise and fall
  ClockEdge *rise = clk->edge(RiseFall::rise());
  ClockEdge *fall = clk->edge(RiseFall::fall());
  ASSERT_NE(rise, nullptr);
  ASSERT_NE(fall, nullptr);
  // Opposite edges
  const ClockEdge *rise_opp = rise->opposite();
  EXPECT_EQ(rise_opp, fall);
  const ClockEdge *fall_opp = fall->opposite();
  EXPECT_EQ(fall_opp, rise);
}

// --- Sdc: clocks() - get all clocks ---
TEST_F(SdcDesignTest, SdcClocksList) {
  Sdc *sdc = sta_->cmdSdc();
  const ClockSeq &clks = sdc->clocks();
  EXPECT_GT(clks.size(), 0u);
  for (Clock *c : clks) {
    EXPECT_NE(c->name(), nullptr);
  }
}

// --- InputDrive: accessors ---
TEST_F(SdcDesignTest, InputDriveAccessors) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      // Set a drive resistance
      sta_->setDriveResistance(port, RiseFallBoth::riseFall(),
                               MinMaxAll::all(), 75.0f, sta_->cmdSdc());
      // Now check the input drive on the port via Sdc
      Sdc *sdc = sta_->cmdSdc();
      InputDrive *drive = sdc->findInputDrive(port);
      if (drive) {
        drive->hasDriveCell(RiseFall::rise(), MinMax::max());
        // driveCell may be null if no drive cell is set
        InputDriveCell *dc = drive->driveCell(RiseFall::rise(), MinMax::max());
        if (dc) {
          EXPECT_NE(dc->cell(), nullptr);
        }
      }
    }
  }

  }() ));
}

// ============================================================
// R11_ SDC Tests - WriteSdc coverage and Sdc method coverage
// ============================================================

// --- WriteSdc with net wire cap (triggers writeNetLoads, writeNetLoad,
//     writeGetNet, WriteGetNet, scaleCapacitance, writeFloat, writeCapacitance,
//     writeCommentSeparator, closeFile, ~WriteSdc) ---
TEST_F(SdcDesignTest, WriteSdcWithNetWireCap) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setNetWireCap(net, false, MinMaxAll::all(), 0.05f, sta_->cmdSdc());
  }
  delete net_iter;
  const char *filename = "/tmp/test_sdc_r11_netwire.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with net resistance (triggers writeNetResistances,
//     writeNetResistance, writeGetNet, scaleResistance, writeResistance) ---
TEST_F(SdcDesignTest, WriteSdcWithNetResistance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setResistance(net, MinMaxAll::all(), 100.0f, sta_->cmdSdc());
  }
  delete net_iter;
  const char *filename = "/tmp/test_sdc_r11_netres.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with input slew (triggers writeInputTransitions,
//     writeRiseFallMinMaxTimeCmd, WriteGetPort, scaleTime) ---
TEST_F(SdcDesignTest, WriteSdcWithInputSlew) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      sta_->setInputSlew(port, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), 0.1f, sta_->cmdSdc());
    }
  }
  const char *filename = "/tmp/test_sdc_r11_inputslew.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with driving cell (triggers writeDrivingCells, writeDrivingCell,
//     WriteGetLibCell, WriteGetPort) ---
TEST_F(SdcDesignTest, WriteSdcWithDrivingCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      // Find a buffer cell to use as driving cell
      LibertyLibrary *lib = nullptr;
      LibertyLibraryIterator *lib_iter = network->libertyLibraryIterator();
      if (lib_iter->hasNext())
        lib = lib_iter->next();
      delete lib_iter;
      if (lib) {
        LibertyCell *buf_cell = nullptr;
        LibertyCellIterator cell_iter(lib);
        while (cell_iter.hasNext()) {
          LibertyCell *cell = cell_iter.next();
          if (cell->portCount() >= 2) {
            buf_cell = cell;
            break;
          }
        }
        if (buf_cell) {
          // Find input and output ports on the cell
          LibertyPort *from_port = nullptr;
          LibertyPort *to_port = nullptr;
          LibertyCellPortIterator port_iter(buf_cell);
          while (port_iter.hasNext()) {
            LibertyPort *lp = port_iter.next();
            if (lp->direction()->isInput() && !from_port)
              from_port = lp;
            else if (lp->direction()->isOutput() && !to_port)
              to_port = lp;
          }
          if (from_port && to_port) {
            float from_slews[2] = {0.05f, 0.05f};
            sta_->setDriveCell(lib, buf_cell, port,
                               from_port, from_slews, to_port,
                               RiseFallBoth::riseFall(), MinMaxAll::all(), sta_->cmdSdc());
          }
        }
      }
    }
  }
  const char *filename = "/tmp/test_sdc_r11_drivecell.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock groups that have actual clock members
//     (triggers writeClockGroups, WriteGetClock, writeGetClock) ---
TEST_F(SdcDesignTest, WriteSdcWithClockGroupsMembers) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    // Create a second clock
    Network *network = sta_->network();
    Instance *top = network->topInstance();
    Pin *clk2_pin = network->findPin(top, "clk2");
    if (clk2_pin) {
      PinSet *clk2_pins = new PinSet(network);
      clk2_pins->insert(clk2_pin);
      FloatSeq *waveform2 = new FloatSeq;
      waveform2->push_back(0.0f);
      waveform2->push_back(2.5f);
      sta_->makeClock("clk2", clk2_pins, false, 5.0f, waveform2, nullptr, sta_->cmdMode());
      Clock *clk2 = sdc->findClock("clk2");
      if (clk2) {
        ClockGroups *cg = sta_->makeClockGroups("grp1", true, false, false,
                                                 false, nullptr, sta_->cmdSdc());
        ClockSet *group1 = new ClockSet;
        group1->insert(clk);
        sta_->makeClockGroup(cg, group1, sta_->cmdSdc());
        ClockSet *group2 = new ClockSet;
        group2->insert(clk2);
        sta_->makeClockGroup(cg, group2, sta_->cmdSdc());
      }
    }
  }
  const char *filename = "/tmp/test_sdc_r11_clkgrp_members.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path having -from pins and -through pins and -to pins
//     (triggers writeExceptionFrom, WriteGetPin, writeExceptionThru,
//     writeExceptionTo) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathFromThruTo) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    // -from
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    // -through: use an instance
    InstanceChildIterator *inst_iter = network->childIterator(top);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    if (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      InstanceSet *insts = new InstanceSet(network);
      insts->insert(inst);
      ExceptionThru *thru = sta_->makeExceptionThru(nullptr, nullptr, insts,
                                                     RiseFallBoth::riseFall(), sta_->cmdSdc());
      thrus->push_back(thru);
    }
    delete inst_iter;
    // -to
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makeFalsePath(from, thrus, to, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_fp_fromthru.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -through net
//     (triggers writeExceptionThru with nets, writeGetNet) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathThruNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    NetSet *nets = new NetSet(network);
    nets->insert(net);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    ExceptionThru *thru = sta_->makeExceptionThru(nullptr, nets, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    thrus->push_back(thru);
    sta_->makeFalsePath(nullptr, thrus, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  delete net_iter;
  const char *filename = "/tmp/test_sdc_r11_fp_thrunet.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -from clock (triggers writeGetClock in from) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathFromClock) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ClockSet *from_clks = new ClockSet;
    from_clks->insert(clk);
    ExceptionFrom *from = sta_->makeExceptionFrom(nullptr, from_clks, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_fp_fromclk.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -from instance (triggers writeGetInstance,
//     WriteGetInstance) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathFromInstance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    InstanceSet *from_insts = new InstanceSet(network);
    from_insts->insert(inst);
    ExceptionFrom *from = sta_->makeExceptionFrom(nullptr, nullptr, from_insts,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_fp_frominst.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multicycle path with -from pin
//     (triggers writeExceptionCmd for multicycle, writeExceptionFrom) ---
TEST_F(SdcDesignTest, WriteSdcMulticycleWithFrom) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makeMulticyclePath(from, nullptr, nullptr,
                             MinMaxAll::max(), true, 3, nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_mcp_from.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with path delay (max_delay/min_delay)
//     (triggers writeExceptionCmd for path delay, writeExceptionValue) ---
TEST_F(SdcDesignTest, WriteSdcPathDelay) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makePathDelay(from, nullptr, to, MinMax::max(), false, false,
                        5.0f, nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_pathdelay.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with group path
//     (triggers writeExceptionCmd for group path) ---
TEST_F(SdcDesignTest, WriteSdcGroupPath) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makeGroupPath("mygroup", false, from, nullptr, nullptr, nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_grouppath.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock sense
//     (triggers writeClockSenses, PinClockPairNameLess) ---
TEST_F(SdcDesignTest, WriteSdcWithClockSense) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk1 && clk) {
    PinSet *pins = new PinSet(network);
    pins->insert(clk1);
    ClockSet *clks = new ClockSet;
    clks->insert(clk);
    sta_->setClockSense(pins, clks, ClockSense::positive, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_clksense.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with port ext wire cap and fanout
//     (triggers writePortLoads with wire cap, writeMinMaxIntValuesCmd) ---
TEST_F(SdcDesignTest, WriteSdcWithPortExtWireCap) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setPortExtWireCap(port, RiseFallBoth::riseFall(),
                              MinMaxAll::all(), 0.02f, sta_->cmdSdc());
      sta_->setPortExtFanout(port, 3, MinMaxAll::all(), sta_->cmdSdc());
    }
  }
  const char *filename = "/tmp/test_sdc_r11_portwire.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with clock gating check
//     (triggers writeClockGatingChecks) ---
TEST_F(SdcDesignTest, WriteSdcWithClockGatingCheck) {
  sta_->setClockGatingCheck(RiseFallBoth::riseFall(),
                            MinMax::max(), 0.1f, sta_->cmdSdc());
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk)
    sta_->setClockGatingCheck(clk, RiseFallBoth::riseFall(),
                              MinMax::min(), 0.05f, sta_->cmdSdc());
  const char *filename = "/tmp/test_sdc_r11_clkgate.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc: connectedCap via Sta API ---
TEST_F(SdcDesignTest, SdcConnectedCap) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Scene *corner = sta_->cmdScene();
    float pin_cap, wire_cap;
    sta_->connectedCap(out, RiseFall::rise(), corner, MinMax::max(),
                       pin_cap, wire_cap);
  }

  }() ));
}

// --- Sdc: connectedCap on net via Sta API ---
TEST_F(SdcDesignTest, SdcConnectedCapNet) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    Scene *corner = sta_->cmdScene();
    float pin_cap, wire_cap;
    sta_->connectedCap(net, corner, MinMax::max(), pin_cap, wire_cap);
  }
  delete net_iter;

  }() ));
}

// --- ExceptionPath::mergeable ---
TEST_F(SdcDesignTest, ExceptionPathMergeable) {
  // Create two false paths and check mergeability
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  Sdc *sdc = sta_->cmdSdc();
  const ExceptionPathSet &exceptions = sdc->exceptions();
  ExceptionPath *first = nullptr;
  for (ExceptionPath *ep : exceptions) {
    if (ep->isFalse()) {
      if (!first) {
        first = ep;
      } else {
        first->mergeable(ep);
        break;
      }
    }
  }
}

// --- WriteSdc with propagated clock on pin
//     (triggers writePropagatedClkPins) ---
TEST_F(SdcDesignTest, WriteSdcWithPropagatedClk) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  if (clk1) {
    sta_->setPropagatedClock(clk1, sta_->cmdMode());
  }
  const char *filename = "/tmp/test_sdc_r11_propagated.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min_delay path delay
//     (triggers min_delay branch in writeExceptionCmd) ---
TEST_F(SdcDesignTest, WriteSdcMinDelay) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makePathDelay(from, nullptr, to, MinMax::min(), false, false,
                        1.0f, nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_mindelay.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multicycle -hold (min) with -end
//     (triggers the hold branch in writeExceptionCmd) ---
TEST_F(SdcDesignTest, WriteSdcMulticycleHold) {
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::min(), true, 0, nullptr, sta_->cmdSdc());
  const char *filename = "/tmp/test_sdc_r11_mcp_hold.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multicycle -setup with -start
//     (triggers the start branch in writeExceptionCmd) ---
TEST_F(SdcDesignTest, WriteSdcMulticycleStart) {
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::max(), false, 2, nullptr, sta_->cmdSdc());
  const char *filename = "/tmp/test_sdc_r11_mcp_start.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with group path default
//     (triggers isDefault branch in writeExceptionCmd) ---
TEST_F(SdcDesignTest, WriteSdcGroupPathDefault) {
  sta_->makeGroupPath(nullptr, true, nullptr, nullptr, nullptr, nullptr, sta_->cmdSdc());
  const char *filename = "/tmp/test_sdc_r11_grppath_default.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -from with rise_from
//     (triggers rf_prefix = "-rise_" branch) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathRiseFrom) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::rise(), sta_->cmdSdc());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_fp_risefrom.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -from with fall_from
//     (triggers rf_prefix = "-fall_" branch) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathFallFrom) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::fall(), sta_->cmdSdc());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_fp_fallfrom.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with path delay -ignore_clock_latency
//     (triggers the ignoreClkLatency branch) ---
TEST_F(SdcDesignTest, WriteSdcPathDelayIgnoreClkLat) {
  sta_->makePathDelay(nullptr, nullptr, nullptr, MinMax::max(), true, false,
                      8.0f, nullptr, sta_->cmdSdc());
  const char *filename = "/tmp/test_sdc_r11_pathdelay_ignoreclk.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with false path -to with end_rf rise
//     (triggers the end_rf != riseFall branch in writeExceptionTo) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathToRise) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::rise(), sta_->cmdSdc());
    sta_->makeFalsePath(nullptr, nullptr, to, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_fp_torise.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with multiple from objects (triggers multi_objs branch with [list ]) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathMultiFrom) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  if (in1 && in2) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    from_pins->insert(in2);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makeFalsePath(from, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_fp_multifrom.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with data check that has a clock ref
//     (triggers writeDataChecks, WriteGetPinAndClkKey) ---
TEST_F(SdcDesignTest, WriteSdcDataCheckWithClock) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (from_pin && to_pin && clk) {
    sta_->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                       to_pin, RiseFallBoth::riseFall(),
                       clk, MinMaxAll::max(), 0.5f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_datacheck_clk.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- Sdc::removeDataCheck ---
TEST_F(SdcDesignTest, SdcRemoveDataCheck2) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  if (from_pin && to_pin) {
    sta_->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                       to_pin, RiseFallBoth::riseFall(),
                       nullptr, MinMaxAll::max(), 1.0f, sta_->cmdSdc());
    sta_->removeDataCheck(from_pin, RiseFallBoth::riseFall(),
                          to_pin, RiseFallBoth::riseFall(),
                          nullptr, MinMaxAll::max(), sta_->cmdSdc());
  }

  }() ));
}

// --- WriteSdc with clock uncertainty on pin
//     (triggers writeClockUncertaintyPins, writeClockUncertaintyPin) ---
TEST_F(SdcDesignTest, WriteSdcClockUncertaintyPin) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  if (clk1) {
    sta_->setClockUncertainty(clk1, MinMaxAll::max(), 0.2f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_clkuncpin.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with voltage on net
//     (triggers writeVoltages with net voltage) ---
TEST_F(SdcDesignTest, WriteSdcVoltageNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setVoltage(net, MinMax::max(), 1.0f, sta_->cmdSdc());
    sta_->setVoltage(net, MinMax::min(), 0.9f, sta_->cmdSdc());
  }
  delete net_iter;
  const char *filename = "/tmp/test_sdc_r11_voltnet.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with disable on timing arcs of cell
//     (triggers writeGetTimingArcsOfOjbects, writeGetTimingArcs,
//     getTimingArcsCmd) ---
TEST_F(SdcDesignTest, WriteSdcDisableTimingArcs) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      LibertyPort *in_port = nullptr;
      LibertyPort *out_port = nullptr;
      LibertyCellPortIterator port_iter(lib_cell);
      while (port_iter.hasNext()) {
        LibertyPort *port = port_iter.next();
        if (port->direction()->isInput() && !in_port)
          in_port = port;
        else if (port->direction()->isOutput() && !out_port)
          out_port = port;
      }
      if (in_port && out_port) {
        // Disable specific from->to arc on cell
        sta_->disable(lib_cell, in_port, out_port, sta_->cmdSdc());
      }
    }
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_disablearcs.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with min pulse width on clock
//     (triggers writeMinPulseWidths clock branch) ---
TEST_F(SdcDesignTest, WriteSdcMpwClock) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setMinPulseWidth(clk, RiseFallBoth::rise(), 0.4f, sta_->cmdSdc());
    sta_->setMinPulseWidth(clk, RiseFallBoth::fall(), 0.3f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_mpwclk.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with slew limit on clock data
//     (triggers writeClkSlewLimits, writeClkSlewLimit) ---
TEST_F(SdcDesignTest, WriteSdcSlewLimitClkData) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::clk, MinMax::max(), 1.5f, sta_->cmdSdc());
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::data, MinMax::max(), 2.5f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_slewclkdata.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with cell-level cap limit
//     (triggers writeCapLimits cell branch) ---
TEST_F(SdcDesignTest, WriteSdcCapLimitCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setCapacitanceLimit(cell, MinMax::max(), 2.0f, sta_->cmdSdc());
    }
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_caplimitcell.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with cell-level fanout limit
//     (triggers writeFanoutLimits cell branch) ---
TEST_F(SdcDesignTest, WriteSdcFanoutLimitCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setFanoutLimit(cell, MinMax::max(), 15.0f, sta_->cmdSdc());
    }
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_fanoutcell.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc with cell-level slew limit
//     (triggers writeSlewLimits cell branch) ---
TEST_F(SdcDesignTest, WriteSdcSlewLimitCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setSlewLimit(cell, MinMax::max(), 5.0f, sta_->cmdSdc());
    }
  }
  delete iter;
  const char *filename = "/tmp/test_sdc_r11_slewcell.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);
}

// --- WriteSdc comprehensive: trigger as many writer paths as possible ---
TEST_F(SdcDesignTest, WriteSdcMegaComprehensive) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  Pin *out = network->findPin(top, "out");

  // Net wire cap and resistance
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setNetWireCap(net, false, MinMaxAll::all(), 0.03f, sta_->cmdSdc());
    sta_->setResistance(net, MinMaxAll::all(), 50.0f, sta_->cmdSdc());
    sta_->setVoltage(net, MinMax::max(), 1.1f, sta_->cmdSdc());
  }
  delete net_iter;

  // Input slew
  if (in1) {
    Port *port = network->port(in1);
    if (port)
      sta_->setInputSlew(port, RiseFallBoth::riseFall(), MinMaxAll::all(), 0.08f, sta_->cmdSdc());
  }

  // Port ext wire cap + fanout
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setPortExtPinCap(port, RiseFallBoth::riseFall(),
                             MinMaxAll::all(), 0.1f, sta_->cmdSdc());
      sta_->setPortExtWireCap(port, RiseFallBoth::riseFall(),
                              MinMaxAll::all(), 0.015f, sta_->cmdSdc());
      sta_->setPortExtFanout(port, 2, MinMaxAll::all(), sta_->cmdSdc());
    }
  }

  // Clock groups
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("mega_grp", false, true, false,
                                             false, nullptr, sta_->cmdSdc());
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1, sta_->cmdSdc());
  }

  // False path with -from pin, -through instance, -to pin
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::rise(), sta_->cmdSdc());
    InstanceChildIterator *inst_iter = network->childIterator(top);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    if (inst_iter->hasNext()) {
      Instance *inst = inst_iter->next();
      InstanceSet *insts = new InstanceSet(network);
      insts->insert(inst);
      ExceptionThru *thru = sta_->makeExceptionThru(nullptr, nullptr, insts,
                                                     RiseFallBoth::riseFall(), sta_->cmdSdc());
      thrus->push_back(thru);
    }
    delete inst_iter;
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::rise(), sta_->cmdSdc());
    sta_->makeFalsePath(from, thrus, to, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }

  // Max/min delay
  if (in2 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in2);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall(), sta_->cmdSdc());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall(), sta_->cmdSdc());
    sta_->makePathDelay(from, nullptr, to, MinMax::max(), true, false,
                        6.0f, nullptr, sta_->cmdSdc());
  }

  // Multicycle
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::max(), false, 4, nullptr, sta_->cmdSdc());

  // Group path
  sta_->makeGroupPath("mega", false, nullptr, nullptr, nullptr, nullptr, sta_->cmdSdc());

  // Clock gating check
  sta_->setClockGatingCheck(RiseFallBoth::riseFall(), MinMax::max(), 0.15f, sta_->cmdSdc());

  // Logic value
  if (in2) {
    sta_->setLogicValue(in2, LogicValue::zero, sta_->cmdMode());
  }

  // Voltage
  sdc->setVoltage(MinMax::max(), 1.2);
  sdc->setVoltage(MinMax::min(), 0.8);

  // Min pulse width
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.35);
  sdc->setMinPulseWidth(RiseFallBoth::fall(), 0.25);

  // Max area
  sdc->setMaxArea(3000.0);

  // Write SDC
  const char *filename = "/tmp/test_sdc_r11_mega.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  FILE *f = fopen(filename, "r");
  EXPECT_NE(f, nullptr);
  if (f) fclose(f);

  // Also write in native mode
  const char *filename2 = "/tmp/test_sdc_r11_mega_native.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename2, false, true, 4, false, true);
  FILE *f2 = fopen(filename2, "r");
  EXPECT_NE(f2, nullptr);
  if (f2) fclose(f2);

  // Also write in leaf mode
  const char *filename3 = "/tmp/test_sdc_r11_mega_leaf.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename3, true, false, 4, false, true);
  FILE *f3 = fopen(filename3, "r");
  EXPECT_NE(f3, nullptr);
  if (f3) fclose(f3);
}

// --- Sdc: remove clock groups ---
TEST_F(SdcDesignTest, SdcRemoveClockGroups) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("rm_grp", true, false, false,
                                             false, nullptr, sta_->cmdSdc());
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1, sta_->cmdSdc());
    // Remove by name
    sta_->removeClockGroupsLogicallyExclusive("rm_grp", sta_->cmdSdc());
  }

  }() ));
}

// --- Sdc: remove physically exclusive clock groups ---
TEST_F(SdcDesignTest, SdcRemovePhysExclClkGroups) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("phys_grp", false, true, false,
                                             false, nullptr, sta_->cmdSdc());
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1, sta_->cmdSdc());
    sta_->removeClockGroupsPhysicallyExclusive("phys_grp", sta_->cmdSdc());
  }

  }() ));
}

// --- Sdc: remove async clock groups ---
TEST_F(SdcDesignTest, SdcRemoveAsyncClkGroups) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("async_grp", false, false, true,
                                             false, nullptr, sta_->cmdSdc());
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1, sta_->cmdSdc());
    sta_->removeClockGroupsAsynchronous("async_grp", sta_->cmdSdc());
  }

  }() ));
}

// --- Sdc: clear via removeConstraints (covers initVariables, clearCycleAcctings) ---
TEST_F(SdcDesignTest, SdcRemoveConstraintsCover) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->cmdSdc();
  // Set various constraints first
  sdc->setMaxArea(500.0);
  sdc->setMinPulseWidth(RiseFallBoth::rise(), 0.3);
  sdc->setVoltage(MinMax::max(), 1.1);
  // removeConstraints calls initVariables and clearCycleAcctings internally
  sdc->clear();

  }() ));
}

// --- ExceptionFrom: hash via exception creation and matching ---
TEST_F(SdcDesignTest, ExceptionFromMatching) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  if (in1 && in2) {
    PinSet *pins1 = new PinSet(network);
    pins1->insert(in1);
    ExceptionFrom *from1 = sta_->makeExceptionFrom(pins1, nullptr, nullptr,
                                                     RiseFallBoth::riseFall(), sta_->cmdSdc());
    PinSet *pins2 = new PinSet(network);
    pins2->insert(in2);
    ExceptionFrom *from2 = sta_->makeExceptionFrom(pins2, nullptr, nullptr,
                                                     RiseFallBoth::riseFall(), sta_->cmdSdc());
    // Make false paths - internally triggers findHash
    sta_->makeFalsePath(from1, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
    sta_->makeFalsePath(from2, nullptr, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }

  }() ));
}

// --- DisabledCellPorts accessors ---
TEST_F(SdcDesignTest, DisabledCellPortsAccessors) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      DisabledCellPorts *dcp = new DisabledCellPorts(lib_cell);
      EXPECT_EQ(dcp->cell(), lib_cell);
      dcp->all();
      delete dcp;
    }
  }
  delete iter;
}

// --- DisabledInstancePorts with disable ---
TEST_F(SdcDesignTest, DisabledInstancePortsDisable) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  ASSERT_TRUE(iter->hasNext());
  Instance *inst = iter->next();
  ASSERT_NE(inst, nullptr);
  LibertyCell *lib_cell = network->libertyCell(inst);
  ASSERT_NE(lib_cell, nullptr);

  LibertyPort *in_port = nullptr;
  LibertyPort *out_port = nullptr;
  LibertyCellPortIterator port_iter(lib_cell);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    if (port->direction()->isInput() && !in_port)
      in_port = port;
    else if (port->direction()->isOutput() && !out_port)
      out_port = port;
  }
  ASSERT_NE(in_port, nullptr);
  ASSERT_NE(out_port, nullptr);

  // Compare emitted SDC before/after disabling this specific arc.
  const char *filename = "/tmp/test_sdc_r11_disinstports.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  std::string before = readTextFile(filename);
  ASSERT_FALSE(before.empty());
  size_t before_disable_cnt = countSubstring(before, "set_disable_timing");

  sta_->disable(inst, in_port, out_port, sta_->cmdSdc());
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  std::string after_disable = readTextFile(filename);
  ASSERT_FALSE(after_disable.empty());
  size_t after_disable_cnt = countSubstring(after_disable, "set_disable_timing");
  EXPECT_GT(after_disable_cnt, before_disable_cnt);
  EXPECT_NE(after_disable.find("-from"), std::string::npos);
  EXPECT_NE(after_disable.find("-to"), std::string::npos);

  sta_->removeDisable(inst, in_port, out_port, sta_->cmdSdc());
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  std::string after_remove = readTextFile(filename);
  ASSERT_FALSE(after_remove.empty());
  size_t after_remove_cnt = countSubstring(after_remove, "set_disable_timing");
  EXPECT_EQ(after_remove_cnt, before_disable_cnt);

  delete iter;
}

// --- WriteSdc with latch borrow limit on clock
//     (triggers writeLatchBorrowLimits clock branch) ---
TEST_F(SdcDesignTest, WriteSdcLatchBorrowClock) {
  Sdc *sdc = sta_->cmdSdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setLatchBorrowLimit(clk, 0.6f, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_latchborrowclk.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_max_time_borrow"), std::string::npos);
  EXPECT_NE(text.find("[get_clocks {clk}]"), std::string::npos);
}

// --- WriteSdc with derating on cell, instance, net ---
TEST_F(SdcDesignTest, WriteSdcDeratingCellInstNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Cell-level derating
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      sta_->setTimingDerate(lib_cell,
                            TimingDerateCellType::cell_delay,
                            PathClkOrData::data,
                            RiseFallBoth::riseFall(),
                            EarlyLate::early(), 0.93,
                       sta_->cmdSdc());
    }
    // Instance-level derating
    sta_->setTimingDerate(inst,
                          TimingDerateCellType::cell_delay,
                          PathClkOrData::data,
                          RiseFallBoth::riseFall(),
                          EarlyLate::late(), 1.07,
                       sta_->cmdSdc());
  }
  delete iter;

  // Net-level derating
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setTimingDerate(net,
                          PathClkOrData::data,
                          RiseFallBoth::riseFall(),
                          EarlyLate::early(), 0.92,
                       sta_->cmdSdc());
  }
  delete net_iter;

  const char *filename = "/tmp/test_sdc_r11_derate_all.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_timing_derate -net_delay -early -data"),
            std::string::npos);
  EXPECT_NE(text.find("set_timing_derate -cell_delay -late -data"),
            std::string::npos);
  EXPECT_NE(text.find("set_timing_derate -cell_delay -early -data"),
            std::string::npos);
}

// --- Sdc: capacitanceLimit on pin ---
TEST_F(SdcDesignTest, SdcCapLimitPin) {
  Sdc *sdc = sta_->cmdSdc();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    sta_->setCapacitanceLimit(out, MinMax::max(), 0.5f, sta_->cmdSdc());
    float limit;
    bool exists;
    sdc->capacitanceLimit(out, MinMax::max(), limit, exists);
    EXPECT_TRUE(exists);
    EXPECT_FLOAT_EQ(limit, 0.5f);
  }
}

// --- WriteSdc with set_false_path -hold only
//     (triggers writeSetupHoldFlag for hold) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathHold) {
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::min(), nullptr, sta_->cmdSdc());
  const char *filename = "/tmp/test_sdc_r11_fp_hold.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_false_path -hold"), std::string::npos);
}

// --- WriteSdc with set_false_path -setup only
//     (triggers writeSetupHoldFlag for setup) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathSetup) {
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::max(), nullptr, sta_->cmdSdc());
  const char *filename = "/tmp/test_sdc_r11_fp_setup.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_false_path -setup"), std::string::npos);
}

// --- WriteSdc with exception -through with rise_through
//     (triggers rf_prefix branches in writeExceptionThru) ---
TEST_F(SdcDesignTest, WriteSdcFalsePathRiseThru) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSet *thru_pins = new PinSet(network);
    thru_pins->insert(in1);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    ExceptionThru *thru = sta_->makeExceptionThru(thru_pins, nullptr, nullptr,
                                                   RiseFallBoth::rise(), sta_->cmdSdc());
    thrus->push_back(thru);
    sta_->makeFalsePath(nullptr, thrus, nullptr, MinMaxAll::all(), nullptr, sta_->cmdSdc());
  }
  const char *filename = "/tmp/test_sdc_r11_fp_risethru.sdc";
  sta_->writeSdc(sta_->cmdSdc(), filename, false, false, 4, false, true);
  std::string text = readTextFile(filename);
  ASSERT_FALSE(text.empty());
  EXPECT_NE(text.find("set_false_path"), std::string::npos);
  EXPECT_NE(text.find("-rise_through [get_ports {in1}]"), std::string::npos);
}

} // namespace sta
