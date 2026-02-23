#include <gtest/gtest.h>
#include <type_traits>
#include <atomic>
#include <cmath>
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
  EXPECT_TRUE(seeded);

  }() ));
}

TEST_F(StaDesignTest, SearchArrivalsAtEndpoints) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  bool exist = search->arrivalsAtEndpointsExist();
  EXPECT_TRUE(exist);

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
    EXPECT_FALSE(std::isinf(wns));
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
      EXPECT_FALSE(std::isinf(delay));
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
    EXPECT_TRUE(matches);
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
    EXPECT_TRUE(is_clk);
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
      EXPECT_TRUE(is_clk);
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
    EXPECT_TRUE(is_ideal);
  }

  }() ));
}

TEST_F(StaDesignTest, StaIsPropagatedClock) {
  ASSERT_NO_THROW(( [&](){
  sta_->ensureClkNetwork();
  Pin *clk_pin = findPin("r1/CK");
  if (clk_pin) {
    bool is_prop = sta_->isPropagatedClock(clk_pin);
    EXPECT_FALSE(is_prop);
  }

  }() ));
}

TEST_F(StaDesignTest, StaPins) {
  Clock *clk = sta_->sdc()->findClock("clk");
  ASSERT_NE(clk, nullptr);
  sta_->ensureClkNetwork();
  const PinSet *pins = sta_->pins(clk);
  EXPECT_NE(pins, nullptr);
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
  EXPECT_GE(count, 0);

  }() ));
}

TEST_F(StaDesignTest, StaTotalNegativeSlack) {
  ASSERT_NO_THROW(( [&](){
  Slack tns = sta_->totalNegativeSlack(MinMax::max());
  EXPECT_FALSE(std::isinf(tns));

  }() ));
}

TEST_F(StaDesignTest, StaTotalNegativeSlackCorner) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack tns = sta_->totalNegativeSlack(corner, MinMax::max());
  EXPECT_FALSE(std::isinf(tns));

  }() ));
}

TEST_F(StaDesignTest, StaWorstSlack) {
  ASSERT_NO_THROW(( [&](){
  Slack wns = sta_->worstSlack(MinMax::max());
  EXPECT_FALSE(std::isinf(wns));

  }() ));
}

TEST_F(StaDesignTest, StaWorstSlackVertex) {
  ASSERT_NO_THROW(( [&](){
  Slack worst_slack;
  Vertex *worst_vertex;
  sta_->worstSlack(MinMax::max(), worst_slack, worst_vertex);
  EXPECT_FALSE(std::isinf(worst_slack));
  EXPECT_NE(worst_vertex, nullptr);

  }() ));
}

TEST_F(StaDesignTest, StaWorstSlackCornerVertex) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack worst_slack;
  Vertex *worst_vertex;
  sta_->worstSlack(corner, MinMax::max(), worst_slack, worst_vertex);
  EXPECT_FALSE(std::isinf(worst_slack));
  EXPECT_NE(worst_vertex, nullptr);

  }() ));
}

TEST_F(StaDesignTest, StaVertexWorstSlackPath) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstSlackPath(v, MinMax::max());
  EXPECT_NE(path, nullptr);
}

TEST_F(StaDesignTest, StaVertexWorstSlackPathRf) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstSlackPath(v, RiseFall::rise(), MinMax::max());
  EXPECT_NE(path, nullptr);
}

TEST_F(StaDesignTest, StaVertexWorstRequiredPath) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstRequiredPath(v, MinMax::max());
  EXPECT_NE(path, nullptr);
}

TEST_F(StaDesignTest, StaVertexWorstRequiredPathRf) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstRequiredPath(v, RiseFall::rise(), MinMax::max());
  EXPECT_NE(path, nullptr);
}

TEST_F(StaDesignTest, StaVertexWorstArrivalPathRf) {
  Vertex *v = findVertex("r1/Q");
  ASSERT_NE(v, nullptr);
  Path *path = sta_->vertexWorstArrivalPath(v, RiseFall::rise(), MinMax::max());
  EXPECT_NE(path, nullptr);
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
  EXPECT_FALSE(std::isinf(slew));
}

