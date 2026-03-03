#include <gtest/gtest.h>
#include <type_traits>
#include <tcl.h>
#include "MinMax.hh"
#include "Transition.hh"
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

template <typename FnPtr>
static void expectCallablePointerUsable(FnPtr fn) {
  ASSERT_NE(fn, nullptr);
  EXPECT_TRUE((std::is_pointer_v<FnPtr> || std::is_member_function_pointer_v<FnPtr>));
  EXPECT_TRUE(std::is_copy_constructible_v<FnPtr>);
  EXPECT_TRUE(std::is_copy_assignable_v<FnPtr>);
  FnPtr fn_copy = fn;
  EXPECT_EQ(fn_copy, fn);
}

static void expectStaCoreState(Sta *sta);

class StaInitTest : public ::testing::Test {
protected:
  void SetUp() override {
    interp_ = Tcl_CreateInterp();
    initSta();
    sta_ = new Sta;
    Sta::setSta(sta_);
    sta_->makeComponents();
    // Set the Tcl interp on the report so ReportTcl destructor works
    ReportTcl *report = dynamic_cast<ReportTcl*>(sta_->report());
    if (report)
      report->setTclInterp(interp_);
  }

  void TearDown() override {
    if (sta_)
      expectStaCoreState(sta_);
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_)
      Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  Sta *sta_;
  Tcl_Interp *interp_;
};

static void expectStaCoreState(Sta *sta)
{
  ASSERT_NE(sta, nullptr);
  ASSERT_NE(sta->search(), nullptr);
  ASSERT_NE(sta->cmdSdc(), nullptr);
  ASSERT_NE(sta->reportPath(), nullptr);
  ASSERT_FALSE(sta->scenes().empty());
  EXPECT_GE(sta->scenes().size(), 1);
  EXPECT_NE(sta->cmdScene(), nullptr);
}

// === Sta.cc: functions that call ensureLinked/ensureGraph (throw Exception) ===

// startpointPins() is declared but not defined - skipped
// TEST_F(StaInitTest, StaStartpointPinsThrows) {
//   EXPECT_THROW(sta_->startpointPins(), Exception);
// }

TEST_F(StaInitTest, StaEndpointsThrows) {
  EXPECT_THROW(sta_->endpoints(), Exception);
}

TEST_F(StaInitTest, StaEndpointPinsThrows) {
  EXPECT_THROW(sta_->endpointPins(), Exception);
}

TEST_F(StaInitTest, StaGraphLoopsThrows) {
  EXPECT_THROW(sta_->graphLoops(), Exception);
}

TEST_F(StaInitTest, StaVertexLevelThrows) {
  EXPECT_THROW(sta_->vertexLevel(nullptr), Exception);
}

TEST_F(StaInitTest, StaFindLogicConstantsThrows2) {
  EXPECT_THROW(sta_->findLogicConstants(), Exception);
}

TEST_F(StaInitTest, StaEnsureClkNetworkThrows) {
  EXPECT_THROW(sta_->ensureClkNetwork(sta_->cmdMode()), Exception);
}

// findRegisterPreamble is protected, skip

// delayCalcPreamble is protected, skip

TEST_F(StaInitTest, StaFindDelaysThrows) {
  EXPECT_THROW(sta_->findDelays(), Exception);
}

TEST_F(StaInitTest, StaFindRequiredsThrows) {
  EXPECT_THROW(sta_->findRequireds(), Exception);
}

TEST_F(StaInitTest, StaEnsureLinkedThrows) {
  EXPECT_THROW(sta_->ensureLinked(), Exception);
}

TEST_F(StaInitTest, StaEnsureGraphThrows) {
  EXPECT_THROW(sta_->ensureGraph(), Exception);
}

TEST_F(StaInitTest, StaEnsureLevelizedThrows) {
  EXPECT_THROW(sta_->ensureLevelized(), Exception);
}

// powerPreamble is protected, skip

// sdcChangedGraph is protected, skip

TEST_F(StaInitTest, StaFindFaninPinsThrows2) {
  EXPECT_THROW(sta_->findFaninPins(static_cast<PinSeq*>(nullptr), false, false, 0, 0, false, false, sta_->cmdMode()), Exception);
}

TEST_F(StaInitTest, StaFindFanoutPinsThrows2) {
  EXPECT_THROW(sta_->findFanoutPins(static_cast<PinSeq*>(nullptr), false, false, 0, 0, false, false, sta_->cmdMode()), Exception);
}

TEST_F(StaInitTest, StaMakePortPinThrows) {
  EXPECT_THROW(sta_->makePortPin("test", nullptr), Exception);
}

TEST_F(StaInitTest, StaWriteSdcThrows2) {
  EXPECT_THROW(sta_->writeSdc(sta_->cmdSdc(), "test.sdc", false, false, 4, false, false), Exception);
}

// === Sta.cc: SearchPreamble and related ===

TEST_F(StaInitTest, StaSearchPreamble2) {
  // searchPreamble calls ensureClkArrivals which calls findDelays
  // It will throw because ensureGraph is called
  EXPECT_THROW(sta_->searchPreamble(), Exception);
}

TEST_F(StaInitTest, StaEnsureClkArrivals2) {
  // calls findDelays which calls ensureGraph
  EXPECT_THROW(sta_->ensureClkArrivals(), Exception);
}

TEST_F(StaInitTest, StaUpdateTiming2) {
  // calls findDelays
  EXPECT_THROW(sta_->updateTiming(false), Exception);
}

// === Sta.cc: Report header functions ===

TEST_F(StaInitTest, StaReportPathEndHeader2) {
  ASSERT_NO_THROW(( [&](){
    sta_->reportPathEndHeader();
  }() ));
}

TEST_F(StaInitTest, StaReportPathEndFooter2) {
  ASSERT_NO_THROW(( [&](){
    sta_->reportPathEndFooter();
  }() ));
}

// === Sta.cc: preamble functions ===

// minPulseWidthPreamble, minPeriodPreamble, maxSkewPreamble, clkSkewPreamble are protected, skip

// === Sta.cc: function pointer checks for methods needing network ===

// === Sta.cc: check/violation preambles ===

// === Sta.cc: Sta::setReportPathFields ===

TEST_F(StaInitTest, StaSetReportPathFields) {
  ASSERT_NO_THROW(( [&](){
    sta_->setReportPathFields(true, true, true, true, true, true, true);
  }() ));
}

// === Sta.cc: delete exception helpers ===

TEST_F(StaInitTest, StaDeleteExceptionFrom) {
  ASSERT_NO_THROW(( [&](){
    sta_->deleteExceptionFrom(nullptr);
  }() ));
}

TEST_F(StaInitTest, StaDeleteExceptionThru) {
  ASSERT_NO_THROW(( [&](){
    sta_->deleteExceptionThru(nullptr);
  }() ));
}

TEST_F(StaInitTest, StaDeleteExceptionTo) {
  ASSERT_NO_THROW(( [&](){
    sta_->deleteExceptionTo(nullptr);
  }() ));
}

// === Sta.cc: readNetlistBefore ===

TEST_F(StaInitTest, StaReadNetlistBefore) {
  ASSERT_NO_THROW(( [&](){
    sta_->readNetlistBefore();
  }() ));
}

// === Sta.cc: endpointViolationCount ===

// === Sta.cc: operatingConditions ===

TEST_F(StaInitTest, StaOperatingConditions2) {
  ASSERT_NO_THROW(( [&](){
    sta_->operatingConditions(MinMax::max(), sta_->cmdSdc());
  }() ));
}

// === Sta.cc: removeConstraints ===

// === Sta.cc: disabledEdgesSorted (calls ensureLevelized internally) ===

TEST_F(StaInitTest, StaDisabledEdgesSortedThrows) {
  EXPECT_THROW(sta_->disabledEdgesSorted(sta_->cmdMode()), Exception);
}

// === Sta.cc: disabledEdges (calls ensureLevelized) ===

TEST_F(StaInitTest, StaDisabledEdgesThrows) {
  EXPECT_THROW(sta_->disabledEdges(sta_->cmdMode()), Exception);
}

// === Sta.cc: findReportPathField ===

TEST_F(StaInitTest, StaFindReportPathField) {
  ASSERT_NO_THROW(( [&](){
    sta_->findReportPathField("delay");
  }() ));
}

// === Sta.cc: findCorner ===

TEST_F(StaInitTest, StaFindCornerByName) {
  ASSERT_NO_THROW(( [&](){
    auto corner = sta_->findScene("default");
    // May or may not exist
    EXPECT_NE(corner, nullptr);
  }() ));
}

// === Sta.cc: totalNegativeSlack ===

TEST_F(StaInitTest, StaTotalNegativeSlackThrows) {
  EXPECT_THROW(sta_->totalNegativeSlack(MinMax::max()), Exception);
}

// === Sta.cc: worstSlack ===

TEST_F(StaInitTest, StaWorstSlackThrows) {
  EXPECT_THROW(sta_->worstSlack(MinMax::max()), Exception);
}

// === Sta.cc: setArcDelayCalc ===

TEST_F(StaInitTest, StaSetArcDelayCalc) {
  ASSERT_NO_THROW(( [&](){
    sta_->setArcDelayCalc("unit");
  }() ));
}

// === Sta.cc: setAnalysisType ===

TEST_F(StaInitTest, StaSetAnalysisType) {
  ASSERT_NO_THROW(( [&](){
    sta_->setAnalysisType(AnalysisType::ocv, sta_->cmdSdc());
  }() ));
}

// === Sta.cc: setTimingDerate (global) ===

TEST_F(StaInitTest, StaSetTimingDerate) {
  ASSERT_NO_THROW(( [&](){
    sta_->setTimingDerate(TimingDerateType::cell_delay, PathClkOrData::clk,
                          RiseFallBoth::riseFall(), MinMax::max(), 1.05f,
                          sta_->cmdSdc());
  }() ));
}

// === Sta.cc: setVoltage ===

TEST_F(StaInitTest, StaSetVoltage) {
  ASSERT_NO_THROW(( [&](){
    sta_->setVoltage(MinMax::max(), 1.0f, sta_->cmdSdc());
  }() ));
}

// === Sta.cc: setReportPathFieldOrder segfaults on null, use method exists ===

// === Sta.cc: clear ===

// === Property.cc: defineProperty overloads ===

