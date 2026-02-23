// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "MinMax.hh"
#include "Transition.hh"
#include "LibertyClass.hh"
#include "TimingModel.hh"

namespace sta {

class Unit;
class Units;
class Report;
class Table;
class TableModel;
class TableAxis;
class OutputWaveforms;

using FloatSeq = std::vector<float>;
using FloatTable = std::vector<FloatSeq>;
// Sequence of 1D tables (order 1).
using Table1Seq = std::vector<Table*>;
using Waveform = Table;

TableAxisVariable
stringTableAxisVariable(const char *variable);
const char *
tableVariableString(TableAxisVariable variable);
const Unit *
tableVariableUnit(TableAxisVariable variable,
                  const Units *units);

class GateTableModel : public GateTimingModel
{
public:
  GateTableModel(LibertyCell *cell,
                 TableModel *delay_model,
                 TableModel *delay_sigma_models[EarlyLate::index_count],
                 TableModel *slew_model,
                 TableModel *slew_sigma_models[EarlyLate::index_count],
                 ReceiverModelPtr receiver_model,
                 OutputWaveforms *output_waveforms);
  ~GateTableModel() override;
  void gateDelay(const Pvt *pvt,
                 float in_slew,
                 float load_cap,
                 bool pocv_enabled,
                 // Return values.
                 ArcDelay &gate_delay,
                 Slew &drvr_slew) const override;
  // deprecated 2024-01-07
  // related_out_cap arg removed.
  void gateDelay(const Pvt *pvt,
                 float in_slew,
                 float load_cap,
                 float related_out_cap,
                 bool pocv_enabled,
                 ArcDelay &gate_delay,
                 Slew &drvr_slew) const __attribute__ ((deprecated));
  std::string reportGateDelay(const Pvt *pvt,
                              float in_slew,
                              float load_cap,
                              bool pocv_enabled,
                              int digits) const override;
  float driveResistance(const Pvt *pvt) const override;

  const TableModel *delayModel() const { return delay_model_.get(); }
  const TableModel *slewModel() const { return slew_model_.get(); }
  const TableModel *delaySigmaModel(const EarlyLate *el) const;
  const TableModel *slewSigmaModel(const EarlyLate *el) const;
  const ReceiverModel *receiverModel() const { return receiver_model_.get(); }
  OutputWaveforms *outputWaveforms() const { return output_waveforms_.get(); }
  // Check the axes before making the model.
  // Return true if the model axes are supported.
  static bool checkAxes(const TablePtr &table);

protected:
  void maxCapSlew(float in_slew,
                  const Pvt *pvt,
                  float &slew,
                  float &cap) const;
  void setIsScaled(bool is_scaled) override;
  float axisValue(const TableAxis *axis,
                  float load_cap,
                  float in_slew,
                  float related_out_cap) const;
  float findValue(const Pvt *pvt,
                  const TableModel *model,
                  float in_slew,
                  float load_cap,
                  float related_out_cap) const;
  std::string reportTableLookup(const char *result_name,
                                const Pvt *pvt,
                                const TableModel *model,
                                float in_slew,
                                float load_cap,
                                float related_out_cap,
                                int digits) const;
  void findAxisValues(const TableModel *model,
                      float in_slew,
                      float load_cap,
                      float related_out_cap,
                      // Return values.
                      float &axis_value1,
                      float &axis_value2,
                      float &axis_value3) const;
  static bool checkAxis(const TableAxis *axis);

  std::unique_ptr<TableModel> delay_model_;
  std::array<std::unique_ptr<TableModel>, EarlyLate::index_count> delay_sigma_models_;
  std::unique_ptr<TableModel> slew_model_;
  std::array<std::unique_ptr<TableModel>, EarlyLate::index_count> slew_sigma_models_;
  ReceiverModelPtr receiver_model_;
  std::unique_ptr<OutputWaveforms> output_waveforms_;
};

class CheckTableModel : public CheckTimingModel
{
public:
  CheckTableModel(LibertyCell *cell,
                  TableModel *model,
                  TableModel *sigma_models[EarlyLate::index_count]);
  ~CheckTableModel() override;
  ArcDelay checkDelay(const Pvt *pvt,
                      float from_slew,
                      float to_slew,
                      float related_out_cap,
                      bool pocv_enabled) const override;
  std::string reportCheckDelay(const Pvt *pvt,
                               float from_slew,
                               const char *from_slew_annotation,
                               float to_slew,
                               float related_out_cap,
                               bool pocv_enabled,
                               int digits) const override;
  const TableModel *model() const { return model_.get(); }
  const TableModel *sigmaModel(const EarlyLate *el) const;

