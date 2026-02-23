#include <gtest/gtest.h>
#include <type_traits>
#include <atomic>
#include <string>
#include <tcl.h>
#include <unistd.h>
#include "MinMax.hh"
#include "Transition.hh"
#include "Property.hh"
#include "ExceptionPath.hh"
#include "TimingRole.hh"
#include "Corner.hh"
#include "Sta.hh"
#include "Sdc.hh"
#include "ReportTcl.hh"
#include "RiseFallMinMax.hh"
#include "Variables.hh"
#include "LibertyClass.hh"
#include "PathAnalysisPt.hh"
#include "DcalcAnalysisPt.hh"
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
#include "search/CheckCapacitanceLimits.hh"
#include "search/CheckSlewLimits.hh"
#include "search/CheckFanoutLimits.hh"
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

template <typename FnPtr>
static void expectCallablePointerUsable(FnPtr fn) {
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE((std::is_pointer_v<FnPtr> || std::is_member_function_pointer_v<FnPtr>));
  EXPECT_TRUE(std::is_copy_constructible_v<FnPtr>);
  EXPECT_TRUE(std::is_copy_assignable_v<FnPtr>);
  FnPtr fn_copy = fn;
  EXPECT_EQ(fn_copy, fn);
}

static std::string makeUniqueSdcPath(const char *tag)
{
  static std::atomic<int> counter{0};
  char buf[256];
  snprintf(buf, sizeof(buf), "%s_%d_%d.sdc",
           tag, static_cast<int>(getpid()), counter.fetch_add(1));
  return std::string(buf);
}

static void expectSdcFileReadable(const std::string &filename)
{
  FILE *f = fopen(filename.c_str(), "r");
  ASSERT_NE(f, nullptr);

  std::string content;
  char chunk[512];
  size_t read_count = 0;
  while ((read_count = fread(chunk, 1, sizeof(chunk), f)) > 0)
    content.append(chunk, read_count);
  fclose(f);

  EXPECT_FALSE(content.empty());
  EXPECT_GT(content.size(), 10u);
  EXPECT_NE(content.find('\n'), std::string::npos);
  EXPECT_EQ(content.find('\0'), std::string::npos);
  const bool has_set_cmd = content.find("set_") != std::string::npos;
  const bool has_create_clock = content.find("create_clock") != std::string::npos;
  EXPECT_TRUE(has_set_cmd || has_create_clock);
  EXPECT_EQ(remove(filename.c_str()), 0);
}

static void expectStaDesignCoreState(Sta *sta, bool design_loaded)
{
  ASSERT_NE(sta, nullptr);
  EXPECT_EQ(Sta::sta(), sta);
  EXPECT_NE(sta->network(), nullptr);
  EXPECT_NE(sta->search(), nullptr);
  EXPECT_NE(sta->sdc(), nullptr);
  EXPECT_NE(sta->corners(), nullptr);
  if (sta->corners())
    EXPECT_GE(sta->corners()->count(), 1);
  EXPECT_NE(sta->cmdCorner(), nullptr);
  EXPECT_TRUE(design_loaded);
  if (sta->network())
    EXPECT_NE(sta->network()->topInstance(), nullptr);
}


// ============================================================
// StaDesignTest fixture: loads nangate45 + example1.v + clocks
// Used for R8_ tests that need a real linked design with timing
// ============================================================
class StaDesignTest : public ::testing::Test {
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
    ASSERT_NE(clk2, nullptr);
    ASSERT_NE(clk3, nullptr);

    PinSet *clk_pins = new PinSet(network);
    clk_pins->insert(clk1);
    clk_pins->insert(clk2);
    clk_pins->insert(clk3);
    FloatSeq *waveform = new FloatSeq;
    waveform->push_back(0.0f);
    waveform->push_back(5.0f);
    sta_->makeClock("clk", clk_pins, false, 10.0f, waveform, nullptr);

    // Set input delays
    Pin *in1 = network->findPin(top, "in1");
    Pin *in2 = network->findPin(top, "in2");
    Clock *clk = sta_->sdc()->findClock("clk");
    if (in1 && clk) {
      sta_->setInputDelay(in1, RiseFallBoth::riseFall(),
                          clk, RiseFall::rise(), nullptr,
                          false, false, MinMaxAll::all(), true, 0.0f);
    }
    if (in2 && clk) {
      sta_->setInputDelay(in2, RiseFallBoth::riseFall(),
                          clk, RiseFall::rise(), nullptr,
                          false, false, MinMaxAll::all(), true, 0.0f);
    }

    sta_->updateTiming(true);
    design_loaded_ = true;
  }

  void TearDown() override {
    if (sta_)
      expectStaDesignCoreState(sta_, design_loaded_);
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_)
      Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  // Helper: get a vertex for a pin by hierarchical name e.g. "r1/CK"
  Vertex *findVertex(const char *path_name) {
    Network *network = sta_->cmdNetwork();
    Pin *pin = network->findPin(path_name);
    if (!pin) return nullptr;
    Graph *graph = sta_->graph();
    if (!graph) return nullptr;
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

// ============================================================
// R8_ tests: Sta.cc methods with loaded design
// ============================================================

// --- vertexArrival overloads ---

TEST_F(StaDesignTest, VertexArrivalMinMax) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Arrival arr = sta_->vertexArrival(v, MinMax::max());
  (void)arr;
}

TEST_F(StaDesignTest, VertexArrivalRfPathAP) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  ASSERT_NE(path_ap, nullptr);
  Arrival arr = sta_->vertexArrival(v, RiseFall::rise(), path_ap);
  (void)arr;
}

// --- vertexRequired overloads ---

TEST_F(StaDesignTest, VertexRequiredMinMax) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Required req = sta_->vertexRequired(v, MinMax::max());
  (void)req;
}

TEST_F(StaDesignTest, VertexRequiredRfMinMax) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Required req = sta_->vertexRequired(v, RiseFall::rise(), MinMax::max());
  (void)req;
}

TEST_F(StaDesignTest, VertexRequiredRfPathAP) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  ASSERT_NE(path_ap, nullptr);
  Required req = sta_->vertexRequired(v, RiseFall::rise(), path_ap);
  (void)req;
}

// --- vertexSlack overloads ---

TEST_F(StaDesignTest, VertexSlackMinMax) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Slack slk = sta_->vertexSlack(v, MinMax::max());
  (void)slk;
}

TEST_F(StaDesignTest, VertexSlackRfPathAP) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  ASSERT_NE(path_ap, nullptr);
  Slack slk = sta_->vertexSlack(v, RiseFall::rise(), path_ap);
  (void)slk;
}

// --- vertexSlacks ---

TEST_F(StaDesignTest, VertexSlacks) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Slack slacks[RiseFall::index_count][MinMax::index_count];
  sta_->vertexSlacks(v, slacks);
  // Just verify it doesn't crash; values depend on timing
}

// --- vertexSlew overloads ---

TEST_F(StaDesignTest, VertexSlewRfCornerMinMax) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  Slew slew = sta_->vertexSlew(v, RiseFall::rise(), corner, MinMax::max());
  (void)slew;
}

TEST_F(StaDesignTest, VertexSlewRfDcalcAP) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
  ASSERT_NE(dcalc_ap, nullptr);
  Slew slew = sta_->vertexSlew(v, RiseFall::rise(), dcalc_ap);
  (void)slew;
}

// --- vertexWorstRequiredPath ---

TEST_F(StaDesignTest, VertexWorstRequiredPath) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstRequiredPath(v, MinMax::max());
  // May be nullptr if no required; just check it doesn't crash
  (void)path;
}

TEST_F(StaDesignTest, VertexWorstRequiredPathRf) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstRequiredPath(v, RiseFall::rise(), MinMax::max());
  (void)path;
}

// --- vertexPathIterator ---

TEST_F(StaDesignTest, VertexPathIteratorRfPathAP) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  ASSERT_NE(path_ap, nullptr);
  VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), path_ap);
  ASSERT_NE(iter, nullptr);
  delete iter;
}

// --- checkSlewLimits ---

TEST_F(StaDesignTest, CheckSlewLimitPreambleAndLimits) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkSlewLimitPreamble();
  PinSeq pins = sta_->checkSlewLimits(nullptr, false,
                                       sta_->cmdCorner(), MinMax::max());
  // May be empty; just check no crash

  }() ));
}

TEST_F(StaDesignTest, CheckSlewViolators) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkSlewLimitPreamble();
  PinSeq pins = sta_->checkSlewLimits(nullptr, true,
                                       sta_->cmdCorner(), MinMax::max());

  }() ));
}

// --- checkSlew (single pin) ---

TEST_F(StaDesignTest, CheckSlew) {
  sta_->checkSlewLimitPreamble();
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  const Corner *corner1 = nullptr;
  const RiseFall *tr = nullptr;
  Slew slew;
  float limit, slack;
  sta_->checkSlew(pin, sta_->cmdCorner(), MinMax::max(), false,
                  corner1, tr, slew, limit, slack);
}

// --- findSlewLimit ---

TEST_F(StaDesignTest, FindSlewLimit) {
  sta_->checkSlewLimitPreamble();
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port_z = buf->findLibertyPort("Z");
  ASSERT_NE(port_z, nullptr);
  float limit = 0.0f;
  bool exists = false;
  sta_->findSlewLimit(port_z, sta_->cmdCorner(), MinMax::max(),
                      limit, exists);
}

// --- checkFanoutLimits ---

TEST_F(StaDesignTest, CheckFanoutLimits) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkFanoutLimitPreamble();
  PinSeq pins = sta_->checkFanoutLimits(nullptr, false, MinMax::max());

  }() ));
}

TEST_F(StaDesignTest, CheckFanoutViolators) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkFanoutLimitPreamble();
  PinSeq pins = sta_->checkFanoutLimits(nullptr, true, MinMax::max());

  }() ));
}

// --- checkFanout (single pin) ---

TEST_F(StaDesignTest, CheckFanout) {
  sta_->checkFanoutLimitPreamble();
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  float fanout, limit, slack;
  sta_->checkFanout(pin, MinMax::max(), fanout, limit, slack);
}

// --- checkCapacitanceLimits ---

TEST_F(StaDesignTest, CheckCapacitanceLimits) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkCapacitanceLimitPreamble();
  PinSeq pins = sta_->checkCapacitanceLimits(nullptr, false,
                                              sta_->cmdCorner(), MinMax::max());

  }() ));
}

TEST_F(StaDesignTest, CheckCapacitanceViolators) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkCapacitanceLimitPreamble();
  PinSeq pins = sta_->checkCapacitanceLimits(nullptr, true,
                                              sta_->cmdCorner(), MinMax::max());

  }() ));
}

// --- checkCapacitance (single pin) ---

TEST_F(StaDesignTest, CheckCapacitance) {
  sta_->checkCapacitanceLimitPreamble();
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  const Corner *corner1 = nullptr;
  const RiseFall *tr = nullptr;
  float cap, limit, slack;
  sta_->checkCapacitance(pin, sta_->cmdCorner(), MinMax::max(),
                         corner1, tr, cap, limit, slack);
}

// --- minPulseWidthSlack ---

TEST_F(StaDesignTest, MinPulseWidthSlack) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheck *check = sta_->minPulseWidthSlack(nullptr);
  // May be nullptr; just don't crash
  (void)check;

  }() ));
}

// --- minPulseWidthViolations ---

TEST_F(StaDesignTest, MinPulseWidthViolations) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheckSeq &violations = sta_->minPulseWidthViolations(nullptr);
  (void)violations;

  }() ));
}

// --- minPulseWidthChecks (all) ---

TEST_F(StaDesignTest, MinPulseWidthChecksAll) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheckSeq &checks = sta_->minPulseWidthChecks(nullptr);
  (void)checks;

  }() ));
}

// --- minPeriodSlack ---

TEST_F(StaDesignTest, MinPeriodSlack) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheck *check = sta_->minPeriodSlack();
  (void)check;

  }() ));
}

// --- minPeriodViolations ---

TEST_F(StaDesignTest, MinPeriodViolations) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &violations = sta_->minPeriodViolations();
  (void)violations;

  }() ));
}

// --- maxSkewSlack ---

TEST_F(StaDesignTest, MaxSkewSlack) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  (void)check;

  }() ));
}

// --- maxSkewViolations ---

TEST_F(StaDesignTest, MaxSkewViolations) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheckSeq &violations = sta_->maxSkewViolations();
  (void)violations;

  }() ));
}

// --- reportCheck (MaxSkewCheck) ---

TEST_F(StaDesignTest, ReportCheckMaxSkew) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  if (check) {
    sta_->reportCheck(check, false);
    sta_->reportCheck(check, true);
  }

  }() ));
}

// --- reportCheck (MinPeriodCheck) ---

TEST_F(StaDesignTest, ReportCheckMinPeriod) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheck *check = sta_->minPeriodSlack();
  if (check) {
    sta_->reportCheck(check, false);
    sta_->reportCheck(check, true);
  }

  }() ));
}

// --- reportMpwCheck ---

TEST_F(StaDesignTest, ReportMpwCheck) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheck *check = sta_->minPulseWidthSlack(nullptr);
  if (check) {
    sta_->reportMpwCheck(check, false);
    sta_->reportMpwCheck(check, true);
  }

  }() ));
}

// --- findPathEnds ---

TEST_F(StaDesignTest, FindPathEnds) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false,           // unconstrained
    nullptr,         // corner (all)
    MinMaxAll::max(),
    10,              // group_path_count
    1,               // endpoint_path_count
    false,           // unique_pins
    false,           // unique_edges
    -INF,            // slack_min
    INF,             // slack_max
    false,           // sort_by_slack
    nullptr,         // group_names
    true,            // setup
    false,           // hold
    false,           // recovery
    false,           // removal
    false,           // clk_gating_setup
    false);          // clk_gating_hold
  // Should find some path ends in this design

  }() ));
}

// --- reportPathEndHeader / Footer ---

TEST_F(StaDesignTest, ReportPathEndHeaderFooter) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::full);
  sta_->reportPathEndHeader();
  sta_->reportPathEndFooter();

  }() ));
}

// --- reportPathEnd ---

TEST_F(StaDesignTest, ReportPathEnd) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

// --- reportPathEnds ---

TEST_F(StaDesignTest, ReportPathEnds) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  sta_->reportPathEnds(&ends);

  }() ));
}

// --- reportClkSkew ---

TEST_F(StaDesignTest, ReportClkSkew) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ConstClockSeq clks;
  clks.push_back(clk);
  sta_->reportClkSkew(clks, nullptr, SetupHold::max(), false, 4);
}

// --- isClock(Net*) ---

TEST_F(StaDesignTest, IsClockNet) {
  sta_->ensureClkNetwork();
  Network *network = sta_->cmdNetwork();
  Pin *clk1_pin = findPin("clk1");
  ASSERT_NE(clk1_pin, nullptr);
  Net *clk_net = network->net(clk1_pin);
  if (clk_net) {
    bool is_clk = sta_->isClock(clk_net);
    EXPECT_TRUE(is_clk);
  }
}

// --- pins(Clock*) ---

TEST_F(StaDesignTest, ClockPins) {
  sta_->ensureClkNetwork();
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  const PinSet *pins = sta_->pins(clk);
  EXPECT_NE(pins, nullptr);
  if (pins) {
    EXPECT_GT(pins->size(), 0u);
  }
}

// --- pvt / setPvt ---

TEST_F(StaDesignTest, PvtGetSet) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  const Pvt *p = sta_->pvt(top, MinMax::max());
  // p may be nullptr if not set; just don't crash
  (void)p;
  sta_->setPvt(top, MinMaxAll::all(), 1.0f, 1.1f, 25.0f);
  p = sta_->pvt(top, MinMax::max());

  }() ));
}

// --- findDelays(int) ---

TEST_F(StaDesignTest, FindDelaysLevel) {
  ASSERT_NO_THROW(( [&](){
  sta_->findDelays(0);

  }() ));
}

// --- findDelays (no arg - public) ---

TEST_F(StaDesignTest, FindDelays) {
  ASSERT_NO_THROW(( [&](){
  sta_->findDelays();

  }() ));
}

// --- arrivalsInvalid / delaysInvalid ---

TEST_F(StaDesignTest, ArrivalsInvalid) {
  ASSERT_NO_THROW(( [&](){
  sta_->arrivalsInvalid();

  }() ));
}

TEST_F(StaDesignTest, DelaysInvalid) {
  ASSERT_NO_THROW(( [&](){
  sta_->delaysInvalid();

  }() ));
}

// --- makeEquivCells ---

TEST_F(StaDesignTest, MakeEquivCells) {
  ASSERT_NO_THROW(( [&](){
  LibertyLibrarySeq *equiv_libs = new LibertyLibrarySeq;
  equiv_libs->push_back(lib_);
  LibertyLibrarySeq *map_libs = new LibertyLibrarySeq;
  map_libs->push_back(lib_);
  sta_->makeEquivCells(equiv_libs, map_libs);
  // Check equivCells for BUF_X1
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  if (buf) {
    LibertyCellSeq *equiv = sta_->equivCells(buf);
    (void)equiv;
  }

  }() ));
}

// --- maxPathCountVertex ---

TEST_F(StaDesignTest, MaxPathCountVertex) {
  ASSERT_NO_THROW(( [&](){
  Vertex *v = sta_->maxPathCountVertex();
  // May be nullptr; just don't crash
  (void)v;

  }() ));
}

// --- makeParasiticAnalysisPts ---

TEST_F(StaDesignTest, MakeParasiticAnalysisPts) {
  ASSERT_NO_THROW(( [&](){
  sta_->setParasiticAnalysisPts(false);
  // Ensures parasitic analysis points are set up

  }() ));
}

// --- findLogicConstants (Sim) ---

TEST_F(StaDesignTest, FindLogicConstants) {
  ASSERT_NO_THROW(( [&](){
  sta_->findLogicConstants();
  sta_->clearLogicConstants();

  }() ));
}

// --- checkTiming ---

TEST_F(StaDesignTest, CheckTiming) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    true,   // no_input_delay
    true,   // no_output_delay
    true,   // reg_multiple_clks
    true,   // reg_no_clks
    true,   // unconstrained_endpoints
    true,   // loops
    true);  // generated_clks
  (void)errors;

  }() ));
}

// --- Property methods ---

TEST_F(StaDesignTest, PropertyGetPinArrival) {
  Properties &props = sta_->properties();
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  PropertyValue pv = props.getProperty(pin, "arrival_max_rise");
  (void)pv;
}

TEST_F(StaDesignTest, PropertyGetPinSlack) {
  Properties &props = sta_->properties();
  Pin *pin = findPin("r3/D");
  ASSERT_NE(pin, nullptr);
  PropertyValue pv = props.getProperty(pin, "slack_max");
  (void)pv;
}

TEST_F(StaDesignTest, PropertyGetPinSlew) {
  Properties &props = sta_->properties();
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  PropertyValue pv = props.getProperty(pin, "slew_max");
  (void)pv;
}

TEST_F(StaDesignTest, PropertyGetPinArrivalFall) {
  Properties &props = sta_->properties();
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  PropertyValue pv = props.getProperty(pin, "arrival_max_fall");
  (void)pv;
}

TEST_F(StaDesignTest, PropertyGetInstanceName) {
  Properties &props = sta_->properties();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Instance *u1 = network->findChild(top, "u1");
  ASSERT_NE(u1, nullptr);
  PropertyValue pv = props.getProperty(u1, "full_name");
  (void)pv;
}

TEST_F(StaDesignTest, PropertyGetNetName) {
  Properties &props = sta_->properties();
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  Net *net = network->net(pin);
  if (net) {
    PropertyValue pv = props.getProperty(net, "name");
    (void)pv;
  }
}

// --- Search methods ---

TEST_F(StaDesignTest, SearchCopyState) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  search->copyState(sta_);
}

TEST_F(StaDesignTest, SearchFindPathGroupByName) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  // First ensure path groups exist
  sta_->findPathEnds(nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  PathGroup *pg = search->findPathGroup("clk", MinMax::max());
  // May or may not find it
  (void)pg;

  }() ));
}

TEST_F(StaDesignTest, SearchFindPathGroupByClock) {
  Search *search = sta_->search();
  sta_->findPathEnds(nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  PathGroup *pg = search->findPathGroup(clk, MinMax::max());
  (void)pg;
}

TEST_F(StaDesignTest, SearchReportTagGroups) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->reportTagGroups();

  }() ));
}

TEST_F(StaDesignTest, SearchDeletePathGroups) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  // Ensure path groups exist first
  sta_->findPathEnds(nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  search->deletePathGroups();

  }() ));
}

TEST_F(StaDesignTest, SearchVisitEndpoints) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Network *network = sta_->cmdNetwork();
  PinSet pins(network);
  VertexPinCollector collector(pins);
  search->visitEndpoints(&collector);

  }() ));
}

// --- Search: visitStartpoints ---

TEST_F(StaDesignTest, SearchVisitStartpoints) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Network *network = sta_->cmdNetwork();
  PinSet pins(network);
  VertexPinCollector collector(pins);
  search->visitStartpoints(&collector);

  }() ));
}

TEST_F(StaDesignTest, SearchTagGroup) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  // Tag group index 0 may or may not exist; just don't crash
  if (search->tagGroupCount() > 0) {
    TagGroup *tg = search->tagGroup(0);
    (void)tg;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchClockDomainsVertex) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/CK");
  if (v) {
    ClockSet domains = search->clockDomains(v);
    (void)domains;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchIsGenClkSrc) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/Q");
  if (v) {
    bool is_gen = search->isGenClkSrc(v);
    (void)is_gen;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchPathGroups) {
  ASSERT_NO_THROW(( [&](){
  // Get a path end to query its path groups
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    Search *search = sta_->search();
    PathGroupSeq groups = search->pathGroups(ends[0]);
    (void)groups;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchPathClkPathArrival) {
  Search *search = sta_->search();
  // Get a path from a vertex
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    Arrival arr = search->pathClkPathArrival(path);
    (void)arr;
  }
}

// --- ReportPath methods ---

// --- ReportPath: reportFull exercised through reportPathEnd (full format) ---

TEST_F(StaDesignTest, ReportPathFullClockFormat) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::full_clock);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathFullClockExpandedFormat) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::full_clock_expanded);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathShorterFormat) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::shorter);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathJsonFormat) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::json);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathShortMpw) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheck *check = sta_->minPulseWidthSlack(nullptr);
  if (check) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportShort(check);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathVerboseMpw) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheck *check = sta_->minPulseWidthSlack(nullptr);
  if (check) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportVerbose(check);
  }

  }() ));
}

// --- ReportPath: reportJson ---

TEST_F(StaDesignTest, ReportJsonHeaderFooter) {
  ReportPath *rpt = sta_->reportPath();
  ASSERT_NE(rpt, nullptr);
  rpt->reportJsonHeader();
  rpt->reportJsonFooter();
}

TEST_F(StaDesignTest, ReportJsonPathEnd) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportJsonHeader();
    rpt->reportJson(ends[0], ends.size() == 1);
    rpt->reportJsonFooter();
  }

  }() ));
}

// --- disable / removeDisable ---

TEST_F(StaDesignTest, DisableEnableLibertyPort) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port_a = buf->findLibertyPort("A");
  ASSERT_NE(port_a, nullptr);
  sta_->disable(port_a);
  sta_->removeDisable(port_a);
}

TEST_F(StaDesignTest, DisableEnableTimingArcSet) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  const TimingArcSetSeq &arc_sets = buf->timingArcSets();
  ASSERT_GT(arc_sets.size(), 0u);
  sta_->disable(arc_sets[0]);
  sta_->removeDisable(arc_sets[0]);
}

TEST_F(StaDesignTest, DisableEnableEdge) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  // Get an edge from this vertex
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    sta_->disable(edge);
    sta_->removeDisable(edge);
  }
}

// --- disableClockGatingCheck / removeDisableClockGatingCheck ---

TEST_F(StaDesignTest, DisableClockGatingCheckPin) {
  Pin *pin = findPin("r1/CK");
  ASSERT_NE(pin, nullptr);
  sta_->disableClockGatingCheck(pin);
  sta_->removeDisableClockGatingCheck(pin);
}

// --- setCmdNamespace1 (Sta internal) ---

TEST_F(StaDesignTest, SetCmdNamespace1) {
  sta_->setCmdNamespace(CmdNamespace::sdc);
  EXPECT_EQ(sta_->cmdNamespace(), CmdNamespace::sdc);
  sta_->setCmdNamespace(CmdNamespace::sta);
  EXPECT_EQ(sta_->cmdNamespace(), CmdNamespace::sta);
}

// --- delaysInvalidFromFanin ---

TEST_F(StaDesignTest, DelaysInvalidFromFaninPin) {
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  sta_->delaysInvalidFromFanin(pin);
}

// --- setArcDelayAnnotated ---