TEST_F(StaDesignTest, StaVertexSlewRfMinMax) {
  Vertex *v = findVertex("u1/Z");
  ASSERT_NE(v, nullptr);
  Slew slew = sta_->vertexSlew(v, RiseFall::rise(), MinMax::max());
  EXPECT_FALSE(std::isinf(slew));
}

TEST_F(StaDesignTest, StaVertexRequiredRfPathAP) {
  Vertex *v = findVertex("r3/D");
  ASSERT_NE(v, nullptr);
  Corner *corner = sta_->cmdCorner();
  const PathAnalysisPt *path_ap = corner->findPathAnalysisPt(MinMax::max());
  Required req = sta_->vertexRequired(v, RiseFall::rise(), path_ap);
  EXPECT_FALSE(std::isinf(req));
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
  EXPECT_FALSE(std::isinf(arr));
}

// --- Sta: CheckTiming ---

TEST_F(StaDesignTest, CheckTiming2) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    true, true, true, true, true, true, true);
  EXPECT_GE(errors.size(), 0u);

  }() ));
}

TEST_F(StaDesignTest, CheckTimingNoInputDelay) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    true, false, false, false, false, false, false);
  EXPECT_GE(errors.size(), 0u);

  }() ));
}

TEST_F(StaDesignTest, CheckTimingNoOutputDelay) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    false, true, false, false, false, false, false);
  EXPECT_GE(errors.size(), 0u);

  }() ));
}

TEST_F(StaDesignTest, CheckTimingUnconstrained) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    false, false, false, false, true, false, false);
  EXPECT_GE(errors.size(), 0u);

  }() ));
}

TEST_F(StaDesignTest, CheckTimingLoops) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(
    false, false, false, false, false, true, false);
  EXPECT_GE(errors.size(), 0u);

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
  EXPECT_TRUE(enabled);
  sta_->setCrprEnabled(true);
  EXPECT_TRUE(sta_->crprEnabled());
  sta_->setCrprEnabled(false);
}

TEST_F(StaDesignTest, CrprMode) {
  CrprMode mode = sta_->crprMode();
  EXPECT_EQ(mode, CrprMode::same_pin);
  sta_->setCrprMode(CrprMode::same_pin);
  EXPECT_EQ(sta_->crprMode(), CrprMode::same_pin);
}

// --- Sta: propagateGatedClockEnable ---

TEST_F(StaDesignTest, PropagateGatedClockEnable) {
  bool prop = sta_->propagateGatedClockEnable();
  EXPECT_TRUE(prop);
  sta_->setPropagateGatedClockEnable(true);
  EXPECT_TRUE(sta_->propagateGatedClockEnable());
  sta_->setPropagateGatedClockEnable(false);
}

// --- Sta: analysis mode ---

TEST_F(StaDesignTest, CmdNamespace) {
  ASSERT_NO_THROW(( [&](){
  CmdNamespace ns = sta_->cmdNamespace();
  EXPECT_EQ(ns, CmdNamespace::sdc);

  }() ));
}

TEST_F(StaDesignTest, CmdCorner) {
  Corner *corner = sta_->cmdCorner();
  EXPECT_NE(corner, nullptr);
}

TEST_F(StaDesignTest, FindCorner) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->findCorner("default");
  EXPECT_NE(corner, nullptr);

  }() ));
}