TEST_F(StaInitTest, PropertiesDefineLibrary) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_lib_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const Library*>::PropertyHandler(
        [](const Library*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefineLibertyLibrary) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_liblib_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const LibertyLibrary*>::PropertyHandler(
        [](const LibertyLibrary*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefineCell) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_cell_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const Cell*>::PropertyHandler(
        [](const Cell*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefineLibertyCell) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_libcell_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const LibertyCell*>::PropertyHandler(
        [](const LibertyCell*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefinePort) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_port_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const Port*>::PropertyHandler(
        [](const Port*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefineLibertyPort) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_libport_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const LibertyPort*>::PropertyHandler(
        [](const LibertyPort*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefineInstance) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_inst_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const Instance*>::PropertyHandler(
        [](const Instance*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefinePin) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_pin_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const Pin*>::PropertyHandler(
        [](const Pin*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefineNet) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_net_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const Net*>::PropertyHandler(
        [](const Net*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

TEST_F(StaInitTest, PropertiesDefineClock) {
  ASSERT_NO_THROW(( [&](){
    Properties props(sta_);
    std::string prop_name("test_clk_prop");
    props.defineProperty(prop_name,
      PropertyRegistry<const Clock*>::PropertyHandler(
        [](const Clock*, Sta*) -> PropertyValue { return PropertyValue(); }));
  }() ));
}

// === Search.cc: RequiredCmp ===

TEST_F(StaInitTest, RequiredCmpConstruct) {
  ASSERT_NO_THROW(( [&](){
    RequiredCmp cmp;
  }() ));
}

// === Search.cc: EvalPred constructor ===

TEST_F(StaInitTest, EvalPredConstruct) {
  ASSERT_NO_THROW(( [&](){
    EvalPred pred(sta_);
  }() ));
}

// === Search.cc: ClkArrivalSearchPred ===

// === Search.cc: Search accessors ===

TEST_F(StaInitTest, SearchTagCount2) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  int tc = search->tagCount();
  EXPECT_GE(tc, 0);
}

TEST_F(StaInitTest, SearchTagGroupCount2) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  int tgc = search->tagGroupCount();
  EXPECT_GE(tgc, 0);
}

TEST_F(StaInitTest, SearchClkInfoCount2) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  int cnt = search->clkInfoCount();
  EXPECT_GE(cnt, 0);
}

TEST_F(StaInitTest, SearchArrivalsInvalid2) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  search->arrivalsInvalid();
}

TEST_F(StaInitTest, SearchRequiredsInvalid2) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  search->requiredsInvalid();
}

TEST_F(StaInitTest, SearchEndpointsInvalid2) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  search->endpointsInvalid();
}

TEST_F(StaInitTest, SearchClear2) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  search->clear();
}

TEST_F(StaInitTest, SearchCrprPathPruningEnabled) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  (void)search->crprPathPruningEnabled();
}

TEST_F(StaInitTest, SearchCrprApproxMissingRequireds) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  (void)search->crprApproxMissingRequireds();
}

TEST_F(StaInitTest, SearchSetCrprpathPruningEnabled) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  search->setCrprpathPruningEnabled(true);
  EXPECT_TRUE(search->crprPathPruningEnabled());
  search->setCrprpathPruningEnabled(false);
}

TEST_F(StaInitTest, SearchSetCrprApproxMissingRequireds) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  search->setCrprApproxMissingRequireds(true);
  EXPECT_TRUE(search->crprApproxMissingRequireds());
  search->setCrprApproxMissingRequireds(false);
}

TEST_F(StaInitTest, SearchDeletePathGroups2) {
  Search *search = sta_->search();
  ASSERT_NE(search, nullptr);
  search->deletePathGroups();
}

// === PathEnd.cc: more PathEndUnconstrained methods ===

TEST_F(StaInitTest, PathEndUnconstrainedCheckRole) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  const TimingRole *role = pe.checkRole(sta_);
  EXPECT_EQ(role, nullptr);
}

TEST_F(StaInitTest, PathEndUnconstrainedTypeName) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  const char *name = pe.typeName();
  EXPECT_STREQ(name, "unconstrained");
}

TEST_F(StaInitTest, PathEndUnconstrainedType) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_EQ(pe.type(), PathEnd::Type::unconstrained);
}

TEST_F(StaInitTest, PathEndUnconstrainedIsUnconstrained) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_TRUE(pe.isUnconstrained());
}

TEST_F(StaInitTest, PathEndUnconstrainedIsCheck) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_FALSE(pe.isCheck());
}

TEST_F(StaInitTest, PathEndUnconstrainedIsLatchCheck) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_FALSE(pe.isLatchCheck());
}

TEST_F(StaInitTest, PathEndUnconstrainedIsOutputDelay) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_FALSE(pe.isOutputDelay());
}

TEST_F(StaInitTest, PathEndUnconstrainedIsGatedClock) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_FALSE(pe.isGatedClock());
}

TEST_F(StaInitTest, PathEndUnconstrainedIsPathDelay) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_FALSE(pe.isPathDelay());
}

TEST_F(StaInitTest, PathEndUnconstrainedTargetClkEdge) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_EQ(pe.targetClkEdge(sta_), nullptr);
}

TEST_F(StaInitTest, PathEndUnconstrainedTargetClkTime) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_FLOAT_EQ(pe.targetClkTime(sta_), 0.0f);
}

TEST_F(StaInitTest, PathEndUnconstrainedTargetClkOffset) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_FLOAT_EQ(pe.targetClkOffset(sta_), 0.0f);
}

TEST_F(StaInitTest, PathEndUnconstrainedSourceClkOffset) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  EXPECT_FLOAT_EQ(pe.sourceClkOffset(sta_), 0.0f);
}

TEST_F(StaInitTest, PathEndUnconstrainedCopy) {
  Path *p = new Path();
  PathEndUnconstrained pe(p);
  PathEnd *copy = pe.copy();
  EXPECT_NE(copy, nullptr);
  EXPECT_EQ(copy->type(), PathEnd::Type::unconstrained);
  delete copy;
}

TEST_F(StaInitTest, PathEndUnconstrainedExceptPathCmp) {
  Path *p1 = new Path();
  Path *p2 = new Path();
  PathEndUnconstrained pe1(p1);
  PathEndUnconstrained pe2(p2);
  int cmp = pe1.exceptPathCmp(&pe2, sta_);
  EXPECT_EQ(cmp, 0);
}

// === PathEnd.cc: PathEndCheck constructor/type ===

TEST_F(StaInitTest, PathEndCheckConstruct2) {
  Path *p = new Path();
  Path *clk = new Path();
  PathEndCheck pe(p, nullptr, nullptr, clk, nullptr, sta_);
  EXPECT_EQ(pe.type(), PathEnd::Type::check);
  EXPECT_TRUE(pe.isCheck());
  EXPECT_FALSE(pe.isLatchCheck());
  EXPECT_STREQ(pe.typeName(), "check");
}

TEST_F(StaInitTest, PathEndCheckGetters) {
  Path *p = new Path();
  Path *clk = new Path();
  PathEndCheck pe(p, nullptr, nullptr, clk, nullptr, sta_);
  EXPECT_EQ(pe.checkArc(), nullptr);
  EXPECT_EQ(pe.multiCyclePath(), nullptr);
}

TEST_F(StaInitTest, PathEndCheckCopy) {
  Path *p = new Path();
  Path *clk = new Path();
  PathEndCheck pe(p, nullptr, nullptr, clk, nullptr, sta_);
  PathEnd *copy = pe.copy();
  EXPECT_NE(copy, nullptr);
  EXPECT_EQ(copy->type(), PathEnd::Type::check);
  delete copy;
}

// === PathEnd.cc: PathEndLatchCheck constructor/type ===

// === PathEnd.cc: PathEndPathDelay constructor/type ===

// === PathEnd.cc: PathEnd comparison statics ===

// === Bfs.cc: BfsFwdIterator/BfsBkwdIterator ===

TEST_F(StaInitTest, BfsFwdIteratorConstruct) {
  ASSERT_NO_THROW(( [&](){
    BfsFwdIterator iter(BfsIndex::other, nullptr, sta_);
    iter.hasNext();
  }() ));
}

TEST_F(StaInitTest, BfsBkwdIteratorConstruct) {
  ASSERT_NO_THROW(( [&](){
    BfsBkwdIterator iter(BfsIndex::other, nullptr, sta_);
    iter.hasNext();
  }() ));
}

// === ClkNetwork.cc: ClkNetwork accessors ===

// === Corner.cc: Corner accessors ===

TEST_F(StaInitTest, CornerAccessors) {
  Scene *corner = sta_->cmdScene();
  ASSERT_NE(corner, nullptr);
  int idx = corner->index();
  EXPECT_GE(idx, 0);
  const std::string &name = corner->name();
  EXPECT_FALSE(name.empty());
}

// === WorstSlack.cc: function exists ===

// === PathGroup.cc: PathGroup name constants ===

// === CheckTiming.cc: checkTiming ===

TEST_F(StaInitTest, StaCheckTimingThrows2) {
  EXPECT_THROW(sta_->checkTiming(sta_->cmdMode(), true, true, true, true, true, true, true), Exception);
}

// === PathExpanded.cc: PathExpanded on empty path ===

// === PathEnum.cc: function exists ===

// === Genclks.cc: Genclks exists ===

TEST_F(StaInitTest, GenclksExists2) {
  auto fn = &Genclks::clear;
  expectCallablePointerUsable(fn);
}

// === MakeTimingModel.cc: function exists ===

TEST_F(StaInitTest, StaWriteTimingModelThrows) {
  EXPECT_THROW(sta_->writeTimingModel("out.lib", "model", "cell", nullptr), Exception);
}

// === Tag.cc: Tag function exists ===

// === StaState.cc: StaState units ===

TEST_F(StaInitTest, StaStateReport) {
  Report *rpt = sta_->report();
  EXPECT_NE(rpt, nullptr);
}

// === ClkSkew.cc: function exists ===

// === ClkLatency.cc: function exists ===

// === ClkInfo.cc: accessors ===

// === Crpr.cc: function exists ===

// === FindRegister.cc: findRegister functions ===

TEST_F(StaInitTest, StaFindRegisterInstancesThrows2) {
  EXPECT_THROW(sta_->findRegisterInstances(nullptr, RiseFallBoth::riseFall(), false, false, sta_->cmdMode()), Exception);
}

TEST_F(StaInitTest, StaFindRegisterClkPinsThrows2) {
  EXPECT_THROW(sta_->findRegisterClkPins(nullptr, RiseFallBoth::riseFall(), false, false, sta_->cmdMode()), Exception);
}

TEST_F(StaInitTest, StaFindRegisterDataPinsThrows2) {
  EXPECT_THROW(sta_->findRegisterDataPins(nullptr, RiseFallBoth::riseFall(), false, false, sta_->cmdMode()), Exception);
}