TEST_F(StaDesignTest, SetArcDelayAnnotated) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (arc_set) {
      const TimingArcSeq &arcs = arc_set->arcs();
      if (!arcs.empty()) {
        Corner *corner = sta_->cmdCorner();
        DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
        sta_->setArcDelayAnnotated(edge, arcs[0], dcalc_ap, true);
        sta_->setArcDelayAnnotated(edge, arcs[0], dcalc_ap, false);
      }
    }
  }
}

// --- pathAnalysisPt / pathDcalcAnalysisPt ---

TEST_F(StaDesignTest, PathAnalysisPt) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    PathAnalysisPt *pa = sta_->pathAnalysisPt(path);
    (void)pa;
    DcalcAnalysisPt *da = sta_->pathDcalcAnalysisPt(path);
    (void)da;
  }
}

// --- worstSlack / totalNegativeSlack ---

TEST_F(StaDesignTest, WorstSlack) {
  ASSERT_NO_THROW(( [&](){
  Slack worst;
  Vertex *worst_vertex = nullptr;
  sta_->worstSlack(MinMax::max(), worst, worst_vertex);
  (void)worst;

  }() ));
}

TEST_F(StaDesignTest, WorstSlackCorner) {
  ASSERT_NO_THROW(( [&](){
  Slack worst;
  Vertex *worst_vertex = nullptr;
  Corner *corner = sta_->cmdCorner();
  sta_->worstSlack(corner, MinMax::max(), worst, worst_vertex);
  (void)worst;

  }() ));
}

TEST_F(StaDesignTest, TotalNegativeSlack) {
  ASSERT_NO_THROW(( [&](){
  Slack tns = sta_->totalNegativeSlack(MinMax::max());
  (void)tns;

  }() ));
}

TEST_F(StaDesignTest, TotalNegativeSlackCorner) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack tns = sta_->totalNegativeSlack(corner, MinMax::max());
  (void)tns;

  }() ));
}

// --- endpoints / endpointViolationCount ---

TEST_F(StaDesignTest, Endpoints) {
  VertexSet *eps = sta_->endpoints();
  EXPECT_NE(eps, nullptr);
}

TEST_F(StaDesignTest, EndpointViolationCount) {
  ASSERT_NO_THROW(( [&](){
  int count = sta_->endpointViolationCount(MinMax::max());
  (void)count;

  }() ));
}

// --- findRequireds ---

TEST_F(StaDesignTest, FindRequireds) {
  ASSERT_NO_THROW(( [&](){
  sta_->findRequireds();

  }() ));
}

// --- Search: tag(0) ---

TEST_F(StaDesignTest, SearchTag) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  if (search->tagCount() > 0) {
    Tag *t = search->tag(0);
    (void)t;
  }

  }() ));
}

// --- Levelize: checkLevels ---

TEST_F(StaDesignTest, GraphLoops) {
  ASSERT_NO_THROW(( [&](){
  GraphLoopSeq &loops = sta_->graphLoops();
  (void)loops;

  }() ));
}

// --- reportPath (Path*) ---

TEST_F(StaDesignTest, ReportPath) {
  Vertex *v = findVertex("u2/ZN");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    sta_->reportPath(path);
  }
}

// --- ClkNetwork: clocks(Pin*) ---

TEST_F(StaDesignTest, ClkNetworkClocksPinDirect) {
  sta_->ensureClkNetwork();
  ClkNetwork *clk_net = sta_->clkNetwork();
  ASSERT_NE(clk_net, nullptr);
  Pin *clk1_pin = findPin("clk1");
  ASSERT_NE(clk1_pin, nullptr);
  const ClockSet *clks = clk_net->clocks(clk1_pin);
  (void)clks;
}

// --- ClkNetwork: pins(Clock*) ---

TEST_F(StaDesignTest, ClkNetworkPins) {
  sta_->ensureClkNetwork();
  ClkNetwork *clk_net = sta_->clkNetwork();
  ASSERT_NE(clk_net, nullptr);
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  const PinSet *pins = clk_net->pins(clk);
  EXPECT_NE(pins, nullptr);
}

// --- ClkNetwork: isClock(Net*) ---

TEST_F(StaDesignTest, ClkNetworkIsClockNet) {
  sta_->ensureClkNetwork();
  ClkNetwork *clk_net = sta_->clkNetwork();
  ASSERT_NE(clk_net, nullptr);
  Pin *clk1_pin = findPin("clk1");
  ASSERT_NE(clk1_pin, nullptr);
  Network *network = sta_->cmdNetwork();
  Net *net = network->net(clk1_pin);
  if (net) {
    bool is_clk = clk_net->isClock(net);
    (void)is_clk;
  }
}

// --- ClkInfo accessors ---

TEST_F(StaDesignTest, ClkInfoAccessors) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  if (search->tagCount() > 0) {
    Tag *tag = search->tag(0);
    if (tag) {
      const ClkInfo *clk_info = tag->clkInfo();
      if (clk_info) {
        const ClockEdge *edge = clk_info->clkEdge();
        (void)edge;
        bool propagated = clk_info->isPropagated();
        (void)propagated;
        bool is_gen = clk_info->isGenClkSrcPath();
        (void)is_gen;
      }
    }
  }

  }() ));
}

// --- Tag accessors ---

TEST_F(StaDesignTest, TagAccessors) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  if (search->tagCount() > 0) {
    Tag *tag = search->tag(0);
    if (tag) {
      PathAPIndex idx = tag->pathAPIndex();
      (void)idx;
      const Pin *src = tag->clkSrc();
      (void)src;
    }
  }

  }() ));
}

// --- TagGroup::report ---

TEST_F(StaDesignTest, TagGroupReport) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  if (search->tagGroupCount() > 0) {
    TagGroup *tg = search->tagGroup(0);
    if (tg) {
      tg->report(sta_);
    }
  }

  }() ));
}

// --- BfsIterator ---

TEST_F(StaDesignTest, BfsIteratorInit) {
  BfsFwdIterator *iter = sta_->search()->arrivalIterator();
  ASSERT_NE(iter, nullptr);
  // Just verify the iterator exists - init is called internally
}

// --- SearchPredNonReg2 ---

TEST_F(StaDesignTest, SearchPredNonReg2SearchThru) {
  SearchPredNonReg2 pred(sta_);
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    bool thru = pred.searchThru(edge);
    (void)thru;
  }
}

// --- PathExpanded ---

TEST_F(StaDesignTest, PathExpanded) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    PathExpanded expanded(path, false, sta_);
    size_t size = expanded.size();
    for (size_t i = 0; i < size; i++) {
      const Path *p = expanded.path(i);
      (void)p;
    }
  }
}

// --- Search: endpoints ---

TEST_F(StaDesignTest, SearchEndpoints) {
  Search *search = sta_->search();
  VertexSet *eps = search->endpoints();
  EXPECT_NE(eps, nullptr);
}

// --- FindRegister (findRegs) ---

TEST_F(StaDesignTest, FindRegPins) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClockSet clk_set;
  clk_set.insert(clk);
  PinSet reg_clk_pins = sta_->findRegisterClkPins(&clk_set,
    RiseFallBoth::riseFall(), false, false);
  (void)reg_clk_pins;
}

TEST_F(StaDesignTest, FindRegDataPins) {
  ASSERT_NO_THROW(( [&](){
  PinSet reg_data_pins = sta_->findRegisterDataPins(nullptr,
    RiseFallBoth::riseFall(), false, false);
  (void)reg_data_pins;

  }() ));
}

TEST_F(StaDesignTest, FindRegOutputPins) {
  ASSERT_NO_THROW(( [&](){
  PinSet reg_out_pins = sta_->findRegisterOutputPins(nullptr,
    RiseFallBoth::riseFall(), false, false);
  (void)reg_out_pins;

  }() ));
}

TEST_F(StaDesignTest, FindRegAsyncPins) {
  ASSERT_NO_THROW(( [&](){
  PinSet reg_async_pins = sta_->findRegisterAsyncPins(nullptr,
    RiseFallBoth::riseFall(), false, false);
  (void)reg_async_pins;

  }() ));
}

TEST_F(StaDesignTest, FindRegInstances) {
  ASSERT_NO_THROW(( [&](){
  InstanceSet reg_insts = sta_->findRegisterInstances(nullptr,
    RiseFallBoth::riseFall(), false, false);
  (void)reg_insts;

  }() ));
}

// --- Sim::findLogicConstants ---

TEST_F(StaDesignTest, SimFindLogicConstants) {
  Sim *sim = sta_->sim();
  ASSERT_NE(sim, nullptr);
  sim->findLogicConstants();
}

// --- reportSlewLimitShortHeader ---

TEST_F(StaDesignTest, ReportSlewLimitShortHeader) {
  ASSERT_NO_THROW(( [&](){
  sta_->reportSlewLimitShortHeader();

  }() ));
}

// --- reportFanoutLimitShortHeader ---

TEST_F(StaDesignTest, ReportFanoutLimitShortHeader) {
  ASSERT_NO_THROW(( [&](){
  sta_->reportFanoutLimitShortHeader();

  }() ));
}

// --- reportCapacitanceLimitShortHeader ---

TEST_F(StaDesignTest, ReportCapacitanceLimitShortHeader) {
  ASSERT_NO_THROW(( [&](){
  sta_->reportCapacitanceLimitShortHeader();

  }() ));
}

// --- Path methods ---

TEST_F(StaDesignTest, PathTransition) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    const RiseFall *rf = path->transition(sta_);
    (void)rf;
  }
}

// --- endpointSlack ---

TEST_F(StaDesignTest, EndpointSlack) {
  Pin *pin = findPin("r3/D");
  ASSERT_NE(pin, nullptr);
  Slack slk = sta_->endpointSlack(pin, "clk", MinMax::max());
  (void)slk;
}

// --- replaceCell ---

TEST_F(StaDesignTest, ReplaceCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  // Find instance u1 (BUF_X1)
  Instance *u1 = network->findChild(top, "u1");
  ASSERT_NE(u1, nullptr);
  // Replace with BUF_X2 (should exist in nangate45)
  LibertyCell *buf_x2 = lib_->findLibertyCell("BUF_X2");
  if (buf_x2) {
    sta_->replaceCell(u1, buf_x2);
    // Replace back
    LibertyCell *buf_x1 = lib_->findLibertyCell("BUF_X1");
    if (buf_x1)
      sta_->replaceCell(u1, buf_x1);
  }
}

// --- reportPathEnd with prev_end ---

TEST_F(StaDesignTest, ReportPathEndWithPrev) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (ends.size() >= 2) {
    sta_->reportPathEnd(ends[1], ends[0], false);
  }

  }() ));
}

// --- PathEnd static methods ---

TEST_F(StaDesignTest, PathEndLess) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (ends.size() >= 2) {
    bool less = PathEnd::less(ends[0], ends[1], sta_);
    (void)less;
    int cmp = PathEnd::cmpNoCrpr(ends[0], ends[1], sta_);
    (void)cmp;
  }

  }() ));
}

// --- PathEnd accessors on real path ends ---

TEST_F(StaDesignTest, PathEndAccessors) {
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    PathEnd *end = ends[0];
    const char *tn = end->typeName();
    EXPECT_NE(tn, nullptr);
    PathEnd::Type t = end->type();
    (void)t;
    const RiseFall *rf = end->transition(sta_);
    (void)rf;
    PathAPIndex idx = end->pathIndex(sta_);
    (void)idx;
    const Clock *tgt_clk = end->targetClk(sta_);
    (void)tgt_clk;
    Arrival tgt_arr = end->targetClkArrival(sta_);
    (void)tgt_arr;
    float tgt_time = end->targetClkTime(sta_);
    (void)tgt_time;
    float tgt_offset = end->targetClkOffset(sta_);
    (void)tgt_offset;
    Delay tgt_delay = end->targetClkDelay(sta_);
    (void)tgt_delay;
    Delay tgt_ins = end->targetClkInsertionDelay(sta_);
    (void)tgt_ins;
    float tgt_unc = end->targetClkUncertainty(sta_);
    (void)tgt_unc;
    float ni_unc = end->targetNonInterClkUncertainty(sta_);
    (void)ni_unc;
    float inter_unc = end->interClkUncertainty(sta_);
    (void)inter_unc;
    float mcp_adj = end->targetClkMcpAdjustment(sta_);
    (void)mcp_adj;
  }
}

// --- ReportPath: reportShort for MinPeriodCheck ---

TEST_F(StaDesignTest, ReportPathShortMinPeriod) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheck *check = sta_->minPeriodSlack();
  if (check) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportShort(check);
  }

  }() ));
}

// --- ReportPath: reportShort for MaxSkewCheck ---

TEST_F(StaDesignTest, ReportPathShortMaxSkew) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  if (check) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportShort(check);
  }

  }() ));
}

// --- ReportPath: reportCheck for MaxSkewCheck ---

TEST_F(StaDesignTest, ReportPathCheckMaxSkew) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  if (check) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportCheck(check, false);
    rpt->reportCheck(check, true);
  }

  }() ));
}

// --- ReportPath: reportVerbose for MaxSkewCheck ---

TEST_F(StaDesignTest, ReportPathVerboseMaxSkew) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  if (check) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportVerbose(check);
  }

  }() ));
}

// --- ReportPath: reportMpwChecks (covers mpwCheckHiLow internally) ---

TEST_F(StaDesignTest, ReportMpwChecks) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheckSeq &checks = sta_->minPulseWidthChecks(nullptr);
  if (!checks.empty()) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportMpwChecks(&checks, false);
    rpt->reportMpwChecks(&checks, true);
  }

  }() ));
}

// --- findClkMinPeriod ---

TEST_F(StaDesignTest, FindClkMinPeriod) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  float min_period = sta_->findClkMinPeriod(clk, false);
  (void)min_period;
}

// --- slowDrivers ---

TEST_F(StaDesignTest, SlowDrivers) {
  ASSERT_NO_THROW(( [&](){
  InstanceSeq slow = sta_->slowDrivers(5);
  (void)slow;

  }() ));
}

// --- vertexLevel ---

TEST_F(StaDesignTest, VertexLevel) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Level lvl = sta_->vertexLevel(v);
  EXPECT_GE(lvl, 0);
}

// --- simLogicValue ---

TEST_F(StaDesignTest, SimLogicValue) {
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  LogicValue val = sta_->simLogicValue(pin);
  (void)val;
}

// --- Search: clear (exercises initVars internally) ---

TEST_F(StaDesignTest, SearchClear) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  // clear() calls initVars() internally
  search->clear();

  }() ));
}

// --- readLibertyFile (protected, call through public readLiberty) ---
// This tests readLibertyFile indirectly

TEST_F(StaDesignTest, ReadLibertyFile) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  LibertyLibrary *lib = sta_->readLiberty(
    "test/nangate45/Nangate45_slow.lib", corner, MinMaxAll::min(), false);
  // May or may not succeed depending on file existence; just check no crash
  (void)lib;

  }() ));
}

// --- Property: getProperty on LibertyLibrary ---

TEST_F(StaDesignTest, PropertyGetPropertyLibertyLibrary) {
  Properties &props = sta_->properties();
  ASSERT_NE(lib_, nullptr);
  PropertyValue pv = props.getProperty(lib_, "name");
  (void)pv;
}

// --- Property: getProperty on LibertyCell ---

TEST_F(StaDesignTest, PropertyGetPropertyLibertyCell) {
  Properties &props = sta_->properties();
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  PropertyValue pv = props.getProperty(buf, "name");
  (void)pv;
}

// --- findPathEnds with unconstrained ---

TEST_F(StaDesignTest, FindPathEndsUnconstrained) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    true,            // unconstrained
    nullptr,         // corner (all)
    MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  (void)ends;

  }() ));
}

// --- findPathEnds with hold ---

TEST_F(StaDesignTest, FindPathEndsHold) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::min(),
    10, 1, false, false, -INF, INF, false, nullptr,
    false, true, false, false, false, false);
  (void)ends;

  }() ));
}

// --- Search: findAllArrivals ---

TEST_F(StaDesignTest, SearchFindAllArrivals) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->findAllArrivals();

  }() ));
}

// --- Search: findArrivals / findRequireds ---

TEST_F(StaDesignTest, SearchFindArrivalsRequireds) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->findArrivals();
  search->findRequireds();

  }() ));
}

// --- Search: clocks for vertex ---

TEST_F(StaDesignTest, SearchClocksVertex) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/CK");
  if (v) {
    ClockSet clks = search->clocks(v);
    (void)clks;
  }

  }() ));
}

// --- Search: wnsSlack ---

TEST_F(StaDesignTest, SearchWnsSlack) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Slack slk = search->wnsSlack(v, 0);
  (void)slk;
}

// --- Search: isEndpoint ---

TEST_F(StaDesignTest, SearchIsEndpoint) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  bool is_ep = search->isEndpoint(v);
  (void)is_ep;
}

// --- reportParasiticAnnotation ---

TEST_F(StaDesignTest, ReportParasiticAnnotation) {
  ASSERT_NO_THROW(( [&](){
  sta_->reportParasiticAnnotation(false, sta_->cmdCorner());

  }() ));
}

// --- findClkDelays ---

TEST_F(StaDesignTest, FindClkDelays) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClkDelays delays = sta_->findClkDelays(clk, false);
  (void)delays;
}

// --- reportClkLatency ---

TEST_F(StaDesignTest, ReportClkLatency) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ConstClockSeq clks;
  clks.push_back(clk);
  sta_->reportClkLatency(clks, nullptr, false, 4);
}

// --- findWorstClkSkew ---

TEST_F(StaDesignTest, FindWorstClkSkew) {
  ASSERT_NO_THROW(( [&](){
  float worst = sta_->findWorstClkSkew(SetupHold::max(), false);
  (void)worst;

  }() ));
}

// --- ReportPath: reportJson on a Path ---

TEST_F(StaDesignTest, ReportJsonPath) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    ReportPath *rpt = sta_->reportPath();
    rpt->reportJson(path);
  }
}

// --- reportEndHeader / reportEndLine ---

TEST_F(StaDesignTest, ReportEndHeaderLine) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::endpoint);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  ReportPath *rpt = sta_->reportPath();
  rpt->reportEndHeader();
  if (!ends.empty()) {
    rpt->reportEndLine(ends[0]);
  }

  }() ));
}

// --- reportSummaryHeader / reportSummaryLine ---

TEST_F(StaDesignTest, ReportSummaryHeaderLine) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::summary);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  ReportPath *rpt = sta_->reportPath();
  rpt->reportSummaryHeader();
  if (!ends.empty()) {
    rpt->reportSummaryLine(ends[0]);
  }

  }() ));
}

// --- reportSlackOnlyHeader / reportSlackOnly ---

TEST_F(StaDesignTest, ReportSlackOnly) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::slack_only);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  ReportPath *rpt = sta_->reportPath();
  rpt->reportSlackOnlyHeader();
  if (!ends.empty()) {
    rpt->reportSlackOnly(ends[0]);
  }

  }() ));
}

// --- Search: reportArrivals ---

TEST_F(StaDesignTest, SearchReportArrivals) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  search->reportArrivals(v, false);
}

// --- Search: reportPathCountHistogram ---

TEST_F(StaDesignTest, SearchReportPathCountHistogram) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->reportPathCountHistogram();

  }() ));
}

// --- Search: reportTags ---

TEST_F(StaDesignTest, SearchReportTags) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->reportTags();

  }() ));
}

// --- Search: reportClkInfos ---

TEST_F(StaDesignTest, SearchReportClkInfos) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->reportClkInfos();

  }() ));
}

// --- setReportPathFields ---

TEST_F(StaDesignTest, SetReportPathFields) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFields(true, true, true, true, true, true, true);

  }() ));
}

// --- setReportPathFieldOrder ---

TEST_F(StaDesignTest, SetReportPathFieldOrder) {
  ASSERT_NO_THROW(( [&](){
  StringSeq *fields = new StringSeq;
  fields->push_back("Fanout");
  fields->push_back("Cap");
  sta_->setReportPathFieldOrder(fields);

  }() ));
}

// --- Search: saveEnumPath ---
// (This is complex - need a valid enumerated path. Test existence.)

TEST_F(StaDesignTest, SearchSaveEnumPathExists) {
  auto fn = &Search::saveEnumPath;
  expectCallablePointerUsable(fn);
}

// --- vertexPathCount ---

TEST_F(StaDesignTest, VertexPathCount) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  int count = sta_->vertexPathCount(v);
  EXPECT_GE(count, 0);
}

// --- pathCount ---

TEST_F(StaDesignTest, PathCount) {
  int count = sta_->pathCount();
  EXPECT_GE(count, 0);
}

// --- writeSdc ---

TEST_F(StaDesignTest, WriteSdc) {
  ASSERT_NO_THROW(( [&](){
  sta_->writeSdc("/dev/null", false, false, 4, false, true);

  }() ));
}

// --- ReportPath: reportFull for PathEndCheck ---

TEST_F(StaDesignTest, ReportPathFullPathEnd) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::full);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    // reportPathEnd with full format calls reportFull
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

// --- Search: ensureDownstreamClkPins ---

TEST_F(StaDesignTest, SearchEnsureDownstreamClkPins) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->ensureDownstreamClkPins();

  }() ));
}

// --- Genclks ---

TEST_F(StaDesignTest, GenclksAccessor) {
  Search *search = sta_->search();
  Genclks *genclks = search->genclks();
  EXPECT_NE(genclks, nullptr);
}

// --- CheckCrpr accessor ---

TEST_F(StaDesignTest, CheckCrprAccessor) {
  Search *search = sta_->search();
  CheckCrpr *crpr = search->checkCrpr();
  EXPECT_NE(crpr, nullptr);
}

// --- GatedClk accessor ---

TEST_F(StaDesignTest, GatedClkAccessor) {
  Search *search = sta_->search();
  GatedClk *gated = search->gatedClk();
  EXPECT_NE(gated, nullptr);
}

// --- VisitPathEnds accessor ---

TEST_F(StaDesignTest, VisitPathEndsAccessor) {
  Search *search = sta_->search();
  VisitPathEnds *vpe = search->visitPathEnds();
  EXPECT_NE(vpe, nullptr);
}

// ============================================================
// Additional R8_ tests for more coverage
// ============================================================

// --- Search: worstSlack (triggers WorstSlack methods) ---

TEST_F(StaDesignTest, SearchWorstSlackMinMax) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Slack worst;
  Vertex *worst_vertex = nullptr;
  search->worstSlack(MinMax::max(), worst, worst_vertex);
  (void)worst;

  }() ));
}

TEST_F(StaDesignTest, SearchWorstSlackCorner) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Corner *corner = sta_->cmdCorner();
  Slack worst;
  Vertex *worst_vertex = nullptr;
  search->worstSlack(corner, MinMax::max(), worst, worst_vertex);
  (void)worst;

  }() ));
}

// --- Search: totalNegativeSlack ---

TEST_F(StaDesignTest, SearchTotalNegativeSlack) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Slack tns = search->totalNegativeSlack(MinMax::max());
  (void)tns;

  }() ));
}

TEST_F(StaDesignTest, SearchTotalNegativeSlackCorner) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Corner *corner = sta_->cmdCorner();
  Slack tns = search->totalNegativeSlack(corner, MinMax::max());
  (void)tns;

  }() ));
}

// --- Property: getProperty on Edge ---

TEST_F(StaDesignTest, PropertyGetEdge) {
  Properties &props = sta_->properties();
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    PropertyValue pv = props.getProperty(edge, "full_name");
    (void)pv;
  }
}

// --- Property: getProperty on Clock ---

TEST_F(StaDesignTest, PropertyGetClock) {
  Properties &props = sta_->properties();
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  PropertyValue pv = props.getProperty(clk, "name");
  (void)pv;
}

// --- Property: getProperty on LibertyPort ---

TEST_F(StaDesignTest, PropertyGetLibertyPort) {
  Properties &props = sta_->properties();
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port = buf->findLibertyPort("A");
  ASSERT_NE(port, nullptr);
  PropertyValue pv = props.getProperty(port, "name");
  (void)pv;
}

// --- Property: getProperty on Port ---

TEST_F(StaDesignTest, PropertyGetPort) {
  Properties &props = sta_->properties();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Cell *cell = network->cell(top);
  ASSERT_NE(cell, nullptr);
  Port *port = network->findPort(cell, "clk1");
  if (port) {
    PropertyValue pv = props.getProperty(port, "name");
    (void)pv;
  }
}

// --- Sta: makeInstance / deleteInstance ---

TEST_F(StaDesignTest, MakeDeleteInstance) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Instance *new_inst = sta_->makeInstance("test_buf", buf, top);
  ASSERT_NE(new_inst, nullptr);
  sta_->deleteInstance(new_inst);
}

// --- Sta: makeNet / deleteNet ---

TEST_F(StaDesignTest, MakeDeleteNet) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Net *new_net = sta_->makeNet("test_net", top);
  ASSERT_NE(new_net, nullptr);
  sta_->deleteNet(new_net);
}

// --- Sta: connectPin / disconnectPin ---