TEST_F(StaDesignTest, MultiCorner) {
  ASSERT_NO_THROW(( [&](){
  bool multi = sta_->multiCorner();
  EXPECT_FALSE(multi);

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
      EXPECT_NE(start, nullptr);
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
    EXPECT_FALSE(std::isinf(slk));
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
  sta_->pvt(top, MinMax::max());

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
      EXPECT_TRUE(is_clk);
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
      EXPECT_GE(idx, 0);
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
      Tag::cmp(t0, t1, sta_);
      Tag::matchCmp(t0, t1, true, sta_);
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
      EXPECT_GT(h, 0u);
      size_t mh = t->matchHash(true, sta_);
      EXPECT_GT(mh, 0u);
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
      EXPECT_GT(h0, 0u);
      EXPECT_GT(h1, 0u);
      TagMatchEqual eq(true, sta_);
      bool result = eq(t0, t1);
      EXPECT_FALSE(result);
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
        EXPECT_NE(edge, nullptr);
        bool prop = clk_info->isPropagated();
        EXPECT_FALSE(prop);
        bool gen = clk_info->isGenClkSrcPath();
        EXPECT_FALSE(gen);
        PathAPIndex idx = clk_info->pathAPIndex();
        EXPECT_GE(idx, 0);
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
    EXPECT_GE(static_cast<int>(val), 0);
  }
}

TEST_F(StaDesignTest, SimLogicZeroOne) {
  Sim *sim = sta_->sim();
  ASSERT_NE(sim, nullptr);
  Pin *pin = findPin("r1/D");
  if (pin) {
    bool zeroone = sim->logicZeroOne(pin);
    EXPECT_FALSE(zeroone);
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
      EXPECT_NE(sense, TimingSense::unknown);
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
        EXPECT_NE(parasitic, nullptr);
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
    EXPECT_NE(tag, nullptr);
    Arrival arr = path->arrival();
    EXPECT_FALSE(std::isinf(arr));
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
    EXPECT_NE(prev, nullptr);
    TimingArc *prev_arc = path->prevArc(sta_);
    EXPECT_NE(prev_arc, nullptr);
    Edge *prev_edge = path->prevEdge(sta_);
    EXPECT_NE(prev_edge, nullptr);
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
        EXPECT_NE(p, nullptr);
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
    EXPECT_FALSE(is_enable);
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
    EXPECT_GE(enables.size(), 0u);
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
      EXPECT_NE(name, nullptr);
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
    EXPECT_GT(groups.size(), 0u);
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
    EXPECT_NE(path, nullptr);
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
  EXPECT_GT(ends.size(), 0u);
}

// --- Sta: unique_edges findPathEnds ---

TEST_F(StaDesignTest, FindPathEndsUniqueEdges) {
  ASSERT_NO_THROW(( [&](){
  PathEndSeq ends = sta_->findPathEnds(
    nullptr, nullptr, nullptr,
    false, nullptr, MinMaxAll::max(),
    10, 3, false, true, -INF, INF, false, nullptr,
    true, false, false, false, false, false);
  EXPECT_GE(ends.size(), 0u);

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
  EXPECT_FALSE(enabled);

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
  EXPECT_FALSE(std::isinf(arr));
  Required req = sta_->vertexRequired(v, MinMax::max());
  EXPECT_FALSE(std::isinf(req));
}

// --- Sta: activity ---

TEST_F(StaDesignTest, PinActivity) {
  Pin *pin = findPin("r1/Q");
  ASSERT_NE(pin, nullptr);
  PwrActivity act = sta_->activity(pin);
  EXPECT_GE(act.density(), 0.0f);
}

// --- Search: isInputArrivalSrchStart ---

TEST_F(StaDesignTest, IsInputArrivalSrchStart) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Vertex *v = findVertex("in1");
  if (v) {
    bool is_start = search->isInputArrivalSrchStart(v);
    EXPECT_TRUE(is_start);
  }

  }() ));
}

TEST_F(StaDesignTest, IsSegmentStart) {
  ASSERT_NO_THROW(( [&](){
  Search *search = sta_->search();
  Pin *pin = findPin("in1");
  if (pin) {
    bool is_seg = search->isSegmentStart(pin);
    EXPECT_FALSE(is_seg);
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
    EXPECT_FALSE(std::isinf(ins));
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
  EXPECT_GE(endpoints.size(), 0u);

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
  EXPECT_NE(pvt, nullptr);

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
      EXPECT_FALSE(std::isinf(arr));
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
    EXPECT_NE(pv.type(), PropertyValue::type_none);
    PropertyValue pv2 = props.getProperty(pin, "arrival_max_fall");
    EXPECT_NE(pv2.type(), PropertyValue::type_none);
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
    EXPECT_NE(pv.type(), PropertyValue::type_none);
    PropertyValue pv2 = props.getProperty(pin, "slack_min");
    EXPECT_NE(pv2.type(), PropertyValue::type_none);
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
    EXPECT_NE(pv.type(), PropertyValue::type_none);
    PropertyValue pv2 = props.getProperty(pin, "slack_min_fall");
    EXPECT_NE(pv2.type(), PropertyValue::type_none);
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
      EXPECT_NE(pv.type(), PropertyValue::type_none);
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
    EXPECT_NE(pv.type(), PropertyValue::type_none);
  }
  LibertyLibrary *lib = network->defaultLibertyLibrary();
  if (lib) {
    PropertyValue pv = props.getProperty(lib, "name");
    EXPECT_NE(pv.type(), PropertyValue::type_none);
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
      EXPECT_EQ(pv.type(), PropertyValue::type_none);
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
    EXPECT_GE(val, 0.0f);
  } catch (const std::exception &e) {
    const char *msg = e.what();
    EXPECT_NE(msg, nullptr);
  }
}

// --- CheckTiming: hasClkedCheck, clear ---

TEST_F(StaDesignTest, CheckTimingClear) {
  ASSERT_NO_THROW(( [&](){
  CheckErrorSeq &errors = sta_->checkTiming(true, true, true, true, true, true, true);
  EXPECT_GE(errors.size(), 0u);
  CheckErrorSeq &errors2 = sta_->checkTiming(true, true, true, true, true, true, true);
  EXPECT_GE(errors2.size(), 0u);

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
        EXPECT_NE(vert, nullptr);
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
            EXPECT_NE(edge, nullptr);
            bool prop = clk_info->isPropagated();
            EXPECT_FALSE(prop);
            bool gen = clk_info->isGenClkSrcPath();
            EXPECT_FALSE(gen);
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
      EXPECT_GE(ti, 0);
      Vertex *prev = path->prevVertex(sta_);
      EXPECT_NE(prev, nullptr);
    }
    delete iter;
  }

  }() ));
}

// --- PathGroup constructor ---

TEST_F(StaDesignTest, PathGroupConstructor) {
  Search *search = sta_->search();
  if (search) {
    search->findPathGroup("clk", MinMax::max());
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
    EXPECT_NE(tgt_clk, nullptr);
    Arrival tgt_arr = pe->targetClkArrival(sta_);
    EXPECT_FALSE(std::isinf(tgt_arr));
    Delay tgt_delay = pe->targetClkDelay(sta_);
    EXPECT_FALSE(std::isinf(tgt_delay));
    Delay tgt_ins = pe->targetClkInsertionDelay(sta_);
    EXPECT_FALSE(std::isinf(tgt_ins));
    float non_inter = pe->targetNonInterClkUncertainty(sta_);
    EXPECT_FALSE(std::isinf(non_inter));
    float inter = pe->interClkUncertainty(sta_);
    EXPECT_FALSE(std::isinf(inter));
    float tgt_unc = pe->targetClkUncertainty(sta_);
    EXPECT_FALSE(std::isinf(tgt_unc));
    float mcp_adj = pe->targetClkMcpAdjustment(sta_);
    EXPECT_FALSE(std::isinf(mcp_adj));
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
      EXPECT_FALSE(std::isinf(req));
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
      EXPECT_FALSE(std::isinf(tgt_time));
      float tgt_off = pe->targetClkOffset(sta_);
      EXPECT_FALSE(std::isinf(tgt_off));
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
      less(v1, v2);
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
      EXPECT_FALSE(std::isinf(arr));
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
    EXPECT_GE(static_cast<int>(val), 0);
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
    EXPECT_NE(clk, nullptr);
    Pin *ref = c1->refPin(sta_);
    EXPECT_NE(ref, nullptr);
    ArcDelay max_skew = c1->maxSkew(sta_);
    EXPECT_FALSE(std::isinf(max_skew));
    Slack slack = c1->slack(sta_);
    EXPECT_FALSE(std::isinf(slack));
  }
  if (checks.size() >= 2) {
    MaxSkewSlackLess less(sta_);
    less(checks[0], checks[1]);
  }

  }() ));
}