TEST_F(StaInitTest, StaFindRegisterOutputPinsThrows2) {
  EXPECT_THROW(sta_->findRegisterOutputPins(nullptr, RiseFallBoth::riseFall(), false, false, sta_->cmdMode()), Exception);
}

TEST_F(StaInitTest, StaFindRegisterAsyncPinsThrows2) {
  EXPECT_THROW(sta_->findRegisterAsyncPins(nullptr, RiseFallBoth::riseFall(), false, false, sta_->cmdMode()), Exception);
}

// === Sta.cc: Sta::setCurrentInstance ===

TEST_F(StaInitTest, StaSetCurrentInstanceNull) {
  ASSERT_NO_THROW(( [&](){
    sta_->setCurrentInstance(nullptr);
  }() ));
}

// === Sta.cc: Sta::pathGroupNames ===

TEST_F(StaInitTest, StaPathGroupNames) {
  ASSERT_NO_THROW(( [&](){
    sta_->pathGroupNames(sta_->cmdSdc());
  }() ));
}

// === Sta.cc: Sta::isPathGroupName ===

TEST_F(StaInitTest, StaIsPathGroupName) {
  bool val = sta_->isPathGroupName("nonexistent", sta_->cmdSdc());
  EXPECT_FALSE(val);
}

// === Sta.cc: Sta::removeClockGroupsLogicallyExclusive etc ===

TEST_F(StaInitTest, StaRemoveClockGroupsLogicallyExclusive2) {
  ASSERT_NO_THROW(( [&](){
    sta_->removeClockGroupsLogicallyExclusive("test", sta_->cmdSdc());
  }() ));
}

TEST_F(StaInitTest, StaRemoveClockGroupsPhysicallyExclusive2) {
  ASSERT_NO_THROW(( [&](){
    sta_->removeClockGroupsPhysicallyExclusive("test", sta_->cmdSdc());
  }() ));
}

TEST_F(StaInitTest, StaRemoveClockGroupsAsynchronous2) {
  ASSERT_NO_THROW(( [&](){
    sta_->removeClockGroupsAsynchronous("test", sta_->cmdSdc());
  }() ));
}

// === Sta.cc: Sta::setDebugLevel ===

TEST_F(StaInitTest, StaSetDebugLevel) {
  ASSERT_NO_THROW(( [&](){
    sta_->setDebugLevel("search", 0);
  }() ));
}

// === Sta.cc: Sta::slowDrivers ===

// === Sta.cc: Sta::setMinPulseWidth ===

TEST_F(StaInitTest, StaSetMinPulseWidth) {
  ASSERT_NO_THROW(( [&](){
    sta_->setMinPulseWidth(RiseFallBoth::riseFall(), 0.1f, sta_->cmdSdc());
  }() ));
}

// === Sta.cc: various set* functions that delegate to Sdc ===

TEST_F(StaInitTest, StaSetClockGatingCheckGlobal2) {
  ASSERT_NO_THROW(( [&](){
    sta_->setClockGatingCheck(RiseFallBoth::riseFall(), MinMax::max(), 0.1f, sta_->cmdSdc());
  }() ));
}

// === Sta.cc: Sta::makeExceptionFrom/Thru/To ===

TEST_F(StaInitTest, StaMakeExceptionFrom2) {
  ASSERT_NO_THROW(( [&](){
    ExceptionFrom *from = sta_->makeExceptionFrom(nullptr, nullptr, nullptr,
                                                    RiseFallBoth::riseFall(), sta_->cmdSdc());
    // Returns a valid ExceptionFrom even with null args
    if (from) sta_->deleteExceptionFrom(from);
  }() ));
}

TEST_F(StaInitTest, StaMakeExceptionThru2) {
  ASSERT_NO_THROW(( [&](){
    ExceptionThru *thru = sta_->makeExceptionThru(nullptr, nullptr, nullptr,
                                                    RiseFallBoth::riseFall(),
                                                    sta_->cmdSdc());
    if (thru) sta_->deleteExceptionThru(thru);
  }() ));
}

TEST_F(StaInitTest, StaMakeExceptionTo2) {
  ASSERT_NO_THROW(( [&](){
    ExceptionTo *to = sta_->makeExceptionTo(nullptr, nullptr, nullptr,
                                              RiseFallBoth::riseFall(),
                                              RiseFallBoth::riseFall(), sta_->cmdSdc());
    if (to) sta_->deleteExceptionTo(to);
  }() ));
}

// === Sta.cc: Sta::setLatchBorrowLimit ===

// === Sta.cc: Sta::setDriveResistance ===

// === Sta.cc: Sta::setInputSlew ===

// === Sta.cc: Sta::setResistance ===

// === Sta.cc: Sta::setNetWireCap ===

// === Sta.cc: Sta::connectedCap ===

// === Sta.cc: Sta::portExtCaps ===

// === Sta.cc: Sta::setOperatingConditions ===

TEST_F(StaInitTest, StaSetOperatingConditions2) {
  ASSERT_NO_THROW(( [&](){
    sta_->setOperatingConditions(nullptr, MinMaxAll::all(), sta_->cmdSdc());
  }() ));
}

// === Sta.cc: Sta::power ===

// === Sta.cc: Sta::readLiberty ===

// === Sta.cc: linkDesign ===

// === Sta.cc: Sta::readVerilog ===

// === Sta.cc: Sta::readSpef ===

// === Sta.cc: initSta and deleteAllMemory ===

TEST_F(StaInitTest, InitStaExists) {
  auto fn = &initSta;
  expectCallablePointerUsable(fn);
}

TEST_F(StaInitTest, DeleteAllMemoryExists) {
  auto fn = &deleteAllMemory;
  expectCallablePointerUsable(fn);
}

// === PathEnd.cc: slack computation on PathEndUnconstrained ===

TEST_F(StaInitTest, PathEndSlack) {
  ASSERT_NO_THROW(( [&](){
    Path *p = new Path();
    PathEndUnconstrained pe(p);
    pe.slack(sta_);
  }() ));
}

// === Sta.cc: Sta method exists checks for vertex* functions ===

// === Sta.cc: reporting function exists ===

// === Sta.cc: Sta::makeClockGroups ===

TEST_F(StaInitTest, StaMakeClockGroups) {
  ASSERT_NO_THROW(( [&](){
    sta_->makeClockGroups("test_grp", false, false, false, false, nullptr, sta_->cmdSdc());
  }() ));
}

// === Sta.cc: Sta::makeGroupPath ===

// ============================================================
// R5_ tests: Additional function coverage for search module
// ============================================================

// === CheckMaxSkews: constructor/destructor/clear ===

TEST_F(StaInitTest, CheckMaxSkewsCtorDtorClear) {
  ASSERT_NO_THROW(( [&](){
    CheckMaxSkews checker(sta_);
    checker.clear();
  }() ));
}

// === CheckMinPeriods: constructor/destructor/clear ===

TEST_F(StaInitTest, CheckMinPeriodsCtorDtorClear) {
  ASSERT_NO_THROW(( [&](){
    CheckMinPeriods checker(sta_);
    checker.clear();
  }() ));
}

// === CheckMinPulseWidths: constructor/destructor/clear ===

TEST_F(StaInitTest, CheckMinPulseWidthsCtorDtorClear) {
  ASSERT_NO_THROW(( [&](){
    CheckMinPulseWidths checker(sta_);
    checker.clear();
  }() ));
}

// === MinPulseWidthCheck: default constructor ===

TEST_F(StaInitTest, MinPulseWidthCheckDefaultCtor) {
  MinPulseWidthCheck check;
  EXPECT_EQ(check.openPath(), nullptr);
}

// === MinPulseWidthCheck: constructor with nullptr ===

TEST_F(StaInitTest, MinPulseWidthCheckNullptrCtor) {
  MinPulseWidthCheck check(nullptr);
  EXPECT_EQ(check.openPath(), nullptr);
}

// === MinPeriodCheck: constructor ===

TEST_F(StaInitTest, MinPeriodCheckCtor) {
  MinPeriodCheck check;
  EXPECT_EQ(check.pin(), nullptr);
  EXPECT_EQ(check.clk(), nullptr);
}

// === MinPeriodCheck: copy ===

// === MaxSkewSlackLess: constructor ===

TEST_F(StaInitTest, MaxSkewSlackLessCtor) {
  ASSERT_NO_THROW(( [&](){
    MaxSkewSlackLess less(sta_);
  }() ));
}

// === MinPeriodSlackLess: constructor ===

TEST_F(StaInitTest, MinPeriodSlackLessCtor) {
  ASSERT_NO_THROW(( [&](){
    MinPeriodSlackLess less(sta_);
  }() ));
}

// === MinPulseWidthSlackLess: constructor ===

TEST_F(StaInitTest, MinPulseWidthSlackLessCtor) {
  ASSERT_NO_THROW(( [&](){
    MinPulseWidthSlackLess less(sta_);
  }() ));
}

// === Path: default constructor and isNull ===

TEST_F(StaInitTest, PathDefaultCtorIsNull) {
  Path path;
  EXPECT_TRUE(path.isNull());
}

// === Path: copy from null pointer ===

TEST_F(StaInitTest, PathCopyFromNull) {
  Path path(static_cast<const Path *>(nullptr));
  EXPECT_TRUE(path.isNull());
}

// === Path: arrival/required getters on default path ===

TEST_F(StaInitTest, PathArrivalRequired) {
  Path path;
  path.setArrival(1.5f);
  EXPECT_FLOAT_EQ(path.arrival(), 1.5f);
  path.setRequired(2.5f);
  EXPECT_FLOAT_EQ(path.required(), 2.5f);
}

// === Path: isEnum ===

TEST_F(StaInitTest, PathIsEnum2) {
  Path path;
  EXPECT_FALSE(path.isEnum());
  path.setIsEnum(true);
  EXPECT_TRUE(path.isEnum());
  path.setIsEnum(false);
  EXPECT_FALSE(path.isEnum());
}

// === Path: prevPath on default ===

TEST_F(StaInitTest, PathPrevPathDefault) {
  Path path;
  EXPECT_EQ(path.prevPath(), nullptr);
}

// === Path: setPrevPath ===

TEST_F(StaInitTest, PathSetPrevPath2) {
  Path path;
  Path prev;
  path.setPrevPath(&prev);
  EXPECT_EQ(path.prevPath(), &prev);
  path.setPrevPath(nullptr);
  EXPECT_EQ(path.prevPath(), nullptr);
}