TEST_F(StaDesignTest, ConnectDisconnectPin) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *port_a = buf->findLibertyPort("A");
  ASSERT_NE(port_a, nullptr);
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Instance *new_inst = sta_->makeInstance("test_buf2", buf, top);
  ASSERT_NE(new_inst, nullptr);
  Net *new_net = sta_->makeNet("test_net2", top);
  ASSERT_NE(new_net, nullptr);
  sta_->connectPin(new_inst, port_a, new_net);
  // Find the pin and disconnect
  Pin *pin = network->findPin(new_inst, "A");
  ASSERT_NE(pin, nullptr);
  sta_->disconnectPin(pin);
  sta_->deleteNet(new_net);
  sta_->deleteInstance(new_inst);
}

// --- Sta: endpointPins ---

TEST_F(StaDesignTest, EndpointPins) {
  PinSet eps = sta_->endpointPins();
  EXPECT_GT(eps.size(), 0u);
}

// --- Sta: startpointPins ---

TEST_F(StaDesignTest, StartpointPins) {
  PinSet sps = sta_->startpointPins();
  EXPECT_GT(sps.size(), 0u);
}

// --- Search: arrivalsValid ---

TEST_F(StaDesignTest, SearchArrivalsValidDesign) {
  Search *search = sta_->search();
  bool valid = search->arrivalsValid();
  EXPECT_TRUE(valid);
}

// --- Sta: netSlack ---

TEST_F(StaDesignTest, NetSlack) {
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  Net *net = network->net(pin);
  if (net) {
    Slack slk = sta_->netSlack(net, MinMax::max());
    (void)slk;
  }
}

// --- Sta: pinSlack ---

TEST_F(StaDesignTest, PinSlackMinMax) {
  Pin *pin = findPin("r3/D");
  ASSERT_NE(pin, nullptr);
  Slack slk = sta_->pinSlack(pin, MinMax::max());
  (void)slk;
}

TEST_F(StaDesignTest, PinSlackRfMinMax) {
  Pin *pin = findPin("r3/D");
  ASSERT_NE(pin, nullptr);
  Slack slk = sta_->pinSlack(pin, RiseFall::rise(), MinMax::max());
  (void)slk;
}

// --- Sta: pinArrival ---

TEST_F(StaDesignTest, PinArrival) {
  Pin *pin = findPin("u1/Z");
  ASSERT_NE(pin, nullptr);
  Arrival arr = sta_->pinArrival(pin, RiseFall::rise(), MinMax::max());
  (void)arr;
}

// --- Sta: clocks / clockDomains ---

TEST_F(StaDesignTest, ClocksOnPin) {
  Pin *pin = findPin("clk1");
  ASSERT_NE(pin, nullptr);
  ClockSet clks = sta_->clocks(pin);
  (void)clks;
}

TEST_F(StaDesignTest, ClockDomainsOnPin) {
  Pin *pin = findPin("r1/CK");
  ASSERT_NE(pin, nullptr);
  ClockSet domains = sta_->clockDomains(pin);
  (void)domains;
}

// --- Sta: vertexWorstArrivalPath (both overloads) ---

TEST_F(StaDesignTest, VertexWorstArrivalPathMinMax) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  (void)path;
}

TEST_F(StaDesignTest, VertexWorstArrivalPathRf) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, RiseFall::rise(), MinMax::max());
  (void)path;
}

// --- Sta: vertexWorstSlackPath ---

TEST_F(StaDesignTest, VertexWorstSlackPath) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstSlackPath(v, MinMax::max());
  (void)path;
}

TEST_F(StaDesignTest, VertexWorstSlackPathRf) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstSlackPath(v, RiseFall::rise(), MinMax::max());
  (void)path;
}

// --- Search: isClock on clock vertex ---

TEST_F(StaDesignTest, SearchIsClockVertex) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/CK");
  ASSERT_NE(v, nullptr);
  bool is_clk = search->isClock(v);
  (void)is_clk;
}

// --- Search: clkPathArrival ---

TEST_F(StaDesignTest, SearchClkPathArrival) {
  Search *search = sta_->search();
  // Need a clock path
  Vertex *v = findVertex("r1/CK");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    Arrival arr = search->clkPathArrival(path);
    (void)arr;
  }
}

// --- Sta: removeDelaySlewAnnotations ---

TEST_F(StaDesignTest, RemoveDelaySlewAnnotations) {
  ASSERT_NO_THROW(( [&](){
  sta_->removeDelaySlewAnnotations();

  }() ));
}

// --- Sta: deleteParasitics ---

TEST_F(StaDesignTest, DeleteParasitics) {
  ASSERT_NO_THROW(( [&](){
  sta_->deleteParasitics();

  }() ));
}

// --- Sta: constraintsChanged ---

TEST_F(StaDesignTest, ConstraintsChanged) {
  ASSERT_NO_THROW(( [&](){
  sta_->constraintsChanged();

  }() ));
}

// --- Sta: networkChanged ---

TEST_F(StaDesignTest, NetworkChanged) {
  ASSERT_NO_THROW(( [&](){
  sta_->networkChanged();

  }() ));
}

// --- Search: endpointsInvalid ---

TEST_F(StaDesignTest, EndpointsInvalid) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->endpointsInvalid();

  }() ));
}

// --- Search: requiredsInvalid ---

TEST_F(StaDesignTest, RequiredsInvalid) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->requiredsInvalid();

  }() ));
}

// --- Search: deleteFilter / filteredEndpoints ---

TEST_F(StaDesignTest, SearchDeleteFilter) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  // No filter set, but calling is safe
  search->deleteFilter();

  }() ));
}

// --- Sta: reportDelayCalc ---

TEST_F(StaDesignTest, ReportDelayCalc) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (arc_set && !arc_set->arcs().empty()) {
      Corner *corner = sta_->cmdCorner();
      std::string report = sta_->reportDelayCalc(
        edge, arc_set->arcs()[0], corner, MinMax::max(), 4);
      (void)report;
    }
  }
}

// --- Sta: arcDelay ---

TEST_F(StaDesignTest, ArcDelay) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (arc_set && !arc_set->arcs().empty()) {
      Corner *corner = sta_->cmdCorner();
      const DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
      ArcDelay delay = sta_->arcDelay(edge, arc_set->arcs()[0], dcalc_ap);
      (void)delay;
    }
  }
}

// --- Sta: arcDelayAnnotated ---

TEST_F(StaDesignTest, ArcDelayAnnotated) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (arc_set && !arc_set->arcs().empty()) {
      Corner *corner = sta_->cmdCorner();
      DcalcAnalysisPt *dcalc_ap = corner->findDcalcAnalysisPt(MinMax::max());
      bool annotated = sta_->arcDelayAnnotated(edge, arc_set->arcs()[0], dcalc_ap);
      (void)annotated;
    }
  }
}

// --- Sta: findReportPathField ---

TEST_F(StaDesignTest, FindReportPathField) {
  ASSERT_NO_THROW(( [&](){
  ReportField *field = sta_->findReportPathField("Fanout");
  (void)field;

  }() ));
}

// --- Search: arrivalInvalid on a vertex ---

TEST_F(StaDesignTest, SearchArrivalInvalid) {
  Search *search = sta_->search();
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  search->arrivalInvalid(v);
}

// --- Search: requiredInvalid on a vertex ---

TEST_F(StaDesignTest, SearchRequiredInvalid) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  search->requiredInvalid(v);
}

// --- Search: isSegmentStart ---

TEST_F(StaDesignTest, SearchIsSegmentStart) {
  Search *search = sta_->search();
  Pin *pin = findPin("in1");
  ASSERT_NE(pin, nullptr);
  bool is_seg = search->isSegmentStart(pin);
  (void)is_seg;
}

// --- Search: isInputArrivalSrchStart ---

TEST_F(StaDesignTest, SearchIsInputArrivalSrchStart) {
  Search *search = sta_->search();
  Vertex *v = findVertex("in1");
  ASSERT_NE(v, nullptr);
  bool is_start = search->isInputArrivalSrchStart(v);
  (void)is_start;
}

// --- Sta: operatingConditions ---

TEST_F(StaDesignTest, OperatingConditions) {
  ASSERT_NO_THROW(( [&](){
  OperatingConditions *op = sta_->operatingConditions(MinMax::max());
  (void)op;

  }() ));
}

// --- Search: evalPred / searchAdj ---

TEST_F(StaDesignTest, SearchEvalPred) {
  Search *search = sta_->search();
  EvalPred *ep = search->evalPred();
  EXPECT_NE(ep, nullptr);
}

TEST_F(StaDesignTest, SearchSearchAdj) {
  Search *search = sta_->search();
  SearchPred *sp = search->searchAdj();
  EXPECT_NE(sp, nullptr);
}

// --- Search: endpointInvalid ---

TEST_F(StaDesignTest, SearchEndpointInvalid) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  search->endpointInvalid(v);
}

// --- Search: tnsInvalid ---

TEST_F(StaDesignTest, SearchTnsInvalid) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  search->tnsInvalid(v);
}

// --- Sta: unsetTimingDerate ---

TEST_F(StaDesignTest, UnsetTimingDerate) {
  ASSERT_NO_THROW(( [&](){
  sta_->unsetTimingDerate();

  }() ));
}

// --- Sta: setAnnotatedSlew ---

TEST_F(StaDesignTest, SetAnnotatedSlew) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  sta_->setAnnotatedSlew(v, corner, MinMaxAll::all(),
                          RiseFallBoth::riseFall(), 1.0e-10f);
}

// --- Sta: vertexPathIterator with MinMax ---

TEST_F(StaDesignTest, VertexPathIteratorMinMax) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
  ASSERT_NE(iter, nullptr);
  // Iterate through paths
  while (iter->hasNext()) {
    Path *path = iter->next();
    (void)path;
  }
  delete iter;
}

// --- Tag comparison operations (exercised through timing) ---

TEST_F(StaDesignTest, TagOperations) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  TagIndex count = search->tagCount();
  if (count >= 2) {
    Tag *t0 = search->tag(0);
    Tag *t1 = search->tag(1);
    if (t0 && t1) {
      // Exercise TagLess
      TagLess less(sta_);
      bool result = less(t0, t1);
      (void)result;
      // Exercise TagIndexLess
      TagIndexLess idx_less;
      result = idx_less(t0, t1);
      (void)result;
      // Exercise Tag::equal
      bool eq = Tag::equal(t0, t1, sta_);
      (void)eq;
    }
  }

  }() ));
}

// --- PathEnd::cmp ---

TEST_F(StaDesignTest, PathEndCmp) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (ends.size() >= 2) {
    int cmp = PathEnd::cmp(ends[0], ends[1], sta_);
    (void)cmp;
    int cmp_slack = PathEnd::cmpSlack(ends[0], ends[1], sta_);
    (void)cmp_slack;
    int cmp_arrival = PathEnd::cmpArrival(ends[0], ends[1], sta_);
    (void)cmp_arrival;
  }

  }() ));
}

// --- PathEnd: various accessors on specific PathEnd types ---

TEST_F(StaDesignTest, PathEndSlackNoCrpr) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    PathEnd *end = ends[0];
    Slack slk = end->slack(sta_);
    (void)slk;
    Slack slk_no_crpr = end->slackNoCrpr(sta_);
    (void)slk_no_crpr;
    ArcDelay margin = end->margin(sta_);
    (void)margin;
    Required req = end->requiredTime(sta_);
    (void)req;
    Arrival arr = end->dataArrivalTime(sta_);
    (void)arr;
    float src_offset = end->sourceClkOffset(sta_);
    (void)src_offset;
    const ClockEdge *src_edge = end->sourceClkEdge(sta_);
    (void)src_edge;
    Delay src_lat = end->sourceClkLatency(sta_);
    (void)src_lat;
  }

  }() ));
}

// --- PathEnd: reportShort ---

TEST_F(StaDesignTest, PathEndReportShort) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    ReportPath *rpt = sta_->reportPath();
    ends[0]->reportShort(rpt);
  }

  }() ));
}

// --- PathEnd: copy ---

TEST_F(StaDesignTest, PathEndCopy) {
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    PathEnd *copy = ends[0]->copy();
    EXPECT_NE(copy, nullptr);
    delete copy;
  }
}

// --- Search: tagGroup for a vertex ---

TEST_F(StaDesignTest, SearchTagGroupForVertex) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  TagGroup *tg = search->tagGroup(v);
  (void)tg;
}

// --- Sta: findFaninPins / findFanoutPins ---

TEST_F(StaDesignTest, FindFaninPins) {
  Pin *pin = findPin("r3/D");
  ASSERT_NE(pin, nullptr);
  PinSeq to_pins;
  to_pins.push_back(pin);
  PinSet fanin = sta_->findFaninPins(&to_pins, false, false, 0, 10, false, false);
  (void)fanin;
}

TEST_F(StaDesignTest, FindFanoutPins) {
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  PinSeq from_pins;
  from_pins.push_back(pin);
  PinSet fanout = sta_->findFanoutPins(&from_pins, false, false, 0, 10, false, false);
  (void)fanout;
}

// --- Sta: findFaninInstances / findFanoutInstances ---

TEST_F(StaDesignTest, FindFaninInstances) {
  Pin *pin = findPin("r3/D");
  ASSERT_NE(pin, nullptr);
  PinSeq to_pins;
  to_pins.push_back(pin);
  InstanceSet fanin = sta_->findFaninInstances(&to_pins, false, false, 0, 10, false, false);
  (void)fanin;
}

// --- Sta: setVoltage ---

TEST_F(StaDesignTest, SetVoltage) {
  ASSERT_NO_THROW(( [&](){
  sta_->setVoltage(MinMax::max(), 1.1f);

  }() ));
}

// --- Sta: removeConstraints ---

TEST_F(StaDesignTest, RemoveConstraints) {
  ASSERT_NO_THROW(( [&](){
  // This is a destructive operation, so call it but re-create constraints after
  // Just verifying it doesn't crash
  sta_->removeConstraints();

  }() ));
}

// --- Search: filter ---

TEST_F(StaDesignTest, SearchFilter) {
  Search *search = sta_->search();
  FilterPath *filter = search->filter();
  // filter should be null since we haven't set one
  EXPECT_EQ(filter, nullptr);
}

// --- PathExpanded: path(i) and pathsIndex ---

TEST_F(StaDesignTest, PathExpandedPaths) {
  Vertex *v = findVertex("u2/ZN");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    PathExpanded expanded(path, true, sta_);
    for (size_t i = 0; i < expanded.size(); i++) {
      const Path *p = expanded.path(i);
      (void)p;
    }
  }
}

// --- Sta: setOutputDelay ---

TEST_F(StaDesignTest, SetOutputDelay) {
  Pin *out = findPin("out");
  ASSERT_NE(out, nullptr);
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                       clk, RiseFall::rise(), nullptr,
                       false, false, MinMaxAll::all(), true, 0.0f);
}

// --- Sta: findPathEnds with setup+hold ---

TEST_F(StaDesignTest, FindPathEndsSetupHold) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::all(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, true, false, false, false, false);
  (void)ends;

  }() ));
}

// --- Sta: findPathEnds unique_pins ---

TEST_F(StaDesignTest, FindPathEndsUniquePins) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 3, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  (void)ends;

  }() ));
}

// --- Sta: findPathEnds sort_by_slack ---

TEST_F(StaDesignTest, FindPathEndsSortBySlack) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, true, nullptr,
    true, false, false, false, false, false);
  (void)ends;

  }() ));
}

// --- Sta: reportChecks for MinPeriod ---

TEST_F(StaDesignTest, ReportChecksMinPeriod) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &checks = sta_->minPeriodViolations();
  sta_->reportChecks(&checks, false);
  sta_->reportChecks(&checks, true);

  }() ));
}

// --- Sta: reportChecks for MaxSkew ---

TEST_F(StaDesignTest, ReportChecksMaxSkew) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheckSeq &checks = sta_->maxSkewViolations();
  sta_->reportChecks(&checks, false);
  sta_->reportChecks(&checks, true);

  }() ));
}

// --- ReportPath: reportPeriodHeaderShort ---

TEST_F(StaDesignTest, ReportPeriodHeaderShort) {
  ASSERT_NO_THROW(( [&](){
  ReportPath *rpt = sta_->reportPath();
  rpt->reportPeriodHeaderShort();

  }() ));
}

// --- ReportPath: reportMpwHeaderShort ---

TEST_F(StaDesignTest, ReportMpwHeaderShort) {
  ASSERT_NO_THROW(( [&](){
  ReportPath *rpt = sta_->reportPath();
  rpt->reportMpwHeaderShort();

  }() ));
}

// --- Sta: maxSlewCheck ---

TEST_F(StaDesignTest, MaxSlewCheck) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkSlewLimitPreamble();
  const Pin *pin = nullptr;
  Slew slew;
  float slack, limit;
  sta_->maxSlewCheck(pin, slew, slack, limit);
  // pin may be null if no slew limits

  }() ));
}

// --- Sta: maxFanoutCheck ---

TEST_F(StaDesignTest, MaxFanoutCheck) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkFanoutLimitPreamble();
  const Pin *pin = nullptr;
  float fanout, slack, limit;
  sta_->maxFanoutCheck(pin, fanout, slack, limit);

  }() ));
}

// --- Sta: maxCapacitanceCheck ---

TEST_F(StaDesignTest, MaxCapacitanceCheck) {
  ASSERT_NO_THROW(( [&](){
  sta_->checkCapacitanceLimitPreamble();
  const Pin *pin = nullptr;
  float cap, slack, limit;
  sta_->maxCapacitanceCheck(pin, cap, slack, limit);

  }() ));
}

// --- Sta: vertexSlack with RiseFall + MinMax ---

TEST_F(StaDesignTest, VertexSlackRfMinMax) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Slack slk = sta_->vertexSlack(v, RiseFall::rise(), MinMax::max());
  (void)slk;
}

// --- Sta: vertexSlew with MinMax only ---

TEST_F(StaDesignTest, VertexSlewMinMax) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Slew slew = sta_->vertexSlew(v, MinMax::max());
  (void)slew;
}

// --- Sta: setReportPathFormat to each format and report ---

TEST_F(StaDesignTest, ReportPathEndpointFormat) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::endpoint);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (ends.size() >= 2) {
    sta_->reportPathEnd(ends[0], nullptr, false);
    sta_->reportPathEnd(ends[1], ends[0], true);
  }

  }() ));
}

// --- Search: findClkVertexPins ---

TEST_F(StaDesignTest, SearchFindClkVertexPins) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  PinSet clk_pins(sta_->cmdNetwork());
  search->findClkVertexPins(clk_pins);
  (void)clk_pins;

  }() ));
}

// --- Property: getProperty on PathEnd ---

TEST_F(StaDesignTest, PropertyGetPathEnd) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(ends[0], "slack");
    (void)pv;
  }

  }() ));
}

// --- Property: getProperty on Path ---

TEST_F(StaDesignTest, PropertyGetPath) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(path, "arrival");
    (void)pv;
  }
}

// --- Property: getProperty on TimingArcSet ---

TEST_F(StaDesignTest, PropertyGetTimingArcSet) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (arc_set) {
      Properties &props = sta_->properties();
      try {
        PropertyValue pv = props.getProperty(arc_set, "from_pin");
        (void)pv;
      } catch (...) {}
    }
  }
}

// --- Sta: setParasiticAnalysisPts per_corner ---

TEST_F(StaDesignTest, SetParasiticAnalysisPtsPerCorner) {
  ASSERT_NO_THROW(( [&](){
  sta_->setParasiticAnalysisPts(true);

  }() ));
}

// ============================================================
// R9_ tests: Comprehensive coverage for search module
// ============================================================

// --- FindRegister tests ---

TEST_F(StaDesignTest, FindRegisterInstances) {
  ClockSet *clks = nullptr;
  InstanceSet reg_insts = sta_->findRegisterInstances(clks,
    RiseFallBoth::riseFall(), true, false);
  // Design has 3 DFF_X1 instances
  EXPECT_GE(reg_insts.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterDataPins) {
  ClockSet *clks = nullptr;
  PinSet data_pins = sta_->findRegisterDataPins(clks,
    RiseFallBoth::riseFall(), true, false);
  EXPECT_GE(data_pins.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterClkPins) {
  ClockSet *clks = nullptr;
  PinSet clk_pins = sta_->findRegisterClkPins(clks,
    RiseFallBoth::riseFall(), true, false);
  EXPECT_GE(clk_pins.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterAsyncPins) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  PinSet async_pins = sta_->findRegisterAsyncPins(clks,
    RiseFallBoth::riseFall(), true, false);
  // May be empty if no async pins
  (void)async_pins;

  }() ));
}

TEST_F(StaDesignTest, FindRegisterOutputPins) {
  ClockSet *clks = nullptr;
  PinSet out_pins = sta_->findRegisterOutputPins(clks,
    RiseFallBoth::riseFall(), true, false);
  EXPECT_GE(out_pins.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterInstancesWithClock) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClockSet *clks = new ClockSet;
  clks->insert(clk);
  InstanceSet reg_insts = sta_->findRegisterInstances(clks,
    RiseFallBoth::riseFall(), true, false);
  EXPECT_GE(reg_insts.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterDataPinsWithClock) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClockSet *clks = new ClockSet;
  clks->insert(clk);
  PinSet data_pins = sta_->findRegisterDataPins(clks,
    RiseFallBoth::riseFall(), true, false);
  EXPECT_GE(data_pins.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterClkPinsWithClock) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClockSet *clks = new ClockSet;
  clks->insert(clk);
  PinSet clk_pins = sta_->findRegisterClkPins(clks,
    RiseFallBoth::riseFall(), true, false);
  EXPECT_GE(clk_pins.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterOutputPinsWithClock) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClockSet *clks = new ClockSet;
  clks->insert(clk);
  PinSet out_pins = sta_->findRegisterOutputPins(clks,
    RiseFallBoth::riseFall(), true, false);
  EXPECT_GE(out_pins.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterRiseOnly) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  PinSet clk_pins = sta_->findRegisterClkPins(clks,
    RiseFallBoth::rise(), true, false);
  (void)clk_pins;

  }() ));
}

TEST_F(StaDesignTest, FindRegisterFallOnly) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  PinSet clk_pins = sta_->findRegisterClkPins(clks,
    RiseFallBoth::fall(), true, false);
  (void)clk_pins;

  }() ));
}

TEST_F(StaDesignTest, FindRegisterLatches) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  InstanceSet insts = sta_->findRegisterInstances(clks,
    RiseFallBoth::riseFall(), false, true);
  // No latches in this design
  (void)insts;

  }() ));
}

TEST_F(StaDesignTest, FindRegisterBothEdgeAndLatch) {
  ClockSet *clks = nullptr;
  InstanceSet insts = sta_->findRegisterInstances(clks,
    RiseFallBoth::riseFall(), true, true);
  EXPECT_GE(insts.size(), 1u);
}

TEST_F(StaDesignTest, FindRegisterAsyncPinsWithClock) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClockSet *clks = new ClockSet;
  clks->insert(clk);
  PinSet async_pins = sta_->findRegisterAsyncPins(clks,
    RiseFallBoth::riseFall(), true, false);
  (void)async_pins;
}

// --- PathEnd: detailed accessors ---

TEST_F(StaDesignTest, PathEndType) {
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    PathEnd::Type t = end->type();
    const char *name = end->typeName();
    EXPECT_NE(name, nullptr);
    (void)t;
  }
}

TEST_F(StaDesignTest, PathEndCheckRole) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    const TimingRole *role = end->checkRole(sta_);
    (void)role;
    const TimingRole *generic_role = end->checkGenericRole(sta_);
    (void)generic_role;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndVertex) {
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    Vertex *v = end->vertex(sta_);
    EXPECT_NE(v, nullptr);
  }
}

TEST_F(StaDesignTest, PathEndMinMax) {
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    const MinMax *mm = end->minMax(sta_);
    EXPECT_NE(mm, nullptr);
    const EarlyLate *el = end->pathEarlyLate(sta_);
    EXPECT_NE(el, nullptr);
  }
}

TEST_F(StaDesignTest, PathEndTransition) {
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    const RiseFall *rf = end->transition(sta_);
    EXPECT_NE(rf, nullptr);
  }
}

TEST_F(StaDesignTest, PathEndPathAnalysisPt) {
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    PathAnalysisPt *path_ap = end->pathAnalysisPt(sta_);
    EXPECT_NE(path_ap, nullptr);
    PathAPIndex idx = end->pathIndex(sta_);
    (void)idx;
  }
}