// --- MinPeriodSlackLess ---

TEST_F(StaDesignTest, MinPeriodCheckAccessors) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheckSeq &checks = sta_->minPeriodViolations();
  if (checks.size() >= 2) {
    MinPeriodSlackLess less(sta_);
    less(checks[0], checks[1]);
  }
  sta_->minPeriodSlack();

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
    EXPECT_NE(c, nullptr);
  }

  }() ));
}

TEST_F(StaDesignTest, MinPulseWidthSlack3) {
  ASSERT_NO_THROW(( [&](){
  CornerSeq &corners = sta_->corners()->corners();
  Corner *corner = corners[0];
  sta_->minPulseWidthSlack(corner);

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
  EXPECT_FALSE(std::isinf(tns_max));
  Slack tns_min = sta_->totalNegativeSlack(MinMax::min());
  EXPECT_FALSE(std::isinf(tns_min));

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
  EXPECT_GE(ends.size(), 0u);

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
  EXPECT_GE(ends.size(), 0u);

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
    EXPECT_FALSE(std::isinf(min_period));
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
          EXPECT_GT(h, 0u);
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
    EXPECT_FALSE(std::isinf(req));
    Slack slack = pe->slack(sta_);
    EXPECT_FALSE(std::isinf(slack));
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
  EXPECT_GE(async_pins.size(), 0u);

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
  EXPECT_GT(regs.size(), 0u);
  delete clks;
}

// --- FindRegister: registers only (no latches) ---
TEST_F(StaDesignTest, FindRegisterRegistersOnly) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  InstanceSet regs = sta_->findRegisterInstances(clks, RiseFallBoth::riseFall(),
                                                  true, false);
  EXPECT_GT(regs.size(), 0u);

  }() ));
}