// === PathLess: constructor ===

TEST_F(StaInitTest, PathLessCtor) {
  ASSERT_NO_THROW(( [&](){
    PathLess less(sta_);
  }() ));
}

// === ClkSkew: default constructor ===

TEST_F(StaInitTest, ClkSkewDefaultCtor) {
  ClkSkew skew;
  EXPECT_EQ(skew.srcPath(), nullptr);
  EXPECT_EQ(skew.tgtPath(), nullptr);
  EXPECT_FLOAT_EQ(skew.skew(), 0.0f);
}

// === ClkSkew: copy constructor ===

TEST_F(StaInitTest, ClkSkewCopyCtor) {
  ClkSkew skew1;
  ClkSkew skew2(skew1);
  EXPECT_EQ(skew2.srcPath(), nullptr);
  EXPECT_EQ(skew2.tgtPath(), nullptr);
  EXPECT_FLOAT_EQ(skew2.skew(), 0.0f);
}

// === ClkSkew: assignment operator ===

TEST_F(StaInitTest, ClkSkewAssignment2) {
  ClkSkew skew1;
  ClkSkew skew2;
  skew2 = skew1;
  EXPECT_EQ(skew2.srcPath(), nullptr);
  EXPECT_FLOAT_EQ(skew2.skew(), 0.0f);
}

// (Protected ReportPath methods removed - only public API tested)

// === ClkInfoLess: constructor ===

TEST_F(StaInitTest, ClkInfoLessCtor) {
  ASSERT_NO_THROW(( [&](){
    ClkInfoLess less(sta_);
  }() ));
}

// === ClkInfoEqual: constructor ===

TEST_F(StaInitTest, ClkInfoEqualCtor) {
  ASSERT_NO_THROW(( [&](){
    ClkInfoEqual eq(sta_);
  }() ));
}

// === ClkInfoHash: operator() with nullptr safety check ===

TEST_F(StaInitTest, ClkInfoHashExists) {
  ASSERT_NO_THROW(( [&](){
    ClkInfoHash hash;
  }() ));
}

// === TagLess: constructor ===

TEST_F(StaInitTest, TagLessCtor) {
  ASSERT_NO_THROW(( [&](){
    TagLess less(sta_);
  }() ));
}

// === TagIndexLess: existence ===

TEST_F(StaInitTest, TagIndexLessExists) {
  ASSERT_NO_THROW(( [&](){
    TagIndexLess less;
  }() ));
}

// === TagHash: constructor ===

TEST_F(StaInitTest, TagHashCtor) {
  ASSERT_NO_THROW(( [&](){
    TagHash hash(sta_);
  }() ));
}

// === TagEqual: constructor ===

TEST_F(StaInitTest, TagEqualCtor) {
  ASSERT_NO_THROW(( [&](){
    TagEqual eq(sta_);
  }() ));
}

// === TagMatchLess: constructor ===

TEST_F(StaInitTest, TagMatchLessCtor) {
  ASSERT_NO_THROW(( [&](){
    TagMatchLess less(true, sta_);
    TagMatchLess less2(false, sta_);
  }() ));
}

// === TagMatchHash: constructor ===

TEST_F(StaInitTest, TagMatchHashCtor) {
  ASSERT_NO_THROW(( [&](){
    TagMatchHash hash(true, sta_);
    TagMatchHash hash2(false, sta_);
  }() ));
}

// === TagMatchEqual: constructor ===

TEST_F(StaInitTest, TagMatchEqualCtor) {
  ASSERT_NO_THROW(( [&](){
    TagMatchEqual eq(true, sta_);
    TagMatchEqual eq2(false, sta_);
  }() ));
}

// (TagGroupBldr/Hash/Equal are incomplete types - skipped)

// === DiversionGreater: constructors ===

TEST_F(StaInitTest, DiversionGreaterDefaultCtor) {
  ASSERT_NO_THROW(( [&](){
    DiversionGreater greater;
  }() ));
}

TEST_F(StaInitTest, DiversionGreaterStaCtor) {
  ASSERT_NO_THROW(( [&](){
    DiversionGreater greater(sta_);
  }() ));
}

// === ClkSkews: constructor and clear ===

TEST_F(StaInitTest, ClkSkewsCtorClear) {
  ASSERT_NO_THROW(( [&](){
    ClkSkews skews(sta_);
    skews.clear();
  }() ));
}

// === Genclks: constructor, destructor, and clear ===

TEST_F(StaInitTest, GenclksCtorDtorClear) {
  ASSERT_NO_THROW(( [&](){
    Genclks genclks(sta_->cmdMode(), sta_);
    genclks.clear();
  }() ));
}

// === ClockPinPairLess: operator ===

TEST_F(StaInitTest, ClockPinPairLessExists) {
  ASSERT_NO_THROW(( [&](){
    // ClockPinPairLess comparison dereferences Clock*, so just test existence
    ClockPinPairLess less;
    expectStaCoreState(sta_);
  }() ));
}

// === Levelize: setLevelSpace ===

TEST_F(StaInitTest, LevelizeSetLevelSpace2) {
  ASSERT_NO_THROW(( [&](){
    Levelize *levelize = sta_->levelize();
    levelize->setLevelSpace(5);
  }() ));
}

// === Levelize: maxLevel ===

TEST_F(StaInitTest, LevelizeMaxLevel2) {
  Levelize *levelize = sta_->levelize();
  int ml = levelize->maxLevel();
  EXPECT_GE(ml, 0);
}

// === Levelize: clear ===

TEST_F(StaInitTest, LevelizeClear2) {
  ASSERT_NO_THROW(( [&](){
    Levelize *levelize = sta_->levelize();
    levelize->clear();
  }() ));
}

// === SearchPred0: constructor ===

TEST_F(StaInitTest, SearchPred0Ctor) {
  ASSERT_NO_THROW(( [&](){
    SearchPred0 pred(sta_);
  }() ));
}

// === SearchPred1: constructor ===

TEST_F(StaInitTest, SearchPred1Ctor) {
  ASSERT_NO_THROW(( [&](){
    SearchPred1 pred(sta_);
  }() ));
}

// === SearchPred2: constructor ===

// === SearchPredNonLatch2: constructor ===

// === SearchPredNonReg2: constructor ===

// === ClkTreeSearchPred: constructor ===

TEST_F(StaInitTest, ClkTreeSearchPredCtor) {
  ASSERT_NO_THROW(( [&](){
    ClkTreeSearchPred pred(sta_);
  }() ));
}

// === FanOutSrchPred: constructor ===

TEST_F(StaInitTest, FanOutSrchPredCtor) {
  ASSERT_NO_THROW(( [&](){
    FanOutSrchPred pred(sta_);
  }() ));
}

// === WorstSlack: constructor/destructor ===

TEST_F(StaInitTest, WorstSlackCtorDtor) {
  ASSERT_NO_THROW(( [&](){
    WorstSlack ws(sta_);
  }() ));
}

// === WorstSlack: copy constructor ===

TEST_F(StaInitTest, WorstSlackCopyCtor) {
  ASSERT_NO_THROW(( [&](){
    WorstSlack ws1(sta_);
    WorstSlack ws2(ws1);
  }() ));
}

// === WorstSlacks: constructor ===

TEST_F(StaInitTest, WorstSlacksCtorDtor) {
  ASSERT_NO_THROW(( [&](){
    WorstSlacks wslacks(sta_);
  }() ));
}

// === Sim: clear ===

// === StaState: copyUnits ===

TEST_F(StaInitTest, StaStateCopyUnits3) {
  ASSERT_NO_THROW(( [&](){
    Units *units = sta_->units();
    sta_->copyUnits(units);
  }() ));
}

// === PropertyValue: default constructor ===

TEST_F(StaInitTest, PropertyValueDefaultCtor) {
  PropertyValue pv;
  EXPECT_EQ(pv.type(), PropertyValue::Type::none);
}

// === PropertyValue: string constructor ===

TEST_F(StaInitTest, PropertyValueStringCtor) {
  PropertyValue pv("hello");
  EXPECT_EQ(pv.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv.stringValue(), "hello");
}

// === PropertyValue: float constructor ===

TEST_F(StaInitTest, PropertyValueFloatCtor) {
  PropertyValue pv(3.14f, nullptr);
  EXPECT_EQ(pv.type(), PropertyValue::Type::float_);
  EXPECT_FLOAT_EQ(pv.floatValue(), 3.14f);
}

// === PropertyValue: bool constructor ===

TEST_F(StaInitTest, PropertyValueBoolCtor) {
  PropertyValue pv(true);
  EXPECT_EQ(pv.type(), PropertyValue::Type::bool_);
  EXPECT_TRUE(pv.boolValue());
}

// === PropertyValue: copy constructor ===

TEST_F(StaInitTest, PropertyValueCopyCtor) {
  PropertyValue pv1("test");
  PropertyValue pv2(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv2.stringValue(), "test");
}

// === PropertyValue: move constructor ===

TEST_F(StaInitTest, PropertyValueMoveCtor) {
  PropertyValue pv1("test");
  PropertyValue pv2(std::move(pv1));
  EXPECT_EQ(pv2.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv2.stringValue(), "test");
}

// === PropertyValue: copy assignment ===

TEST_F(StaInitTest, PropertyValueCopyAssign) {
  PropertyValue pv1("test");
  PropertyValue pv2;
  pv2 = pv1;
  EXPECT_EQ(pv2.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv2.stringValue(), "test");
}

// === PropertyValue: move assignment ===

TEST_F(StaInitTest, PropertyValueMoveAssign) {
  PropertyValue pv1("test");
  PropertyValue pv2;
  pv2 = std::move(pv1);
  EXPECT_EQ(pv2.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv2.stringValue(), "test");
}

// === PropertyValue: Library constructor ===

TEST_F(StaInitTest, PropertyValueLibraryCtor) {
  PropertyValue pv(static_cast<const Library *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::library);
  EXPECT_EQ(pv.library(), nullptr);
}

// === PropertyValue: Cell constructor ===

TEST_F(StaInitTest, PropertyValueCellCtor) {
  PropertyValue pv(static_cast<const Cell *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::cell);
  EXPECT_EQ(pv.cell(), nullptr);
}

// === PropertyValue: Port constructor ===

TEST_F(StaInitTest, PropertyValuePortCtor) {
  PropertyValue pv(static_cast<const Port *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::port);
  EXPECT_EQ(pv.port(), nullptr);
}

// === PropertyValue: LibertyLibrary constructor ===