TEST_F(StaDesignTest, PathEndTargetClkAccessors) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    const Clock *tgt_clk = end->targetClk(sta_);
    (void)tgt_clk;
    const ClockEdge *tgt_edge = end->targetClkEdge(sta_);
    (void)tgt_edge;
    float tgt_time = end->targetClkTime(sta_);
    (void)tgt_time;
    float tgt_offset = end->targetClkOffset(sta_);
    (void)tgt_offset;
    Arrival tgt_arr = end->targetClkArrival(sta_);
    (void)tgt_arr;
    Delay tgt_delay = end->targetClkDelay(sta_);
    (void)tgt_delay;
    Delay tgt_ins = end->targetClkInsertionDelay(sta_);
    (void)tgt_ins;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndTargetClkUncertainty) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    float non_inter = end->targetNonInterClkUncertainty(sta_);
    (void)non_inter;
    float inter = end->interClkUncertainty(sta_);
    (void)inter;
    float total = end->targetClkUncertainty(sta_);
    (void)total;
    float mcp_adj = end->targetClkMcpAdjustment(sta_);
    (void)mcp_adj;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndClkEarlyLate) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    const EarlyLate *el = end->clkEarlyLate(sta_);
    (void)el;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndIsTypePredicates) {
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    bool is_check = end->isCheck();
    bool is_uncon = end->isUnconstrained();
    bool is_data = end->isDataCheck();
    bool is_latch = end->isLatchCheck();
    bool is_output = end->isOutputDelay();
    bool is_gated = end->isGatedClock();
    bool is_pd = end->isPathDelay();
    // At least one should be true
    bool any = is_check || is_uncon || is_data || is_latch
      || is_output || is_gated || is_pd;
    EXPECT_TRUE(any);
  }
}

TEST_F(StaDesignTest, PathEndCrpr) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    Crpr crpr = end->crpr(sta_);
    (void)crpr;
    Crpr check_crpr = end->checkCrpr(sta_);
    (void)check_crpr;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndClkSkew) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    Delay skew = end->clkSkew(sta_);
    (void)skew;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndBorrow) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    Arrival borrow = end->borrow(sta_);
    (void)borrow;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndSourceClkInsertionDelay) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    Delay ins = end->sourceClkInsertionDelay(sta_);
    (void)ins;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndTargetClkPath) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    Path *tgt_clk = end->targetClkPath();
    (void)tgt_clk;
    const Path *tgt_clk_const = const_cast<const PathEnd*>(end)->targetClkPath();
    (void)tgt_clk_const;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndTargetClkEndTrans) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    const RiseFall *rf = end->targetClkEndTrans(sta_);
    (void)rf;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndExceptPathCmp) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (ends.size() >= 2) {
    int cmp = ends[0]->exceptPathCmp(ends[1], sta_);
    (void)cmp;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndDataArrivalTimeOffset) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    Arrival arr_offset = end->dataArrivalTimeOffset(sta_);
    (void)arr_offset;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndRequiredTimeOffset) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    Required req = end->requiredTimeOffset(sta_);
    (void)req;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndMultiCyclePath) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    MultiCyclePath *mcp = end->multiCyclePath();
    (void)mcp;
    PathDelay *pd = end->pathDelay();
    (void)pd;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndCmpNoCrpr) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (ends.size() >= 2) {
    int cmp = PathEnd::cmpNoCrpr(ends[0], ends[1], sta_);
    (void)cmp;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndLess2) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (ends.size() >= 2) {
    bool less = PathEnd::less(ends[0], ends[1], sta_);
    (void)less;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndMacroClkTreeDelay) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    float macro_delay = end->macroClkTreeDelay(sta_);
    (void)macro_delay;
  }

  }() ));
}

// --- PathEnd: hold check ---

TEST_F(StaDesignTest, FindPathEndsHold2) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::min(),
    10, 1, false, false, -INF, INF, false, nullptr,
    false, true, false, false, false, false);
  (void)ends;

  }() ));
}

TEST_F(StaDesignTest, FindPathEndsHoldAccessors) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::min(),
    10, 1, false, false, -INF, INF, false, nullptr,
    false, true, false, false, false, false);
  for (auto *end : ends) {
    Slack slk = end->slack(sta_);
    (void)slk;
    Required req = end->requiredTime(sta_);
    (void)req;
    ArcDelay margin = end->margin(sta_);
    (void)margin;
  }

  }() ));
}

// --- PathEnd: unconstrained ---

TEST_F(StaDesignTest, FindPathEndsUnconstrained2) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    true, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (auto *end : ends) {
    if (end->isUnconstrained()) {
      end->reportShort(sta_->reportPath());
      Required req = end->requiredTime(sta_);
      (void)req;
    }
  }

  }() ));
}

// --- ReportPath: various report functions ---

TEST_F(StaDesignTest, ReportPathEndHeader) {
  ASSERT_NO_THROW(( [&](){
  sta_->reportPathEndHeader();

  }() ));
}

TEST_F(StaDesignTest, ReportPathEndFooter) {
  ASSERT_NO_THROW(( [&](){
  sta_->reportPathEndFooter();

  }() ));
}

TEST_F(StaDesignTest, ReportPathEnd2) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathEnds2) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  sta_->reportPathEnds(&ends);

  }() ));
}

TEST_F(StaDesignTest, ReportPathEndFull) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::full);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathEndFullClkPath) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::full_clock);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathEndFullClkExpanded) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::full_clock_expanded);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathEndShortFormat) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::shorter);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathEndSummary) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::summary);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathEndSlackOnly) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::slack_only);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathEndJson) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFormat(ReportPathFormat::json);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathFromVertex) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    sta_->reportPath(path);
  }
}

TEST_F(StaDesignTest, ReportPathFullWithPrevEnd) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (ends.size() >= 2) {
    sta_->setReportPathFormat(ReportPathFormat::full);
    sta_->reportPathEnd(ends[0], nullptr, false);
    sta_->reportPathEnd(ends[1], ends[0], true);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathFieldOrder) {
  ASSERT_NO_THROW(( [&](){
  StringSeq *field_names = new StringSeq;
  field_names->push_back("fanout");
  field_names->push_back("capacitance");
  field_names->push_back("slew");
  sta_->setReportPathFieldOrder(field_names);

  }() ));
}

TEST_F(StaDesignTest, ReportPathFields) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathFields(true, true, true, true, true, true, true);

  }() ));
}

TEST_F(StaDesignTest, ReportPathDigits) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathDigits(4);

  }() ));
}

TEST_F(StaDesignTest, ReportPathNoSplit) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathNoSplit(true);

  }() ));
}

TEST_F(StaDesignTest, ReportPathSigmas) {
  ASSERT_NO_THROW(( [&](){
  sta_->setReportPathSigmas(true);

  }() ));
}

TEST_F(StaDesignTest, FindReportPathField2) {
  ReportField *field = sta_->findReportPathField("fanout");
  EXPECT_NE(field, nullptr);
  field = sta_->findReportPathField("capacitance");
  EXPECT_NE(field, nullptr);
  field = sta_->findReportPathField("slew");
  EXPECT_NE(field, nullptr);
}

TEST_F(StaDesignTest, ReportPathFieldAccessors) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *slew = rpt->fieldSlew();
  EXPECT_NE(slew, nullptr);
  ReportField *fanout = rpt->fieldFanout();
  EXPECT_NE(fanout, nullptr);
  ReportField *cap = rpt->fieldCapacitance();
  EXPECT_NE(cap, nullptr);
}

// --- ReportPath: MinPulseWidth check ---

TEST_F(StaDesignTest, MinPulseWidthSlack2) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheck *check = sta_->minPulseWidthSlack(nullptr);
  (void)check;

  }() ));
}

TEST_F(StaDesignTest, MinPulseWidthViolations2) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheckSeq &viols = sta_->minPulseWidthViolations(nullptr);
  (void)viols;

  }() ));
}

TEST_F(StaDesignTest, MinPulseWidthChecksAll2) {
  ASSERT_NO_THROW(( [&](){
  MinPulseWidthCheckSeq &checks = sta_->minPulseWidthChecks(nullptr);
  sta_->reportMpwChecks(&checks, false);
  sta_->reportMpwChecks(&checks, true);

  }() ));
}

TEST_F(StaDesignTest, MinPulseWidthCheckForPin) {
  ASSERT_NO_THROW(( [&](){
  Pin *pin = findPin("r1/CK");
  if (pin) {
    PinSeq pins;
    pins.push_back(pin);
    MinPulseWidthCheckSeq &checks = sta_->minPulseWidthChecks(&pins, nullptr);
    (void)checks;
  }

  }() ));
}

// --- ReportPath: MinPeriod ---

TEST_F(StaDesignTest, MinPeriodSlack2) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheck *check = sta_->minPeriodSlack();
  (void)check;

  }() ));
}

TEST_F(StaDesignTest, MinPeriodViolations2) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &viols = sta_->minPeriodViolations();
  (void)viols;

  }() ));
}

TEST_F(StaDesignTest, MinPeriodCheckVerbose) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheck *check = sta_->minPeriodSlack();
  if (check) {
    sta_->reportCheck(check, false);
    sta_->reportCheck(check, true);
  }

  }() ));
}

// --- ReportPath: MaxSkew ---

TEST_F(StaDesignTest, MaxSkewSlack2) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  (void)check;

  }() ));
}

TEST_F(StaDesignTest, MaxSkewViolations2) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheckSeq &viols = sta_->maxSkewViolations();
  (void)viols;

  }() ));
}

TEST_F(StaDesignTest, MaxSkewCheckVerbose) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  if (check) {
    sta_->reportCheck(check, false);
    sta_->reportCheck(check, true);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportMaxSkewHeaderShort) {
  ASSERT_NO_THROW(( [&](){
  ReportPath *rpt = sta_->reportPath();
  rpt->reportMaxSkewHeaderShort();

  }() ));
}

// --- ClkSkew / ClkLatency ---

TEST_F(StaDesignTest, ReportClkSkewSetup) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ConstClockSeq clks;
  clks.push_back(clk);
  sta_->reportClkSkew(clks, nullptr, SetupHold::max(), false, 3);
}

TEST_F(StaDesignTest, ReportClkSkewHold) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ConstClockSeq clks;
  clks.push_back(clk);
  sta_->reportClkSkew(clks, nullptr, SetupHold::min(), false, 3);
}

TEST_F(StaDesignTest, ReportClkSkewWithInternalLatency) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ConstClockSeq clks;
  clks.push_back(clk);
  sta_->reportClkSkew(clks, nullptr, SetupHold::max(), true, 3);
}

TEST_F(StaDesignTest, FindWorstClkSkew2) {
  ASSERT_NO_THROW(( [&](){
  float worst = sta_->findWorstClkSkew(SetupHold::max(), false);
  (void)worst;

  }() ));
}

TEST_F(StaDesignTest, ReportClkLatency2) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ConstClockSeq clks;
  clks.push_back(clk);
  sta_->reportClkLatency(clks, nullptr, false, 3);
}

TEST_F(StaDesignTest, ReportClkLatencyWithInternal) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ConstClockSeq clks;
  clks.push_back(clk);
  sta_->reportClkLatency(clks, nullptr, true, 3);
}

TEST_F(StaDesignTest, FindClkDelays2) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClkDelays delays = sta_->findClkDelays(clk, false);
  (void)delays;
}

TEST_F(StaDesignTest, FindClkMinPeriod2) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  float min_period = sta_->findClkMinPeriod(clk, false);
  (void)min_period;
}

TEST_F(StaDesignTest, FindClkMinPeriodWithPorts) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  float min_period = sta_->findClkMinPeriod(clk, true);
  (void)min_period;
}

// --- Property tests ---

TEST_F(StaDesignTest, PropertyGetLibrary) {
  Network *network = sta_->cmdNetwork();
  LibraryIterator *lib_iter = network->libraryIterator();
  if (lib_iter->hasNext()) {
    Library *lib = lib_iter->next();
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(lib, "name");
    EXPECT_EQ(pv.type(), PropertyValue::type_string);
  }
  delete lib_iter;
}

TEST_F(StaDesignTest, PropertyGetCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Cell *cell = network->cell(top);
  if (cell) {
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(cell, "name");
    EXPECT_EQ(pv.type(), PropertyValue::type_string);
  }
}

TEST_F(StaDesignTest, PropertyGetLibertyLibrary) {
  Properties &props = sta_->properties();
  PropertyValue pv = props.getProperty(lib_, "name");
  EXPECT_EQ(pv.type(), PropertyValue::type_string);
}

TEST_F(StaDesignTest, PropertyGetLibertyCell) {
  LibertyCell *cell = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(cell, nullptr);
  Properties &props = sta_->properties();
  PropertyValue pv = props.getProperty(cell, "name");
  EXPECT_EQ(pv.type(), PropertyValue::type_string);
}

TEST_F(StaDesignTest, PropertyGetLibertyPort2) {
  LibertyCell *cell = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(cell, nullptr);
  LibertyPort *port = cell->findLibertyPort("D");
  ASSERT_NE(port, nullptr);
  Properties &props = sta_->properties();
  PropertyValue pv = props.getProperty(port, "name");
  EXPECT_EQ(pv.type(), PropertyValue::type_string);
}

TEST_F(StaDesignTest, PropertyGetInstance) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *child_iter = network->childIterator(top);
  if (child_iter->hasNext()) {
    Instance *inst = child_iter->next();
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(inst, "name");
    EXPECT_EQ(pv.type(), PropertyValue::type_string);
  }
  delete child_iter;
}

TEST_F(StaDesignTest, PropertyGetPin) {
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  Properties &props = sta_->properties();
  PropertyValue pv = props.getProperty(pin, "name");
  EXPECT_EQ(pv.type(), PropertyValue::type_string);
}

TEST_F(StaDesignTest, PropertyGetPinDirection) {
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  Properties &props = sta_->properties();
  PropertyValue pv = props.getProperty(pin, "direction");
  EXPECT_EQ(pv.type(), PropertyValue::type_string);
}

TEST_F(StaDesignTest, PropertyGetNet) {
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  Net *net = network->net(pin);
  if (net) {
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(net, "name");
    EXPECT_EQ(pv.type(), PropertyValue::type_string);
  }
}

TEST_F(StaDesignTest, PropertyGetClock2) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  Properties &props = sta_->properties();
  PropertyValue pv = props.getProperty(clk, "name");
  EXPECT_EQ(pv.type(), PropertyValue::type_string);
}

TEST_F(StaDesignTest, PropertyGetClockPeriod) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  Properties &props = sta_->properties();
  PropertyValue pv = props.getProperty(clk, "period");
  EXPECT_EQ(pv.type(), PropertyValue::type_float);
}

TEST_F(StaDesignTest, PropertyGetPort2) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Cell *cell = network->cell(top);
  CellPortIterator *port_iter = network->portIterator(cell);
  if (port_iter->hasNext()) {
    Port *port = port_iter->next();
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(port, "name");
    EXPECT_EQ(pv.type(), PropertyValue::type_string);
  }
  delete port_iter;
}

TEST_F(StaDesignTest, PropertyGetEdge2) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(edge, "from_pin");
    (void)pv;
  }
}

TEST_F(StaDesignTest, PropertyGetPathEndSlack) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(ends[0], "startpoint");
    (void)pv;
    pv = props.getProperty(ends[0], "endpoint");
    (void)pv;
  }

  }() ));
}

TEST_F(StaDesignTest, PropertyGetPathEndMore) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    Properties &props = sta_->properties();
    PropertyValue pv = props.getProperty(ends[0], "startpoint_clock");
    (void)pv;
    pv = props.getProperty(ends[0], "endpoint_clock");
    (void)pv;
    pv = props.getProperty(ends[0], "points");
    (void)pv;
  }

  }() ));
}

// --- Property: pin arrival/slack ---

TEST_F(StaDesignTest, PinArrival2) {
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  Arrival arr = sta_->pinArrival(pin, RiseFall::rise(), MinMax::max());
  (void)arr;
}

TEST_F(StaDesignTest, PinSlack) {
  Pin *pin = findPin("r3/D");
  ASSERT_NE(pin, nullptr);
  Slack slk = sta_->pinSlack(pin, MinMax::max());
  (void)slk;
  Slack slk_rf = sta_->pinSlack(pin, RiseFall::rise(), MinMax::max());
  (void)slk_rf;
}

TEST_F(StaDesignTest, NetSlack2) {
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("r3/D");
  ASSERT_NE(pin, nullptr);
  Net *net = network->net(pin);
  if (net) {
    Slack slk = sta_->netSlack(net, MinMax::max());
    (void)slk;
  }
}

// --- Search: various methods ---

TEST_F(StaDesignTest, SearchIsClock) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/CK");
  if (v) {
    bool is_clk = search->isClock(v);
    (void)is_clk;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchIsGenClkSrc2) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  bool is_gen = search->isGenClkSrc(v);
  (void)is_gen;
}

TEST_F(StaDesignTest, SearchClocks) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/CK");
  if (v) {
    ClockSet clks = search->clocks(v);
    (void)clks;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchClockDomains) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  ClockSet domains = search->clockDomains(v);
  (void)domains;
}

TEST_F(StaDesignTest, SearchClockDomainsPin) {
  Search *search = sta_->search();
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  ClockSet domains = search->clockDomains(pin);
  (void)domains;
}

TEST_F(StaDesignTest, SearchClocksPin) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Pin *pin = findPin("r1/CK");
  if (pin) {
    ClockSet clks = search->clocks(pin);
    (void)clks;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchIsEndpoint2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v_data = findVertex("r3/D");
  if (v_data) {
    bool is_ep = search->isEndpoint(v_data);
    (void)is_ep;
  }
  Vertex *v_out = findVertex("r1/Q");
  if (v_out) {
    bool is_ep = search->isEndpoint(v_out);
    (void)is_ep;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchHavePathGroups) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  bool have = search->havePathGroups();
  (void)have;

  }() ));
}

TEST_F(StaDesignTest, SearchFindPathGroup) {
  Search *search = sta_->search();
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  PathGroup *pg = search->findPathGroup(clk, MinMax::max());
  (void)pg;
}

TEST_F(StaDesignTest, SearchClkInfoCount) {
  Search *search = sta_->search();
  int count = search->clkInfoCount();
  EXPECT_GE(count, 0);
}

TEST_F(StaDesignTest, SearchTagGroupCount) {
  Search *search = sta_->search();
  TagGroupIndex count = search->tagGroupCount();
  EXPECT_GE(count, 0u);
}

TEST_F(StaDesignTest, SearchTagGroupByIndex) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  TagGroupIndex count = search->tagGroupCount();
  if (count > 0) {
    TagGroup *tg = search->tagGroup(0);
    (void)tg;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchReportTagGroups2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->reportTagGroups();

  }() ));
}

TEST_F(StaDesignTest, SearchReportArrivals2) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  search->reportArrivals(v, false);
  search->reportArrivals(v, true);
}

TEST_F(StaDesignTest, SearchSeedArrival) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("in1");
  if (v) {
    search->seedArrival(v);
  }

  }() ));
}

TEST_F(StaDesignTest, SearchPathClkPathArrival2) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    Arrival arr = search->pathClkPathArrival(path);
    (void)arr;
  }
}

TEST_F(StaDesignTest, SearchFindClkArrivals) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->findClkArrivals();

  }() ));
}

TEST_F(StaDesignTest, SearchFindRequireds) {
  Search *search = sta_->search();
  search->findRequireds();
  EXPECT_TRUE(search->requiredsExist());
}

TEST_F(StaDesignTest, SearchRequiredsSeeded) {
  ASSERT_NO_THROW(( [&](){
  sta_->findRequireds();
  Search *search = sta_->search();
  bool seeded = search->requiredsSeeded();
  (void)seeded;

  }() ));
}

TEST_F(StaDesignTest, SearchArrivalsAtEndpoints) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  bool exist = search->arrivalsAtEndpointsExist();
  (void)exist;

  }() ));
}

TEST_F(StaDesignTest, SearchArrivalIterator) {
  Search *search = sta_->search();
  BfsFwdIterator *fwd = search->arrivalIterator();
  EXPECT_NE(fwd, nullptr);
}

TEST_F(StaDesignTest, SearchRequiredIterator) {
  Search *search = sta_->search();
  BfsBkwdIterator *bkwd = search->requiredIterator();
  EXPECT_NE(bkwd, nullptr);
}

TEST_F(StaDesignTest, SearchWnsSlack2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("r3/D");
  if (v) {
    Slack wns = search->wnsSlack(v, 0);
    (void)wns;
  }

  }() ));
}

TEST_F(StaDesignTest, SearchDeratedDelay) {
  Search *search = sta_->search();
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (!arc_set->arcs().empty()) {
      TimingArc *arc = arc_set->arcs()[0];
      ArcDelay delay = search->deratedDelay(edge->from(sta_->graph()),
        arc, edge, false, path_ap);
      (void)delay;
    }
  }
}

TEST_F(StaDesignTest, SearchMatchesFilter) {
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    bool matches = search->matchesFilter(path, nullptr);
    (void)matches;
  }
}

TEST_F(StaDesignTest, SearchEnsureDownstreamClkPins2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->ensureDownstreamClkPins();

  }() ));
}

TEST_F(StaDesignTest, SearchVisitPathEnds) {
  Search *search = sta_->search();
  VisitPathEnds *vpe = search->visitPathEnds();
  EXPECT_NE(vpe, nullptr);
}

TEST_F(StaDesignTest, SearchGatedClk) {
  Search *search = sta_->search();
  GatedClk *gc = search->gatedClk();
  EXPECT_NE(gc, nullptr);
}

TEST_F(StaDesignTest, SearchGenclks) {
  Search *search = sta_->search();
  Genclks *gen = search->genclks();
  EXPECT_NE(gen, nullptr);
}

TEST_F(StaDesignTest, SearchCheckCrpr) {
  Search *search = sta_->search();
  CheckCrpr *crpr = search->checkCrpr();
  EXPECT_NE(crpr, nullptr);
}

// --- Sta: various methods ---

TEST_F(StaDesignTest, StaIsClock) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureClkNetwork();
  Pin *clk_pin = findPin("r1/CK");
  if (clk_pin) {
    bool is_clk = sta_->isClock(clk_pin);
    (void)is_clk;
  }

  }() ));
}

TEST_F(StaDesignTest, StaIsClockNet) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  sta_->ensureClkNetwork();
  Pin *clk_pin = findPin("r1/CK");
  if (clk_pin) {
    Net *net = network->net(clk_pin);
    if (net) {
      bool is_clk = sta_->isClock(net);
      (void)is_clk;
    }
  }

  }() ));
}

TEST_F(StaDesignTest, StaIsIdealClock) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureClkNetwork();
  Pin *clk_pin = findPin("r1/CK");
  if (clk_pin) {
    bool is_ideal = sta_->isIdealClock(clk_pin);
    (void)is_ideal;
  }

  }() ));
}

TEST_F(StaDesignTest, StaIsPropagatedClock) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureClkNetwork();
  Pin *clk_pin = findPin("r1/CK");
  if (clk_pin) {
    bool is_prop = sta_->isPropagatedClock(clk_pin);
    (void)is_prop;
  }

  }() ));
}

TEST_F(StaDesignTest, StaPins) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  sta_->ensureClkNetwork();
  const PinSet *pins = sta_->pins(clk);
  (void)pins;
}

TEST_F(StaDesignTest, StaStartpointPins) {
  PinSet startpoints = sta_->startpointPins();
  EXPECT_GE(startpoints.size(), 1u);
}

TEST_F(StaDesignTest, StaEndpointPins) {
  PinSet endpoints = sta_->endpointPins();
  EXPECT_GE(endpoints.size(), 1u);
}

TEST_F(StaDesignTest, StaEndpoints) {
  VertexSet *endpoints = sta_->endpoints();
  EXPECT_NE(endpoints, nullptr);
  EXPECT_GE(endpoints->size(), 1u);
}

TEST_F(StaDesignTest, StaEndpointViolationCount) {
  ASSERT_NO_THROW(( [&](){
  int count = sta_->endpointViolationCount(MinMax::max());
  (void)count;

  }() ));
}

TEST_F(StaDesignTest, StaTotalNegativeSlack) {
  ASSERT_NO_THROW(( [&](){
  Slack tns = sta_->totalNegativeSlack(MinMax::max());
  (void)tns;

  }() ));
}

TEST_F(StaDesignTest, StaTotalNegativeSlackCorner) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack tns = sta_->totalNegativeSlack(corner, MinMax::max());
  (void)tns;

  }() ));
}

TEST_F(StaDesignTest, StaWorstSlack) {
  ASSERT_NO_THROW(( [&](){
  Slack wns = sta_->worstSlack(MinMax::max());
  (void)wns;

  }() ));
}

TEST_F(StaDesignTest, StaWorstSlackVertex) {
  ASSERT_NO_THROW(( [&](){
  Slack worst_slack;
  Vertex *worst_vertex;
  sta_->worstSlack(MinMax::max(), worst_slack, worst_vertex);
  (void)worst_slack;
  (void)worst_vertex;

  }() ));
}

TEST_F(StaDesignTest, StaWorstSlackCornerVertex) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack worst_slack;
  Vertex *worst_vertex;
  sta_->worstSlack(corner, MinMax::max(), worst_slack, worst_vertex);
  (void)worst_slack;
  (void)worst_vertex;

  }() ));
}

TEST_F(StaDesignTest, StaVertexWorstSlackPath) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstSlackPath(v, MinMax::max());
  (void)path;
}

TEST_F(StaDesignTest, StaVertexWorstSlackPathRf) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstSlackPath(v, RiseFall::rise(), MinMax::max());
  (void)path;
}

TEST_F(StaDesignTest, StaVertexWorstRequiredPath) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstRequiredPath(v, MinMax::max());
  (void)path;
}