  // Check the axes before making the model.
  // Return true if the model axes are supported.
  static bool checkAxes(const TablePtr table);

protected:
  void setIsScaled(bool is_scaled) override;
  float findValue(const Pvt *pvt,
                  const TableModel *model,
                  float from_slew,
                  float to_slew,
                  float related_out_cap) const;
  void findAxisValues(float from_slew,
                      float to_slew,
                      float related_out_cap,
                      // Return values.
                      float &axis_value1,
                      float &axis_value2,
                      float &axis_value3) const;
  float axisValue(const TableAxis *axis,
                  float load_cap,
                  float in_slew,
                  float related_out_cap) const;
  std::string reportTableDelay(const char *result_name,
                               const Pvt *pvt,
                               const TableModel *model,
                               float from_slew,
                               const char *from_slew_annotation,
                               float to_slew,
                               float related_out_cap,
                               int digits) const;
  static bool checkAxis(const TableAxis *axis);

  std::unique_ptr<TableModel> model_;
  std::array<std::unique_ptr<TableModel>, EarlyLate::index_count> sigma_models_;
};

class TableAxis
{
public:
  TableAxis(TableAxisVariable variable,
            FloatSeq &&values);
  TableAxisVariable variable() const { return variable_; }
  const char *variableString() const;
  const Unit *unit(const Units *units);
  size_t size() const { return values_.size(); }
  bool inBounds(float value) const;
  float axisValue(size_t index) const { return values_[index]; }
  // Find the index for value such that axis[index] <= value < axis[index+1].
  size_t findAxisIndex(float value) const;
  void findAxisIndex(float value,
                     // Return values.
                     size_t &index,
                     bool &exists) const;
  size_t findAxisClosestIndex(float value) const;
  const FloatSeq &values() const { return values_; }
  float min() const;
  float max() const;

private:
  TableAxisVariable variable_;
  FloatSeq values_;
};

// 0, 1, 2, or 3 dimension float tables.
class Table
{
public:
  Table();
  explicit Table(float value);
  Table(FloatSeq *values,
        TableAxisPtr axis1);
  Table(FloatSeq &&values,
        TableAxisPtr axis1);
  Table(FloatTable &&values,
        TableAxisPtr axis1,
        TableAxisPtr axis2);
  Table(FloatTable &&values,
        TableAxisPtr axis1,
        TableAxisPtr axis2,
        TableAxisPtr axis3);
  Table(Table &&table);
  Table(const Table &table);
  Table &operator=(Table &&table);

  void setScaleFactorType(ScaleFactorType type);
  int order() const { return order_; }
  const TableAxis *axis1() const { return axis1_.get(); }
  const TableAxis *axis2() const { return axis2_.get(); }
  const TableAxis *axis3() const { return axis3_.get(); }
  const TableAxisPtr axis1ptr() const { return axis1_; }
  void setIsScaled(bool is_scaled);

  float value(size_t axis_idx1,
              size_t axis_idx2,
              size_t axis_idx3) const;
  // Single-index value (order 1 only).
  float value(size_t index1) const;
  // Two-index value (order 2 and 3).
  float value(size_t axis_index1,
              size_t axis_index2) const;

  // Table interpolated lookup.
  float findValue(float axis_value1,
                  float axis_value2,
                  float axis_value3) const;
  // One-argument lookup (order 1).
  void findValue(float axis_value1,
                 float &value,
                 bool &extrapolated) const;
  float findValue(float axis_value1) const;
  float findValueClip(float axis_value1) const;
  // Table interpolated lookup with scale factor.
  float findValue(const LibertyLibrary *library,
                  const LibertyCell *cell,
                  const Pvt *pvt,
                  float axis_value1,
                  float axis_value2,
                  float axis_value3) const;

  std::string reportValue(const char *result_name,
                          const LibertyCell *cell,
                          const Pvt *pvt,
                          float value1,
                          const char *comment1,
                          float value2,
                          float value3,
                          const Unit *table_unit,
                          int digits) const;
  void report(const Units *units,
              Report *report) const;

  // Order 1: pointer to value sequence (nullptr if not order 1).
  FloatSeq *values() const;
  // Order 2 and 3: pointer to value table (nullptr otherwise).
  FloatTable *values3();
  const FloatTable *values3() const;

private:
  void clear();
  std::string reportValueOrder0(const char *result_name,
                                const char *comment1,
                                const Unit *table_unit,
                                int digits) const;
  std::string reportValueOrder1(const char *result_name,
                                const LibertyCell *cell,
                                float value1,
                                const char *comment1,
                                float value2,
                                float value3,
                                const Unit *table_unit,
                                int digits) const;
  std::string reportValueOrder2(const char *result_name,
                                const LibertyCell *cell,
                                float value1,
                                const char *comment1,
                                float value2,
                                float value3,
                                const Unit *table_unit,
                                int digits) const;
  std::string reportValueOrder3(const char *result_name,
                                const LibertyCell *cell,
                                float value1,
                                const char *comment1,
                                float value2,
                                float value3,
                                const Unit *table_unit,
                                int digits) const;