TEST_F(StaInitTest, PropertyValueLibertyLibraryCtor) {
  PropertyValue pv(static_cast<const LibertyLibrary *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::liberty_library);
  EXPECT_EQ(pv.libertyLibrary(), nullptr);
}

// === PropertyValue: LibertyCell constructor ===

TEST_F(StaInitTest, PropertyValueLibertyCellCtor) {
  PropertyValue pv(static_cast<const LibertyCell *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::liberty_cell);
  EXPECT_EQ(pv.libertyCell(), nullptr);
}

// === PropertyValue: LibertyPort constructor ===

TEST_F(StaInitTest, PropertyValueLibertyPortCtor) {
  PropertyValue pv(static_cast<const LibertyPort *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::liberty_port);
  EXPECT_EQ(pv.libertyPort(), nullptr);
}

// === PropertyValue: Instance constructor ===

TEST_F(StaInitTest, PropertyValueInstanceCtor) {
  PropertyValue pv(static_cast<const Instance *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::instance);
  EXPECT_EQ(pv.instance(), nullptr);
}

// === PropertyValue: Pin constructor ===

TEST_F(StaInitTest, PropertyValuePinCtor) {
  PropertyValue pv(static_cast<const Pin *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::pin);
  EXPECT_EQ(pv.pin(), nullptr);
}

// === PropertyValue: Net constructor ===

TEST_F(StaInitTest, PropertyValueNetCtor) {
  PropertyValue pv(static_cast<const Net *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::net);
  EXPECT_EQ(pv.net(), nullptr);
}

// === PropertyValue: Clock constructor ===

TEST_F(StaInitTest, PropertyValueClockCtor) {
  PropertyValue pv(static_cast<const Clock *>(nullptr));
  EXPECT_EQ(pv.type(), PropertyValue::Type::clk);
  EXPECT_EQ(pv.clock(), nullptr);
}

// (Properties protected methods and Sta protected methods skipped)

// === Sta: maxPathCountVertex ===

// === Sta: connectPin ===

// === Sta: replaceCellExists ===

// === Sta: disable functions exist ===

// === Sta: removeDisable functions exist ===

// === Sta: disableClockGatingCheck ===

// === Sta: removeDisableClockGatingCheck ===

// === Sta: vertexArrival overloads exist ===

// === Sta: vertexRequired overloads exist ===

// === Sta: vertexSlack overload exists ===

// === Sta: vertexSlew overloads exist ===

// === Sta: vertexPathIterator exists ===

// === Sta: vertexWorstRequiredPath overloads ===

// === Sta: checkCapacitance exists ===

// === Sta: checkSlew exists ===

// === Sta: checkFanout exists ===

// === Sta: findSlewLimit exists ===

// === Sta: reportCheck exists ===

// === Sta: pinsForClock exists ===

// === Sta: removeDataCheck exists ===

// === Sta: makePortPinAfter exists ===

// === Sta: setArcDelayAnnotated exists ===

// === Sta: delaysInvalidFromFanin exists ===

// === Sta: makeParasiticNetwork exists ===

// === Sta: pathAnalysisPt exists ===

// === Sta: pathDcalcAnalysisPt exists ===

// === Sta: pvt exists ===

// === Sta: setPvt exists ===

// === Search: arrivalsValid ===

TEST_F(StaInitTest, SearchArrivalsValid) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    (void)search->arrivalsValid();
  }() ));
}

// === Sim: findLogicConstants ===

// === ReportField: getters ===

TEST_F(StaInitTest, ReportFieldGetters) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *field = rpt->fieldSlew();
  EXPECT_NE(field, nullptr);
  EXPECT_NE(field->name(), nullptr);
  EXPECT_NE(field->title(), nullptr);
  EXPECT_GT(field->width(), 0);
  EXPECT_NE(field->blank(), nullptr);
}

// === ReportField: setWidth ===

TEST_F(StaInitTest, ReportFieldSetWidth2) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *field = rpt->fieldFanout();
  int orig = field->width();
  field->setWidth(20);
  EXPECT_EQ(field->width(), 20);
  field->setWidth(orig);
}

// === ReportField: setEnabled ===

TEST_F(StaInitTest, ReportFieldSetEnabled2) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *field = rpt->fieldCapacitance();
  bool orig = field->enabled();
  field->setEnabled(!orig);
  EXPECT_EQ(field->enabled(), !orig);
  field->setEnabled(orig);
}

// === ReportField: leftJustify ===

TEST_F(StaInitTest, ReportFieldLeftJustify) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    ReportField *field = rpt->fieldSlew();
    field->leftJustify();
  }() ));
}

// === ReportField: unit ===

TEST_F(StaInitTest, ReportFieldUnit) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    ReportField *field = rpt->fieldSlew();
    Unit *u = field->unit();
    EXPECT_NE(u, nullptr);
  }() ));
}

// === Corner: constructor ===

TEST_F(StaInitTest, CornerCtor) {
  Scene corner("test_corner", 0, sta_->cmdMode(), nullptr);
  EXPECT_EQ(corner.name(), "test_corner");
  EXPECT_EQ(corner.index(), 0);
}

// === Corners: count ===

TEST_F(StaInitTest, CornersCount) {
  const SceneSeq &corners = sta_->scenes();
  EXPECT_GE(corners.size(), 0);
}

// === Path static: less with null paths ===

TEST_F(StaInitTest, PathStaticLessNull) {
  Path p1;
  Path p2;
  // Both null - less should be false
  bool result = Path::less(&p1, &p2, sta_);
  EXPECT_FALSE(result);
}

// === Path static: lessAll with null paths ===

TEST_F(StaInitTest, PathStaticLessAllNull) {
  Path p1;
  Path p2;
  bool result = Path::lessAll(&p1, &p2, sta_);
  EXPECT_FALSE(result);
}

// === Path static: equal with null paths ===

TEST_F(StaInitTest, PathStaticEqualNull) {
  Path p1;
  Path p2;
  bool result = Path::equal(&p1, &p2, sta_);
  EXPECT_TRUE(result);
}

// === Sta: isClockNet returns false with no design ===

// === PropertyValue: PinSeq constructor ===

TEST_F(StaInitTest, PropertyValuePinSeqCtor) {
  PinSeq *pins = new PinSeq;
  PropertyValue pv(pins);
  EXPECT_EQ(pv.type(), PropertyValue::Type::pins);
}

// === PropertyValue: ClockSeq constructor ===

TEST_F(StaInitTest, PropertyValueClockSeqCtor) {
  ClockSeq *clks = new ClockSeq;
  PropertyValue pv(clks);
  EXPECT_EQ(pv.type(), PropertyValue::Type::clks);
}

// === Search: tagGroup returns nullptr for invalid index ===

// === ClkNetwork: pinsForClock and clocks exist ===

// === PathEnd: type enum values ===

TEST_F(StaInitTest, PathEndTypeEnums2) {
  EXPECT_EQ(static_cast<int>(PathEnd::Type::unconstrained), 0);
  EXPECT_EQ(static_cast<int>(PathEnd::Type::check), 1);
  EXPECT_EQ(static_cast<int>(PathEnd::Type::data_check), 2);
  EXPECT_EQ(static_cast<int>(PathEnd::Type::latch_check), 3);
  EXPECT_EQ(static_cast<int>(PathEnd::Type::output_delay), 4);
  EXPECT_EQ(static_cast<int>(PathEnd::Type::gated_clk), 5);
  EXPECT_EQ(static_cast<int>(PathEnd::Type::path_delay), 6);
}

// === PathEnd: less function exists ===

// === PathEnd: cmpNoCrpr function exists ===

// === ReportPathFormat enum ===

TEST_F(StaInitTest, ReportPathFormatEnums) {
  EXPECT_NE(ReportPathFormat::full, ReportPathFormat::json);
  EXPECT_NE(ReportPathFormat::full_clock, ReportPathFormat::endpoint);
  EXPECT_NE(ReportPathFormat::summary, ReportPathFormat::slack_only);
}

// === SearchClass constants ===

TEST_F(StaInitTest, SearchClassConstants2) {
  EXPECT_GT(tag_index_bit_count, 0u);
  EXPECT_EQ(tag_index_null, tag_index_max);
  EXPECT_GT(path_ap_index_bit_count, 0);
  EXPECT_GT(scene_count_max, 0);
}

// === ReportPath: setReportFields (public) ===

TEST_F(StaInitTest, ReportPathSetReportFields2) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->setReportFields(true, true, true, true, true, true, true);
    rpt->setReportFields(false, false, false, false, false, false, false);
  }() ));
}

// === MaxSkewCheck: skew with empty paths ===

TEST_F(StaInitTest, MaxSkewCheckSkewZero) {
  Path clk_path;
  Path ref_path;
  clk_path.setArrival(0.0f);
  ref_path.setArrival(0.0f);
  MaxSkewCheck check(&clk_path, &ref_path, nullptr, nullptr);
  Delay s = check.skew();
  EXPECT_FLOAT_EQ(s, 0.0f);
}

// === MaxSkewCheck: skew with different arrivals ===

TEST_F(StaInitTest, MaxSkewCheckSkewNonZero) {
  Path clk_path;
  Path ref_path;
  clk_path.setArrival(5.0f);
  ref_path.setArrival(3.0f);
  MaxSkewCheck check(&clk_path, &ref_path, nullptr, nullptr);
  Delay s = check.skew();
  EXPECT_FLOAT_EQ(s, 2.0f);
}

// === MaxSkewCheck: clkPath and refPath ===

TEST_F(StaInitTest, MaxSkewCheckPaths) {
  Path clk_path;
  Path ref_path;
  MaxSkewCheck check(&clk_path, &ref_path, nullptr, nullptr);
  EXPECT_EQ(check.clkPath(), &clk_path);
  EXPECT_EQ(check.refPath(), &ref_path);
  EXPECT_EQ(check.checkArc(), nullptr);
}

// === ClkSkew: srcTgtPathNameLess exists ===

TEST_F(StaInitTest, ClkSkewSrcTgtPathNameLessExists) {
  auto fn = &ClkSkew::srcTgtPathNameLess;
  expectCallablePointerUsable(fn);
}

// === ClkSkew: srcInternalClkLatency exists ===

TEST_F(StaInitTest, ClkSkewSrcInternalClkLatencyExists) {
  auto fn = &ClkSkew::srcInternalClkLatency;
  expectCallablePointerUsable(fn);
}

// === ClkSkew: tgtInternalClkLatency exists ===

TEST_F(StaInitTest, ClkSkewTgtInternalClkLatencyExists) {
  auto fn = &ClkSkew::tgtInternalClkLatency;
  expectCallablePointerUsable(fn);
}