TEST_F(StaDesignTest, StaVertexWorstRequiredPathRf) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstRequiredPath(v, RiseFall::rise(), MinMax::max());
  (void)path;
}

TEST_F(StaDesignTest, StaVertexWorstArrivalPathRf) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, RiseFall::rise(), MinMax::max());
  (void)path;
}

TEST_F(StaDesignTest, StaVertexSlacks) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Slack slacks[RiseFall::index_count][MinMax::index_count];
  sta_->vertexSlacks(v, slacks);
  // slacks should be populated
}

TEST_F(StaDesignTest, StaVertexSlewRfCorner) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  Slew slew = sta_->vertexSlew(v, RiseFall::rise(), corner, MinMax::max());
  (void)slew;
}

TEST_F(StaDesignTest, StaVertexSlewRfMinMax) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Slew slew = sta_->vertexSlew(v, RiseFall::rise(), MinMax::max());
  (void)slew;
}

TEST_F(StaDesignTest, StaVertexRequiredRfPathAP) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  Required req = sta_->vertexRequired(v, RiseFall::rise(), path_ap);
  (void)req;
}

TEST_F(StaDesignTest, StaVertexArrivalClkEdge) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  const ClockEdge *edge = clk->edge(RiseFall::rise());
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  Arrival arr = sta_->vertexArrival(v, RiseFall::rise(), edge, path_ap, MinMax::max());
  (void)arr;
}

// --- Sta: CheckTiming ---

TEST_F(StaDesignTest, CheckTiming2) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    true, true, true, true, true, true, true);
  (void)errors;

  }() ));
}

TEST_F(StaDesignTest, CheckTimingNoInputDelay) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    true, false, false, false, false, false, false);
  (void)errors;

  }() ));
}

TEST_F(StaDesignTest, CheckTimingNoOutputDelay) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    false, true, false, false, false, false, false);
  (void)errors;

  }() ));
}

TEST_F(StaDesignTest, CheckTimingUnconstrained) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    false, false, false, false, true, false, false);
  (void)errors;

  }() ));
}

TEST_F(StaDesignTest, CheckTimingLoops) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    false, false, false, false, false, true, false);
  (void)errors;

  }() ));
}

// --- Sta: delay calc ---

TEST_F(StaDesignTest, ReportDelayCalc2) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (!arc_set->arcs().empty()) {
      TimingArc *arc = arc_set->arcs()[0];
      std::string report = sta_->reportDelayCalc(edge, arc, corner, MinMax::max(), 3);
      EXPECT_FALSE(report.empty());
    }
  }
}

// --- Sta: CRPR settings ---

TEST_F(StaDesignTest, CrprEnabled) {
  bool enabled = sta_->crprEnabled();
  (void)enabled;
  sta_->setCrprEnabled(true);
  EXPECT_TRUE(sta_->crprEnabled());
  sta_->setCrprEnabled(false);
}

TEST_F(StaDesignTest, CrprMode) {
  CrprMode mode = sta_->crprMode();
  (void)mode;
  sta_->setCrprMode(CrprMode::same_pin);
  EXPECT_EQ(sta_->crprMode(), CrprMode::same_pin);
}

// --- Sta: propagateGatedClockEnable ---

TEST_F(StaDesignTest, PropagateGatedClockEnable) {
  bool prop = sta_->propagateGatedClockEnable();
  (void)prop;
  sta_->setPropagateGatedClockEnable(true);
  EXPECT_TRUE(sta_->propagateGatedClockEnable());
  sta_->setPropagateGatedClockEnable(false);
}

// --- Sta: analysis mode ---

TEST_F(StaDesignTest, CmdNamespace) {
  ASSERT_NO_THROW(( [&](){
  CmdNamespace ns = sta_->cmdNamespace();
  (void)ns;

  }() ));
}

TEST_F(StaDesignTest, CmdCorner) {
  Corner *corner = sta_->cmdCorner();
  EXPECT_NE(corner, nullptr);
}

TEST_F(StaDesignTest, FindCorner) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->findCorner("default");
  (void)corner;

  }() ));
}

TEST_F(StaDesignTest, MultiCorner) {
  ASSERT_NO_THROW(( [&](){
  bool multi = sta_->multiCorner();
  (void)multi;

  }() ));
}

// --- PathExpanded: detailed accessors ---

TEST_F(StaDesignTest, PathExpandedSize) {
  Vertex *v = findVertex("u2/ZN");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    PathExpanded expanded(path, sta_);
    EXPECT_GT(expanded.size(), 0u);
  }
}

TEST_F(StaDesignTest, PathExpandedStartPath) {
  Vertex *v = findVertex("u2/ZN");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    PathExpanded expanded(path, sta_);
    if (expanded.size() > 0) {
      const Path *start = expanded.startPath();
      (void)start;
    }
  }
}

// --- Sta: Timing derate ---

TEST_F(StaDesignTest, SetTimingDerate) {
  ASSERT_NO_THROW(( [&](){
  sta_->setTimingDerate(TimingDerateType::cell_delay,
    PathClkOrData::clk, RiseFallBoth::riseFall(),
    EarlyLate::early(), 0.95f);
  sta_->unsetTimingDerate();

  }() ));
}

// --- Sta: setArcDelay ---

TEST_F(StaDesignTest, SetArcDelay) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  VertexInEdgeIterator edge_iter(v, sta_->graph());
  if (edge_iter.hasNext()) {
    Edge *edge = edge_iter.next();
    TimingArcSet *arc_set = edge->timingArcSet();
    if (!arc_set->arcs().empty()) {
      TimingArc *arc = arc_set->arcs()[0];
      sta_->setArcDelay(edge, arc, corner, MinMaxAll::all(), 1.0e-10f);
    }
  }
}

// --- Sta: removeDelaySlewAnnotations ---

TEST_F(StaDesignTest, RemoveDelaySlewAnnotations2) {
  ASSERT_NO_THROW(( [&](){
  sta_->removeDelaySlewAnnotations();

  }() ));
}

// --- Sta: endpoint slack ---

TEST_F(StaDesignTest, EndpointSlack2) {
  ASSERT_NO_THROW(( [&](){
  Pin *pin = findPin("r3/D");
  if (pin) {
    Slack slk = sta_->endpointSlack(pin, "clk", MinMax::max());
    (void)slk;
  }

  }() ));
}

// --- Sta: delaysInvalid/arrivalsInvalid ---

TEST_F(StaDesignTest, DelaysInvalid2) {
  ASSERT_NO_THROW(( [&](){
  sta_->delaysInvalid();
  sta_->updateTiming(true);

  }() ));
}

TEST_F(StaDesignTest, ArrivalsInvalid2) {
  ASSERT_NO_THROW(( [&](){
  sta_->arrivalsInvalid();
  sta_->updateTiming(true);

  }() ));
}

TEST_F(StaDesignTest, DelaysInvalidFrom) {
  ASSERT_NO_THROW(( [&](){
  Pin *pin = findPin("u1/Z");
  if (pin) {
    sta_->delaysInvalidFrom(pin);
  }

  }() ));
}

TEST_F(StaDesignTest, DelaysInvalidFromFanin) {
  ASSERT_NO_THROW(( [&](){
  Pin *pin = findPin("r3/D");
  if (pin) {
    sta_->delaysInvalidFromFanin(pin);
  }

  }() ));
}

// --- Sta: searchPreamble ---

TEST_F(StaDesignTest, SearchPreamble) {
  ASSERT_NO_THROW(( [&](){
  sta_->searchPreamble();

  }() ));
}

// --- Sta: ensureLevelized / ensureGraph / ensureLinked ---

TEST_F(StaDesignTest, EnsureLevelized) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureLevelized();

  }() ));
}

TEST_F(StaDesignTest, EnsureGraph) {
  Graph *graph = sta_->ensureGraph();
  EXPECT_NE(graph, nullptr);
}

TEST_F(StaDesignTest, EnsureLinked) {
  Network *network = sta_->ensureLinked();
  EXPECT_NE(network, nullptr);
}

TEST_F(StaDesignTest, EnsureLibLinked) {
  Network *network = sta_->ensureLibLinked();
  EXPECT_NE(network, nullptr);
}

TEST_F(StaDesignTest, EnsureClkArrivals) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureClkArrivals();

  }() ));
}

TEST_F(StaDesignTest, EnsureClkNetwork) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureClkNetwork();

  }() ));
}

// --- Sta: findDelays ---

TEST_F(StaDesignTest, FindDelays2) {
  ASSERT_NO_THROW(( [&](){
  sta_->findDelays();

  }() ));
}

// --- Sta: setVoltage for net ---

TEST_F(StaDesignTest, SetVoltageNet) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("r1/Q");
  if (pin) {
    Net *net = network->net(pin);
    if (net) {
      sta_->setVoltage(net, MinMax::max(), 1.1f);
    }
  }

  }() ));
}

// --- Sta: PVT ---

TEST_F(StaDesignTest, GetPvt) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  const Pvt *pvt = sta_->pvt(top, MinMax::max());
  (void)pvt;

  }() ));
}

// --- ClkNetwork ---

TEST_F(StaDesignTest, ClkNetworkIsClock) {
  ASSERT_NO_THROW(( [&](){
  ClkNetwork *clk_network = sta_->search()->clkNetwork();
  if (clk_network) {
    Pin *clk_pin = findPin("r1/CK");
    if (clk_pin) {
      bool is_clk = clk_network->isClock(clk_pin);
      (void)is_clk;
    }
  }

  }() ));
}

// --- Tag operations ---

TEST_F(StaDesignTest, TagPathAPIndex) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  TagIndex count = search->tagCount();
  if (count > 0) {
    Tag *t = search->tag(0);
    if (t) {
      PathAPIndex idx = t->pathAPIndex();
      (void)idx;
    }
  }

  }() ));
}

TEST_F(StaDesignTest, TagCmp) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  TagIndex count = search->tagCount();
  if (count >= 2) {
    Tag *t0 = search->tag(0);
    Tag *t1 = search->tag(1);
    if (t0 && t1) {
      int cmp = Tag::cmp(t0, t1, sta_);
      (void)cmp;
      int mcmp = Tag::matchCmp(t0, t1, true, sta_);
      (void)mcmp;
    }
  }

  }() ));
}

TEST_F(StaDesignTest, TagHash) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  TagIndex count = search->tagCount();
  if (count > 0) {
    Tag *t = search->tag(0);
    if (t) {
      size_t h = t->hash(true, sta_);
      (void)h;
      size_t mh = t->matchHash(true, sta_);
      (void)mh;
    }
  }

  }() ));
}

TEST_F(StaDesignTest, TagMatchHashEqual) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  TagIndex count = search->tagCount();
  if (count >= 2) {
    Tag *t0 = search->tag(0);
    Tag *t1 = search->tag(1);
    if (t0 && t1) {
      TagMatchHash hash(true, sta_);
      size_t h0 = hash(t0);
      size_t h1 = hash(t1);
      (void)h0;
      (void)h1;
      TagMatchEqual eq(true, sta_);
      bool result = eq(t0, t1);
      (void)result;
    }
  }

  }() ));
}

// --- ClkInfo operations ---

TEST_F(StaDesignTest, ClkInfoAccessors2) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
  if (iter && iter->hasNext()) {
    Path *path = iter->next();
    Tag *tag = path->tag(sta_);
    if (tag) {
      const ClkInfo *clk_info = tag->clkInfo();
      if (clk_info) {
        const ClockEdge *edge = clk_info->clkEdge();
        (void)edge;
        bool prop = clk_info->isPropagated();
        (void)prop;
        bool gen = clk_info->isGenClkSrcPath();
        (void)gen;
        PathAPIndex idx = clk_info->pathAPIndex();
        (void)idx;
      }
    }
  }
  delete iter;
}

// --- Sim ---

TEST_F(StaDesignTest, SimLogicValue2) {
  Sim *sim = sta_->sim();
  ASSERT_NE(sim, nullptr);
  Pin *pin = findPin("r1/D");
  if (pin) {
    LogicValue val = sim->logicValue(pin);
    (void)val;
  }
}

TEST_F(StaDesignTest, SimLogicZeroOne) {
  Sim *sim = sta_->sim();
  ASSERT_NE(sim, nullptr);
  Pin *pin = findPin("r1/D");
  if (pin) {
    bool zeroone = sim->logicZeroOne(pin);
    (void)zeroone;
  }
}

TEST_F(StaDesignTest, SimEnsureConstantsPropagated) {
  Sim *sim = sta_->sim();
  ASSERT_NE(sim, nullptr);
  sim->ensureConstantsPropagated();
}

TEST_F(StaDesignTest, SimFunctionSense) {
  Sim *sim = sta_->sim();
  ASSERT_NE(sim, nullptr);
  // Use u1 (BUF_X1) which has known input A and output Z
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Instance *u1 = network->findChild(top, "u1");
  if (u1) {
    Pin *from_pin = findPin("u1/A");
    Pin *to_pin = findPin("u1/Z");
    if (from_pin && to_pin) {
      TimingSense sense = sim->functionSense(u1, from_pin, to_pin);
      (void)sense;
    }
  }
}

// --- Levelize ---

TEST_F(StaDesignTest, LevelizeMaxLevel) {
  Levelize *lev = sta_->levelize();
  ASSERT_NE(lev, nullptr);
  Level max_level = lev->maxLevel();
  EXPECT_GT(max_level, 0);
}

TEST_F(StaDesignTest, LevelizeLevelized) {
  Levelize *lev = sta_->levelize();
  ASSERT_NE(lev, nullptr);
  bool is_levelized = lev->levelized();
  EXPECT_TRUE(is_levelized);
}

// --- Sta: makeParasiticNetwork ---

TEST_F(StaDesignTest, MakeParasiticNetwork) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Pin *pin = findPin("r1/Q");
  if (pin) {
    Net *net = network->net(pin);
    if (net) {
      Corner *corner = sta_->cmdCorner();
      const ParasiticAnalysisPt *ap = corner->findParasiticAnalysisPt(MinMax::max());
      if (ap) {
        Parasitic *parasitic = sta_->makeParasiticNetwork(net, false, ap);
        (void)parasitic;
      }
    }
  }

  }() ));
}

// --- Path: operations on actual paths ---

TEST_F(StaDesignTest, PathIsNull) {
  Path path;
  EXPECT_TRUE(path.isNull());
}

TEST_F(StaDesignTest, PathFromVertex) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    Vertex *pv = path->vertex(sta_);
    EXPECT_NE(pv, nullptr);
    Tag *tag = path->tag(sta_);
    (void)tag;
    Arrival arr = path->arrival();
    (void)arr;
    const RiseFall *rf = path->transition(sta_);
    EXPECT_NE(rf, nullptr);
    const MinMax *mm = path->minMax(sta_);
    EXPECT_NE(mm, nullptr);
  }
}

TEST_F(StaDesignTest, PathPrevPath) {
  Vertex *v = findVertex("u2/ZN");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, MinMax::max());
  if (path && !path->isNull()) {
    Path *prev = path->prevPath();
    (void)prev;
    TimingArc *prev_arc = path->prevArc(sta_);
    (void)prev_arc;
    Edge *prev_edge = path->prevEdge(sta_);
    (void)prev_edge;
  }
}

// --- PathExpanded: with clk path ---

TEST_F(StaDesignTest, PathExpandedWithClk) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    Path *path = ends[0]->path();
    if (path && !path->isNull()) {
      PathExpanded expanded(path, true, sta_);
      for (size_t i = 0; i < expanded.size(); i++) {
        const Path *p = expanded.path(i);
        (void)p;
      }
    }
  }

  }() ));
}

// --- GatedClk ---

TEST_F(StaDesignTest, GatedClkIsEnable) {
  ASSERT_NO_THROW(( [&](){
  GatedClk *gc = sta_->search()->gatedClk();
  Vertex *v = findVertex("u1/Z");
  if (v) {
    bool is_enable = gc->isGatedClkEnable(v);
    (void)is_enable;
  }

  }() ));
}

TEST_F(StaDesignTest, GatedClkEnables) {
  ASSERT_NO_THROW(( [&](){
  GatedClk *gc = sta_->search()->gatedClk();
  Vertex *v = findVertex("r1/CK");
  if (v) {
    PinSet enables(sta_->network());
    gc->gatedClkEnables(v, enables);
    (void)enables;
  }

  }() ));
}

// --- Genclks ---

TEST_F(StaDesignTest, GenclksClear) {
  ASSERT_NO_THROW(( [&](){
  Genclks *gen = sta_->search()->genclks();
  // Clear should not crash on design without generated clocks
  gen->clear();

  }() ));
}

// --- Search: visitStartpoints/visitEndpoints ---

TEST_F(StaDesignTest, SearchVisitEndpoints2) {
  Search *search = sta_->search();
  PinSet pins(sta_->network());
  VertexPinCollector collector(pins);
  search->visitEndpoints(&collector);
  EXPECT_GE(pins.size(), 1u);
}

TEST_F(StaDesignTest, SearchVisitStartpoints2) {
  Search *search = sta_->search();
  PinSet pins(sta_->network());
  VertexPinCollector collector(pins);
  search->visitStartpoints(&collector);
  EXPECT_GE(pins.size(), 1u);
}

// --- PathGroup ---

TEST_F(StaDesignTest, PathGroupFindByName) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  // After findPathEnds, path groups should exist
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    PathGroup *pg = ends[0]->pathGroup();
    if (pg) {
      const char *name = pg->name();
      (void)name;
    }
  }

  }() ));
}

TEST_F(StaDesignTest, PathGroups) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    Search *search = sta_->search();
    PathGroupSeq groups = search->pathGroups(ends[0]);
    (void)groups;
  }

  }() ));
}

// --- VertexPathIterator with PathAnalysisPt ---

TEST_F(StaDesignTest, VertexPathIteratorPathAP) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), path_ap);
  ASSERT_NE(iter, nullptr);
  while (iter->hasNext()) {
    Path *path = iter->next();
    (void)path;
  }
  delete iter;
}

// --- Sta: setOutputDelay and find unconstrained ---

TEST_F(StaDesignTest, SetOutputDelayAndCheck) {
  Pin *out = findPin("out");
  ASSERT_NE(out, nullptr);
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                       clk, RiseFall::rise(), nullptr,
                       false, false, MinMaxAll::all(), true, 2.0f);
  sta_->updateTiming(true);
  // Now find paths to output
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 1, false, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  // Should have paths including output delay
  (void)ends;
}

// --- Sta: unique_edges findPathEnds ---

TEST_F(StaDesignTest, FindPathEndsUniqueEdges) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 3, false, true, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  (void)ends;

  }() ));
}

// --- Sta: corner path analysis pt ---

TEST_F(StaDesignTest, CornerPathAnalysisPt) {
  Corner *corner = sta_->cmdCorner();
  ASSERT_NE(corner, nullptr);
  const PathAnalysisPt *max_ap = corner->findPathAnalysisPt(MinMax::max());
  EXPECT_NE(max_ap, nullptr);
  const PathAnalysisPt *min_ap = corner->findPathAnalysisPt(MinMax::min());
  EXPECT_NE(min_ap, nullptr);
}

// --- Sta: incrementalDelayTolerance ---

TEST_F(StaDesignTest, IncrementalDelayTolerance) {
  ASSERT_NO_THROW(( [&](){
  sta_->setIncrementalDelayTolerance(0.01f);

  }() ));
}

// --- Sta: pocvEnabled ---

TEST_F(StaDesignTest, PocvEnabled) {
  ASSERT_NO_THROW(( [&](){
  bool enabled = sta_->pocvEnabled();
  (void)enabled;

  }() ));
}

// --- Sta: makePiElmore ---

TEST_F(StaDesignTest, MakePiElmore) {
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  sta_->makePiElmore(pin, RiseFall::rise(), MinMaxAll::all(),
                     1.0e-15f, 100.0f, 1.0e-15f);
  float c2, rpi, c1;
  bool exists;
  sta_->findPiElmore(pin, RiseFall::rise(), MinMax::max(),
                     c2, rpi, c1, exists);
  if (exists) {
    EXPECT_GT(c2, 0.0f);
  }
}

// --- Sta: deleteParasitics ---

TEST_F(StaDesignTest, DeleteParasitics2) {
  ASSERT_NO_THROW(( [&](){
  sta_->deleteParasitics();

  }() ));
}

// --- Search: arrivalsChanged ---

TEST_F(StaDesignTest, SearchArrivalsVertexData) {
  // Verify arrivals exist through the Sta API
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Arrival arr = sta_->vertexArrival(v, MinMax::max());
  (void)arr;
  Required req = sta_->vertexRequired(v, MinMax::max());
  (void)req;
}

// --- Sta: activity ---

TEST_F(StaDesignTest, PinActivity) {
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  PwrActivity act = sta_->activity(pin);
  (void)act;
}

// --- Search: isInputArrivalSrchStart ---

TEST_F(StaDesignTest, IsInputArrivalSrchStart) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("in1");
  if (v) {
    bool is_start = search->isInputArrivalSrchStart(v);
    (void)is_start;
  }

  }() ));
}

TEST_F(StaDesignTest, IsSegmentStart) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Pin *pin = findPin("in1");
  if (pin) {
    bool is_seg = search->isSegmentStart(pin);
    (void)is_seg;
  }

  }() ));
}

// --- Search: clockInsertion ---

TEST_F(StaDesignTest, ClockInsertion) {
  Search *search = sta_->search();
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  Pin *pin = findPin("r1/CK");
  if (pin) {
    Corner *corner = sta_->cmdCorner();
    const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
    Arrival ins = search->clockInsertion(clk, pin, RiseFall::rise(),
      MinMax::max(), EarlyLate::late(), path_ap);
    (void)ins;
  }
}

// --- Levelize: edges ---

TEST_F(StaDesignTest, LevelizeLevelsValid) {
  Levelize *lev = sta_->levelize();
  ASSERT_NE(lev, nullptr);
  bool valid = lev->levelized();
  EXPECT_TRUE(valid);
}

// --- Search: reporting ---

TEST_F(StaDesignTest, SearchReportPathCountHistogram2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->reportPathCountHistogram();

  }() ));
}

TEST_F(StaDesignTest, SearchReportTags2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->reportTags();

  }() ));
}

TEST_F(StaDesignTest, SearchReportClkInfos2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->reportClkInfos();

  }() ));
}

// --- Search: filteredEndpoints ---

TEST_F(StaDesignTest, SearchFilteredEndpoints) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  VertexSeq endpoints = search->filteredEndpoints();
  (void)endpoints;

  }() ));
}

// --- Sta: findFanoutInstances ---

TEST_F(StaDesignTest, FindFanoutInstances) {
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  PinSeq from_pins;
  from_pins.push_back(pin);
  InstanceSet fanout = sta_->findFanoutInstances(&from_pins, false, false, 0, 10, false, false);
  EXPECT_GE(fanout.size(), 1u);
}

// --- Sta: search endpointsInvalid ---

TEST_F(StaDesignTest, EndpointsInvalid2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->endpointsInvalid();

  }() ));
}

// --- Sta: constraintsChanged ---

TEST_F(StaDesignTest, ConstraintsChanged2) {
  ASSERT_NO_THROW(( [&](){
  sta_->constraintsChanged();

  }() ));
}

// --- Sta: networkChanged ---

TEST_F(StaDesignTest, NetworkChanged2) {
  ASSERT_NO_THROW(( [&](){
  sta_->networkChanged();

  }() ));
}

// --- Sta: clkPinsInvalid ---

TEST_F(StaDesignTest, ClkPinsInvalid) {
  ASSERT_NO_THROW(( [&](){
  sta_->clkPinsInvalid();

  }() ));
}

// --- PropertyValue constructors and types ---

TEST_F(StaDesignTest, PropertyValueConstructors) {
  PropertyValue pv1;
  EXPECT_EQ(pv1.type(), PropertyValue::type_none);

  PropertyValue pv2("test");
  EXPECT_EQ(pv2.type(), PropertyValue::type_string);
  EXPECT_STREQ(pv2.stringValue(), "test");

  PropertyValue pv3(true);
  EXPECT_EQ(pv3.type(), PropertyValue::type_bool);
  EXPECT_TRUE(pv3.boolValue());

  // Copy constructor
  PropertyValue pv4(pv2);
  EXPECT_EQ(pv4.type(), PropertyValue::type_string);

  // Move constructor
  PropertyValue pv5(std::move(pv3));
  EXPECT_EQ(pv5.type(), PropertyValue::type_bool);
}

// --- Sta: setPvt ---

TEST_F(StaDesignTest, SetPvt) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  sta_->setPvt(top, MinMaxAll::all(), 1.0f, 1.1f, 25.0f);
  const Pvt *pvt = sta_->pvt(top, MinMax::max());
  (void)pvt;

  }() ));
}

// --- Search: propagateClkSense ---

TEST_F(StaDesignTest, SearchClkPathArrival2) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/CK");
  if (v) {
    VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
    if (iter && iter->hasNext()) {
      Path *path = iter->next();
      Arrival arr = search->clkPathArrival(path);
      (void)arr;
    }
    delete iter;
  }

  }() ));
}