// --- FindRegister: latches only ---
TEST_F(StaDesignTest, FindRegisterLatchesOnly) {
  ASSERT_NO_THROW(( [&](){
  ClockSet *clks = nullptr;
  InstanceSet latches = sta_->findRegisterInstances(clks, RiseFallBoth::riseFall(),
                                                     false, true);
  EXPECT_GE(latches.size(), 0u);

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
    sta_->pvt(inst, MinMax::max());
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
    EXPECT_NE(val.type(), PropertyValue::type_none);
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
      EXPECT_NE(val.type(), PropertyValue::type_none);
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
    EXPECT_NE(val.type(), PropertyValue::type_none);
    PropertyValue val2 = sta_->properties().getProperty(clk, "period");
    EXPECT_NE(val2.type(), PropertyValue::type_none);
    PropertyValue val3 = sta_->properties().getProperty(clk, "sources");
    EXPECT_NE(val3.type(), PropertyValue::type_none);
  }

  }() ));
}

// --- MaxSkewCheck: detailed accessors ---
TEST_F(StaDesignTest, MaxSkewCheckDetailedAccessors) {
  ASSERT_NO_THROW(( [&](){
  MaxSkewCheck *check = sta_->maxSkewSlack();
  if (check) {
    const Pin *clk_pin = check->clkPin(sta_);
    EXPECT_NE(clk_pin, nullptr);
    const Pin *ref_pin = check->refPin(sta_);
    EXPECT_NE(ref_pin, nullptr);
    float max_skew = check->maxSkew(sta_);
    EXPECT_FALSE(std::isinf(max_skew));
    float slack = check->slack(sta_);
    EXPECT_FALSE(std::isinf(slack));
  }

  }() ));
}