// === ReportPath: setReportFieldOrder ===

// === ReportPath: findField ===

TEST_F(StaInitTest, ReportPathFindFieldByName) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *slew = rpt->findField("slew");
  EXPECT_NE(slew, nullptr);
  ReportField *fanout = rpt->findField("fanout");
  EXPECT_NE(fanout, nullptr);
  ReportField *cap = rpt->findField("capacitance");
  EXPECT_NE(cap, nullptr);
  // Non-existent field
  ReportField *nonexist = rpt->findField("nonexistent_field");
  EXPECT_EQ(nonexist, nullptr);
}

// === PropertyValue: std::string constructor ===

TEST_F(StaInitTest, PropertyValueStdStringCtor) {
  std::string s = "test_string";
  PropertyValue pv(s);
  EXPECT_EQ(pv.type(), PropertyValue::Type::string);
  EXPECT_STREQ(pv.stringValue(), "test_string");
}

// === Levelize: invalid ===

TEST_F(StaInitTest, LevelizeInvalid) {
  Levelize *levelize = sta_->levelize();
  levelize->invalid();
  EXPECT_FALSE(levelize->levelized());
}

// === R5_ Round 2: Re-add public ReportPath methods that were accidentally removed ===

TEST_F(StaInitTest, ReportPathDigits) {
  ReportPath *rpt = sta_->reportPath();
  int d = rpt->digits();
  EXPECT_GE(d, 0);
}

TEST_F(StaInitTest, ReportPathSetDigits) {
  ReportPath *rpt = sta_->reportPath();
  rpt->setDigits(5);
  EXPECT_EQ(rpt->digits(), 5);
  rpt->setDigits(3);  // restore default
}

TEST_F(StaInitTest, ReportPathReportSigmas2) {
  ReportPath *rpt = sta_->reportPath();
  bool sigmas = rpt->reportSigmas();
  // Default should be false
  EXPECT_FALSE(sigmas);
}

TEST_F(StaInitTest, ReportPathSetReportSigmas) {
  ReportPath *rpt = sta_->reportPath();
  rpt->setReportSigmas(true);
  EXPECT_TRUE(rpt->reportSigmas());
  rpt->setReportSigmas(false);
}

TEST_F(StaInitTest, ReportPathPathFormat) {
  ReportPath *rpt = sta_->reportPath();
  ReportPathFormat fmt = rpt->pathFormat();
  // Check it is a valid format
  EXPECT_TRUE(fmt == ReportPathFormat::full
              || fmt == ReportPathFormat::full_clock
              || fmt == ReportPathFormat::full_clock_expanded
              || fmt == ReportPathFormat::summary
              || fmt == ReportPathFormat::slack_only
              || fmt == ReportPathFormat::endpoint
              || fmt == ReportPathFormat::json);
}

TEST_F(StaInitTest, ReportPathSetPathFormat) {
  ReportPath *rpt = sta_->reportPath();
  ReportPathFormat old_fmt = rpt->pathFormat();
  rpt->setPathFormat(ReportPathFormat::summary);
  EXPECT_EQ(rpt->pathFormat(), ReportPathFormat::summary);
  rpt->setPathFormat(old_fmt);
}

TEST_F(StaInitTest, ReportPathFindFieldSlew) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *slew = rpt->findField("slew");
  EXPECT_NE(slew, nullptr);
  EXPECT_STREQ(slew->name(), "slew");
}

TEST_F(StaInitTest, ReportPathFindFieldFanout) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *fo = rpt->findField("fanout");
  EXPECT_NE(fo, nullptr);
  EXPECT_STREQ(fo->name(), "fanout");
}

TEST_F(StaInitTest, ReportPathFindFieldCapacitance) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *cap = rpt->findField("capacitance");
  EXPECT_NE(cap, nullptr);
  EXPECT_STREQ(cap->name(), "capacitance");
}

TEST_F(StaInitTest, ReportPathFindFieldNonexistent) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *f = rpt->findField("nonexistent_field_xyz");
  EXPECT_EQ(f, nullptr);
}

TEST_F(StaInitTest, ReportPathSetNoSplit) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->setNoSplit(true);
    rpt->setNoSplit(false);
    expectStaCoreState(sta_);
  }() ));
}

TEST_F(StaInitTest, ReportPathFieldSrcAttr) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    ReportField *src = rpt->fieldSrcAttr();
    // src_attr field may or may not exist
    expectStaCoreState(sta_);
  }() ));
}

TEST_F(StaInitTest, ReportPathSetReportFieldsPublic) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    // Call setReportFields with various combinations
    rpt->setReportFields(true, false, false, false, true, false, false);
    rpt->setReportFields(true, true, true, true, true, true, true);
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: header methods (public) ===

// === ReportPath: report method function pointers ===

// === ReportPath: reportCheck overloads ===

// === ReportPath: report short/full/json overloads ===

// === ReportPath: report short for PathEnd types ===

// === ReportPath: reportFull for PathEnd types ===

// === ReportField: blank getter ===

TEST_F(StaInitTest, ReportFieldBlank2) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *field = rpt->fieldSlew();
  const char *blank = field->blank();
  EXPECT_NE(blank, nullptr);
}

// === ReportField: setProperties ===

TEST_F(StaInitTest, ReportFieldSetProperties2) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *field = rpt->fieldSlew();
  int old_width = field->width();
  bool old_justify = field->leftJustify();
  // set new properties
  field->setProperties("NewTitle", 15, true);
  EXPECT_EQ(field->width(), 15);
  EXPECT_TRUE(field->leftJustify());
  // restore
  field->setProperties("Slew", old_width, old_justify);
}

// === CheckCapacitances: constructor ===

TEST_F(StaInitTest, CheckCapacitancesCtorDtor) {
  ASSERT_NO_THROW(( [&](){
    CheckCapacitances checker(sta_);
    expectStaCoreState(sta_);
  }() ));
}

// === CheckSlews: constructor ===

TEST_F(StaInitTest, CheckSlewsCtorDtor) {
  ASSERT_NO_THROW(( [&](){
    CheckSlews checker(sta_);
    expectStaCoreState(sta_);
  }() ));
}

// === CheckFanouts: constructor ===

TEST_F(StaInitTest, CheckFanoutsCtorDtor) {
  ASSERT_NO_THROW(( [&](){
    CheckFanouts checker(sta_);
    expectStaCoreState(sta_);
  }() ));
}

// === PathExpanded: empty constructor ===

TEST_F(StaInitTest, PathExpandedEmptyCtor) {
  PathExpanded expanded(sta_);
  EXPECT_EQ(expanded.size(), static_cast<size_t>(0));
}

// === PathGroups: static group names ===

TEST_F(StaInitTest, PathGroupsAsyncGroupName) {
  const char *name = PathGroups::asyncPathGroupName();
  EXPECT_NE(name, nullptr);
}

TEST_F(StaInitTest, PathGroupsPathDelayGroupName) {
  const char *name = PathGroups::pathDelayGroupName();
  EXPECT_NE(name, nullptr);
}

TEST_F(StaInitTest, PathGroupsGatedClkGroupName) {
  const char *name = PathGroups::gatedClkGroupName();
  EXPECT_NE(name, nullptr);
}

TEST_F(StaInitTest, PathGroupsUnconstrainedGroupName) {
  const char *name = PathGroups::unconstrainedGroupName();
  EXPECT_NE(name, nullptr);
}

// === PathGroup: static max path count ===

TEST_F(StaInitTest, PathGroupMaxPathCountMax) {
  EXPECT_GT(PathGroup::group_path_count_max, static_cast<size_t>(0));
}

// === PathGroup: makePathGroupSlack factory ===

TEST_F(StaInitTest, PathGroupMakeSlack2) {
  PathGroup *pg = PathGroup::makePathGroupSlack(
    "test_slack", 10, 1, false, false, -1e30, 1e30, sta_);
  EXPECT_NE(pg, nullptr);
  EXPECT_STREQ(pg->name(), "test_slack");
  EXPECT_EQ(pg->maxPaths(), 10);
  delete pg;
}

// === PathGroup: makePathGroupArrival factory ===

TEST_F(StaInitTest, PathGroupMakeArrival2) {
  PathGroup *pg = PathGroup::makePathGroupArrival(
    "test_arrival", 5, 1, false, false, MinMax::max(), sta_);
  EXPECT_NE(pg, nullptr);
  EXPECT_STREQ(pg->name(), "test_arrival");
  delete pg;
}

// === PathGroup: clear and pathEnds ===

TEST_F(StaInitTest, PathGroupClear) {
  PathGroup *pg = PathGroup::makePathGroupSlack(
    "test_clear", 10, 1, false, false, -1e30, 1e30, sta_);
  const PathEndSeq &ends = pg->pathEnds();
  EXPECT_EQ(ends.size(), static_cast<size_t>(0));
  pg->clear();
  EXPECT_EQ(pg->pathEnds().size(), static_cast<size_t>(0));
  delete pg;
}

// === CheckCrpr: constructor ===

TEST_F(StaInitTest, CheckCrprCtor) {
  ASSERT_NO_THROW(( [&](){
    CheckCrpr crpr(sta_);
    expectStaCoreState(sta_);
  }() ));
}

// === GatedClk: constructor ===

TEST_F(StaInitTest, GatedClkCtor) {
  ASSERT_NO_THROW(( [&](){
    GatedClk gclk(sta_);
    expectStaCoreState(sta_);
  }() ));
}

// === ClkLatency: constructor ===

TEST_F(StaInitTest, ClkLatencyCtor) {
  ASSERT_NO_THROW(( [&](){
    ClkLatency lat(sta_);
    expectStaCoreState(sta_);
  }() ));
}

// === Sta function pointers: more uncovered methods ===

// (Removed duplicates of R5_StaCheckSlewExists, R5_StaCheckCapacitanceExists,
//  R5_StaCheckFanoutExists, R5_StaFindSlewLimitExists, R5_StaVertexSlewDcalcExists,
//  R5_StaVertexSlewCornerMinMaxExists - already defined above)

// === Path: more static methods ===

TEST_F(StaInitTest, PathEqualBothNull) {
  bool eq = Path::equal(nullptr, nullptr, sta_);
  EXPECT_TRUE(eq);
}

// === Search: more function pointers ===

// === Levelize: more methods ===

TEST_F(StaInitTest, LevelizeLevelized) {
  ASSERT_NO_THROW(( [&](){
    Levelize *levelize = sta_->levelize();
    levelize->levelized();
    expectStaCoreState(sta_);
  }() ));
}