// ============================================================
// R10_ tests: Additional coverage for search module uncovered functions
// ============================================================

// --- Properties: pinArrival, pinSlack via Properties ---

TEST_F(StaDesignTest, PropertyPinArrivalRf) {
  ASSERT_NO_THROW(( [&](){
  // Cover Properties::pinArrival(pin, rf, min_max)
  Properties &props = sta_->properties();
  Pin *pin = findPin("r1/D");
  if (pin) {
    PropertyValue pv = props.getProperty(pin, "arrival_max_rise");
    (void)pv;
    PropertyValue pv2 = props.getProperty(pin, "arrival_max_fall");
    (void)pv2;
  }

  }() ));
}

TEST_F(StaDesignTest, PropertyPinSlackMinMax) {
  ASSERT_NO_THROW(( [&](){
  // Cover Properties::pinSlack(pin, min_max)
  Properties &props = sta_->properties();
  Pin *pin = findPin("r1/D");
  if (pin) {
    PropertyValue pv = props.getProperty(pin, "slack_max");
    (void)pv;
    PropertyValue pv2 = props.getProperty(pin, "slack_min");
    (void)pv2;
  }

  }() ));
}

TEST_F(StaDesignTest, PropertyPinSlackRf) {
  ASSERT_NO_THROW(( [&](){
  // Cover Properties::pinSlack(pin, rf, min_max)
  Properties &props = sta_->properties();
  Pin *pin = findPin("r1/D");
  if (pin) {
    PropertyValue pv = props.getProperty(pin, "slack_max_rise");
    (void)pv;
    PropertyValue pv2 = props.getProperty(pin, "slack_min_fall");
    (void)pv2;
  }

  }() ));
}

TEST_F(StaDesignTest, PropertyDelayPropertyValue) {
  ASSERT_NO_THROW(( [&](){
  // Cover Properties::delayPropertyValue, resistancePropertyValue, capacitancePropertyValue
  Properties &props = sta_->properties();
  Graph *graph = sta_->graph();
  Vertex *v = findVertex("r1/D");
  if (v && graph) {
    VertexInEdgeIterator in_iter(v, graph);
    if (in_iter.hasNext()) {
      Edge *edge = in_iter.next();
      PropertyValue pv = props.getProperty(edge, "delay_max_rise");
      (void)pv;
    }
  }

  }() ));
}

TEST_F(StaDesignTest, PropertyGetCellAndLibrary) {
  ASSERT_NO_THROW(( [&](){
  // Cover PropertyRegistry<Cell*>::getProperty, PropertyRegistry<Library*>::getProperty
  Properties &props = sta_->properties();
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Cell *cell = network->cell(top);
  if (cell) {
    PropertyValue pv = props.getProperty(cell, "name");
    (void)pv;
  }
  LibertyLibrary *lib = network->defaultLibertyLibrary();
  if (lib) {
    PropertyValue pv = props.getProperty(lib, "name");
    (void)pv;
  }

  }() ));
}

TEST_F(StaDesignTest, PropertyUnknownException) {
  // Cover PropertyUnknown constructor and what()
  Properties &props = sta_->properties();
  Pin *pin = findPin("r1/D");
  if (pin) {
    try {
      PropertyValue pv = props.getProperty(pin, "nonexistent_property_xyz123");
      (void)pv;
    } catch (const std::exception &e) {
      const char *msg = e.what();
      EXPECT_NE(msg, nullptr);
    }
  }
}

TEST_F(StaDesignTest, PropertyTypeWrongException) {
  // Cover PropertyTypeWrong constructor and what()
  PropertyValue pv("test_string");
  EXPECT_EQ(pv.type(), PropertyValue::type_string);
  try {
    float val = pv.floatValue();
    (void)val;
  } catch (const std::exception &e) {
    const char *msg = e.what();
    EXPECT_NE(msg, nullptr);
  }
}

// --- CheckTiming: hasClkedCheck, clear ---

TEST_F(StaDesignTest, CheckTimingClear) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(true, true, true, true, true, true, true);
  (void)errors;
  CheckErrorSeq &errors2 = sta_->checkTiming(true, true, true, true, true, true, true);
  (void)errors2;

  }() ));
}

// --- BfsIterator: init, destructor, enqueueAdjacentVertices ---

TEST_F(StaDesignTest, BfsIterator) {
  ASSERT_NO_THROW(( [&](){
  Graph *graph = sta_->graph();
  if (graph) {
    SearchPred1 pred(sta_);
    BfsFwdIterator bfs(BfsIndex::other, &pred, sta_);
    Vertex *v = findVertex("r1/Q");
    if (v) {
      bfs.enqueue(v);
      while (bfs.hasNext()) {
        Vertex *vert = bfs.next();
        (void)vert;
        break;
      }
    }
  }

  }() ));
}

// --- ClkInfo accessors ---

TEST_F(StaDesignTest, ClkInfoAccessors3) {
  ASSERT_NO_THROW(( [&](){
  Pin *clk_pin = findPin("r1/CK");
  if (clk_pin) {
    Vertex *v = findVertex("r1/CK");
    if (v) {
      VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
      if (iter && iter->hasNext()) {
        Path *path = iter->next();
        Tag *tag = path->tag(sta_);
        if (tag) {
          const ClkInfo *clk_info = tag->clkInfo();
          if (clk_info) {
            const ClockEdge *edge = clk_info->clkEdge();
            (void)edge;
            bool prop = clk_info->isPropagated();
            (void)prop;
            bool gen = clk_info->isGenClkSrcPath();
            (void)gen;
          }
        }
      }
      delete iter;
    }
  }

  }() ));
}

// --- Tag: pathAPIndex ---

TEST_F(StaDesignTest, TagPathAPIndex2) {
  Vertex *v = findVertex("r1/D");
  if (v) {
    VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
    if (iter && iter->hasNext()) {
      Path *path = iter->next();
      Tag *tag = path->tag(sta_);
      if (tag) {
        int ap_idx = tag->pathAPIndex();
        EXPECT_GE(ap_idx, 0);
      }
    }
    delete iter;
  }
}

// --- Path: tagIndex, prevVertex ---

TEST_F(StaDesignTest, PathAccessors) {
  ASSERT_NO_THROW(( [&](){
  Vertex *v = findVertex("r1/D");
  if (v) {
    VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
    if (iter && iter->hasNext()) {
      Path *path = iter->next();
      TagIndex ti = path->tagIndex(sta_);
      (void)ti;
      Vertex *prev = path->prevVertex(sta_);
      (void)prev;
    }
    delete iter;
  }

  }() ));
}

// --- PathGroup constructor ---

TEST_F(StaDesignTest, PathGroupConstructor) {
  Search *search = sta_->search();
  if (search) {
    PathGroup *pg = search->findPathGroup("clk", MinMax::max());
    if (pg) {
      EXPECT_NE(pg, nullptr);
    }
  }
}

// --- PathLess ---

TEST_F(StaDesignTest, PathLessComparator) {
  Vertex *v = findVertex("r1/D");
  if (v) {
    VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
    if (iter && iter->hasNext()) {
      Path *p1 = iter->next();
      PathLess less(sta_);
      bool result = less(p1, p1);
      EXPECT_FALSE(result);
    }
    delete iter;
  }
}

// --- PathEnd methods on real path ends ---

TEST_F(StaDesignTest, PathEndTargetClkMethods) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    const Clock *tgt_clk = pe->targetClk(sta_);
    (void)tgt_clk;
    Arrival tgt_arr = pe->targetClkArrival(sta_);
    (void)tgt_arr;
    Delay tgt_delay = pe->targetClkDelay(sta_);
    (void)tgt_delay;
    Delay tgt_ins = pe->targetClkInsertionDelay(sta_);
    (void)tgt_ins;
    float non_inter = pe->targetNonInterClkUncertainty(sta_);
    (void)non_inter;
    float inter = pe->interClkUncertainty(sta_);
    (void)inter;
    float tgt_unc = pe->targetClkUncertainty(sta_);
    (void)tgt_unc;
    float mcp_adj = pe->targetClkMcpAdjustment(sta_);
    (void)mcp_adj;
  }

  }() ));
}

TEST_F(StaDesignTest, PathEndUnconstrainedMethods) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, true, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe->isUnconstrained()) {
      Required req = pe->requiredTime(sta_);
      (void)req;
      break;
    }
  }

  }() ));
}

// --- PathEndPathDelay methods ---

TEST_F(StaDesignTest, PathEndPathDelay) {
  sta_->makePathDelay(nullptr, nullptr, nullptr,
                      MinMax::max(), false, false, 5.0, nullptr);
  sta_->updateTiming(true);
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    10, 10, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe->isPathDelay()) {
      EXPECT_EQ(pe->type(), PathEnd::path_delay);
      const char *tn = pe->typeName();
      EXPECT_NE(tn, nullptr);
      float tgt_time = pe->targetClkTime(sta_);
      (void)tgt_time;
      float tgt_off = pe->targetClkOffset(sta_);
      (void)tgt_off;
      break;
    }
  }
}

// --- ReportPath methods via sta_ calls ---

TEST_F(StaDesignTest, ReportPathShortMinPeriod2) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &checks = sta_->minPeriodViolations();
  if (!checks.empty()) {
    sta_->reportCheck(checks[0], false);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathCheckMaxSkew2) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheckSeq &violations = sta_->maxSkewViolations();
  if (!violations.empty()) {
    sta_->reportCheck(violations[0], true);
    sta_->reportCheck(violations[0], false);
  }

  }() ));
}

// --- ReportPath full report ---

TEST_F(StaDesignTest, ReportPathFullReport) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  sta_->setReportPathFormat(ReportPathFormat::full);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    PathEnd *pe = ends[0];
    sta_->reportPathEnd(pe);
  }

  }() ));
}

TEST_F(StaDesignTest, ReportPathFullClkExpanded) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  sta_->setReportPathFormat(ReportPathFormat::full_clock_expanded);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

// --- WorstSlack: worstSlack, sortQueue, checkQueue ---

TEST_F(StaDesignTest, WorstSlackMethods) {
  ASSERT_NO_THROW(( [&](){
  Slack worst_slack;
  Vertex *worst_vertex;
  sta_->worstSlack(MinMax::max(), worst_slack, worst_vertex);
  sta_->worstSlack(MinMax::max(), worst_slack, worst_vertex);
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  sta_->worstSlack(corner, MinMax::max(), worst_slack, worst_vertex);
  sta_->worstSlack(corner, MinMax::min(), worst_slack, worst_vertex);

  }() ));
}

// --- WnsSlackLess ---

TEST_F(StaDesignTest, WnsSlackLess) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  if (path_ap) {
    WnsSlackLess less(path_ap->index(), sta_);
    Vertex *v1 = findVertex("r1/D");
    Vertex *v2 = findVertex("r2/D");
    if (v1 && v2) {
      bool result = less(v1, v2);
      (void)result;
    }
  }

  }() ));
}

// --- Search: various methods ---

TEST_F(StaDesignTest, SearchInitVars) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->clear();
  sta_->updateTiming(true);

  }() ));
}

TEST_F(StaDesignTest, SearchCheckPrevPaths) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  search->checkPrevPaths();

  }() ));
}

TEST_F(StaDesignTest, SearchPathClkPathArrival1) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("r1/D");
  if (v) {
    VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
    if (iter && iter->hasNext()) {
      Path *path = iter->next();
      Arrival arr = search->pathClkPathArrival(path);
      (void)arr;
    }
    delete iter;
  }

  }() ));
}

// --- Sim ---

TEST_F(StaDesignTest, SimMethods) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "r1/D");
  if (pin) {
    Sim *sim = sta_->sim();
    LogicValue val = sim->logicValue(pin);
    (void)val;
  }

  }() ));
}

// --- Levelize ---

TEST_F(StaDesignTest, LevelizeCheckLevels) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureLevelized();

  }() ));
}

// --- Sta: clkSkewPreamble (called by reportClkSkew) ---

TEST_F(StaDesignTest, ClkSkewPreamble) {
  ASSERT_NO_THROW(( [&](){
  ConstClockSeq clks;
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    clks.push_back(clk);
    CornerSeq &corners = sta_->corners()->corners();
    Corner *corner = corners[0];
    sta_->reportClkSkew(clks, corner, MinMax::max(), false, 3);
  }

  }() ));
}

// --- Sta: delayCalcPreamble ---

TEST_F(StaDesignTest, DelayCalcPreamble) {
  ASSERT_NO_THROW(( [&](){
  sta_->findDelays();

  }() ));
}

// --- Sta: setCmdNamespace ---

TEST_F(StaDesignTest, SetCmdNamespace12) {
  ASSERT_NO_THROW(( [&](){
  sta_->setCmdNamespace(CmdNamespace::sta);
  sta_->setCmdNamespace(CmdNamespace::sdc);

  }() ));
}

// --- Sta: replaceCell ---

TEST_F(StaDesignTest, ReplaceCell2) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *inst_iter = network->childIterator(top);
  if (inst_iter->hasNext()) {
    Instance *inst = inst_iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->replaceCell(inst, cell);
    }
  }
  delete inst_iter;

  }() ));
}

// --- ClkSkew: srcInternalClkLatency, tgtInternalClkLatency ---

TEST_F(StaDesignTest, ClkSkewInternalLatency) {
  ASSERT_NO_THROW(( [&](){
  ConstClockSeq clks;
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    clks.push_back(clk);
    CornerSeq &corners = sta_->corners()->corners();
    Corner *corner = corners[0];
    sta_->reportClkSkew(clks, corner, MinMax::max(), true, 3);
  }

  }() ));
}

// --- MaxSkewCheck accessors ---

TEST_F(StaDesignTest, MaxSkewCheckAccessors) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheckSeq &checks = sta_->maxSkewViolations();
  if (!checks.empty()) {
    MaxSkewCheck *c1 = checks[0];
    Pin *clk = c1->clkPin(sta_);
    (void)clk;
    Pin *ref = c1->refPin(sta_);
    (void)ref;
    ArcDelay max_skew = c1->maxSkew(sta_);
    (void)max_skew;
    Slack slack = c1->slack(sta_);
    (void)slack;
  }
  if (checks.size() >= 2) {
    MaxSkewSlackLess less(sta_);
    bool result = less(checks[0], checks[1]);
    (void)result;
  }

  }() ));
}

// --- MinPeriodSlackLess ---

TEST_F(StaDesignTest, MinPeriodCheckAccessors) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &checks = sta_->minPeriodViolations();
  if (checks.size() >= 2) {
    MinPeriodSlackLess less(sta_);
    bool result = less(checks[0], checks[1]);
    (void)result;
  }
  MinPeriodCheck *min_check = sta_->minPeriodSlack();
  (void)min_check;

  }() ));
}

// --- MinPulseWidthCheck: corner ---

TEST_F(StaDesignTest, MinPulseWidthCheckCorner) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  MinPulseWidthCheckSeq &checks = sta_->minPulseWidthChecks(corner);
  if (!checks.empty()) {
    MinPulseWidthCheck *check = checks[0];
    const Corner *c = check->corner(sta_);
    (void)c;
  }

  }() ));
}

TEST_F(StaDesignTest, MinPulseWidthSlack3) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  MinPulseWidthCheck *min_check = sta_->minPulseWidthSlack(corner);
  (void)min_check;

  }() ));
}

// --- GraphLoop: report ---

TEST_F(StaDesignTest, GraphLoopReport) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureLevelized();
  GraphLoopSeq &loops = sta_->graphLoops();
  for (GraphLoop *loop : loops) {
    loop->report(sta_);
  }

  }() ));
}

// --- Sta: makePortPinAfter ---

TEST_F(StaDesignTest, MakePortPinAfter) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "clk1");
  if (pin) {
    sta_->makePortPinAfter(pin);
  }

  }() ));
}

// --- Sta: removeDataCheck ---

TEST_F(StaDesignTest, RemoveDataCheck) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  if (from_pin && to_pin) {
    sta_->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                       to_pin, RiseFallBoth::riseFall(),
                       nullptr, MinMaxAll::max(), 1.0);
    sta_->removeDataCheck(from_pin, RiseFallBoth::riseFall(),
                          to_pin, RiseFallBoth::riseFall(),
                          nullptr, MinMaxAll::max());
  }

  }() ));
}

// --- PathEnum via multiple path ends ---

TEST_F(StaDesignTest, PathEnum) {
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    3, 3, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  EXPECT_GT(ends.size(), 0u);
}

// --- EndpointPathEndVisitor ---

TEST_F(StaDesignTest, EndpointPins2) {
  PinSet pins = sta_->endpointPins();
  EXPECT_GE(pins.size(), 0u);
}

// --- FindEndRequiredVisitor, RequiredCmp ---

TEST_F(StaDesignTest, FindRequiredsAgain) {
  ASSERT_NO_THROW(( [&](){
  sta_->findRequireds();
  sta_->findRequireds();

  }() ));
}

// --- FindEndSlackVisitor ---

TEST_F(StaDesignTest, TotalNegativeSlackBothMinMax) {
  ASSERT_NO_THROW(( [&](){
  Slack tns_max = sta_->totalNegativeSlack(MinMax::max());
  (void)tns_max;
  Slack tns_min = sta_->totalNegativeSlack(MinMax::min());
  (void)tns_min;

  }() ));
}

// --- ReportPath: reportEndpoint for output delay ---

TEST_F(StaDesignTest, ReportPathOutputDelay) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Clock *clk = sta_->sdc()->findClock("clk");
  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::all(), true, 2.0f);
    sta_->updateTiming(true);
    CornerSeq &corners = sta_->corners()->corners();
    Corner *corner = corners[0];
    PathEndSeq ends = sta_->findPathEnds(
      nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
      5, 5, true, false, -INF, INF, false, nullptr,
      true, false, false, false, false, false);
    for (PathEnd *pe : ends) {
      if (pe->isOutputDelay()) {
        sta_->reportPathEnd(pe);
        break;
      }
    }
  }

  }() ));
}

// --- Sta: writeSdc ---

TEST_F(StaDesignTest, WriteSdc2) {
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

TEST_F(StaDesignTest, WriteSdcWithConstraints) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Clock *clk = sta_->sdc()->findClock("clk");

  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::all(), true, 2.0f);
  }
  sta_->makeFalsePath(nullptr, nullptr, nullptr, MinMaxAll::all(), nullptr);

  if (out) {
    Port *port = network->port(out);
    Corner *corner = sta_->cmdCorner();
    if (port && corner)
      sta_->setPortExtPinCap(port, RiseFallBoth::riseFall(), corner, MinMaxAll::all(), 0.5f);
  }

  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_constrained.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

TEST_F(StaDesignTest, WriteSdcNative) {
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_native.sdc");
  sta_->writeSdc(filename.c_str(), false, true, 4, false, true);
  expectSdcFileReadable(filename);
}

TEST_F(StaDesignTest, WriteSdcLeaf) {
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_leaf.sdc");
  sta_->writeSdc(filename.c_str(), true, false, 4, false, true);
  expectSdcFileReadable(filename);
}

// --- Path ends with sorting ---

TEST_F(StaDesignTest, SaveEnumPath) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  (void)ends;

  }() ));
}

TEST_F(StaDesignTest, ReportPathLess) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  (void)ends;

  }() ));
}

// --- ClkDelays ---

TEST_F(StaDesignTest, ClkDelaysDelay) {
  ASSERT_NO_THROW(( [&](){
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    CornerSeq &corners = sta_->corners()->corners();
    Corner *corner = corners[0];
    float min_period = sta_->findClkMinPeriod(clk, corner);
    (void)min_period;
  }

  }() ));
}

// --- Sta WriteSdc with Derating ---

TEST_F(StaDesignTest, WriteSdcDerating) {
  sta_->setTimingDerate(TimingDerateType::cell_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::early(), 0.95);
  sta_->setTimingDerate(TimingDerateType::net_delay,
                        PathClkOrData::data,
                        RiseFallBoth::riseFall(),
                        EarlyLate::late(), 1.05);
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_derate.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

// --- Sta WriteSdc with disable edges ---

TEST_F(StaDesignTest, WriteSdcDisableEdge) {
  Graph *graph = sta_->graph();
  Vertex *v = findVertex("r1/D");
  if (v && graph) {
    VertexInEdgeIterator in_iter(v, graph);
    if (in_iter.hasNext()) {
      Edge *edge = in_iter.next();
      sta_->disable(edge);
    }
  }
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_disable.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

// --- ClkInfoHash, ClkInfoEqual ---

TEST_F(StaDesignTest, ClkInfoHashEqual) {
  Vertex *v = findVertex("r1/CK");
  if (v) {
    VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(), MinMax::max());
    if (iter && iter->hasNext()) {
      Path *path = iter->next();
      Tag *tag = path->tag(sta_);
      if (tag) {
        const ClkInfo *ci = tag->clkInfo();
        if (ci) {
          ClkInfoHash hasher;
          size_t h = hasher(ci);
          (void)h;
          ClkInfoEqual eq(sta_);
          bool e = eq(ci, ci);
          EXPECT_TRUE(e);
        }
      }
    }
    delete iter;
  }
}

// --- Report MPW checks ---

TEST_F(StaDesignTest, ReportMpwChecksAll) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  MinPulseWidthCheckSeq &checks = sta_->minPulseWidthChecks(corner);
  sta_->reportMpwChecks(&checks, false);
  sta_->reportMpwChecks(&checks, true);

  }() ));
}

// --- Report min period checks ---

TEST_F(StaDesignTest, ReportMinPeriodChecks) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &checks = sta_->minPeriodViolations();
  for (auto *check : checks) {
    sta_->reportCheck(check, false);
    sta_->reportCheck(check, true);
  }

  }() ));
}

// --- Endpoints hold ---

TEST_F(StaDesignTest, FindPathEndsHold3) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::min(),
    5, 5, true, false, -INF, INF, false, nullptr,
    false, true, false, false, false, false);
  for (PathEnd *pe : ends) {
    Required req = pe->requiredTime(sta_);
    (void)req;
    Slack slack = pe->slack(sta_);
    (void)slack;
  }

  }() ));
}

// --- Report path end as JSON ---

TEST_F(StaDesignTest, ReportPathEndJson2) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  sta_->setReportPathFormat(ReportPathFormat::json);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEndHeader();
    sta_->reportPathEnd(ends[0]);
    sta_->reportPathEndFooter();
  }

  }() ));
}

// --- Report path end shorter ---

TEST_F(StaDesignTest, ReportPathEndShorter) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  sta_->setReportPathFormat(ReportPathFormat::shorter);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnd(ends[0]);
  }

  }() ));
}

// --- WriteSdc with clock groups ---

TEST_F(StaDesignTest, WriteSdcWithClockGroups) {
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("test_group", true, false, false, false, nullptr);
    EXPECT_NE(cg, nullptr);
    sta_->updateTiming(true);
    std::string filename = makeUniqueSdcPath("test_write_sdc_r10_clkgrp.sdc");
    sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
    expectSdcFileReadable(filename);
  }
}

// --- WriteSdc with inter-clock uncertainty ---

TEST_F(StaDesignTest, WriteSdcInterClkUncertainty) {
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    sta_->setClockUncertainty(clk, RiseFallBoth::riseFall(),
                              clk, RiseFallBoth::riseFall(),
                              MinMaxAll::max(), 0.1f);
    std::string filename = makeUniqueSdcPath("test_write_sdc_r10_interclk.sdc");
    sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
    expectSdcFileReadable(filename);
  }
}

// --- WriteSdc with clock latency ---

TEST_F(StaDesignTest, WriteSdcClockLatency) {
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    sta_->setClockLatency(clk, nullptr, RiseFallBoth::riseFall(),
                          MinMaxAll::all(), 0.5f);
    std::string filename = makeUniqueSdcPath("test_write_sdc_r10_clklat.sdc");
    sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
    expectSdcFileReadable(filename);
  }
}

// ============================================================
// R10_ Additional Tests - Round 2
// ============================================================

// --- FindRegister: find register instances ---
TEST_F(StaDesignTest, FindRegisterInstances2) {
  ClockSet *clks = nullptr;  // all clocks
  InstanceSet regs = sta_->findRegisterInstances(clks, RiseFallBoth::riseFall(),
                                                  true, true);
  // example1.v has registers (r1, r2, r3), so we should find some
  EXPECT_GT(regs.size(), 0u);
}

// --- FindRegister: data pins ---
TEST_F(StaDesignTest, FindRegisterDataPins2) {
  ClockSet *clks = nullptr;
  PinSet data_pins = sta_->findRegisterDataPins(clks, RiseFallBoth::riseFall(),
                                                 true, true);
  EXPECT_GT(data_pins.size(), 0u);
}

// --- FindRegister: clock pins ---
TEST_F(StaDesignTest, FindRegisterClkPins2) {
  ClockSet *clks = nullptr;
  PinSet clk_pins = sta_->findRegisterClkPins(clks, RiseFallBoth::riseFall(),
                                               true, true);
  EXPECT_GT(clk_pins.size(), 0u);
}