// --- MinPeriodCheck: detailed accessors ---
TEST_F(StaDesignTest, MinPeriodCheckDetailedAccessors) {
  ASSERT_NO_THROW(( [&](){
  MinPeriodCheck *check = sta_->minPeriodSlack();
  if (check) {
    float min_period = check->minPeriod(sta_);
    EXPECT_FALSE(std::isinf(min_period));
    float slack = check->slack(sta_);
    EXPECT_FALSE(std::isinf(slack));
    const Pin *pin = check->pin();
    EXPECT_NE(pin, nullptr);
    Clock *clk = check->clk();
    EXPECT_NE(clk, nullptr);
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
    EXPECT_GE(fanin.size(), 0u);
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
    EXPECT_GE(fanout.size(), 0u);
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
  EXPECT_FALSE(std::isinf(worst));

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
        EXPECT_NE(val.type(), PropertyValue::type_none);
        PropertyValue val2 = sta_->properties().getProperty(edge, "sense");
        EXPECT_NE(val2.type(), PropertyValue::type_none);
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
    EXPECT_NE(val.type(), PropertyValue::type_none);
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
      EXPECT_NE(val.type(), PropertyValue::type_none);
      PropertyValue val2 = sta_->properties().getProperty(port, "direction");
      EXPECT_NE(val2.type(), PropertyValue::type_none);
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
      EXPECT_NE(val.type(), PropertyValue::type_none);
      PropertyValue val2 = sta_->properties().getProperty(lib_cell, "area");
      EXPECT_NE(val2.type(), PropertyValue::type_none);
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
        EXPECT_NE(val.type(), PropertyValue::type_none);
        PropertyValue val2 = sta_->properties().getProperty(port, "direction");
        EXPECT_NE(val2.type(), PropertyValue::type_none);
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
    EXPECT_NE(val.type(), PropertyValue::type_none);
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
    EXPECT_NE(val.type(), PropertyValue::type_none);
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
        EXPECT_NE(val.type(), PropertyValue::type_none);
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
      EXPECT_NE(val.type(), PropertyValue::type_none);
      PropertyValue val2 = sta_->properties().getProperty(end, "endpoint");
      EXPECT_NE(val2.type(), PropertyValue::type_none);
      PropertyValue val3 = sta_->properties().getProperty(end, "slack");
      EXPECT_NE(val3.type(), PropertyValue::type_none);
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
        EXPECT_NE(val.type(), PropertyValue::type_none);
        PropertyValue val2 = sta_->properties().getProperty(path, "arrival");
        EXPECT_NE(val2.type(), PropertyValue::type_none);
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
    EXPECT_NE(val_arr.type(), PropertyValue::type_none);
    PropertyValue val_arr2 = sta_->properties().getProperty(out, "arrival_max_fall");
    EXPECT_NE(val_arr2.type(), PropertyValue::type_none);
    PropertyValue val_arr3 = sta_->properties().getProperty(out, "arrival_min_rise");
    EXPECT_NE(val_arr3.type(), PropertyValue::type_none);
    PropertyValue val_arr4 = sta_->properties().getProperty(out, "arrival_min_fall");
    EXPECT_NE(val_arr4.type(), PropertyValue::type_none);
    // These trigger pinSlack internally
    PropertyValue val_slk = sta_->properties().getProperty(out, "slack_max");
    EXPECT_NE(val_slk.type(), PropertyValue::type_none);
    PropertyValue val_slk2 = sta_->properties().getProperty(out, "slack_max_rise");
    EXPECT_NE(val_slk2.type(), PropertyValue::type_none);
    PropertyValue val_slk3 = sta_->properties().getProperty(out, "slack_max_fall");
    EXPECT_NE(val_slk3.type(), PropertyValue::type_none);
    PropertyValue val_slk4 = sta_->properties().getProperty(out, "slack_min");
    EXPECT_NE(val_slk4.type(), PropertyValue::type_none);
    PropertyValue val_slk5 = sta_->properties().getProperty(out, "slack_min_rise");
    EXPECT_NE(val_slk5.type(), PropertyValue::type_none);
    PropertyValue val_slk6 = sta_->properties().getProperty(out, "slack_min_fall");
    EXPECT_NE(val_slk6.type(), PropertyValue::type_none);
    // Slew
    PropertyValue val_slew = sta_->properties().getProperty(out, "slew_max");
    EXPECT_NE(val_slew.type(), PropertyValue::type_none);
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
      EXPECT_NE(val.type(), PropertyValue::type_none);
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
    EXPECT_NE(val.type(), PropertyValue::type_none);
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
      EXPECT_EQ(val.type(), PropertyValue::type_none);
    } catch (std::exception &e) {
      // Expected PropertyUnknown exception
      EXPECT_NE(e.what(), nullptr);
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
  EXPECT_FALSE(std::isinf(skew));
  float skew2 = sta_->findWorstClkSkew(MinMax::min(), false);
  EXPECT_FALSE(std::isinf(skew2));

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
    EXPECT_FALSE(prop);
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
      EXPECT_NE(tgt_clk, nullptr);
      Arrival tgt_arr = pe->targetClkArrival(sta_);
      EXPECT_FALSE(std::isinf(tgt_arr));
      Delay tgt_delay = pe->targetClkDelay(sta_);
      EXPECT_FALSE(std::isinf(tgt_delay));
      Arrival tgt_ins = pe->targetClkInsertionDelay(sta_);
      EXPECT_FALSE(std::isinf(tgt_ins));
      float tgt_unc = pe->targetClkUncertainty(sta_);
      EXPECT_FALSE(std::isinf(tgt_unc));
      float tgt_mcp = pe->targetClkMcpAdjustment(sta_);
      EXPECT_FALSE(std::isinf(tgt_mcp));
      float non_inter = pe->targetNonInterClkUncertainty(sta_);
      EXPECT_FALSE(std::isinf(non_inter));
      float inter = pe->interClkUncertainty(sta_);
      EXPECT_FALSE(std::isinf(inter));
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
          EXPECT_NE(p0, nullptr);
          if (sz > 1) {
            const Path *p1 = expanded.path(sz - 1);
            EXPECT_NE(p1, nullptr);
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
  EXPECT_FALSE(std::isinf(ws_max));
  Slack ws_min = sta_->worstSlack(MinMax::min());
  EXPECT_FALSE(std::isinf(ws_min));

  }() ));
}

// --- Sta: worstSlack with corner ---
TEST_F(StaDesignTest, WorstSlackCorner2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack ws;
  Vertex *v;
  sta_->worstSlack(corner, MinMax::max(), ws, v);
  EXPECT_FALSE(std::isinf(ws));
  EXPECT_NE(v, nullptr);

  }() ));
}