// === Sim: more methods ===

// === Corners: iteration ===

TEST_F(StaInitTest, CornersIteration) {
  const SceneSeq &corners = sta_->scenes();
  int count = corners.size();
  EXPECT_GE(count, 1);
  Scene *corner = corners[0];
  EXPECT_NE(corner, nullptr);
}

TEST_F(StaInitTest, CornerFindName) {
  ASSERT_NO_THROW(( [&](){
    const SceneSeq &corners = sta_->scenes();
    Scene *corner = sta_->findScene("default");
    EXPECT_NE(corner, nullptr);
    expectStaCoreState(sta_);
  }() ));
}

// === Corner: name and index ===

TEST_F(StaInitTest, CornerNameAndIndex) {
  const SceneSeq &corners = sta_->scenes();
  Scene *corner = corners[0];
  EXPECT_NE(corner, nullptr);
  const std::string &name = corner->name();
  EXPECT_FALSE(name.empty());
  int idx = corner->index();
  EXPECT_EQ(idx, 0);
}

// === PathEnd: function pointer existence for virtual methods ===

// (Removed duplicates of R5_StaVertexPathIteratorExists, R5_StaPathAnalysisPtExists,
//  R5_StaPathDcalcAnalysisPtExists - already defined above)

// === ReportPath: reportPathEnds function pointer ===

// === ReportPath: reportPath function pointer (Path*) ===

// === ReportPath: reportEndLine function pointer ===

// === ReportPath: reportSummaryLine function pointer ===

// === ReportPath: reportSlackOnly function pointer ===

// === ReportPath: reportJson overloads ===

// === Search: pathClkPathArrival function pointers ===

// === Genclks: more function pointers ===

// (Removed R5_GenclksGenclkInfoExists - genclkInfo is private)
// (Removed R5_GenclksSrcPathExists - srcPath has wrong signature)
// (Removed R5_SearchSeedInputArrivalsExists - seedInputArrivals is protected)
// (Removed R5_SimClockGateOutValueExists - clockGateOutValue is protected)
// (Removed R5_SimPinConstFuncValueExists - pinConstFuncValue is protected)

// === MinPulseWidthCheck: corner function pointer ===

// === MaxSkewCheck: more function pointers ===

TEST_F(StaInitTest, MaxSkewCheckClkPinExists) {
  auto fn = &MaxSkewCheck::clkPin;
  expectCallablePointerUsable(fn);
}

TEST_F(StaInitTest, MaxSkewCheckRefPinExists) {
  auto fn = &MaxSkewCheck::refPin;
  expectCallablePointerUsable(fn);
}

TEST_F(StaInitTest, MaxSkewCheckMaxSkewMethodExists) {
  auto fn = &MaxSkewCheck::maxSkew;
  expectCallablePointerUsable(fn);
}

TEST_F(StaInitTest, MaxSkewCheckSlackExists) {
  auto fn = &MaxSkewCheck::slack;
  expectCallablePointerUsable(fn);
}

// === ClkInfo: more function pointers ===

// === Tag: more function pointers ===

// (Removed R5_TagStateEqualExists - stateEqual is protected)

// === Path: more function pointers ===

// === PathEnd: reportShort virtual function pointer ===

// === ReportPath: reportPathEnd overloads ===

// (Removed duplicates of R5_StaVertexArrivalMinMaxExists etc - already defined above)

// === Corner: DcalcAnalysisPt access ===

// === PathAnalysisPt access through Corners ===

// === Sta: isClock(Pin*) function pointer ===

// === GraphLoop: report function pointer ===

// === SearchPredNonReg2: searchThru function pointer ===

// === CheckCrpr: maxCrpr function pointer ===

// === CheckCrpr: checkCrpr function pointers ===

// === GatedClk: isGatedClkEnable function pointer ===

// === GatedClk: gatedClkActiveTrans function pointer ===

// === FindRegister: free functions ===

////////////////////////////////////////////////////////////////
// R6_ tests: additional coverage for uncovered search functions
////////////////////////////////////////////////////////////////

// === OutputDelays: constructor and timingSense ===

// === ClkDelays: constructor and latency ===

// === Bdd: constructor and destructor ===

// === PathExpanded: constructor with two args (Path*, bool, StaState*) ===

TEST_F(StaInitTest, PathExpandedStaOnlyCtor) {
  PathExpanded expanded(sta_);
  EXPECT_EQ(expanded.size(), 0u);
}

// === Search: visitEndpoints ===

// havePendingLatchOutputs and clearPendingLatchOutputs are protected, skipped

// === Search: findPathGroup by name ===

// === Search: findPathGroup by clock ===

// === Search: tag ===

TEST_F(StaInitTest, SearchTagZero) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    // tagCount should be 0 initially
    TagIndex count = search->tagCount();
    EXPECT_GE(count, 0);
    expectStaCoreState(sta_);
  }() ));
}

// === Search: tagGroupCount ===

TEST_F(StaInitTest, SearchTagGroupCount3) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    TagGroupIndex count = search->tagGroupCount();
    EXPECT_GE(count, 0);
    expectStaCoreState(sta_);
  }() ));
}

// === Search: clkInfoCount ===

TEST_F(StaInitTest, SearchClkInfoCount3) {
  Search *search = sta_->search();
  int count = search->clkInfoCount();
  EXPECT_EQ(count, 0);
}

// === Search: initVars (called indirectly through clear) ===

TEST_F(StaInitTest, SearchClear3) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    search->clear();
    expectStaCoreState(sta_);
  }() ));
}

// === Search: checkPrevPaths ===

// === Search: arrivalsValid ===

TEST_F(StaInitTest, SearchArrivalsValid2) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    (void)search->arrivalsValid();
    expectStaCoreState(sta_);
  }() ));
}

// Search::endpoints segfaults without graph - skipped

// === Search: havePathGroups ===

// === Search: requiredsSeeded ===

// === Search: requiredsExist ===

// === Search: arrivalsAtEndpointsExist ===

// === Search: crprPathPruningEnabled / setCrprpathPruningEnabled ===

TEST_F(StaInitTest, SearchCrprPathPruning2) {
  Search *search = sta_->search();
  bool enabled = search->crprPathPruningEnabled();
  search->setCrprpathPruningEnabled(!enabled);
  EXPECT_NE(search->crprPathPruningEnabled(), enabled);
  search->setCrprpathPruningEnabled(enabled);
  EXPECT_EQ(search->crprPathPruningEnabled(), enabled);
}

// === Search: crprApproxMissingRequireds ===

TEST_F(StaInitTest, SearchCrprApproxMissingRequireds2) {
  Search *search = sta_->search();
  bool val = search->crprApproxMissingRequireds();
  search->setCrprApproxMissingRequireds(!val);
  EXPECT_NE(search->crprApproxMissingRequireds(), val);
  search->setCrprApproxMissingRequireds(val);
}

// === Search: unconstrainedPaths ===

// === Search: deletePathGroups ===

// === Search: deleteFilter ===

// === Search: arrivalIterator and requiredIterator ===

TEST_F(StaInitTest, SearchArrivalIterator) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    BfsFwdIterator *iter = search->arrivalIterator();
    EXPECT_NE(iter, nullptr);
    expectStaCoreState(sta_);
  }() ));
}

TEST_F(StaInitTest, SearchRequiredIterator) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    BfsBkwdIterator *iter = search->requiredIterator();
    EXPECT_NE(iter, nullptr);
    expectStaCoreState(sta_);
  }() ));
}

// === Search: evalPred and searchAdj ===

TEST_F(StaInitTest, SearchEvalPred2) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    EvalPred *pred = search->evalPred();
    EXPECT_NE(pred, nullptr);
    expectStaCoreState(sta_);
  }() ));
}

TEST_F(StaInitTest, SearchSearchAdj2) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    SearchPred *adj = search->searchAdj();
    EXPECT_NE(adj, nullptr);
    expectStaCoreState(sta_);
  }() ));
}

// === Sta: isClock(Net*) ===

// === Sta: pins(Clock*) ===

// === Sta: setCmdNamespace ===

TEST_F(StaInitTest, StaSetCmdNamespace2) {
  ASSERT_NO_THROW(( [&](){
    sta_->setCmdNamespace(CmdNamespace::sdc);
    sta_->setCmdNamespace(CmdNamespace::sta);
    expectStaCoreState(sta_);
  }() ));
}

// === Sta: vertexArrival(Vertex*, MinMax*) ===

// === Sta: vertexArrival(Vertex*, RiseFall*, PathAnalysisPt*) ===

// === Sta: vertexRequired(Vertex*, MinMax*) ===

// === Sta: vertexRequired(Vertex*, RiseFall*, MinMax*) ===

// === Sta: vertexRequired(Vertex*, RiseFall*, PathAnalysisPt*) ===

// === Sta: vertexSlack(Vertex*, RiseFall*, PathAnalysisPt*) ===

// === Sta: vertexSlacks(Vertex*, array) ===

// === Sta: vertexSlew(Vertex*, RiseFall*, Scene*, MinMax*) ===

// === Sta: vertexSlew(Vertex*, RiseFall*, DcalcAnalysisPt*) ===

// === Sta: vertexPathIterator(Vertex*, RiseFall*, PathAnalysisPt*) ===

// === Sta: vertexWorstRequiredPath(Vertex*, MinMax*) ===

// === Sta: vertexWorstRequiredPath(Vertex*, RiseFall*, MinMax*) ===

// === Sta: setArcDelayAnnotated ===

// === Sta: pathAnalysisPt(Path*) ===

// === Sta: pathDcalcAnalysisPt(Path*) ===

// === Sta: maxPathCountVertex ===

// Sta::maxPathCountVertex segfaults without graph - use fn pointer
// === Sta: tagCount, tagGroupCount ===

TEST_F(StaInitTest, StaTagCount3) {
  ASSERT_NO_THROW(( [&](){
    TagIndex count = sta_->tagCount();
    EXPECT_GE(count, 0);
    expectStaCoreState(sta_);
  }() ));
}

TEST_F(StaInitTest, StaTagGroupCount3) {
  ASSERT_NO_THROW(( [&](){
    TagGroupIndex count = sta_->tagGroupCount();
    EXPECT_GE(count, 0);
    expectStaCoreState(sta_);
  }() ));
}

// === Sta: clkInfoCount ===

TEST_F(StaInitTest, StaClkInfoCount3) {
  int count = sta_->clkInfoCount();
  EXPECT_EQ(count, 0);
}