  int order_;
  float value_;              // order 0 only
  FloatSeq values1_;         // order 1 only
  FloatTable values_table_;  // order 2 and 3
  TableAxisPtr axis1_;
  TableAxisPtr axis2_;
  TableAxisPtr axis3_;
};

// Wrapper class for Table to apply scale factors.
class TableModel
{
public:
  TableModel();
  TableModel(TablePtr table,
             TableTemplate *tbl_template,
             ScaleFactorType scale_factor_type,
             const RiseFall *rf);
  void setScaleFactorType(ScaleFactorType type);
  int order() const;
  TableTemplate *tblTemplate() const { return tbl_template_; }
  const TablePtr &table() const { return table_; }
  ScaleFactorType scaleFactorType() const;
  int rfIndex() const { return rf_index_; }
  const TableAxis *axis1() const;
  const TableAxis *axis2() const;
  const TableAxis *axis3() const;
  void setIsScaled(bool is_scaled);
  float value(size_t index1,
              size_t index2,
              size_t index3) const;
  // Table interpolated lookup.
  float findValue(float value1,
                  float value2,
                  float value3) const;
  // Table interpolated lookup with scale factor.
  float findValue(const LibertyCell *cell,
                  const Pvt *pvt,
                  float value1,
                  float value2,
                  float value3) const;
  std::string reportValue(const char *result_name,
                          const LibertyCell *cell,
                          const Pvt *pvt,
                          float value1,
                          const char *comment1,
                          float value2,
                          float value3,
                          const Unit *table_unit,
                          int digits) const;
  std::string report(const Units *units,
                     Report *report) const;

protected:
  float scaleFactor(const LibertyCell *cell,
                    const Pvt *pvt) const;
  std::string reportPvtScaleFactor(const LibertyCell *cell,
                                   const Pvt *pvt,
                                   int digits) const;

  TablePtr table_;
  TableTemplate *tbl_template_;
  // ScaleFactorType gcc barfs if this is dcl'd.
  unsigned scale_factor_type_:scale_factor_bits;
  unsigned rf_index_:RiseFall::index_bit_count;
  bool is_scaled_:1;
};

////////////////////////////////////////////////////////////////

class ReceiverModel
{
public:
  ~ReceiverModel();
  void setCapacitanceModel(TableModel table_model,
                           size_t segment,
                           const RiseFall *rf);
  static bool checkAxes(TablePtr table);

private:
  std::vector<TableModel> capacitance_models_;
};

// Two dimensional (slew/cap) table of one dimensional time/current tables.
class OutputWaveforms
{
public:
  OutputWaveforms(TableAxisPtr slew_axis,
                  TableAxisPtr cap_axis,
                  const RiseFall *rf,
                  Table1Seq &current_waveforms,
                  Table ref_times);
  ~OutputWaveforms();
  const RiseFall *rf() const { return rf_; }
  const TableAxis *slewAxis() const { return slew_axis_.get(); }
  const TableAxis *capAxis() const { return cap_axis_.get(); }
  // Make voltage wavefroms from liberty time/current values.
  // Required before voltageTime, timeVoltage, voltageCurrent.
  void ensureVoltageWaveforms(float vdd);
  float timeCurrent(float slew,
                    float cap,
                    float time);
  float timeVoltage(float slew,
                    float cap,
                    float time);
  float voltageTime(float in_slew,
                    float load_cap,
                    float voltage);
  float voltageCurrent(float slew,
                       float cap,
                       float volt);
  float referenceTime(float slew);
  float beginTime(float slew,
                  float cap);
  float endTime(float slew,
                float cap);
  static bool checkAxes(const TableTemplate *tbl_template);

  Table currentWaveform(float slew,
                        float cap);
  // Waveform closest to slew/cap; no interpolation.
  const Table *currentWaveformRaw(float slew,
                                  float cap);
  Table voltageWaveform(float in_slew,
                        float load_cap);
  // Waveform closest to slew/cap; no interpolation.
  const Table *voltageWaveformRaw(float slew,
                                  float cap);
  Table voltageCurrentWaveform(float slew,
                               float cap);
  // V/I for last segment of min slew/max cap.
  float finalResistance();

private:
  void findVoltages(size_t wave_index,
                    float cap);
  float waveformValue(float slew,
                      float cap,
                      float axis_value,
                      Table1Seq &waveforms);
  float beginEndTime(float slew,
                     float cap,
                     bool begin);
  double voltageTime1(double volt,
                      double dx1,
                      double dx2,
                      size_t wave_index00,
                      size_t wave_index01,
                      size_t wave_index10,
                      size_t wave_index11);
  float voltageTime2(float volt,
                     size_t wave_index);

  // Row.
  TableAxisPtr slew_axis_;
  // Column.
  TableAxisPtr cap_axis_;
  const RiseFall *rf_;
  Table1Seq current_waveforms_;   // from liberty (1D tables)
  Table1Seq voltage_waveforms_;
  Table1Seq voltage_currents_;
  Table ref_times_;
  float vdd_;
  static constexpr size_t voltage_waveform_step_count_ = 100;
};

class DriverWaveform
{
public:
  DriverWaveform(const std::string &name,
                 TablePtr waveforms);
  const char *name() const { return name_.c_str(); }
  Table waveform(float slew);

private:
  std::string name_;
  TablePtr waveforms_;
};

} // namespace