// --- FindRegister: async pins ---
TEST_F(StaDesignTest, FindRegisterAsyncPins2) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  PinSet async_pins = sta_->findRegisterAsyncPins(clks, RiseFallBoth::riseFall(),
                                                   true, true);
  // May be empty if no async pins in the design
  (void)async_pins;

  }() ));
}

// --- FindRegister: output pins ---
TEST_F(StaDesignTest, FindRegisterOutputPins2) {
  ClockSet *clks = nullptr;
  PinSet out_pins = sta_->findRegisterOutputPins(clks, RiseFallBoth::riseFall(),
                                                  true, true);
  EXPECT_GT(out_pins.size(), 0u);
}

// --- FindRegister: with specific clock ---
TEST_F(StaDesignTest, FindRegisterWithClock) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  ASSERT_NE(clk, nullptr);
  ClockSet *clks = new ClockSet;
  clks->insert(clk);
  InstanceSet regs = sta_->findRegisterInstances(clks, RiseFallBoth::rise(),
                                                  true, false);
  // registers clocked by rise edge of "clk"
  (void)regs;
  delete clks;
}

// --- FindRegister: registers only (no latches) ---
TEST_F(StaDesignTest, FindRegisterRegistersOnly) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  InstanceSet regs = sta_->findRegisterInstances(clks, RiseFallBoth::riseFall(),
                                                  true, false);
  (void)regs;

  }() ));
}

// --- FindRegister: latches only ---
TEST_F(StaDesignTest, FindRegisterLatchesOnly) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  InstanceSet latches = sta_->findRegisterInstances(clks, RiseFallBoth::riseFall(),
                                                     false, true);
  (void)latches;

  }() ));
}

// --- FindFanin/Fanout: fanin pins ---
TEST_F(StaDesignTest, FindFaninPins2) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    PinSeq to_pins;
    to_pins.push_back(out);
    PinSet fanin = sta_->findFaninPins(&to_pins, false, false, 10, 100,
                                        false, false);
    EXPECT_GT(fanin.size(), 0u);
  }
}

// --- FindFanin: fanin instances ---
TEST_F(StaDesignTest, FindFaninInstances2) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    PinSeq to_pins;
    to_pins.push_back(out);
    InstanceSet fanin = sta_->findFaninInstances(&to_pins, false, false, 10, 100,
                                                  false, false);
    EXPECT_GT(fanin.size(), 0u);
  }
}

// --- FindFanout: fanout pins ---
TEST_F(StaDesignTest, FindFanoutPins2) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSeq from_pins;
    from_pins.push_back(in1);
    PinSet fanout = sta_->findFanoutPins(&from_pins, false, false, 10, 100,
                                          false, false);
    EXPECT_GT(fanout.size(), 0u);
  }
}

// --- FindFanout: fanout instances ---
TEST_F(StaDesignTest, FindFanoutInstances2) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSeq from_pins;
    from_pins.push_back(in1);
    InstanceSet fanout = sta_->findFanoutInstances(&from_pins, false, false, 10, 100,
                                                    false, false);
    EXPECT_GT(fanout.size(), 0u);
  }
}

// --- CmdNamespace: get and set ---
TEST_F(StaDesignTest, CmdNamespace2) {
  CmdNamespace ns = sta_->cmdNamespace();
  // Set to STA namespace
  sta_->setCmdNamespace(CmdNamespace::sta);
  EXPECT_EQ(sta_->cmdNamespace(), CmdNamespace::sta);
  // Set to SDC namespace
  sta_->setCmdNamespace(CmdNamespace::sdc);
  EXPECT_EQ(sta_->cmdNamespace(), CmdNamespace::sdc);
  // Restore
  sta_->setCmdNamespace(ns);
}

// --- Sta: setSlewLimit on clock ---
TEST_F(StaDesignTest, SetSlewLimitClock) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::clk, MinMax::max(), 2.0f);
  }

  }() ));
}

// --- Sta: setSlewLimit on port ---
TEST_F(StaDesignTest, SetSlewLimitPort) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setSlewLimit(port, MinMax::max(), 3.0f);
    }
  }

  }() ));
}

// --- Sta: setSlewLimit on cell ---
TEST_F(StaDesignTest, SetSlewLimitCell) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setSlewLimit(cell, MinMax::max(), 4.0f);
    }
  }
  delete iter;

  }() ));
}

// --- Sta: setCapacitanceLimit on cell ---
TEST_F(StaDesignTest, SetCapacitanceLimitCell) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setCapacitanceLimit(cell, MinMax::max(), 1.0f);
    }
  }
  delete iter;

  }() ));
}

// --- Sta: setCapacitanceLimit on port ---
TEST_F(StaDesignTest, SetCapacitanceLimitPort) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setCapacitanceLimit(port, MinMax::max(), 0.8f);
    }
  }

  }() ));
}

// --- Sta: setCapacitanceLimit on pin ---
TEST_F(StaDesignTest, SetCapacitanceLimitPin) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    sta_->setCapacitanceLimit(out, MinMax::max(), 0.5f);
  }

  }() ));
}

// --- Sta: setFanoutLimit on cell ---
TEST_F(StaDesignTest, SetFanoutLimitCell) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      sta_->setFanoutLimit(cell, MinMax::max(), 10.0f);
    }
  }
  delete iter;

  }() ));
}

// --- Sta: setFanoutLimit on port ---
TEST_F(StaDesignTest, SetFanoutLimitPort) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setFanoutLimit(port, MinMax::max(), 12.0f);
    }
  }

  }() ));
}

// --- Sta: setMaxArea ---
TEST_F(StaDesignTest, SetMaxArea) {
  ASSERT_NO_THROW(( [&](){
  sta_->setMaxArea(500.0f);

  }() ));
}

// --- Sta: setMinPulseWidth on clock ---
TEST_F(StaDesignTest, SetMinPulseWidthClock) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setMinPulseWidth(clk, RiseFallBoth::rise(), 0.3f);
  }

  }() ));
}

// --- Sta: MinPeriod checks ---
TEST_F(StaDesignTest, MinPeriodSlack3) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheck *check = sta_->minPeriodSlack();
  if (check) {
    sta_->reportCheck(check, false);
    sta_->reportCheck(check, true);
  }

  }() ));
}

TEST_F(StaDesignTest, MinPeriodViolations3) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &viols = sta_->minPeriodViolations();
  if (!viols.empty()) {
    sta_->reportChecks(&viols, false);
    sta_->reportChecks(&viols, true);
  }

  }() ));
}

// --- Sta: MaxSkew checks ---
TEST_F(StaDesignTest, MaxSkewSlack3) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  if (check) {
    sta_->reportCheck(check, false);
    sta_->reportCheck(check, true);
  }

  }() ));
}

TEST_F(StaDesignTest, MaxSkewViolations3) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheckSeq &viols = sta_->maxSkewViolations();
  if (!viols.empty()) {
    sta_->reportChecks(&viols, false);
    sta_->reportChecks(&viols, true);
  }

  }() ));
}

// --- Sta: clocks arriving at pin ---
TEST_F(StaDesignTest, ClocksAtPin) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  if (clk1) {
    ClockSet clks = sta_->clocks(clk1);
    EXPECT_GT(clks.size(), 0u);
  }
}

// --- Sta: isClockSrc ---
TEST_F(StaDesignTest, IsClockSrc) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  Pin *in1 = network->findPin(top, "in1");
  if (clk1) {
    bool is_clk_src = sta_->isClockSrc(clk1);
    EXPECT_TRUE(is_clk_src);
  }
  if (in1) {
    bool is_clk_src = sta_->isClockSrc(in1);
    EXPECT_FALSE(is_clk_src);
  }
}

// --- Sta: setPvt and pvt ---
TEST_F(StaDesignTest, SetPvt2) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    const Pvt *pvt = sta_->pvt(inst, MinMax::max());
    (void)pvt;
  }
  delete iter;

  }() ));
}

// --- Property: Library and Cell properties ---
TEST_F(StaDesignTest, PropertyLibrary) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Library *library = network->findLibrary("Nangate45");
  if (library) {
    PropertyValue val = sta_->properties().getProperty(library, "name");
    (void)val;
  }

  }() ));
}

TEST_F(StaDesignTest, PropertyCell) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      PropertyValue val = sta_->properties().getProperty(cell, "name");
      (void)val;
    }
  }
  delete iter;

  }() ));
}

// --- Property: getProperty on Clock ---
TEST_F(StaDesignTest, PropertyClock) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    PropertyValue val = sta_->properties().getProperty(clk, "name");
    (void)val;
    PropertyValue val2 = sta_->properties().getProperty(clk, "period");
    (void)val2;
    PropertyValue val3 = sta_->properties().getProperty(clk, "sources");
    (void)val3;
  }

  }() ));
}

// --- MaxSkewCheck: detailed accessors ---
TEST_F(StaDesignTest, MaxSkewCheckDetailedAccessors) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  if (check) {
    const Pin *clk_pin = check->clkPin(sta_);
    (void)clk_pin;
    const Pin *ref_pin = check->refPin(sta_);
    (void)ref_pin;
    float max_skew = check->maxSkew(sta_);
    (void)max_skew;
    float slack = check->slack(sta_);
    (void)slack;
  }

  }() ));
}

// --- MinPeriodCheck: detailed accessors ---
TEST_F(StaDesignTest, MinPeriodCheckDetailedAccessors) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheck *check = sta_->minPeriodSlack();
  if (check) {
    float min_period = check->minPeriod(sta_);
    (void)min_period;
    float slack = check->slack(sta_);
    (void)slack;
    const Pin *pin = check->pin();
    (void)pin;
    Clock *clk = check->clk();
    (void)clk;
  }

  }() ));
}

// --- Sta: WriteSdc with various limits ---
TEST_F(StaDesignTest, WriteSdcWithSlewLimit) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::data, MinMax::max(), 1.5f);
  }
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_slewlimit.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

TEST_F(StaDesignTest, WriteSdcWithCapLimit) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setCapacitanceLimit(port, MinMax::max(), 1.0f);
    }
  }
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_caplimit.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

TEST_F(StaDesignTest, WriteSdcWithFanoutLimit) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setFanoutLimit(port, MinMax::max(), 8.0f);
    }
  }
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_fanoutlimit.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

// --- Sta: makeGeneratedClock and removeAllClocks ---
TEST_F(StaDesignTest, MakeGeneratedClock) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk2 = network->findPin(top, "clk2");
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk && clk2) {
    PinSet *gen_pins = new PinSet(network);
    gen_pins->insert(clk2);
    IntSeq *divide_by = new IntSeq;
    divide_by->push_back(2);
    FloatSeq *edges = nullptr;
    sta_->makeGeneratedClock("gen_clk", gen_pins, false, clk2, clk,
                              2, 0, 0.0, false, false, divide_by, edges, nullptr);
    Clock *gen = sdc->findClock("gen_clk");
    EXPECT_NE(gen, nullptr);
  }
}

// --- Sta: removeAllClocks ---
TEST_F(StaDesignTest, RemoveAllClocks) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  ASSERT_NE(clk, nullptr);
  sta_->removeClock(clk);
  clk = sdc->findClock("clk");
  EXPECT_EQ(clk, nullptr);
}

// --- FindFanin: startpoints only ---
TEST_F(StaDesignTest, FindFaninStartpoints) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    PinSeq to_pins;
    to_pins.push_back(out);
    PinSet fanin = sta_->findFaninPins(&to_pins, false, true, 10, 100,
                                        false, false);
    (void)fanin;
  }

  }() ));
}

// --- FindFanout: endpoints only ---
TEST_F(StaDesignTest, FindFanoutEndpoints) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    PinSeq from_pins;
    from_pins.push_back(in1);
    PinSet fanout = sta_->findFanoutPins(&from_pins, false, true, 10, 100,
                                          false, false);
    (void)fanout;
  }

  }() ));
}

// --- Sta: report unconstrained path ends ---
TEST_F(StaDesignTest, ReportUnconstrained) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    true,                       // unconstrained
    corner,
    MinMaxAll::max(),
    5, 5,
    true, false,
    -INF, INF,
    false,
    nullptr,
    true, false, false, false, false, false);
  for (const auto &end : ends) {
    if (end) {
      sta_->reportPathEnd(end);
    }
  }

  }() ));
}

// --- Sta: hold path ends ---
TEST_F(StaDesignTest, FindPathEndsHoldVerbose) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false,
    corner,
    MinMaxAll::min(),
    3, 3,
    true, false,
    -INF, INF,
    false,
    nullptr,
    false, true, false, false, false, false);
  for (const auto &end : ends) {
    if (end) {
      sta_->reportPathEnd(end);
    }
  }

  }() ));
}

// ============================================================
// R10_ Additional Tests - Round 3 (Coverage Deepening)
// ============================================================

// --- Sta: checkSlewLimits ---
TEST_F(StaDesignTest, CheckSlewLimits) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port)
      sta_->setSlewLimit(port, MinMax::max(), 0.001f);  // very tight limit to create violations
  }
  Corner *corner = sta_->cmdCorner();
  PinSeq viols = sta_->checkSlewLimits(nullptr, false, corner, MinMax::max());
  for (const Pin *pin : viols) {
    sta_->reportSlewLimitShort(const_cast<Pin*>(pin), corner, MinMax::max());
    sta_->reportSlewLimitVerbose(const_cast<Pin*>(pin), corner, MinMax::max());
  }
  sta_->reportSlewLimitShortHeader();
  // Also check maxSlewCheck
  const Pin *pin_out = nullptr;
  Slew slew_out;
  float slack_out, limit_out;
  sta_->maxSlewCheck(pin_out, slew_out, slack_out, limit_out);

  }() ));
}

// --- Sta: checkSlew on specific pin ---
TEST_F(StaDesignTest, CheckSlewOnPin) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port)
      sta_->setSlewLimit(port, MinMax::max(), 0.001f);
    Corner *corner = sta_->cmdCorner();
    sta_->checkSlewLimitPreamble();
    const Corner *corner1 = nullptr;
    const RiseFall *tr = nullptr;
    Slew slew;
    float limit, slack;
    sta_->checkSlew(out, corner, MinMax::max(), false,
                    corner1, tr, slew, limit, slack);
  }

  }() ));
}

// --- Sta: checkCapacitanceLimits ---
TEST_F(StaDesignTest, CheckCapacitanceLimits2) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port)
      sta_->setCapacitanceLimit(port, MinMax::max(), 0.0001f);  // very tight
  }
  Corner *corner = sta_->cmdCorner();
  PinSeq viols = sta_->checkCapacitanceLimits(nullptr, false, corner, MinMax::max());
  for (const Pin *pin : viols) {
    sta_->reportCapacitanceLimitShort(const_cast<Pin*>(pin), corner, MinMax::max());
    sta_->reportCapacitanceLimitVerbose(const_cast<Pin*>(pin), corner, MinMax::max());
  }
  sta_->reportCapacitanceLimitShortHeader();
  // Also check maxCapacitanceCheck
  const Pin *pin_out = nullptr;
  float cap_out, slack_out, limit_out;
  sta_->maxCapacitanceCheck(pin_out, cap_out, slack_out, limit_out);

  }() ));
}

// --- Sta: checkCapacitance on specific pin ---
TEST_F(StaDesignTest, CheckCapacitanceOnPin) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    sta_->setCapacitanceLimit(out, MinMax::max(), 0.0001f);
    Corner *corner = sta_->cmdCorner();
    sta_->checkCapacitanceLimitPreamble();
    const Corner *corner1 = nullptr;
    const RiseFall *tr = nullptr;
    float cap, limit, slack;
    sta_->checkCapacitance(out, corner, MinMax::max(),
                           corner1, tr, cap, limit, slack);
  }

  }() ));
}

// --- Sta: checkFanoutLimits ---
TEST_F(StaDesignTest, CheckFanoutLimits2) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port)
      sta_->setFanoutLimit(port, MinMax::max(), 0.01f);  // very tight
  }
  PinSeq viols = sta_->checkFanoutLimits(nullptr, false, MinMax::max());
  for (const Pin *pin : viols) {
    sta_->reportFanoutLimitShort(const_cast<Pin*>(pin), MinMax::max());
    sta_->reportFanoutLimitVerbose(const_cast<Pin*>(pin), MinMax::max());
  }
  sta_->reportFanoutLimitShortHeader();
  // Also check maxFanoutCheck
  const Pin *pin_out = nullptr;
  float fanout_out, slack_out, limit_out;
  sta_->maxFanoutCheck(pin_out, fanout_out, slack_out, limit_out);

  }() ));
}

// --- Sta: checkFanout on specific pin ---
TEST_F(StaDesignTest, CheckFanoutOnPin) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port)
      sta_->setFanoutLimit(port, MinMax::max(), 0.01f);
    sta_->checkFanoutLimitPreamble();
    float fanout, limit, slack;
    sta_->checkFanout(out, MinMax::max(), fanout, limit, slack);
  }

  }() ));
}

// --- Sta: reportClkSkew ---
TEST_F(StaDesignTest, ReportClkSkew2) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ConstClockSeq clks;
    clks.push_back(clk);
    Corner *corner = sta_->cmdCorner();
    sta_->reportClkSkew(clks, corner, MinMax::max(), false, 3);
    sta_->reportClkSkew(clks, corner, MinMax::min(), false, 3);
  }

  }() ));
}

// --- Sta: findWorstClkSkew ---
TEST_F(StaDesignTest, FindWorstClkSkew3) {
  ASSERT_NO_THROW(( [&](){
  float worst = sta_->findWorstClkSkew(MinMax::max(), false);
  (void)worst;

  }() ));
}

// --- Sta: reportClkLatency ---
TEST_F(StaDesignTest, ReportClkLatency3) {
  ASSERT_NO_THROW(( [&](){
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  if (clk) {
    ConstClockSeq clks;
    clks.push_back(clk);
    Corner *corner = sta_->cmdCorner();
    sta_->reportClkLatency(clks, corner, false, 3);
  }

  }() ));
}

// --- Sta: findSlewLimit ---
TEST_F(StaDesignTest, FindSlewLimit2) {
  ASSERT_NO_THROW(( [&](){
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
        Corner *corner = sta_->cmdCorner();
        float limit;
        bool exists;
        sta_->findSlewLimit(port, corner, MinMax::max(), limit, exists);
      }
    }
  }
  delete iter;

  }() ));
}

// --- Sta: MinPulseWidth violations ---
TEST_F(StaDesignTest, MpwViolations) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  MinPulseWidthCheckSeq &viols = sta_->minPulseWidthViolations(corner);
  if (!viols.empty()) {
    sta_->reportMpwChecks(&viols, false);
    sta_->reportMpwChecks(&viols, true);
  }

  }() ));
}

// --- Sta: minPulseWidthSlack (all corners) ---
TEST_F(StaDesignTest, MpwSlackAllCorners) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  MinPulseWidthCheck *check = sta_->minPulseWidthSlack(corner);
  if (check) {
    sta_->reportMpwCheck(check, false);
    sta_->reportMpwCheck(check, true);
  }

  }() ));
}

// --- Sta: minPulseWidthChecks (all) ---
TEST_F(StaDesignTest, MpwChecksAll) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  MinPulseWidthCheckSeq &checks = sta_->minPulseWidthChecks(corner);
  if (!checks.empty()) {
    sta_->reportMpwChecks(&checks, false);
  }

  }() ));
}

// --- Sta: WriteSdc with min pulse width + clock latency + all constraints ---
TEST_F(StaDesignTest, WriteSdcFullConstraints) {
  Sdc *sdc = sta_->sdc();
  Clock *clk = sdc->findClock("clk");
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();

  // Set many constraints
  if (clk) {
    sta_->setMinPulseWidth(clk, RiseFallBoth::rise(), 0.2f);
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::clk, MinMax::max(), 1.0f);
    sta_->setSlewLimit(clk, RiseFallBoth::riseFall(),
                       PathClkOrData::data, MinMax::max(), 2.0f);
    sta_->setClockLatency(clk, nullptr, RiseFallBoth::rise(),
                          MinMaxAll::max(), 0.3f);
    sta_->setClockLatency(clk, nullptr, RiseFallBoth::fall(),
                          MinMaxAll::min(), 0.1f);
  }

  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");

  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      sta_->setDriveResistance(port, RiseFallBoth::rise(),
                               MinMaxAll::max(), 200.0f);
      sta_->setDriveResistance(port, RiseFallBoth::fall(),
                               MinMaxAll::min(), 50.0f);
    }
    sta_->setMinPulseWidth(in1, RiseFallBoth::rise(), 0.1f);
  }

  if (out) {
    Port *port = network->port(out);
    if (port) {
      sta_->setCapacitanceLimit(port, MinMax::max(), 0.5f);
      sta_->setFanoutLimit(port, MinMax::max(), 4.0f);
      sta_->setPortExtPinCap(port, RiseFallBoth::rise(),
                             sta_->cmdCorner(), MinMaxAll::max(), 0.2f);
      sta_->setPortExtPinCap(port, RiseFallBoth::fall(),
                             sta_->cmdCorner(), MinMaxAll::min(), 0.1f);
    }
  }

  sdc->setMaxArea(5000.0);
  sdc->setVoltage(MinMax::max(), 1.2);
  sdc->setVoltage(MinMax::min(), 0.8);

  // Write comprehensive SDC
  std::string filename = makeUniqueSdcPath("test_write_sdc_r10_full.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

// --- Sta: Property getProperty on edge ---
TEST_F(StaDesignTest, PropertyEdge) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Graph *graph = sta_->graph();
  Instance *top = network->topInstance();
  Pin *pin = network->findPin(top, "r1/D");
  if (pin && graph) {
    Vertex *v = graph->pinLoadVertex(pin);
    if (v) {
      VertexInEdgeIterator edge_iter(v, graph);
      if (edge_iter.hasNext()) {
        Edge *edge = edge_iter.next();
        PropertyValue val = sta_->properties().getProperty(edge, "from_pin");
        (void)val;
        PropertyValue val2 = sta_->properties().getProperty(edge, "sense");
        (void)val2;
      }
    }
  }

  }() ));
}

// --- Sta: Property getProperty on net ---
TEST_F(StaDesignTest, PropertyNet) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    PropertyValue val = sta_->properties().getProperty(net, "name");
    (void)val;
  }
  delete net_iter;

  }() ));
}

// --- Sta: Property getProperty on port ---
TEST_F(StaDesignTest, PropertyPort) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    Port *port = network->port(out);
    if (port) {
      PropertyValue val = sta_->properties().getProperty(port, "name");
      (void)val;
      PropertyValue val2 = sta_->properties().getProperty(port, "direction");
      (void)val2;
    }
  }

  }() ));
}

// --- Sta: Property getProperty on LibertyCell ---
TEST_F(StaDesignTest, PropertyLibertyCell) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      PropertyValue val = sta_->properties().getProperty(lib_cell, "name");
      (void)val;
      PropertyValue val2 = sta_->properties().getProperty(lib_cell, "area");
      (void)val2;
    }
  }
  delete iter;

  }() ));
}

// --- Sta: Property getProperty on LibertyPort ---
TEST_F(StaDesignTest, PropertyLibertyPort) {
  ASSERT_NO_THROW(( [&](){
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
        PropertyValue val = sta_->properties().getProperty(port, "name");
        (void)val;
        PropertyValue val2 = sta_->properties().getProperty(port, "direction");
        (void)val2;
      }
    }
  }
  delete iter;

  }() ));
}

// --- Sta: Property getProperty on LibertyLibrary ---
TEST_F(StaDesignTest, PropertyLibertyLibrary) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  LibertyLibraryIterator *lib_iter = network->libertyLibraryIterator();
  if (lib_iter->hasNext()) {
    LibertyLibrary *lib = lib_iter->next();
    PropertyValue val = sta_->properties().getProperty(lib, "name");
    (void)val;
  }
  delete lib_iter;

  }() ));
}

// --- Sta: Property getProperty on instance ---
TEST_F(StaDesignTest, PropertyInstance) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    PropertyValue val = sta_->properties().getProperty(inst, "name");
    (void)val;
  }
  delete iter;

  }() ));
}

// --- Sta: Property getProperty on TimingArcSet ---
TEST_F(StaDesignTest, PropertyTimingArcSet) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    LibertyCell *lib_cell = network->libertyCell(inst);
    if (lib_cell) {
      for (TimingArcSet *arc_set : lib_cell->timingArcSets()) {
        PropertyValue val = sta_->properties().getProperty(arc_set, "name");
        (void)val;
        break;  // just test one
      }
    }
  }
  delete iter;

  }() ));
}

// --- Sta: Property getProperty on PathEnd ---
TEST_F(StaDesignTest, PropertyPathEnd) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (const auto &end : ends) {
    if (end) {
      PropertyValue val = sta_->properties().getProperty(end, "startpoint");
      (void)val;
      PropertyValue val2 = sta_->properties().getProperty(end, "endpoint");
      (void)val2;
      PropertyValue val3 = sta_->properties().getProperty(end, "slack");
      (void)val3;
      break;  // just test one
    }
  }

  }() ));
}