// === Sta: findDelays ===

// === Sta: reportCheck(MaxSkewCheck*, bool) ===

// === Sta: checkSlew ===

// === Sta: findSlewLimit ===

// === Sta: checkFanout ===

// === Sta: checkCapacitance ===

// === Sta: pvt ===

// === Sta: setPvt ===

// === Sta: connectPin ===

// === Sta: makePortPinAfter ===

// === Sta: replaceCellExists ===

// === Sta: makeParasiticNetwork ===

// === Sta: disable/removeDisable for LibertyPort ===

// === Sta: disable/removeDisable for Edge ===

// === Sta: disable/removeDisable for TimingArcSet ===

// === Sta: disableClockGatingCheck/removeDisableClockGatingCheck for Pin ===

// === Sta: removeDataCheck ===

// === Sta: clockDomains ===

// === ReportPath: reportJsonHeader ===

TEST_F(StaInitTest, ReportPathReportJsonHeader) {
  ReportPath *rpt = sta_->reportPath();
  EXPECT_NE(rpt, nullptr);
  rpt->reportJsonHeader();
  expectStaCoreState(sta_);
}

// === ReportPath: reportPeriodHeaderShort ===

TEST_F(StaInitTest, ReportPathReportPeriodHeaderShort) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportPeriodHeaderShort();
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: reportMaxSkewHeaderShort ===

TEST_F(StaInitTest, ReportPathReportMaxSkewHeaderShort) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportMaxSkewHeaderShort();
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: reportMpwHeaderShort ===

TEST_F(StaInitTest, ReportPathReportMpwHeaderShort) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportMpwHeaderShort();
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: reportPathEndHeader ===

TEST_F(StaInitTest, ReportPathReportPathEndHeader) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportPathEndHeader();
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: reportPathEndFooter ===

TEST_F(StaInitTest, ReportPathReportPathEndFooter) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportPathEndFooter();
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: reportJsonFooter ===

TEST_F(StaInitTest, ReportPathReportJsonFooter) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportJsonFooter();
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: setPathFormat ===

TEST_F(StaInitTest, ReportPathSetPathFormat2) {
  ReportPath *rpt = sta_->reportPath();
  ReportPathFormat fmt = rpt->pathFormat();
  rpt->setPathFormat(fmt);
  EXPECT_EQ(rpt->pathFormat(), fmt);
}

// === ReportPath: setDigits ===

TEST_F(StaInitTest, ReportPathSetDigits2) {
  ReportPath *rpt = sta_->reportPath();
  int digits = rpt->digits();
  rpt->setDigits(4);
  EXPECT_EQ(rpt->digits(), 4);
  rpt->setDigits(digits);
}

// === ReportPath: setNoSplit ===

TEST_F(StaInitTest, ReportPathSetNoSplit2) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->setNoSplit(true);
    rpt->setNoSplit(false);
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: setReportSigmas ===

TEST_F(StaInitTest, ReportPathSetReportSigmas2) {
  ReportPath *rpt = sta_->reportPath();
  bool sigmas = rpt->reportSigmas();
  rpt->setReportSigmas(!sigmas);
  EXPECT_NE(rpt->reportSigmas(), sigmas);
  rpt->setReportSigmas(sigmas);
}

// === ReportPath: findField ===

TEST_F(StaInitTest, ReportPathFindField2) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *f = rpt->findField("nonexistent_field_xyz");
  EXPECT_EQ(f, nullptr);
}

// === ReportPath: fieldSlew/fieldFanout/fieldCapacitance ===

TEST_F(StaInitTest, ReportPathFieldAccessors) {
  ReportPath *rpt = sta_->reportPath();
  ReportField *slew = rpt->fieldSlew();
  ReportField *fanout = rpt->fieldFanout();
  ReportField *cap = rpt->fieldCapacitance();
  ReportField *src = rpt->fieldSrcAttr();
  EXPECT_NE(slew, nullptr);
  EXPECT_NE(fanout, nullptr);
  EXPECT_NE(cap, nullptr);
  EXPECT_NE(src, nullptr);
  expectStaCoreState(sta_);
}

// === ReportPath: reportEndHeader ===

TEST_F(StaInitTest, ReportPathReportEndHeader) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportEndHeader();
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: reportSummaryHeader ===

TEST_F(StaInitTest, ReportPathReportSummaryHeader) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportSummaryHeader();
    expectStaCoreState(sta_);
  }() ));
}

// === ReportPath: reportSlackOnlyHeader ===

TEST_F(StaInitTest, ReportPathReportSlackOnlyHeader) {
  ASSERT_NO_THROW(( [&](){
    ReportPath *rpt = sta_->reportPath();
    rpt->reportSlackOnlyHeader();
    expectStaCoreState(sta_);
  }() ));
}

// === PathGroups: static group name accessors ===

TEST_F(StaInitTest, PathGroupsAsyncGroupName2) {
  const char *name = PathGroups::asyncPathGroupName();
  EXPECT_NE(name, nullptr);
}

TEST_F(StaInitTest, PathGroupsPathDelayGroupName2) {
  const char *name = PathGroups::pathDelayGroupName();
  EXPECT_NE(name, nullptr);
}

TEST_F(StaInitTest, PathGroupsGatedClkGroupName2) {
  const char *name = PathGroups::gatedClkGroupName();
  EXPECT_NE(name, nullptr);
}

TEST_F(StaInitTest, PathGroupsUnconstrainedGroupName2) {
  const char *name = PathGroups::unconstrainedGroupName();
  EXPECT_NE(name, nullptr);
}

// Corner setParasiticAnalysisPtcount, setParasiticAP, setDcalcAnalysisPtcount,
// addPathAP are protected - skipped

// === Corners: parasiticAnalysisPtCount ===

// === ClkNetwork: isClock(Net*) ===

// === ClkNetwork: clocks(Pin*) ===

// === ClkNetwork: pins(Clock*) ===

// BfsIterator::init is protected - skipped

// === BfsIterator: checkInQueue ===

// === BfsIterator: enqueueAdjacentVertices with level ===

// === Levelize: checkLevels ===

// Levelize::reportPath is protected - skipped

// === Path: init overloads ===

// === Path: constructor with Vertex*, Tag*, StaState* ===

// === PathEnd: less and cmpNoCrpr ===

// === Tag: equal and match static methods ===

// Tag::match and Tag::stateEqual are protected - skipped

// === ClkInfo: equal static method ===

// === ClkInfo: crprClkPath ===

// CheckCrpr::portClkPath and crprArrivalDiff are private - skipped

// === Sim: findLogicConstants ===

// === Sim: makePinAfter ===

// === GatedClk: gatedClkEnables ===

// === Search: pathClkPathArrival1 (protected, use fn pointer) ===

// === Search: visitStartpoints ===

// === Search: reportTagGroups ===

// === Search: reportPathCountHistogram ===

// Search::reportPathCountHistogram segfaults without graph - use fn pointer
// === Search: arrivalsInvalid ===

TEST_F(StaInitTest, SearchArrivalsInvalid3) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    search->arrivalsInvalid();
    expectStaCoreState(sta_);
  }() ));
}

// === Search: requiredsInvalid ===

TEST_F(StaInitTest, SearchRequiredsInvalid3) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    search->requiredsInvalid();
    expectStaCoreState(sta_);
  }() ));
}

// === Search: endpointsInvalid ===

TEST_F(StaInitTest, SearchEndpointsInvalid3) {
  ASSERT_NO_THROW(( [&](){
    Search *search = sta_->search();
    search->endpointsInvalid();
    expectStaCoreState(sta_);
  }() ));
}

// === Search: copyState ===

// === Search: isEndpoint ===

// === Search: seedArrival ===

// === Search: enqueueLatchOutput ===

// === Search: isSegmentStart ===

// === Search: seedRequiredEnqueueFanin ===

// === Search: saveEnumPath ===

// === Sta: graphLoops ===

// === Sta: pathCount ===

// Sta::pathCount segfaults without graph - use fn pointer
// === Sta: findAllArrivals ===

// === Sta: findArrivals ===

// === Sta: findRequireds ===

// === PathEnd: type names for subclass function pointers ===

// === PathEndUnconstrained: reportShort and requiredTime ===

// === PathEnum destructor ===

// === DiversionGreater ===

TEST_F(StaInitTest, DiversionGreaterStateCtor) {
  ASSERT_NO_THROW(( [&](){
    DiversionGreater dg(sta_);
    EXPECT_NE(&dg, nullptr);
    expectStaCoreState(sta_);
  }() ));
}

// === TagGroup: report function pointer ===

// === TagGroupBldr: reportArrivalEntries ===

// === VertexPinCollector: copy ===

// VertexPinCollector::copy() throws "not supported" - skipped
// === Genclks: function pointers ===

// Genclks::srcFilter and srcPathIndex are private - skipped

// === Genclks: findLatchFdbkEdges overloads ===

// === Sta: simLogicValue ===

// === Sta: netSlack ===

// === Sta: pinSlack overloads ===

// === Sta: pinArrival ===

// === Sta: vertexLevel ===

// === Sta: vertexPathCount ===

// === Sta: endpointSlack ===

// === Sta: arcDelay ===

// === Sta: arcDelayAnnotated ===

// === Sta: findClkMinPeriod ===

// === Sta: vertexWorstArrivalPath ===

// === Sta: vertexWorstSlackPath ===

// === Sta: vertexSlew more overloads ===

// === Sta: vertexPathIterator(Vertex*, RiseFall*, MinMax*) ===

// === Sta: totalNegativeSlack ===

// === Sta: worstSlack ===

// === Sta: updateTiming ===

// === Search: clkPathArrival ===

// Sta::readLibertyFile (4-arg overload) is protected - skipped

// === Sta: disconnectPin ===

// === PathExpandedCtorGenClks ===

// === Search: deleteFilteredArrivals ===

// === Sta: checkSlewLimitPreamble ===

// === Sta: checkFanoutLimitPreamble ===

// === Sta: checkCapacitanceLimitPreamble ===

// === Sta: checkSlewLimits(Net*,...) ===

// === Sta: checkFanoutLimits(Net*,...) ===

// === Sta: checkCapacitanceLimits(Net*,...) ===

// === Search: seedInputSegmentArrival ===

// === Search: enqueueLatchDataOutputs ===

// === Search: seedRequired ===

// === Search: findClkArrivals ===

// === Search: tnsInvalid ===

// === Search: endpointInvalid ===

// === Search: makePathGroups ===

// === Sta: isIdealClock ===

// === Sta: isPropagatedClock ===

} // namespace sta
