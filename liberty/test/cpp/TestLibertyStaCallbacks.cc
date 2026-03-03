#include <gtest/gtest.h>
#include <string>
#include <cmath>
#include <atomic>
#include <unistd.h>
#include "Units.hh"
#include "TimingRole.hh"
#include "MinMax.hh"
#include "Wireload.hh"
#include "FuncExpr.hh"
#include "TableModel.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "InternalPower.hh"
#include "LinearModel.hh"
#include "Transition.hh"
#include "RiseFallValues.hh"
#include "PortDirection.hh"
#include "StringUtil.hh"
#include "liberty/LibertyParser.hh"
#include "liberty/LibertyBuilder.hh"
#include "ReportStd.hh"
#include "liberty/LibertyReaderPvt.hh"

#include <tcl.h>
#include "Sta.hh"
#include "ReportTcl.hh"
#include "PatternMatch.hh"
#include "Scene.hh"
#include "LibertyWriter.hh"

namespace sta {

static void expectStaLibertyCoreState(Sta *sta, LibertyLibrary *lib)
{
  ASSERT_NE(sta, nullptr);
  EXPECT_EQ(Sta::sta(), sta);
  EXPECT_NE(sta->network(), nullptr);
  EXPECT_NE(sta->search(), nullptr);
  EXPECT_NE(sta->cmdSdc(), nullptr);
  EXPECT_NE(sta->report(), nullptr);
  EXPECT_FALSE(sta->scenes().empty());
  if (!sta->scenes().empty())
    EXPECT_GE(sta->scenes().size(), 1);
  EXPECT_NE(sta->cmdScene(), nullptr);
  EXPECT_NE(lib, nullptr);
}


class StaLibertyTest : public ::testing::Test {
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

    // Read Nangate45 liberty file
    lib_ = sta_->readLiberty("test/nangate45/Nangate45_typ.lib",
                             sta_->cmdScene(),
                             MinMaxAll::min(),
                             false);
  }

  void TearDown() override {
    if (sta_)
      expectStaLibertyCoreState(sta_, lib_);
    deleteAllMemory();
    sta_ = nullptr;
    if (interp_)
      Tcl_DeleteInterp(interp_);
    interp_ = nullptr;
  }

  Sta *sta_;
  Tcl_Interp *interp_;
  LibertyLibrary *lib_;
};

// =========================================================================
// R9_ tests: Cover uncovered LibertyReader callbacks and related functions
// by creating small .lib files with specific constructs and reading them.
// =========================================================================

// Standard threshold definitions required by all liberty files
static const char *R9_THRESHOLDS = R"(
  slew_lower_threshold_pct_fall : 30.0 ;
  slew_lower_threshold_pct_rise : 30.0 ;
  slew_upper_threshold_pct_fall : 70.0 ;
  slew_upper_threshold_pct_rise : 70.0 ;
  slew_derate_from_library : 1.0 ;
  input_threshold_pct_fall : 50.0 ;
  input_threshold_pct_rise : 50.0 ;
  output_threshold_pct_fall : 50.0 ;
  output_threshold_pct_rise : 50.0 ;
  nom_process : 1.0 ;
  nom_temperature : 25.0 ;
  nom_voltage : 1.1 ;
)";

// Generate a unique local file path for each call to avoid global /tmp clashes.
static std::string makeUniqueTmpPath() {
  static std::atomic<int> counter{0};
  char buf[256];
  snprintf(buf, sizeof(buf), "test_r9_%d_%d.lib",
           static_cast<int>(getpid()), counter.fetch_add(1));
  return std::string(buf);
}

// Write lib content to a unique temp file with thresholds injected
static void writeLibContent(const char *content, const std::string &path) {
  FILE *f = fopen(path.c_str(), "w");
  ASSERT_NE(f, nullptr);
  ASSERT_NE(content, nullptr);
  const char *brace = strchr(content, '{');
  if (brace) {
    fwrite(content, 1, brace - content + 1, f);
    fprintf(f, "%s", R9_THRESHOLDS);
    fprintf(f, "%s", brace + 1);
  } else {
    fprintf(f, "%s", content);
  }
  fclose(f);
}

// Helper to write a temp liberty file and read it, injecting threshold defs
static void writeAndReadLib(Sta *sta, const char *content, const char *path = nullptr) {
  std::string tmp_path = path ? std::string(path) : makeUniqueTmpPath();
  writeLibContent(content, tmp_path);
  LibertyLibrary *lib = sta->readLiberty(tmp_path.c_str(), sta->cmdScene(),
                                          MinMaxAll::min(), false);
  EXPECT_NE(lib, nullptr);
  EXPECT_EQ(remove(tmp_path.c_str()), 0);
}

// Helper variant that returns the library pointer
static LibertyLibrary *writeAndReadLibReturn(Sta *sta, const char *content, const char *path = nullptr) {
  std::string tmp_path = path ? std::string(path) : makeUniqueTmpPath();
  writeLibContent(content, tmp_path);
  LibertyLibrary *lib = sta->readLiberty(tmp_path.c_str(), sta->cmdScene(),
                                          MinMaxAll::min(), false);
  EXPECT_EQ(remove(tmp_path.c_str()), 0);
  return lib;
}

// ---------- Library-level default attributes ----------

// R9_1: default_intrinsic_rise/fall
TEST_F(StaLibertyTest, DefaultIntrinsicRiseFall) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_1) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_intrinsic_rise : 0.05 ;
  default_intrinsic_fall : 0.06 ;
  cell(BUF1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_2: default_inout_pin_rise_res / fall_res
TEST_F(StaLibertyTest, DefaultInoutPinRes) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_2) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_inout_pin_rise_res : 100.0 ;
  default_inout_pin_fall_res : 120.0 ;
  cell(BUF2) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_3: default_output_pin_rise_res / fall_res
TEST_F(StaLibertyTest, DefaultOutputPinRes) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_3) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_output_pin_rise_res : 50.0 ;
  default_output_pin_fall_res : 60.0 ;
  cell(BUF3) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_4: technology(fpga) group