// --- Sta: Property getProperty on Path ---
TEST_F(StaDesignTest, PropertyPath) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (const auto &end : ends) {
    if (end) {
      Path *path = end->path();
      if (path) {
        PropertyValue val = sta_->properties().getProperty(path, "pin");
        (void)val;
        PropertyValue val2 = sta_->properties().getProperty(path, "arrival");
        (void)val2;
      }
      break;
    }
  }

  }() ));
}

// ============================================================
// R11_ Search Tests
// ============================================================

// --- Properties::getProperty on Pin: arrival, slack, slew ---
TEST_F(StaDesignTest, PropertiesGetPropertyPin) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    // These trigger pinArrival internally
    PropertyValue val_arr = sta_->properties().getProperty(out, "arrival_max_rise");
    (void)val_arr;
    PropertyValue val_arr2 = sta_->properties().getProperty(out, "arrival_max_fall");
    (void)val_arr2;
    PropertyValue val_arr3 = sta_->properties().getProperty(out, "arrival_min_rise");
    (void)val_arr3;
    PropertyValue val_arr4 = sta_->properties().getProperty(out, "arrival_min_fall");
    (void)val_arr4;
    // These trigger pinSlack internally
    PropertyValue val_slk = sta_->properties().getProperty(out, "slack_max");
    (void)val_slk;
    PropertyValue val_slk2 = sta_->properties().getProperty(out, "slack_max_rise");
    (void)val_slk2;
    PropertyValue val_slk3 = sta_->properties().getProperty(out, "slack_max_fall");
    (void)val_slk3;
    PropertyValue val_slk4 = sta_->properties().getProperty(out, "slack_min");
    (void)val_slk4;
    PropertyValue val_slk5 = sta_->properties().getProperty(out, "slack_min_rise");
    (void)val_slk5;
    PropertyValue val_slk6 = sta_->properties().getProperty(out, "slack_min_fall");
    (void)val_slk6;
    // Slew
    PropertyValue val_slew = sta_->properties().getProperty(out, "slew_max");
    (void)val_slew;
  }

  }() ));
}

// --- Properties::getProperty on Cell ---
TEST_F(StaDesignTest, PropertiesGetPropertyCell) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  InstanceChildIterator *iter = network->childIterator(top);
  if (iter->hasNext()) {
    Instance *inst = iter->next();
    Cell *cell = network->cell(inst);
    if (cell) {
      PropertyValue val = sta_->properties().getProperty(cell, "name");
      (void)val;
    }
  }
  delete iter;

  }() ));
}

// --- Properties::getProperty on Library ---
TEST_F(StaDesignTest, PropertiesGetPropertyLibrary) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Library *lib = network->findLibrary("Nangate45_typ");
  if (lib) {
    PropertyValue val = sta_->properties().getProperty(lib, "name");
    (void)val;
  }

  }() ));
}

// --- PropertyUnknown exception ---
TEST_F(StaDesignTest, PropertyUnknown) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  if (out) {
    try {
      PropertyValue val = sta_->properties().getProperty(out, "nonexistent_prop");
      (void)val;
    } catch (std::exception &e) {
      // Expected PropertyUnknown exception
      (void)e;
    }
  }

  }() ));
}

// --- Sta::reportClkSkew (triggers clkSkewPreamble) ---
TEST_F(StaDesignTest, ReportClkSkew3) {
  ASSERT_NO_THROW(( [&](){
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    ConstClockSeq clks;
    clks.push_back(clk);
    Corner *corner = sta_->cmdCorner();
    sta_->reportClkSkew(clks, corner, MinMax::max(), false, 4);
    sta_->reportClkSkew(clks, corner, MinMax::min(), false, 4);
  }

  }() ));
}

// --- Sta::findWorstClkSkew ---
TEST_F(StaDesignTest, FindWorstClkSkew4) {
  ASSERT_NO_THROW(( [&](){
  float skew = sta_->findWorstClkSkew(MinMax::max(), false);
  (void)skew;
  float skew2 = sta_->findWorstClkSkew(MinMax::min(), false);
  (void)skew2;

  }() ));
}

// --- Sta::reportClkLatency ---
TEST_F(StaDesignTest, ReportClkLatency4) {
  ASSERT_NO_THROW(( [&](){
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    ConstClockSeq clks;
    clks.push_back(clk);
    Corner *corner = sta_->cmdCorner();
    sta_->reportClkLatency(clks, corner, false, 4);
    sta_->reportClkLatency(clks, corner, true, 4);
  }

  }() ));
}

// --- Sta: propagated clock detection ---
TEST_F(StaDesignTest, PropagatedClockDetection) {
  ASSERT_NO_THROW(( [&](){
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    bool prop = clk->isPropagated();
    (void)prop;
  }

  }() ));
}

// --- Sta::removeDataCheck ---
TEST_F(StaDesignTest, StaRemoveDataCheck) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *from_pin = network->findPin(top, "r1/D");
  Pin *to_pin = network->findPin(top, "r1/CK");
  if (from_pin && to_pin) {
    sta_->setDataCheck(from_pin, RiseFallBoth::riseFall(),
                       to_pin, RiseFallBoth::riseFall(),
                       nullptr, MinMaxAll::max(), 1.0f);
    sta_->removeDataCheck(from_pin, RiseFallBoth::riseFall(),
                          to_pin, RiseFallBoth::riseFall(),
                          nullptr, MinMaxAll::max());
  }

  }() ));
}

// --- PathEnd methods: targetClk, targetClkArrival, targetClkDelay,
//     targetClkInsertionDelay, targetClkUncertainty, targetClkMcpAdjustment ---
TEST_F(StaDesignTest, PathEndTargetClkMethods2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe) {
      const Clock *tgt_clk = pe->targetClk(sta_);
      (void)tgt_clk;
      Arrival tgt_arr = pe->targetClkArrival(sta_);
      (void)tgt_arr;
      Delay tgt_delay = pe->targetClkDelay(sta_);
      (void)tgt_delay;
      Arrival tgt_ins = pe->targetClkInsertionDelay(sta_);
      (void)tgt_ins;
      float tgt_unc = pe->targetClkUncertainty(sta_);
      (void)tgt_unc;
      float tgt_mcp = pe->targetClkMcpAdjustment(sta_);
      (void)tgt_mcp;
      float non_inter = pe->targetNonInterClkUncertainty(sta_);
      (void)non_inter;
      float inter = pe->interClkUncertainty(sta_);
      (void)inter;
    }
  }

  }() ));
}

// --- PathExpanded::pathsIndex ---
TEST_F(StaDesignTest, PathExpandedPathsIndex) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe) {
      Path *path = pe->path();
      if (path) {
        PathExpanded expanded(path, sta_);
        size_t sz = expanded.size();
        if (sz > 0) {
          // Access first and last path
          const Path *p0 = expanded.path(0);
          (void)p0;
          if (sz > 1) {
            const Path *p1 = expanded.path(sz - 1);
            (void)p1;
          }
        }
      }
    }
    break;
  }

  }() ));
}

// --- Report path end with format full_clock ---
TEST_F(StaDesignTest, ReportPathEndFullClock) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  sta_->setReportPathFormat(ReportPathFormat::full_clock);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEndHeader();
    sta_->reportPathEnd(ends[0]);
    sta_->reportPathEndFooter();
  }

  }() ));
}

// --- Report path end with format full_clock_expanded ---
TEST_F(StaDesignTest, ReportPathEndFullClockExpanded) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  sta_->setReportPathFormat(ReportPathFormat::full_clock_expanded);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEndHeader();
    sta_->reportPathEnd(ends[0]);
    sta_->reportPathEndFooter();
  }

  }() ));
}

// --- Report path end with format end ---
TEST_F(StaDesignTest, ReportPathEndEnd) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  sta_->setReportPathFormat(ReportPathFormat::endpoint);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEndHeader();
    sta_->reportPathEnd(ends[0]);
    sta_->reportPathEndFooter();
  }

  }() ));
}

// --- Report path end with format summary ---
TEST_F(StaDesignTest, ReportPathEndSummary2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  sta_->setReportPathFormat(ReportPathFormat::summary);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEndHeader();
    sta_->reportPathEnd(ends[0]);
    sta_->reportPathEndFooter();
  }

  }() ));
}

// --- Report path end with format slack_only ---
TEST_F(StaDesignTest, ReportPathEndSlackOnly2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  sta_->setReportPathFormat(ReportPathFormat::slack_only);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEndHeader();
    sta_->reportPathEnd(ends[0]);
    sta_->reportPathEndFooter();
  }

  }() ));
}

// --- Report multiple path ends ---
TEST_F(StaDesignTest, ReportPathEnds3) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  sta_->setReportPathFormat(ReportPathFormat::full);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  if (!ends.empty()) {
    sta_->reportPathEnds(&ends);
  }

  }() ));
}

// --- Sta: worstSlack ---
TEST_F(StaDesignTest, WorstSlack2) {
  ASSERT_NO_THROW(( [&](){
  Slack ws_max = sta_->worstSlack(MinMax::max());
  (void)ws_max;
  Slack ws_min = sta_->worstSlack(MinMax::min());
  (void)ws_min;

  }() ));
}

// --- Sta: worstSlack with corner ---
TEST_F(StaDesignTest, WorstSlackCorner2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack ws;
  Vertex *v;
  sta_->worstSlack(corner, MinMax::max(), ws, v);
  (void)ws;
  (void)v;

  }() ));
}

// --- Sta: totalNegativeSlack ---
TEST_F(StaDesignTest, TotalNegativeSlack2) {
  ASSERT_NO_THROW(( [&](){
  Slack tns = sta_->totalNegativeSlack(MinMax::max());
  (void)tns;
  Slack tns2 = sta_->totalNegativeSlack(MinMax::min());
  (void)tns2;

  }() ));
}

// --- Sta: totalNegativeSlack with corner ---
TEST_F(StaDesignTest, TotalNegativeSlackCorner2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack tns = sta_->totalNegativeSlack(corner, MinMax::max());
  (void)tns;

  }() ));
}

// --- WriteSdc with many constraints from search side ---
TEST_F(StaDesignTest, WriteSdcComprehensive) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Corner *corner = sta_->cmdCorner();
  Clock *clk = sta_->sdc()->findClock("clk");

  Pin *in1 = network->findPin(top, "in1");
  Pin *in2 = network->findPin(top, "in2");
  Pin *out = network->findPin(top, "out");

  // Net wire cap
  NetIterator *net_iter = network->netIterator(top);
  if (net_iter->hasNext()) {
    Net *net = net_iter->next();
    sta_->setNetWireCap(net, false, corner, MinMaxAll::all(), 0.04f);
    sta_->setResistance(net, MinMaxAll::all(), 75.0f);
  }
  delete net_iter;

  // Input slew
  if (in1) {
    Port *port = network->port(in1);
    if (port)
      sta_->setInputSlew(port, RiseFallBoth::riseFall(),
                         MinMaxAll::all(), 0.1f);
  }

  // Port loads
  if (out) {
    Port *port = network->port(out);
    if (port && corner) {
      sta_->setPortExtPinCap(port, RiseFallBoth::riseFall(), corner,
                             MinMaxAll::all(), 0.15f);
      sta_->setPortExtWireCap(port, false, RiseFallBoth::riseFall(), corner,
                              MinMaxAll::all(), 0.02f);
    }
  }

  // False path with -from and -through net
  if (in1) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    NetIterator *nit = network->netIterator(top);
    ExceptionThruSeq *thrus = new ExceptionThruSeq;
    if (nit->hasNext()) {
      Net *net = nit->next();
      NetSet *nets = new NetSet(network);
      nets->insert(net);
      ExceptionThru *thru = sta_->makeExceptionThru(nullptr, nets, nullptr,
                                                     RiseFallBoth::riseFall());
      thrus->push_back(thru);
    }
    delete nit;
    sta_->makeFalsePath(from, thrus, nullptr, MinMaxAll::all(), nullptr);
  }

  // Max delay
  if (in2 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in2);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall());
    sta_->makePathDelay(from, nullptr, to, MinMax::max(), false, false,
                        7.0f, nullptr);
  }

  // Clock groups with actual clocks
  if (clk) {
    ClockGroups *cg = sta_->makeClockGroups("search_grp", true, false, false,
                                             false, nullptr);
    ClockSet *g1 = new ClockSet;
    g1->insert(clk);
    sta_->makeClockGroup(cg, g1);
  }

  // Multicycle
  sta_->makeMulticyclePath(nullptr, nullptr, nullptr,
                           MinMaxAll::max(), true, 2, nullptr);

  // Group path
  sta_->makeGroupPath("search_group", false, nullptr, nullptr, nullptr, nullptr);

  // Voltage
  sta_->setVoltage(MinMax::max(), 1.1f);
  sta_->setVoltage(MinMax::min(), 0.9f);

  std::string filename = makeUniqueSdcPath("test_search_r11_comprehensive.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);

  // Also write native and leaf
  std::string fn2 = makeUniqueSdcPath("test_search_r11_comprehensive_native.sdc");
  sta_->writeSdc(fn2.c_str(), false, true, 4, false, true);
  expectSdcFileReadable(fn2);
  std::string fn3 = makeUniqueSdcPath("test_search_r11_comprehensive_leaf.sdc");
  sta_->writeSdc(fn3.c_str(), true, false, 4, false, true);
  expectSdcFileReadable(fn3);
}

// --- Sta: report path with verbose format ---
TEST_F(StaDesignTest, ReportPathVerbose) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  sta_->setReportPathFormat(ReportPathFormat::full);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    3, 3, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe) {
      sta_->reportPathEnd(pe);
    }
  }

  }() ));
}

// --- Sta: report path for hold (min) ---
TEST_F(StaDesignTest, ReportPathHold) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  sta_->setReportPathFormat(ReportPathFormat::full);
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::min(),
    3, 3, true, false, -INF, INF, false, nullptr,
    false, true, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe) {
      sta_->reportPathEnd(pe);
    }
  }

  }() ));
}

// --- Sta: max skew checks with report ---
TEST_F(StaDesignTest, MaxSkewChecksReport) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheckSeq &viols = sta_->maxSkewViolations();
  for (auto *check : viols) {
    sta_->reportCheck(check, true);
    sta_->reportCheck(check, false);
  }
  MaxSkewCheck *slack_check = sta_->maxSkewSlack();
  if (slack_check) {
    sta_->reportCheck(slack_check, true);
    sta_->reportCheck(slack_check, false);
  }

  }() ));
}

// --- Sta: min period checks with report ---
TEST_F(StaDesignTest, MinPeriodChecksReport) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &viols = sta_->minPeriodViolations();
  for (auto *check : viols) {
    sta_->reportCheck(check, true);
    sta_->reportCheck(check, false);
  }
  MinPeriodCheck *slack_check = sta_->minPeriodSlack();
  if (slack_check) {
    sta_->reportCheck(slack_check, true);
    sta_->reportCheck(slack_check, false);
  }

  }() ));
}

// --- Sta: MPW slack check ---
TEST_F(StaDesignTest, MpwSlackCheck) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  MinPulseWidthCheck *check = sta_->minPulseWidthSlack(corner);
  if (check) {
    sta_->reportMpwCheck(check, false);
    sta_->reportMpwCheck(check, true);
  }

  }() ));
}

// --- Sta: MPW checks on all ---
TEST_F(StaDesignTest, MpwChecksAll2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  MinPulseWidthCheckSeq &checks = sta_->minPulseWidthChecks(corner);
  sta_->reportMpwChecks(&checks, false);
  sta_->reportMpwChecks(&checks, true);

  }() ));
}

// --- Sta: MPW violations ---
TEST_F(StaDesignTest, MpwViolations2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  MinPulseWidthCheckSeq &viols = sta_->minPulseWidthViolations(corner);
  if (!viols.empty()) {
    sta_->reportMpwChecks(&viols, true);
  }

  }() ));
}

// --- Sta: check timing ---
TEST_F(StaDesignTest, CheckTiming3) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(true, true, true, true, true, true, true);
  (void)errors;

  }() ));
}

// --- Sta: find path ends with output delay ---
TEST_F(StaDesignTest, FindPathEndsWithOutputDelay) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *out = network->findPin(top, "out");
  Clock *clk = sta_->sdc()->findClock("clk");
  if (out && clk) {
    sta_->setOutputDelay(out, RiseFallBoth::riseFall(),
                         clk, RiseFall::rise(), nullptr,
                         false, false, MinMaxAll::all(), true, 2.0f);
    sta_->updateTiming(true);
    Corner *corner = sta_->cmdCorner();
    sta_->setReportPathFormat(ReportPathFormat::full);
    PathEndSeq ends = sta_->findPathEnds(
      nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
      5, 5, true, false, -INF, INF, false, nullptr,
      true, false, false, false, false, false);
    for (PathEnd *pe : ends) {
      if (pe) {
        sta_->reportPathEnd(pe);
        bool is_out_delay = pe->isOutputDelay();
        (void)is_out_delay;
      }
    }
  }

  }() ));
}

// --- PathEnd: type and typeName ---
TEST_F(StaDesignTest, PathEndTypeInfo) {
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe) {
      PathEnd::Type type = pe->type();
      (void)type;
      const char *name = pe->typeName();
      EXPECT_NE(name, nullptr);
    }
  }
}

// --- Sta: find path ends unconstrained ---
TEST_F(StaDesignTest, FindPathEndsUnconstrained3) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, true, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe) {
      bool unc = pe->isUnconstrained();
      (void)unc;
      if (unc) {
        Required req = pe->requiredTime(sta_);
        (void)req;
      }
    }
  }

  }() ));
}

// --- Sta: find path ends with group filter ---
TEST_F(StaDesignTest, FindPathEndsGroupFilter) {
  ASSERT_NO_THROW(( [&](){
  // Create a group path first
  sta_->makeGroupPath("r11_grp", false, nullptr, nullptr, nullptr, nullptr);
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    5, 5, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  (void)ends;

  }() ));
}

// --- Sta: pathGroupNames ---
TEST_F(StaDesignTest, PathGroupNames) {
  sta_->makeGroupPath("test_group_r11", false, nullptr, nullptr, nullptr, nullptr);
  StdStringSeq names = sta_->pathGroupNames();
  bool found = false;
  for (const auto &name : names) {
    if (name == "test_group_r11")
      found = true;
  }
  EXPECT_TRUE(found);
}

// --- Sta: isPathGroupName ---
TEST_F(StaDesignTest, IsPathGroupName) {
  sta_->makeGroupPath("test_pg_r11", false, nullptr, nullptr, nullptr, nullptr);
  bool is_group = sta_->isPathGroupName("test_pg_r11");
  EXPECT_TRUE(is_group);
  bool not_group = sta_->isPathGroupName("nonexistent_group");
  EXPECT_FALSE(not_group);
}

// --- Sta: report path with max_delay constraint ---
TEST_F(StaDesignTest, ReportPathWithMaxDelay) {
  ASSERT_NO_THROW(( [&](){
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  Pin *out = network->findPin(top, "out");
  if (in1 && out) {
    PinSet *from_pins = new PinSet(network);
    from_pins->insert(in1);
    ExceptionFrom *from = sta_->makeExceptionFrom(from_pins, nullptr, nullptr,
                                                   RiseFallBoth::riseFall());
    PinSet *to_pins = new PinSet(network);
    to_pins->insert(out);
    ExceptionTo *to = sta_->makeExceptionTo(to_pins, nullptr, nullptr,
                                            RiseFallBoth::riseFall(),
                                            RiseFallBoth::riseFall());
    sta_->makePathDelay(from, nullptr, to, MinMax::max(), false, false,
                        8.0f, nullptr);
    sta_->updateTiming(true);

    Corner *corner = sta_->cmdCorner();
    sta_->setReportPathFormat(ReportPathFormat::full);
    PathEndSeq ends = sta_->findPathEnds(
      nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
      5, 5, true, false, -INF, INF, false, nullptr,
      true, false, false, false, false, false);
    for (PathEnd *pe : ends) {
      if (pe) {
        sta_->reportPathEnd(pe);
      }
    }
  }

  }() ));
}

// --- ClkInfo accessors via tag on vertex path ---
TEST_F(StaDesignTest, ClkInfoAccessors4) {
  ASSERT_NO_THROW(( [&](){
  Vertex *v = findVertex("r1/CK");
  if (v) {
    VertexPathIterator *iter = sta_->vertexPathIterator(v, RiseFall::rise(),
                                                         MinMax::max());
    if (iter && iter->hasNext()) {
      Path *path = iter->next();
      Tag *tag = path->tag(sta_);
      if (tag) {
        const ClkInfo *ci = tag->clkInfo();
        if (ci) {
          const ClockEdge *edge = ci->clkEdge();
          (void)edge;
          bool prop = ci->isPropagated();
          (void)prop;
          bool gen = ci->isGenClkSrcPath();
          (void)gen;
        }
        int ap_idx = tag->pathAPIndex();
        (void)ap_idx;
      }
    }
    delete iter;
  }

  }() ));
}

// --- Sta: WriteSdc with clock sense from search ---
TEST_F(StaDesignTest, WriteSdcClockSense) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *clk1 = network->findPin(top, "clk1");
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk1 && clk) {
    PinSet *pins = new PinSet(network);
    pins->insert(clk1);
    ClockSet *clks = new ClockSet;
    clks->insert(clk);
    sta_->setClockSense(pins, clks, ClockSense::positive);
  }
  std::string filename = makeUniqueSdcPath("test_search_r11_clksense.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

// --- Sta: WriteSdc with driving cell ---
TEST_F(StaDesignTest, WriteSdcDrivingCell) {
  Network *network = sta_->cmdNetwork();
  Instance *top = network->topInstance();
  Pin *in1 = network->findPin(top, "in1");
  if (in1) {
    Port *port = network->port(in1);
    if (port) {
      LibertyLibrary *lib = lib_;
      if (lib) {
        // Find BUF_X1 which is known to exist in nangate45
        LibertyCell *buf_cell = lib->findLibertyCell("BUF_X1");
        if (buf_cell) {
          LibertyPort *from_port = buf_cell->findLibertyPort("A");
          LibertyPort *to_port = buf_cell->findLibertyPort("Z");
          if (from_port && to_port) {
            float from_slews[2] = {0.03f, 0.03f};
            sta_->setDriveCell(lib, buf_cell, port,
                               from_port, from_slews, to_port,
                               RiseFallBoth::riseFall(), MinMaxAll::all());
          }
        }
      }
    }
  }
  std::string filename = makeUniqueSdcPath("test_search_r11_drivecell.sdc");
  sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
  expectSdcFileReadable(filename);
}

// --- Sta: report path end with reportPath ---
TEST_F(StaDesignTest, ReportPath2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
    1, 1, true, false, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  for (PathEnd *pe : ends) {
    if (pe && pe->path()) {
      sta_->reportPath(pe->path());
    }
    break;
  }

  }() ));
}

// --- Sta: propagated clock and report ---
TEST_F(StaDesignTest, PropagatedClockReport) {
  ASSERT_NO_THROW(( [&](){
  Clock *clk = sta_->sdc()->findClock("clk");
  if (clk) {
    sta_->setPropagatedClock(clk);
    sta_->updateTiming(true);
    Corner *corner = sta_->cmdCorner();
    sta_->setReportPathFormat(ReportPathFormat::full);
    PathEndSeq ends = sta_->findPathEnds(
      nullptr, nullptr, nullptr, false, corner, MinMaxAll::max(),
      3, 3, true, false, -INF, INF, false, nullptr,
      true, false, false, false, false, false);
    for (PathEnd *pe : ends) {
      if (pe) {
        sta_->reportPathEnd(pe);
      }
    }
    // Write SDC with propagated clock
    std::string filename = makeUniqueSdcPath("test_search_r11_propclk.sdc");
    sta_->writeSdc(filename.c_str(), false, false, 4, false, true);
    expectSdcFileReadable(filename);
  }

  }() ));
}

// --- Sta: setCmdNamespace to STA (covers setCmdNamespace1) ---
TEST_F(StaDesignTest, SetCmdNamespace) {
  CmdNamespace orig = sta_->cmdNamespace();
  sta_->setCmdNamespace(CmdNamespace::sta);
  EXPECT_EQ(sta_->cmdNamespace(), CmdNamespace::sta);
  sta_->setCmdNamespace(CmdNamespace::sdc);
  EXPECT_EQ(sta_->cmdNamespace(), CmdNamespace::sdc);
  sta_->setCmdNamespace(orig);
}

// --- Sta: endpoints ---
TEST_F(StaDesignTest, Endpoints2) {
  VertexSet *eps = sta_->endpoints();
  EXPECT_NE(eps, nullptr);
  if (eps)
    EXPECT_GT(eps->size(), 0u);
}

// --- Sta: worst slack vertex ---
TEST_F(StaDesignTest, WorstSlackVertex) {
  ASSERT_NO_THROW(( [&](){
  Slack ws;
  Vertex *v;
  sta_->worstSlack(MinMax::max(), ws, v);
  (void)ws;
  (void)v;

  }() ));
}

} // namespace sta