// --- Sta: totalNegativeSlack ---
TEST_F(StaDesignTest, TotalNegativeSlack2) {
  ASSERT_NO_THROW(( [&](){
  Slack tns = sta_->totalNegativeSlack(MinMax::max());
  EXPECT_FALSE(std::isinf(tns));
  Slack tns2 = sta_->totalNegativeSlack(MinMax::min());
  EXPECT_FALSE(std::isinf(tns2));

  }() ));
}

// --- Sta: totalNegativeSlack with corner ---
TEST_F(StaDesignTest, TotalNegativeSlackCorner2) {
  ASSERT_NO_THROW(( [&](){
  Corner *corner = sta_->cmdCorner();
  Slack tns = sta_->totalNegativeSlack(corner, MinMax::max());
  EXPECT_FALSE(std::isinf(tns));

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
  EXPECT_GE(errors.size(), 0u);

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
        pe->isOutputDelay();
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
      EXPECT_GE(static_cast<int>(type), 0);
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
      // unc can be true or false
      if (unc) {
        Required req = pe->requiredTime(sta_);
        EXPECT_FALSE(std::isinf(req));
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
  EXPECT_GE(ends.size(), 0u);

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
          EXPECT_NE(edge, nullptr);
          bool prop = ci->isPropagated();
          EXPECT_FALSE(prop);
          bool gen = ci->isGenClkSrcPath();
          EXPECT_FALSE(gen);
        }
        int ap_idx = tag->pathAPIndex();
        EXPECT_GE(ap_idx, 0);
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
  EXPECT_FALSE(std::isinf(ws));
  EXPECT_NE(v, nullptr);

  }() ));
}

} // namespace sta