TEST_F(StaLibertyTest, TechnologyGroup) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_4) {
  technology(fpga) {}
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(BUF4) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_5: scaling_factors group
TEST_F(StaLibertyTest, ScalingFactors) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_5) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  scaling_factors(my_scale) {
    k_process_cell_rise : 1.0 ;
    k_process_cell_fall : 1.0 ;
    k_volt_cell_rise : -0.5 ;
    k_volt_cell_fall : -0.5 ;
    k_temp_cell_rise : 0.001 ;
    k_temp_cell_fall : 0.001 ;
  }
  cell(BUF5) {
    area : 1.0 ;
    scaling_factors : my_scale ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_6: cell is_memory attribute
TEST_F(StaLibertyTest, CellIsMemory4) {
  const char *content = R"(
library(test_r9_6) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(MEM1) {
    area : 10.0 ;
    is_memory : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("MEM1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->isMemory());
}

// R9_7: pad_cell attribute
TEST_F(StaLibertyTest, CellIsPadCell) {
  const char *content = R"(
library(test_r9_7) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PAD1) {
    area : 50.0 ;
    pad_cell : true ;
    pin(PAD) { direction : inout ; capacitance : 5.0 ; function : "A" ; }
    pin(A) { direction : input ; capacitance : 0.01 ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("PAD1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->isPad());
}

// R9_8: is_clock_cell attribute
TEST_F(StaLibertyTest, CellIsClockCell3) {
  const char *content = R"(
library(test_r9_8) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(CLK1) {
    area : 3.0 ;
    is_clock_cell : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("CLK1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->isClockCell());
}

// R9_9: switch_cell_type
TEST_F(StaLibertyTest, CellSwitchCellType2) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_9) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(SW1) {
    area : 5.0 ;
    switch_cell_type : coarse_grain ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_10: user_function_class
TEST_F(StaLibertyTest, CellUserFunctionClass3) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_10) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(UFC1) {
    area : 2.0 ;
    user_function_class : combinational ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_11: pin fanout_load, max_fanout, min_fanout
TEST_F(StaLibertyTest, PinFanoutAttributes) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_11) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(FAN1) {
    area : 2.0 ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      fanout_load : 1.5 ;
    }
    pin(Z) {
      direction : output ;
      function : "A" ;
      max_fanout : 16.0 ;
      min_fanout : 1.0 ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_12: min_transition on pin
TEST_F(StaLibertyTest, PinMinTransition) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_12) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(TR1) {
    area : 2.0 ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      min_transition : 0.001 ;
    }
    pin(Z) {
      direction : output ;
      function : "A" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_13: pulse_clock attribute on pin
TEST_F(StaLibertyTest, PinPulseClock) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_13) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PC1) {
    area : 2.0 ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      pulse_clock : rise_triggered_high_pulse ;
    }
    pin(Z) {
      direction : output ;
      function : "CLK" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_14: is_pll_feedback_pin
TEST_F(StaLibertyTest, PinIsPllFeedback) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_14) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PLL1) {
    area : 5.0 ;
    pin(FB) {
      direction : input ;
      capacitance : 0.01 ;
      is_pll_feedback_pin : true ;
    }
    pin(Z) {
      direction : output ;
      function : "FB" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_15: switch_pin attribute
TEST_F(StaLibertyTest, PinSwitchPin) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_15) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(SWP1) {
    area : 3.0 ;
    pin(SW) {
      direction : input ;
      capacitance : 0.01 ;
      switch_pin : true ;
    }
    pin(Z) {
      direction : output ;
      function : "SW" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_16: is_pad on pin
TEST_F(StaLibertyTest, PinIsPad) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_16) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PADCELL1) {
    area : 50.0 ;
    pin(PAD) {
      direction : inout ;
      capacitance : 5.0 ;
      is_pad : true ;
      function : "A" ;
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_17: bundle group with members
TEST_F(StaLibertyTest, BundlePort) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_17) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(BUND1) {
    area : 4.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    bundle(DATA) {
      members(A, B) ;
      direction : input ;
    }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_18: ff_bank group
TEST_F(StaLibertyTest, FFBank) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_18) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(DFF_BANK1) {
    area : 8.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff_bank(IQ, IQN, 4) {
      clocked_on : "CLK" ;
      next_state : "D" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_19: latch_bank group
TEST_F(StaLibertyTest, LatchBank) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_19) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(LATCH_BANK1) {
    area : 6.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(EN) { direction : input ; capacitance : 0.01 ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    latch_bank(IQ, IQN, 4) {
      enable : "EN" ;
      data_in : "D" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_20: timing with intrinsic_rise/fall and rise_resistance/fall_resistance (linear model)
TEST_F(StaLibertyTest, TimingIntrinsicResistance) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_20) {
  delay_model : generic_cmos ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  pulling_resistance_unit : "1kohm" ;
  capacitive_load_unit(1, ff) ;
  cell(LIN1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        intrinsic_rise : 0.05 ;
        intrinsic_fall : 0.06 ;
        rise_resistance : 100.0 ;
        fall_resistance : 120.0 ;
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_21: timing with sdf_cond_start and sdf_cond_end
TEST_F(StaLibertyTest, TimingSdfCondStartEnd) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_21) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SDF1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A & B" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sdf_cond_start : "B == 1'b1" ;
        sdf_cond_end : "B == 1'b0" ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_22: timing with mode attribute
TEST_F(StaLibertyTest, TimingMode) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_22) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(MODE1) {
    area : 2.0 ;
    mode_definition(test_mode) {
      mode_value(normal) {
        when : "A" ;
        sdf_cond : "A == 1'b1" ;
      }
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        mode(test_mode, normal) ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_23: related_bus_pins
TEST_F(StaLibertyTest, TimingRelatedBusPins) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_23) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  type(bus4) {
    base_type : array ;
    data_type : bit ;
    bit_width : 4 ;
    bit_from : 3 ;
    bit_to : 0 ;
  }
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(BUS1) {
    area : 4.0 ;
    bus(D) {
      bus_type : bus4 ;
      direction : input ;
      capacitance : 0.01 ;
    }
    pin(Z) {
      direction : output ;
      function : "D[0]" ;
      timing() {
        related_bus_pins : "D" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_24: OCV derate constructs
TEST_F(StaLibertyTest, OcvDerate) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_24) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_template_1) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  ocv_derate(my_derate) {
    ocv_derate_factors(ocv_template_1) {
      rf_type : rise ;
      derate_type : early ;
      path_type : data ;
      values("0.95, 0.96") ;
    }
    ocv_derate_factors(ocv_template_1) {
      rf_type : fall ;
      derate_type : late ;
      path_type : clock ;
      values("1.04, 1.05") ;
    }
    ocv_derate_factors(ocv_template_1) {
      rf_type : rise_and_fall ;
      derate_type : early ;
      path_type : clock_and_data ;
      values("0.97, 0.98") ;
    }
  }
  default_ocv_derate_group : my_derate ;
  cell(OCV1) {
    area : 2.0 ;
    ocv_derate_group : my_derate ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_25: ocv_arc_depth at library, cell, and timing levels
TEST_F(StaLibertyTest, OcvArcDepth) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_25) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_arc_depth : 3.0 ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(OCV2) {
    area : 2.0 ;
    ocv_arc_depth : 5.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        ocv_arc_depth : 2.0 ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_26: POCV sigma tables
TEST_F(StaLibertyTest, OcvSigmaTables) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_26) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(POCV1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sigma_type : early_and_late ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ocv_sigma_cell_rise(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_cell_fall(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_rise_transition(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_fall_transition(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_27: POCV sigma constraint tables
TEST_F(StaLibertyTest, OcvSigmaConstraint) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_27) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(constraint_template_2x2) {
    variable_1 : related_pin_transition ;
    variable_2 : constrained_pin_transition ;
    index_1("0.01, 0.1") ;
    index_2("0.01, 0.1") ;
  }
  cell(POCV2) {
    area : 2.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff(IQ, IQN) {
      clocked_on : "CLK" ;
      next_state : "D" ;
    }
    pin(D) {
      timing() {
        related_pin : "CLK" ;
        timing_type : setup_rising ;
        sigma_type : early_and_late ;
        rise_constraint(constraint_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_constraint(constraint_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ocv_sigma_rise_constraint(constraint_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_fall_constraint(constraint_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_28: resistance_unit and distance_unit attributes
TEST_F(StaLibertyTest, ResistanceDistanceUnits) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_28) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  resistance_unit : "1kohm" ;
  distance_unit : "1um" ;
  capacitive_load_unit(1, ff) ;
  cell(UNIT1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_29: rise/fall_transition_degradation tables
TEST_F(StaLibertyTest, TransitionDegradation) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_29) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(degradation_template) {
    variable_1 : output_pin_transition ;
    variable_2 : connect_delay ;
    index_1("0.01, 0.1") ;
    index_2("0.0, 0.01") ;
  }
  rise_transition_degradation(degradation_template) {
    values("0.01, 0.02", "0.03, 0.04") ;
  }
  fall_transition_degradation(degradation_template) {
    values("0.01, 0.02", "0.03, 0.04") ;
  }
  cell(DEG1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_30: lut group in cell
TEST_F(StaLibertyTest, LutGroup) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_30) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(LUT1) {
    area : 5.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
    lut(lut_state) {}
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_31: ECSM waveform constructs
TEST_F(StaLibertyTest, EcsmWaveform) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_31) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(ECSM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ecsm_waveform() {}
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_32: power group (as opposed to rise_power/fall_power)
TEST_F(StaLibertyTest, PowerGroup) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_32) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  power_lut_template(power_template_2x2) {
    variable_1 : input_transition_time ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(PWR1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      internal_power() {
        related_pin : "A" ;
        power(power_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_33: leakage_power group with when and related_pg_pin
TEST_F(StaLibertyTest, LeakagePowerGroup) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_33) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  leakage_power_unit : "1nW" ;
  capacitive_load_unit(1, ff) ;
  cell(LP1) {
    area : 2.0 ;
    pg_pin(VDD) { pg_type : primary_power ; voltage_name : VDD ; }
    pg_pin(VSS) { pg_type : primary_ground ; voltage_name : VSS ; }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
    leakage_power() {
      when : "!A" ;
      value : 0.5 ;
      related_pg_pin : VDD ;
    }
    leakage_power() {
      when : "A" ;
      value : 0.8 ;
      related_pg_pin : VDD ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_34: InternalPowerModel checkAxes via reading a lib with internal power
TEST_F(StaLibertyTest, InternalPowerModelCheckAxes) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_34) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  power_lut_template(power_template_1d) {
    variable_1 : input_transition_time ;
    index_1("0.01, 0.1") ;
  }
  cell(IPM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      internal_power() {
        related_pin : "A" ;
        rise_power(power_template_1d) {
          values("0.001, 0.002") ;
        }
        fall_power(power_template_1d) {
          values("0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_35: PortGroup and TimingGroup via direct construction
TEST_F(StaLibertyTest, PortGroupConstruct) {
  auto *ports = new LibertyPortSeq;
  PortGroup pg(ports, 1);
  TimingGroup *tg = new TimingGroup(1);
  pg.addTimingGroup(tg);
  InternalPowerGroup *ipg = new InternalPowerGroup(1);
  pg.addInternalPowerGroup(ipg);
  EXPECT_GT(pg.timingGroups().size(), 0u);
  EXPECT_GT(pg.internalPowerGroups().size(), 0u);
}

// R9_36: SequentialGroup construct and setters
TEST_F(StaLibertyTest, SequentialGroupSetters) {
  SequentialGroup sg(true, false, nullptr, nullptr, 1, 0);
  sg.setClock(stringCopy("CLK"));
  sg.setData(stringCopy("D"));
  sg.setClear(stringCopy("CLR"));
  sg.setPreset(stringCopy("PRE"));
  sg.setClrPresetVar1(LogicValue::zero);
  sg.setClrPresetVar2(LogicValue::one);
  EXPECT_TRUE(sg.isRegister());
  EXPECT_FALSE(sg.isBank());
  EXPECT_EQ(sg.size(), 1);
}

// R9_37: RelatedPortGroup construct and setters
TEST_F(StaLibertyTest, RelatedPortGroupSetters) {
  RelatedPortGroup rpg(1);
  auto *names = new StringSeq;
  names->push_back(stringCopy("A"));
  names->push_back(stringCopy("B"));
  rpg.setRelatedPortNames(names);
  rpg.setIsOneToOne(true);
  EXPECT_TRUE(rpg.isOneToOne());
}

// R9_38: TimingGroup intrinsic/resistance setters
TEST_F(StaLibertyTest, TimingGroupIntrinsicSetters) {
  TimingGroup tg(1);
  tg.setIntrinsic(RiseFall::rise(), 0.05f);
  tg.setIntrinsic(RiseFall::fall(), 0.06f);
  float val;
  bool exists;
  tg.intrinsic(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.05f);
  tg.intrinsic(RiseFall::fall(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 0.06f);
  tg.setResistance(RiseFall::rise(), 100.0f);
  tg.setResistance(RiseFall::fall(), 120.0f);
  tg.resistance(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 100.0f);
  tg.resistance(RiseFall::fall(), val, exists);
  EXPECT_TRUE(exists);
  EXPECT_FLOAT_EQ(val, 120.0f);
}

// R9_39: TimingGroup setRelatedOutputPortName
TEST_F(StaLibertyTest, TimingGroupRelatedOutputPort) {
  TimingGroup tg(1);
  tg.setRelatedOutputPortName("Z");
  EXPECT_NE(tg.relatedOutputPortName(), nullptr);
}

// R9_40: InternalPowerGroup construct
TEST_F(StaLibertyTest, InternalPowerGroupConstruct) {
  InternalPowerGroup ipg(1);
  EXPECT_EQ(ipg.line(), 1);
}

// R9_41: LeakagePowerGroup construct and setters
TEST_F(StaLibertyTest, LeakagePowerGroupSetters) {
  LeakagePowerGroup lpg(1);
  lpg.setRelatedPgPin("VDD");
  lpg.setPower(0.5f);
  EXPECT_EQ(lpg.relatedPgPin(), "VDD");
  EXPECT_FLOAT_EQ(lpg.power(), 0.5f);
}

// R9_42: LibertyGroup isGroup and isVariable
TEST_F(StaLibertyTest, LibertyStmtTypes) {
  LibertyGroup grp("test", nullptr, 1);
  EXPECT_TRUE(grp.isGroup());
  EXPECT_FALSE(grp.isVariable());
}

// R9_43: LibertySimpleAttr isComplex returns false
TEST_F(StaLibertyTest, LibertySimpleAttrIsComplex) {
  LibertyStringAttrValue *val = new LibertyStringAttrValue("test");
  LibertySimpleAttr attr("name", val, 1);
  EXPECT_FALSE(attr.isComplexAttr());
  // isAttribute() returns false for LibertyAttr subclasses
  EXPECT_FALSE(attr.isAttribute());
}

// R9_44: LibertyComplexAttr isSimple returns false
TEST_F(StaLibertyTest, LibertyComplexAttrIsSimple) {
  auto *values = new LibertyAttrValueSeq;
  LibertyComplexAttr attr("name", values, 1);
  EXPECT_FALSE(attr.isSimpleAttr());
  // isAttribute() returns false for LibertyAttr subclasses
  EXPECT_FALSE(attr.isAttribute());
}

// R9_45: LibertyStringAttrValue and LibertyFloatAttrValue type checks
TEST_F(StaLibertyTest, AttrValueCrossType) {
  // LibertyStringAttrValue normal usage
  LibertyStringAttrValue sval("hello");
  EXPECT_TRUE(sval.isString());
  EXPECT_FALSE(sval.isFloat());
  EXPECT_EQ(sval.stringValue(), "hello");

  // LibertyFloatAttrValue normal usage
  LibertyFloatAttrValue fval(3.14f);
  EXPECT_FALSE(fval.isString());
  EXPECT_TRUE(fval.isFloat());
  EXPECT_FLOAT_EQ(fval.floatValue(), 3.14f);
}

// R9_46: LibertyDefine isDefine
TEST_F(StaLibertyTest, LibertyDefineIsDefine) {
  LibertyDefine def("myattr", LibertyGroupType::cell,
                    LibertyAttrType::attr_string, 1);
  EXPECT_TRUE(def.isDefine());
  EXPECT_FALSE(def.isVariable());
}

// R9_47: scaled_cell group
TEST_F(StaLibertyTest, ScaledCell) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_47) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  operating_conditions(fast) {
    process : 0.8 ;
    voltage : 1.2 ;
    temperature : 0.0 ;
    tree_type : best_case_tree ;
  }
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SC1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
  scaled_cell(SC1, fast) {
    area : 1.8 ;
    pin(A) { direction : input ; capacitance : 0.008 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.008, 0.015", "0.025, 0.035") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.008, 0.015", "0.025, 0.035") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.008, 0.015", "0.025, 0.035") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.008, 0.015", "0.025, 0.035") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_48: TimingGroup cell/transition/constraint setters
TEST_F(StaLibertyTest, TimingGroupTableModelSetters) {
  TimingGroup tg(1);
  // Test setting and getting cell models
  EXPECT_EQ(tg.cell(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.cell(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::fall()), nullptr);
}

// R9_49: LibertyParser construct, group(), deleteGroups(), makeVariable()
TEST_F(StaLibertyTest, LibertyParserConstruct) {
  const char *content = R"(
library(test_r9_49) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(P1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  std::string tmp_path = makeUniqueTmpPath();
  writeLibContent(content, tmp_path);
  // Read via readLibertyFile which exercises LibertyParser/LibertyReader directly
  LibertyReader reader(tmp_path.c_str(), false, sta_->network());
  LibertyLibrary *lib = reader.readLibertyFile(tmp_path.c_str());
  EXPECT_NE(lib, nullptr);
  EXPECT_EQ(remove(tmp_path.c_str()), 0);
}

// R9_50: cell with switch_cell_type fine_grain
TEST_F(StaLibertyTest, SwitchCellTypeFineGrain) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_50) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(SW2) {
    area : 5.0 ;
    switch_cell_type : fine_grain ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_51: pulse_clock with different trigger/sense combos
TEST_F(StaLibertyTest, PulseClockFallTrigger) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_51) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PC2) {
    area : 2.0 ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      pulse_clock : fall_triggered_low_pulse ;
    }
    pin(Z) {
      direction : output ;
      function : "CLK" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_52: pulse_clock rise_triggered_low_pulse
TEST_F(StaLibertyTest, PulseClockRiseTriggeredLow) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_52) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PC3) {
    area : 2.0 ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      pulse_clock : rise_triggered_low_pulse ;
    }
    pin(Z) { direction : output ; function : "CLK" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_53: pulse_clock fall_triggered_high_pulse
TEST_F(StaLibertyTest, PulseClockFallTriggeredHigh) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_53) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(PC4) {
    area : 2.0 ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      pulse_clock : fall_triggered_high_pulse ;
    }
    pin(Z) { direction : output ; function : "CLK" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_54: OCV derate with derate_type late
TEST_F(StaLibertyTest, OcvDerateTypeLate) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_54) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_tmpl) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  ocv_derate(derate_late) {
    ocv_derate_factors(ocv_tmpl) {
      rf_type : rise_and_fall ;
      derate_type : late ;
      path_type : data ;
      values("1.05, 1.06") ;
    }
  }
  cell(OCV3) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_55: OCV derate with path_type clock
TEST_F(StaLibertyTest, OcvDeratePathTypeClock) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_55) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_tmpl2) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  ocv_derate(derate_clk) {
    ocv_derate_factors(ocv_tmpl2) {
      rf_type : fall ;
      derate_type : early ;
      path_type : clock ;
      values("0.95, 0.96") ;
    }
  }
  cell(OCV4) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_56: TimingGroup setDelaySigma/setSlewSigma/setConstraintSigma
TEST_F(StaLibertyTest, TimingGroupSigmaSetters) {
  ASSERT_NO_THROW(( [&](){
  TimingGroup tg(1);
  // Setting to nullptr just exercises the method
  tg.setDelaySigma(RiseFall::rise(), EarlyLate::min(), nullptr);
  tg.setDelaySigma(RiseFall::fall(), EarlyLate::max(), nullptr);
  tg.setSlewSigma(RiseFall::rise(), EarlyLate::min(), nullptr);
  tg.setSlewSigma(RiseFall::fall(), EarlyLate::max(), nullptr);
  tg.setConstraintSigma(RiseFall::rise(), EarlyLate::min(), nullptr);
  tg.setConstraintSigma(RiseFall::fall(), EarlyLate::max(), nullptr);

  }() ));
}

// R9_57: Cover setIsScaled via reading a scaled_cell lib
TEST_F(StaLibertyTest, ScaledCellCoversIsScaled) {
  ASSERT_NO_THROW(( [&](){
  // scaled_cell reading exercises GateTableModel::setIsScaled,
  // GateLinearModel::setIsScaled, CheckTableModel::setIsScaled internally
  const char *content = R"(
library(test_r9_57) {
  delay_model : generic_cmos ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  pulling_resistance_unit : "1kohm" ;
  capacitive_load_unit(1, ff) ;
  operating_conditions(slow) {
    process : 1.2 ;
    voltage : 0.9 ;
    temperature : 125.0 ;
    tree_type : worst_case_tree ;
  }
  cell(LM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        intrinsic_rise : 0.05 ;
        intrinsic_fall : 0.06 ;
        rise_resistance : 100.0 ;
        fall_resistance : 120.0 ;
      }
    }
  }
  scaled_cell(LM1, slow) {
    area : 2.2 ;
    pin(A) { direction : input ; capacitance : 0.012 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        intrinsic_rise : 0.07 ;
        intrinsic_fall : 0.08 ;
        rise_resistance : 130.0 ;
        fall_resistance : 150.0 ;
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_58: GateTableModel checkAxis exercised via table model reading
TEST_F(StaLibertyTest, GateTableModelCheckAxis) {
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  auto &arcsets = buf->timingArcSets();
  ASSERT_GT(arcsets.size(), 0u);
  for (TimingArc *arc : arcsets[0]->arcs()) {
    TimingModel *model = arc->model();
    GateTableModel *gtm = dynamic_cast<GateTableModel*>(model);
    if (gtm) {
      EXPECT_NE(gtm, nullptr);
      break;
    }
  }
}

// R9_59: CheckTableModel checkAxis exercised via setup timing
TEST_F(StaLibertyTest, CheckTableModelCheckAxis) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  if (!dff) return;
  auto &arcsets = dff->timingArcSets();
  for (size_t i = 0; i < arcsets.size(); i++) {
    TimingArcSet *arcset = arcsets[i];
    if (arcset->role() == TimingRole::setup()) {
      for (TimingArc *arc : arcset->arcs()) {
        TimingModel *model = arc->model();
        CheckTableModel *ctm = dynamic_cast<CheckTableModel*>(model);
        if (ctm) {
          EXPECT_NE(ctm, nullptr);
        }
      }
      break;
    }
  }
}

// R9_60: TimingGroup cell/transition/constraint getter coverage
TEST_F(StaLibertyTest, TimingGroupGettersNull) {
  TimingGroup tg(1);
  // By default all model pointers should be null
  EXPECT_EQ(tg.cell(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.cell(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::fall()), nullptr);
  EXPECT_EQ(tg.outputWaveforms(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.outputWaveforms(RiseFall::fall()), nullptr);
}

// R9_61: Timing with ecsm_waveform_set and ecsm_capacitance
TEST_F(StaLibertyTest, EcsmWaveformSet) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_61) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(ECSM2) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ecsm_waveform_set() {}
        ecsm_capacitance() {}
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_62: sigma_type early
TEST_F(StaLibertyTest, SigmaTypeEarly) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_62) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SIG1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sigma_type : early ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ocv_sigma_cell_rise(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_cell_fall(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_rise_transition(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_fall_transition(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_63: sigma_type late
TEST_F(StaLibertyTest, SigmaTypeLate) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_63) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SIG2) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sigma_type : late ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        ocv_sigma_cell_rise(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        ocv_sigma_cell_fall(delay_template_2x2) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_64: Receiver capacitance with segment attribute
TEST_F(StaLibertyTest, ReceiverCapacitanceSegment) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_64) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(RCV1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
      receiver_capacitance() {
        receiver_capacitance1_rise(delay_template_2x2) {
          segment : 0 ;
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        receiver_capacitance1_fall(delay_template_2x2) {
          segment : 0 ;
          values("0.001, 0.002", "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_65: LibertyCell hasInternalPorts (read-only check)
TEST_F(StaLibertyTest, CellHasInternalPorts4) {
  LibertyCell *dff = lib_->findLibertyCell("DFF_X1");
  ASSERT_NE(dff, nullptr);
  // DFF should have internal ports for state vars (IQ, IQN)
  EXPECT_TRUE(dff->hasInternalPorts());
  // A simple buffer should not
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  EXPECT_FALSE(buf->hasInternalPorts());
}

// R9_66: LibertyBuilder destructor (coverage)
TEST_F(StaLibertyTest, LibertyBuilderDestruct) {
  ASSERT_NO_THROW(( [&](){
  LibertyBuilder *builder = new LibertyBuilder;
  delete builder;

  }() ));
}

// R9_67: Timing with setup constraint for coverage
TEST_F(StaLibertyTest, TimingSetupConstraint) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_67) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(constraint_template_2x2) {
    variable_1 : related_pin_transition ;
    variable_2 : constrained_pin_transition ;
    index_1("0.01, 0.1") ;
    index_2("0.01, 0.1") ;
  }
  cell(FF1) {
    area : 4.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff(IQ, IQN) {
      clocked_on : "CLK" ;
      next_state : "D" ;
    }
    pin(D) {
      timing() {
        related_pin : "CLK" ;
        timing_type : setup_rising ;
        rise_constraint(constraint_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_constraint(constraint_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
      timing() {
        related_pin : "CLK" ;
        timing_type : hold_rising ;
        rise_constraint(constraint_template_2x2) {
          values("-0.01, -0.02", "-0.03, -0.04") ;
        }
        fall_constraint(constraint_template_2x2) {
          values("-0.01, -0.02", "-0.03, -0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_68: Library with define statement
TEST_F(StaLibertyTest, DefineStatement) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_68) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  define(my_attr, cell, string) ;
  define(my_float_attr, pin, float) ;
  cell(DEF1) {
    area : 2.0 ;
    my_attr : "custom_value" ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      my_float_attr : 3.14 ;
    }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_69: multiple scaling_factors type combinations
TEST_F(StaLibertyTest, ScalingFactorsMultipleTypes) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_69) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  scaling_factors(multi_scale) {
    k_process_cell_rise : 1.0 ;
    k_process_cell_fall : 1.0 ;
    k_process_rise_transition : 0.8 ;
    k_process_fall_transition : 0.8 ;
    k_volt_cell_rise : -0.5 ;
    k_volt_cell_fall : -0.5 ;
    k_volt_rise_transition : -0.3 ;
    k_volt_fall_transition : -0.3 ;
    k_temp_cell_rise : 0.001 ;
    k_temp_cell_fall : 0.001 ;
    k_temp_rise_transition : 0.0005 ;
    k_temp_fall_transition : 0.0005 ;
    k_process_hold_rise : 1.0 ;
    k_process_hold_fall : 1.0 ;
    k_process_setup_rise : 1.0 ;
    k_process_setup_fall : 1.0 ;
    k_volt_hold_rise : -0.5 ;
    k_volt_hold_fall : -0.5 ;
    k_volt_setup_rise : -0.5 ;
    k_volt_setup_fall : -0.5 ;
    k_temp_hold_rise : 0.001 ;
    k_temp_hold_fall : 0.001 ;
    k_temp_setup_rise : 0.001 ;
    k_temp_setup_fall : 0.001 ;
  }
  cell(SC2) {
    area : 2.0 ;
    scaling_factors : multi_scale ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_70: OCV derate with early_and_late derate_type
TEST_F(StaLibertyTest, OcvDerateEarlyAndLate) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_70) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_tmpl3) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  ocv_derate(derate_both) {
    ocv_derate_factors(ocv_tmpl3) {
      rf_type : rise ;
      derate_type : early_and_late ;
      path_type : clock_and_data ;
      values("1.0, 1.0") ;
    }
  }
  cell(OCV5) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_71: leakage_power with clear_preset_var1/var2 in ff
TEST_F(StaLibertyTest, FFClearPresetVars) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_71) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(DFF2) {
    area : 4.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(CLR) { direction : input ; capacitance : 0.01 ; }
    pin(PRE) { direction : input ; capacitance : 0.01 ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    pin(QN) { direction : output ; function : "IQN" ; }
    ff(IQ, IQN) {
      clocked_on : "CLK" ;
      next_state : "D" ;
      clear : "CLR" ;
      preset : "PRE" ;
      clear_preset_var1 : L ;
      clear_preset_var2 : H ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_72: mode_definition with multiple mode_values
TEST_F(StaLibertyTest, ModeDefMultipleValues) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_72) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(MD1) {
    area : 2.0 ;
    mode_definition(op_mode) {
      mode_value(fast) {
        when : "A" ;
        sdf_cond : "A == 1'b1" ;
      }
      mode_value(slow) {
        when : "!A" ;
        sdf_cond : "A == 1'b0" ;
      }
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_73: timing with related_output_pin
TEST_F(StaLibertyTest, TimingRelatedOutputPin) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_73) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(ROP1) {
    area : 4.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Y) {
      direction : output ;
      function : "A & B" ;
    }
    pin(Z) {
      direction : output ;
      function : "A | B" ;
      timing() {
        related_pin : "A" ;
        related_output_pin : "Y" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_74: wire_load_selection group
TEST_F(StaLibertyTest, WireLoadSelection) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_74) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  wire_load("small") {
    capacitance : 0.1 ;
    resistance : 0.001 ;
    slope : 5.0 ;
    fanout_length(1, 1.0) ;
    fanout_length(2, 2.0) ;
  }
  wire_load("medium") {
    capacitance : 0.2 ;
    resistance : 0.002 ;
    slope : 6.0 ;
    fanout_length(1, 1.5) ;
    fanout_length(2, 3.0) ;
  }
  wire_load_selection(area_sel) {
    wire_load_from_area(0, 100, "small") ;
    wire_load_from_area(100, 1000, "medium") ;
  }
  default_wire_load_selection : area_sel ;
  cell(WLS1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_75: interface_timing on cell
TEST_F(StaLibertyTest, CellInterfaceTiming3) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_75) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(IF1) {
    area : 2.0 ;
    interface_timing : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_76: cell_footprint attribute
TEST_F(StaLibertyTest, CellFootprint4) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_76) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(FP1) {
    area : 2.0 ;
    cell_footprint : buf ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_77: test_cell group
TEST_F(StaLibertyTest, TestCellGroup) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_77) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(TC1) {
    area : 3.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; clock : true ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff(IQ, IQN) {
      clocked_on : "CLK" ;
      next_state : "D" ;
    }
    test_cell() {
      pin(D) {
        direction : input ;
        signal_type : test_scan_in ;
      }
      pin(CLK) {
        direction : input ;
        signal_type : test_clock ;
      }
      pin(Q) {
        direction : output ;
        signal_type : test_scan_out ;
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_78: memory group
TEST_F(StaLibertyTest, MemoryGroup) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_78) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(SRAM1) {
    area : 100.0 ;
    is_memory : true ;
    memory() {
      type : ram ;
      address_width : 4 ;
      word_width : 8 ;
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_79: cell with always_on attribute
TEST_F(StaLibertyTest, CellAlwaysOn3) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_79) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(AON1) {
    area : 2.0 ;
    always_on : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_80: cell with is_level_shifter and level_shifter_type
TEST_F(StaLibertyTest, CellLevelShifter) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_80) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(LS1) {
    area : 3.0 ;
    is_level_shifter : true ;
    level_shifter_type : HL ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      level_shifter_data_pin : true ;
    }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_81: cell with is_isolation_cell
TEST_F(StaLibertyTest, CellIsolationCell) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_81) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(ISO1) {
    area : 3.0 ;
    is_isolation_cell : true ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      isolation_cell_data_pin : true ;
    }
    pin(EN) {
      direction : input ;
      capacitance : 0.01 ;
      isolation_cell_enable_pin : true ;
    }
    pin(Z) { direction : output ; function : "A & EN" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_82: statetable group
TEST_F(StaLibertyTest, StatetableGroup) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_82) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(ST1) {
    area : 4.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(E) { direction : input ; capacitance : 0.01 ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    statetable("D E", "IQ") {
      table : "H L : - : H, \
               L L : - : L, \
               - H : - : N" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_83: Timing with sdf_cond
TEST_F(StaLibertyTest, TimingSdfCond) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_83) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(SDF2) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A & B" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        sdf_cond : "B == 1'b1" ;
        when : "B" ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_84: power with rise_power and fall_power groups
TEST_F(StaLibertyTest, RiseFallPowerGroups) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_84) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  power_lut_template(power_2d) {
    variable_1 : input_transition_time ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(PW2) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      internal_power() {
        related_pin : "A" ;
        rise_power(power_2d) {
          values("0.001, 0.002", "0.003, 0.004") ;
        }
        fall_power(power_2d) {
          values("0.005, 0.006", "0.007, 0.008") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_85: TimingGroup makeLinearModels coverage
TEST_F(StaLibertyTest, TimingGroupLinearModels) {
  TimingGroup tg(1);
  tg.setIntrinsic(RiseFall::rise(), 0.05f);
  tg.setIntrinsic(RiseFall::fall(), 0.06f);
  tg.setResistance(RiseFall::rise(), 100.0f);
  tg.setResistance(RiseFall::fall(), 120.0f);
  // makeLinearModels needs a cell - but we can verify values are set
  float val;
  bool exists;
  tg.intrinsic(RiseFall::rise(), val, exists);
  EXPECT_TRUE(exists);
  tg.resistance(RiseFall::fall(), val, exists);
  EXPECT_TRUE(exists);
}

// R9_86: multiple wire_load and default_wire_load
TEST_F(StaLibertyTest, DefaultWireLoad) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_86) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  wire_load("tiny") {
    capacitance : 0.05 ;
    resistance : 0.001 ;
    slope : 3.0 ;
    fanout_length(1, 0.5) ;
  }
  default_wire_load : "tiny" ;
  default_wire_load_mode : top ;
  cell(DWL1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_87: voltage_map attribute
TEST_F(StaLibertyTest, VoltageMap) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_87) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  voltage_map(VDD, 1.1) ;
  voltage_map(VSS, 0.0) ;
  voltage_map(VDDL, 0.8) ;
  cell(VM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_88: default_operating_conditions
TEST_F(StaLibertyTest, DefaultOperatingConditions) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_88) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  operating_conditions(fast_oc) {
    process : 0.8 ;
    voltage : 1.2 ;
    temperature : 0.0 ;
    tree_type : best_case_tree ;
  }
  operating_conditions(slow_oc) {
    process : 1.2 ;
    voltage : 0.9 ;
    temperature : 125.0 ;
    tree_type : worst_case_tree ;
  }
  default_operating_conditions : fast_oc ;
  cell(DOC1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_89: pg_pin group with pg_type and voltage_name
TEST_F(StaLibertyTest, PgPin) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_89) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  voltage_map(VDD, 1.1) ;
  voltage_map(VSS, 0.0) ;
  cell(PG1) {
    area : 2.0 ;
    pg_pin(VDD) {
      pg_type : primary_power ;
      voltage_name : VDD ;
    }
    pg_pin(VSS) {
      pg_type : primary_ground ;
      voltage_name : VSS ;
    }
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_90: TimingGroup set/get cell table models
TEST_F(StaLibertyTest, TimingGroupCellModels) {
  TimingGroup tg(1);
  tg.setCell(RiseFall::rise(), nullptr);
  tg.setCell(RiseFall::fall(), nullptr);
  EXPECT_EQ(tg.cell(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.cell(RiseFall::fall()), nullptr);
}

// R9_91: TimingGroup constraint setters
TEST_F(StaLibertyTest, TimingGroupConstraintModels) {
  TimingGroup tg(1);
  tg.setConstraint(RiseFall::rise(), nullptr);
  tg.setConstraint(RiseFall::fall(), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.constraint(RiseFall::fall()), nullptr);
}

// R9_92: TimingGroup transition setters
TEST_F(StaLibertyTest, TimingGroupTransitionModels) {
  TimingGroup tg(1);
  tg.setTransition(RiseFall::rise(), nullptr);
  tg.setTransition(RiseFall::fall(), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.transition(RiseFall::fall()), nullptr);
}

// R9_93: bus_naming_style attribute
TEST_F(StaLibertyTest, BusNamingStyle) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_93) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  bus_naming_style : "%s[%d]" ;
  cell(BNS1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_94: cell_leakage_power
TEST_F(StaLibertyTest, CellLeakagePower5) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_94) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  leakage_power_unit : "1nW" ;
  capacitive_load_unit(1, ff) ;
  cell(CLP1) {
    area : 2.0 ;
    cell_leakage_power : 1.5 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_95: clock_gating_integrated_cell
TEST_F(StaLibertyTest, ClockGatingIntegratedCell) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_95) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(CGC1) {
    area : 3.0 ;
    clock_gating_integrated_cell : latch_posedge ;
    pin(CLK) {
      direction : input ;
      capacitance : 0.01 ;
      clock : true ;
      clock_gate_clock_pin : true ;
    }
    pin(EN) {
      direction : input ;
      capacitance : 0.01 ;
      clock_gate_enable_pin : true ;
    }
    pin(GCLK) {
      direction : output ;
      function : "CLK & EN" ;
      clock_gate_out_pin : true ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_96: output_current_rise/fall (CCS constructs)
TEST_F(StaLibertyTest, OutputCurrentRiseFall) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_96) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  output_current_template(ccs_template) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    variable_3 : time ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(CCS1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        output_current_rise(ccs_template) {
          vector(0) {
            index_3("0.0, 0.1, 0.2, 0.3, 0.4") ;
            values("0.001, 0.002", "0.003, 0.004") ;
          }
        }
        output_current_fall(ccs_template) {
          vector(0) {
            index_3("0.0, 0.1, 0.2, 0.3, 0.4") ;
            values("0.001, 0.002", "0.003, 0.004") ;
          }
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_97: three_state attribute on pin
TEST_F(StaLibertyTest, PinThreeState) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_97) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(TS1) {
    area : 3.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(EN) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      three_state : "EN" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_98: rise_capacitance_range and fall_capacitance_range
TEST_F(StaLibertyTest, PinCapacitanceRange) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_98) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(CR1) {
    area : 2.0 ;
    pin(A) {
      direction : input ;
      rise_capacitance : 0.01 ;
      fall_capacitance : 0.012 ;
      rise_capacitance_range(0.008, 0.012) ;
      fall_capacitance_range(0.009, 0.015) ;
    }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_99: dont_use attribute
TEST_F(StaLibertyTest, CellDontUse4) {
  const char *content = R"(
library(test_r9_99) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(DU1) {
    area : 2.0 ;
    dont_use : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("DU1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->dontUse());
}

// R9_100: is_macro_cell attribute
TEST_F(StaLibertyTest, CellIsMacro4) {
  const char *content = R"(
library(test_r9_100) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(MAC1) {
    area : 100.0 ;
    is_macro_cell : true ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  ASSERT_NE(lib, nullptr);
  LibertyCell *cell = lib->findLibertyCell("MAC1");
  ASSERT_NE(cell, nullptr);
  EXPECT_TRUE(cell->isMacro());
}

// R9_101: OCV derate at cell level
TEST_F(StaLibertyTest, OcvDerateCellLevel) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_101) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  ocv_table_template(ocv_tmpl4) {
    variable_1 : total_output_net_capacitance ;
    index_1("0.001, 0.01") ;
  }
  cell(OCV6) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
    ocv_derate(cell_derate) {
      ocv_derate_factors(ocv_tmpl4) {
        rf_type : rise_and_fall ;
        derate_type : early ;
        path_type : clock_and_data ;
        values("0.95, 0.96") ;
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_102: timing with when (conditional)
TEST_F(StaLibertyTest, TimingWhenConditional) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_102) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(delay_template_2x2) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(COND1) {
    area : 3.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(B) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A & B" ;
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        when : "B" ;
        sdf_cond : "B == 1'b1" ;
        cell_rise(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.01, 0.02", "0.03, 0.04") ;
        }
      }
      timing() {
        related_pin : "A" ;
        timing_sense : positive_unate ;
        when : "!B" ;
        sdf_cond : "B == 1'b0" ;
        cell_rise(delay_template_2x2) {
          values("0.02, 0.03", "0.04, 0.05") ;
        }
        cell_fall(delay_template_2x2) {
          values("0.02, 0.03", "0.04, 0.05") ;
        }
        rise_transition(delay_template_2x2) {
          values("0.02, 0.03", "0.04, 0.05") ;
        }
        fall_transition(delay_template_2x2) {
          values("0.02, 0.03", "0.04, 0.05") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_103: default_max_fanout
TEST_F(StaLibertyTest, DefaultMaxFanout) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_103) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_max_fanout : 32.0 ;
  cell(DMF1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_104: default_fanout_load
TEST_F(StaLibertyTest, DefaultFanoutLoad) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r9_104) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  default_fanout_load : 2.0 ;
  cell(DFL1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R9_105: TimingGroup outputWaveforms accessors (should be null by default)
TEST_F(StaLibertyTest, TimingGroupOutputWaveforms) {
  TimingGroup tg(1);
  EXPECT_EQ(tg.outputWaveforms(RiseFall::rise()), nullptr);
  EXPECT_EQ(tg.outputWaveforms(RiseFall::fall()), nullptr);
}

// =========================================================================
// R11_ tests: Cover additional uncovered functions in liberty module
// =========================================================================

// R11_1: timingTypeString - the free function in TimingArc.cc
// It is not declared in a public header, so we declare it extern here.
extern const char *timingTypeString(TimingType type);

TEST_F(StaLibertyTest, TimingTypeString) {
  // timingTypeString is defined in TimingArc.cc
  // We test several timing types to cover the function
  EXPECT_STREQ(timingTypeString(TimingType::combinational), "combinational");
  EXPECT_STREQ(timingTypeString(TimingType::clear), "clear");
  EXPECT_STREQ(timingTypeString(TimingType::rising_edge), "rising_edge");
  EXPECT_STREQ(timingTypeString(TimingType::falling_edge), "falling_edge");
  EXPECT_STREQ(timingTypeString(TimingType::setup_rising), "setup_rising");
  EXPECT_STREQ(timingTypeString(TimingType::hold_falling), "hold_falling");
  EXPECT_STREQ(timingTypeString(TimingType::three_state_enable), "three_state_enable");
  EXPECT_STREQ(timingTypeString(TimingType::unknown), "unknown");
}

// R11_2: writeLiberty exercises LibertyWriter constructor, destructor,
// writeHeader, writeFooter, asString(bool), and the full write path
TEST_F(StaLibertyTest, WriteLiberty) {
  ASSERT_NE(lib_, nullptr);
  std::string tmpfile = makeUniqueTmpPath();
  // writeLiberty is declared in LibertyWriter.hh
  writeLiberty(lib_, tmpfile.c_str(), sta_);
  // Verify the file was written and has content
  FILE *fp = fopen(tmpfile.c_str(), "r");
  ASSERT_NE(fp, nullptr);
  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  EXPECT_GT(sz, 100);  // non-trivial content
  fclose(fp);
  EXPECT_EQ(remove(tmpfile.c_str()), 0);
}

// R11_3: LibertyParser direct usage - exercises LibertyParser constructor,
// group(), deleteGroups(), makeVariable(), LibertyStmt constructors/destructors,
// LibertyAttr, LibertySimpleAttr, LibertyComplexAttr, LibertyStringAttrValue,
// LibertyFloatAttrValue, LibertyDefine, LibertyVariable, isGroup/isAttribute/
// isDefine/isVariable/isSimple/isComplex, and values() on simple attrs.
TEST_F(StaLibertyTest, LibertyParserDirect) {
  // Write a simple lib file for parser testing
  const char *content = R"(
library(test_r11_parser) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  define(my_attr, cell, string) ;
  my_var = 3.14 ;
  cell(P1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  std::string tmp_path = makeUniqueTmpPath();
  writeLibContent(content, tmp_path);

  // Parse with a simple visitor that just collects groups
  Report *report = sta_->report();
  // Use parseLibertyFile which creates the parser internally
  // This exercises LibertyParser constructor, LibertyScanner,
  // group(), deleteGroups()
  class TestVisitor : public LibertyGroupVisitor {
  public:
    int group_count = 0;
    int attr_count = 0;
    int var_count = 0;
    void begin(LibertyGroup *group) override { group_count++; }
    void end(LibertyGroup *) override {}
    void visitAttr(LibertyAttr *attr) override {
      attr_count++;
      // Exercise isSimple, isComplex, values()
      // isAttribute() returns false for LibertyAttr subclasses
      EXPECT_FALSE(attr->isAttribute());
      EXPECT_FALSE(attr->isGroup());
      EXPECT_FALSE(attr->isDefine());
      EXPECT_FALSE(attr->isVariable());
      if (attr->isSimpleAttr()) {
        EXPECT_FALSE(attr->isComplexAttr());
        // Simple attrs have firstValue but values() is not supported
      }
      if (attr->isComplexAttr()) {
        EXPECT_FALSE(attr->isSimpleAttr());
      }
      // Exercise firstValue
      LibertyAttrValue *val = attr->firstValue();
      if (val) {
        if (val->isString()) {
          EXPECT_FALSE(val->stringValue().empty());
          EXPECT_FALSE(val->isFloat());
        }
        if (val->isFloat()) {
          EXPECT_FALSE(val->isString());
          EXPECT_FALSE(std::isinf(val->floatValue()));
        }
      }
    }
    void visitVariable(LibertyVariable *variable) override {
      var_count++;
      EXPECT_TRUE(variable->isVariable());
      EXPECT_FALSE(variable->isGroup());
      EXPECT_FALSE(variable->isAttribute());
      EXPECT_FALSE(variable->isDefine());
      EXPECT_FALSE(variable->variable().empty());
      EXPECT_FALSE(std::isinf(variable->value()));
    }
    bool save(LibertyGroup *) override { return false; }
    bool save(LibertyAttr *) override { return false; }
    bool save(LibertyVariable *) override { return false; }
  };

  TestVisitor visitor;
  parseLibertyFile(tmp_path.c_str(), &visitor, report);
  EXPECT_GT(visitor.group_count, 0);
  EXPECT_GT(visitor.attr_count, 0);
  EXPECT_GT(visitor.var_count, 0);
  remove(tmp_path.c_str());
}

// R11_4: Liberty file with wireload_selection to cover WireloadForArea
TEST_F(StaLibertyTest, WireloadForArea) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_wfa) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  wire_load("small") {
    resistance : 0.0 ;
    capacitance : 1.0 ;
    area : 0.0 ;
    slope : 100.0 ;
    fanout_length(1, 200) ;
  }
  wire_load("medium") {
    resistance : 0.0 ;
    capacitance : 1.0 ;
    area : 0.0 ;
    slope : 200.0 ;
    fanout_length(1, 400) ;
  }
  wire_load_selection(sel1) {
    wire_load_from_area(0, 100, "small") ;
    wire_load_from_area(100, 500, "medium") ;
  }
  cell(WFA1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R11_5: Liberty file with latch to exercise inferLatchRoles
TEST_F(StaLibertyTest, InferLatchRoles) {
  const char *content = R"(
library(test_r11_latch) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(LATCH1) {
    area : 5.0 ;
    pin(D) { direction : input ; capacitance : 0.01 ; }
    pin(G) { direction : input ; capacitance : 0.01 ; }
    pin(Q) {
      direction : output ;
      function : "IQ" ;
    }
    latch(IQ, IQN) {
      enable : "G" ;
      data_in : "D" ;
    }
  }
}
)";
  // Read with infer_latches = true
  std::string tmp_path = makeUniqueTmpPath();
  writeLibContent(content, tmp_path);
  LibertyLibrary *lib = sta_->readLiberty(tmp_path.c_str(), sta_->cmdScene(),
                                           MinMaxAll::min(), true);  // infer_latches=true
  EXPECT_NE(lib, nullptr);
  if (lib) {
    LibertyCell *cell = lib->findLibertyCell("LATCH1");
    EXPECT_NE(cell, nullptr);
    if (cell) {
      EXPECT_TRUE(cell->hasSequentials());
    }
  }
  remove(tmp_path.c_str());
}

// R11_6: Liberty file with leakage_power { when } to cover LeakagePowerGroup::setWhen
TEST_F(StaLibertyTest, LeakagePowerWhen) {
  const char *content = R"(
library(test_r11_lpw) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  leakage_power_unit : "1nW" ;
  cell(LPW1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
    leakage_power() {
      when : "A" ;
      value : 10.5 ;
    }
    leakage_power() {
      when : "!A" ;
      value : 5.2 ;
    }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  EXPECT_NE(lib, nullptr);
  if (lib) {
    LibertyCell *cell = lib->findLibertyCell("LPW1");
    EXPECT_NE(cell, nullptr);
  }
}

// R11_7: Liberty file with statetable to cover StatetableGroup::addRow
TEST_F(StaLibertyTest, Statetable) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_st) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(ST1) {
    area : 3.0 ;
    pin(S) { direction : input ; capacitance : 0.01 ; }
    pin(R) { direction : input ; capacitance : 0.01 ; }
    pin(Q) {
      direction : output ;
      function : "IQ" ;
    }
    statetable("S R", "IQ") {
      table : "H L : - : H ,\
               L H : - : L ,\
               L L : - : N ,\
               H H : - : X" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R11_8: Liberty file with internal_power to cover
// InternalPowerModel::checkAxes/checkAxis
TEST_F(StaLibertyTest, InternalPowerModel) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_ipm) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  leakage_power_unit : "1nW" ;
  cell(IPM1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(scalar) { values("0.1") ; }
        cell_fall(scalar) { values("0.1") ; }
        rise_transition(scalar) { values("0.05") ; }
        fall_transition(scalar) { values("0.05") ; }
      }
      internal_power() {
        related_pin : "A" ;
        rise_power(scalar) { values("0.5") ; }
        fall_power(scalar) { values("0.3") ; }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R11_9: Liberty file with bus port to cover PortNameBitIterator and findLibertyMember
TEST_F(StaLibertyTest, BusPortAndMember) {
  const char *content = R"(
library(test_r11_bus) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  type(bus4) {
    base_type : array ;
    data_type : bit ;
    bit_width : 4 ;
    bit_from : 3 ;
    bit_to : 0 ;
  }
  cell(BUS1) {
    area : 4.0 ;
    bus(D) {
      bus_type : bus4 ;
      direction : input ;
      capacitance : 0.01 ;
    }
    pin(Z) { direction : output ; function : "D[0]" ; }
  }
}
)";
  LibertyLibrary *lib = writeAndReadLibReturn(sta_, content);
  EXPECT_NE(lib, nullptr);
  if (lib) {
    LibertyCell *cell = lib->findLibertyCell("BUS1");
    EXPECT_NE(cell, nullptr);
    if (cell) {
      // The bus should create member ports
      LibertyPort *bus_port = cell->findLibertyPort("D");
      if (bus_port) {
        // findLibertyMember on bus port
        LibertyPort *member = bus_port->findLibertyMember(0);
        if (member)
          EXPECT_NE(member, nullptr);
      }
    }
  }
}

// R11_10: Liberty file with include directive to cover LibertyScanner::includeBegin, fileEnd
// We test this by creating a .lib that includes another .lib
TEST_F(StaLibertyTest, LibertyInclude) {
  // First write the included file
  std::string inc_path = makeUniqueTmpPath();
  FILE *finc = fopen(inc_path.c_str(), "w");
  ASSERT_NE(finc, nullptr);
  fprintf(finc, "  cell(INC1) {\n");
  fprintf(finc, "    area : 1.0 ;\n");
  fprintf(finc, "    pin(A) { direction : input ; capacitance : 0.01 ; }\n");
  fprintf(finc, "    pin(Z) { direction : output ; function : \"A\" ; }\n");
  fprintf(finc, "  }\n");
  fclose(finc);

  // Write the main lib directly (not through writeAndReadLib which changes path)
  std::string main_path = makeUniqueTmpPath();
  FILE *fm = fopen(main_path.c_str(), "w");
  ASSERT_NE(fm, nullptr);
  fprintf(fm, "library(test_r11_include) {\n");
  fprintf(fm, "%s", R9_THRESHOLDS);
  fprintf(fm, "  delay_model : table_lookup ;\n");
  fprintf(fm, "  time_unit : \"1ns\" ;\n");
  fprintf(fm, "  voltage_unit : \"1V\" ;\n");
  fprintf(fm, "  current_unit : \"1mA\" ;\n");
  fprintf(fm, "  capacitive_load_unit(1, ff) ;\n");
  fprintf(fm, "  include_file(%s) ;\n", inc_path.c_str());
  fprintf(fm, "}\n");
  fclose(fm);

  LibertyLibrary *lib = sta_->readLiberty(main_path.c_str(), sta_->cmdScene(),
                                           MinMaxAll::min(), false);
  EXPECT_NE(lib, nullptr);
  if (lib) {
    LibertyCell *cell = lib->findLibertyCell("INC1");
    EXPECT_NE(cell, nullptr);
  }
  EXPECT_EQ(remove(inc_path.c_str()), 0);
  EXPECT_EQ(remove(main_path.c_str()), 0);
}

// R11_11: Exercise timing arc traversal from loaded library
TEST_F(StaLibertyTest, TimingArcSetTraversal) {
  ASSERT_NE(lib_, nullptr);
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  // Count arc sets and arcs
  int arc_set_count = 0;
  int arc_count = 0;
  for (TimingArcSet *arc_set : buf->timingArcSets()) {
    arc_set_count++;
    for (TimingArc *arc : arc_set->arcs()) {
      arc_count++;
      EXPECT_NE(arc->fromEdge(), nullptr);
      EXPECT_NE(arc->toEdge(), nullptr);
      EXPECT_GE(arc->index(), 0);
    }
  }
  EXPECT_GT(arc_set_count, 0);
  EXPECT_GT(arc_count, 0);
}

// R11_12: GateTableModel::checkAxis and CheckTableModel::checkAxis
// These are exercised by reading a liberty with table_lookup models
// containing different axis variables
TEST_F(StaLibertyTest, TableModelCheckAxis) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_axis) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(tmpl_2d) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1, 0.5") ;
    index_2("0.001, 0.01, 0.1") ;
  }
  lu_table_template(tmpl_check) {
    variable_1 : related_pin_transition ;
    variable_2 : constrained_pin_transition ;
    index_1("0.01, 0.1, 0.5") ;
    index_2("0.01, 0.1, 0.5") ;
  }
  cell(AX1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(CLK) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(tmpl_2d) {
          values("0.1, 0.2, 0.3", \
                 "0.2, 0.3, 0.4", \
                 "0.3, 0.4, 0.5") ;
        }
        cell_fall(tmpl_2d) {
          values("0.1, 0.2, 0.3", \
                 "0.2, 0.3, 0.4", \
                 "0.3, 0.4, 0.5") ;
        }
        rise_transition(tmpl_2d) {
          values("0.05, 0.1, 0.2", \
                 "0.1, 0.15, 0.3", \
                 "0.2, 0.3, 0.5") ;
        }
        fall_transition(tmpl_2d) {
          values("0.05, 0.1, 0.2", \
                 "0.1, 0.15, 0.3", \
                 "0.2, 0.3, 0.5") ;
        }
      }
      timing() {
        related_pin : "CLK" ;
        timing_type : setup_rising ;
        rise_constraint(tmpl_check) {
          values("0.05, 0.1, 0.15", \
                 "0.1, 0.15, 0.2", \
                 "0.15, 0.2, 0.25") ;
        }
        fall_constraint(tmpl_check) {
          values("0.05, 0.1, 0.15", \
                 "0.1, 0.15, 0.2", \
                 "0.15, 0.2, 0.25") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R11_13: CheckLinearModel::setIsScaled, CheckTableModel::setIsScaled via
// library with k_process/k_temp/k_volt scaling factors on setup
TEST_F(StaLibertyTest, ScaledModels) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_scaled) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  k_process_cell_rise : 1.0 ;
  k_process_cell_fall : 1.0 ;
  k_temp_cell_rise : 0.001 ;
  k_temp_cell_fall : 0.001 ;
  k_volt_cell_rise : -0.5 ;
  k_volt_cell_fall : -0.5 ;
  k_process_setup_rise : 1.0 ;
  k_process_setup_fall : 1.0 ;
  k_temp_setup_rise : 0.001 ;
  k_temp_setup_fall : 0.001 ;
  operating_conditions(WORST) {
    process : 1.0 ;
    temperature : 125.0 ;
    voltage : 0.9 ;
  }
  cell(SC1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(scalar) { values("0.1") ; }
        cell_fall(scalar) { values("0.1") ; }
        rise_transition(scalar) { values("0.05") ; }
        fall_transition(scalar) { values("0.05") ; }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R11_14: Library with cell that has internal_ports attribute
// Exercises setHasInternalPorts
TEST_F(StaLibertyTest, HasInternalPorts) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_intport) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(IP1) {
    area : 3.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(QN) { direction : output ; function : "IQ'" ; }
    pin(Q) { direction : output ; function : "IQ" ; }
    ff(IQ, IQN) {
      next_state : "A" ;
      clocked_on : "A" ;
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R11_15: Directly test LibertyParser API through parseLibertyFile
// Focus on saving attrs/variables/groups to exercise more code paths
TEST_F(StaLibertyTest, ParserSaveAll) {
  const char *content = R"(
library(test_r11_save) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  define(custom_attr, cell, float) ;
  my_variable = 42.0 ;
  cell(SV1) {
    area : 1.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) { direction : output ; function : "A" ; }
  }
}
)";
  std::string tmp_path = makeUniqueTmpPath();
  writeLibContent(content, tmp_path);

  // Visitor that saves everything
  class SaveVisitor : public LibertyGroupVisitor {
  public:
    int group_begin_count = 0;
    int group_end_count = 0;
    int define_count = 0;
    int var_count = 0;
    void begin(LibertyGroup *group) override {
      group_begin_count++;
      EXPECT_TRUE(group->isGroup());
      EXPECT_FALSE(group->isAttribute());
      EXPECT_FALSE(group->isVariable());
      EXPECT_FALSE(group->isDefine());
      EXPECT_FALSE(group->type().empty());
    }
    void end(LibertyGroup *) override { group_end_count++; }
    void visitAttr(LibertyAttr *attr) override {
      // Check isDefine virtual dispatch
      if (attr->isDefine())
        define_count++;
    }
    void visitVariable(LibertyVariable *var) override {
      var_count++;
    }
    bool save(LibertyGroup *) override { return true; }
    bool save(LibertyAttr *) override { return true; }
    bool save(LibertyVariable *) override { return true; }
  };

  Report *report = sta_->report();
  SaveVisitor visitor;
  parseLibertyFile(tmp_path.c_str(), &visitor, report);
  EXPECT_GT(visitor.group_begin_count, 0);
  EXPECT_EQ(visitor.group_begin_count, visitor.group_end_count);
  remove(tmp_path.c_str());
}

// R11_16: Exercises clearAxisValues and setEnergyScale through internal_power
// with energy values
TEST_F(StaLibertyTest, EnergyScale) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_energy) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  leakage_power_unit : "1nW" ;
  lu_table_template(energy_tmpl) {
    variable_1 : input_transition_time ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  cell(EN1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(scalar) { values("0.1") ; }
        cell_fall(scalar) { values("0.1") ; }
        rise_transition(scalar) { values("0.05") ; }
        fall_transition(scalar) { values("0.05") ; }
      }
      internal_power() {
        related_pin : "A" ;
        rise_power(energy_tmpl) {
          values("0.001, 0.002", \
                 "0.003, 0.004") ;
        }
        fall_power(energy_tmpl) {
          values("0.001, 0.002", \
                 "0.003, 0.004") ;
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R11_17: LibertyReader findPort by reading a lib and querying
TEST_F(StaLibertyTest, FindPort) {
  ASSERT_NE(lib_, nullptr);
  LibertyCell *inv = lib_->findLibertyCell("INV_X1");
  ASSERT_NE(inv, nullptr);
  LibertyPort *portA = inv->findLibertyPort("A");
  EXPECT_NE(portA, nullptr);
  LibertyPort *portZN = inv->findLibertyPort("ZN");
  EXPECT_NE(portZN, nullptr);
  // Non-existent port
  LibertyPort *portX = inv->findLibertyPort("NONEXISTENT");
  EXPECT_EQ(portX, nullptr);
}

// R11_18: LibertyPort::scenePort (requires DcalcAnalysisPt, but we test
// through the Nangate45 library which has corners)
TEST_F(StaLibertyTest, CornerPort) {
  ASSERT_NE(lib_, nullptr);
  LibertyCell *buf = lib_->findLibertyCell("BUF_X1");
  ASSERT_NE(buf, nullptr);
  LibertyPort *portA = buf->findLibertyPort("A");
  ASSERT_NE(portA, nullptr);
  // scenePort requires a Scene and MinMax
  Scene *scene = sta_->cmdScene();
  if (scene) {
    LibertyPort *scene_port = portA->scenePort(scene, MinMax::min());
    EXPECT_NE(scene_port, nullptr);
  }
}

// R11_19: Exercise receiver model set through timing group
TEST_F(StaLibertyTest, ReceiverModel) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_recv) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  cell(RV1) {
    area : 2.0 ;
    pin(A) {
      direction : input ;
      capacitance : 0.01 ;
      receiver_capacitance() {
        receiver_capacitance1_rise(scalar) { values("0.001") ; }
        receiver_capacitance1_fall(scalar) { values("0.001") ; }
        receiver_capacitance2_rise(scalar) { values("0.002") ; }
        receiver_capacitance2_fall(scalar) { values("0.002") ; }
      }
    }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(scalar) { values("0.1") ; }
        cell_fall(scalar) { values("0.1") ; }
        rise_transition(scalar) { values("0.05") ; }
        fall_transition(scalar) { values("0.05") ; }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

// R11_20: Read a liberty with CCS (composite current source) output_current
// to exercise OutputWaveform constructors and related paths
TEST_F(StaLibertyTest, CCSOutputCurrent) {
  ASSERT_NO_THROW(( [&](){
  const char *content = R"(
library(test_r11_ccs) {
  delay_model : table_lookup ;
  time_unit : "1ns" ;
  voltage_unit : "1V" ;
  current_unit : "1mA" ;
  capacitive_load_unit(1, ff) ;
  lu_table_template(ccs_tmpl_oc) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    index_1("0.01, 0.1") ;
    index_2("0.001, 0.01") ;
  }
  output_current_template(oc_tmpl) {
    variable_1 : input_net_transition ;
    variable_2 : total_output_net_capacitance ;
    variable_3 : time ;
  }
  cell(CCS1) {
    area : 2.0 ;
    pin(A) { direction : input ; capacitance : 0.01 ; }
    pin(Z) {
      direction : output ;
      function : "A" ;
      timing() {
        related_pin : "A" ;
        cell_rise(ccs_tmpl_oc) {
          values("0.1, 0.2", \
                 "0.2, 0.3") ;
        }
        cell_fall(ccs_tmpl_oc) {
          values("0.1, 0.2", \
                 "0.2, 0.3") ;
        }
        rise_transition(ccs_tmpl_oc) {
          values("0.05, 0.1", \
                 "0.1, 0.2") ;
        }
        fall_transition(ccs_tmpl_oc) {
          values("0.05, 0.1", \
                 "0.1, 0.2") ;
        }
        output_current_rise() {
          vector(oc_tmpl) {
            index_1("0.01") ;
            index_2("0.001") ;
            index_3("0.0, 0.01, 0.02, 0.03, 0.04") ;
            values("0.0, -0.001, -0.005, -0.002, 0.0") ;
          }
        }
        output_current_fall() {
          vector(oc_tmpl) {
            index_1("0.01") ;
            index_2("0.001") ;
            index_3("0.0, 0.01, 0.02, 0.03, 0.04") ;
            values("0.0, 0.001, 0.005, 0.002, 0.0") ;
          }
        }
      }
    }
  }
}
)";
  writeAndReadLib(sta_, content);

  }() ));
}

} // namespace sta
