// OpenSTA, Static Timing Analyzer
// Copyright (c) 2023, Parallax Software, Inc.
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

#pragma once

#include <string>
#include <memory>

#include "MinMax.hh"
#include "Vector.hh"
#include "Transition.hh"
#include "LibertyClass.hh"
#include "TimingModel.hh"

namespace sta {

using std::string;

class Unit;
class Units;
class Report;
class Table;
class OutputWaveforms;
class Table1;

typedef Vector<float> FloatSeq;
typedef Vector<FloatSeq*> FloatTable;
typedef Vector<Table1*> Table1Seq;

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
  GateTableModel(TableModel *delay_model,
		 TableModel *delay_sigma_models[EarlyLate::index_count],
		 TableModel *slew_model,
		 TableModel *slew_sigma_models[EarlyLate::index_count],
                 ReceiverModelPtr receiver_model,
                 OutputWaveforms *output_waveforms);
  virtual ~GateTableModel();
  void gateDelay(const LibertyCell *cell,
                 const Pvt *pvt,
                 float in_slew,
                 float load_cap,
                 float related_out_cap,
                 bool pocv_enabled,
                 // Return values.
                 ArcDelay &gate_delay,
                 Slew &drvr_slew) const override;
  string reportGateDelay(const LibertyCell *cell,
                         const Pvt *pvt,
                         float in_slew,
                         float load_cap,
                         float related_out_cap,
                         bool pocv_enabled,
                         int digits) const override;
  float driveResistance(const LibertyCell *cell,
                        const Pvt *pvt) const override;

  const TableModel *delayModel() const { return delay_model_; }
  const TableModel *slewModel() const { return slew_model_;  }
  ReceiverModelPtr receiverModel() const { return receiver_model_; }
  OutputWaveforms *outputWaveforms() const { return output_waveforms_; }
  // Check the axes before making the model.
  // Return true if the model axes are supported.
  static bool checkAxes(const TablePtr table);

protected:
  void maxCapSlew(const LibertyCell *cell,
		  float in_slew,
		  const Pvt *pvt,
		  float &slew,
		  float &cap) const;
  void setIsScaled(bool is_scaled) override;
  float axisValue(TableAxisPtr axis,
		  float load_cap,
		  float in_slew,
		  float related_out_cap) const;
  float findValue(const LibertyLibrary *library,
		  const LibertyCell *cell,
		  const Pvt *pvt,
		  const TableModel *model,
		  float in_slew,
		  float load_cap,
		  float related_out_cap) const;
  string reportTableLookup(const char *result_name,
                           const LibertyLibrary *library,
                           const LibertyCell *cell,
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
  static bool checkAxis(TableAxisPtr axis);

  TableModel *delay_model_;
  TableModel *delay_sigma_models_[EarlyLate::index_count];
  TableModel *slew_model_;
  TableModel *slew_sigma_models_[EarlyLate::index_count];
  ReceiverModelPtr receiver_model_;
  OutputWaveforms *output_waveforms_;
};

class CheckTableModel : public CheckTimingModel
{
public:
  explicit CheckTableModel(TableModel *model,
			   TableModel *sigma_models[EarlyLate::index_count]);
  virtual ~CheckTableModel();
  void checkDelay(const LibertyCell *cell,
                  const Pvt *pvt,
                  float from_slew,
                  float to_slew,
                  float related_out_cap,
                  bool pocv_enabled,
                  // Return values.
                  ArcDelay &margin) const override;
  string reportCheckDelay(const LibertyCell *cell,
                          const Pvt *pvt,
                          float from_slew,
                          const char *from_slew_annotation,
                          float to_slew,
                          float related_out_cap,
                          bool pocv_enabled,
                          int digits) const override;
  const TableModel *model() const { return model_; }

  // Check the axes before making the model.
  // Return true if the model axes are supported.
  static bool checkAxes(const TablePtr table);

protected:
  void setIsScaled(bool is_scaled) override;
  float findValue(const LibertyLibrary *library,
		  const LibertyCell *cell,
		  const Pvt *pvt,
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
  float axisValue(TableAxisPtr axis,
		  float load_cap,
		  float in_slew,
		  float related_out_cap) const;
  string reportTableDelay(const char *result_name,
                          const LibertyLibrary *library,
                          const LibertyCell *cell,
                          const Pvt *pvt,
                          const TableModel *model,
                          float from_slew,
                          const char *from_slew_annotation,
                          float to_slew,
                          float related_out_cap,
                          int digits) const;
  static bool checkAxis(TableAxisPtr axis);

  TableModel *model_;
  TableModel *sigma_models_[EarlyLate::index_count];
};

// Wrapper class for Table to apply scale factors.
class TableModel
{
public:
  TableModel(TablePtr table,
             TableTemplate *tbl_template,
	     ScaleFactorType scale_factor_type,
	     RiseFall *rf);
  void setScaleFactorType(ScaleFactorType type);
  int order() const;
  TableTemplate *tblTemplate() const { return tbl_template_; }
  TableAxisPtr axis1() const;
  TableAxisPtr axis2() const;
  TableAxisPtr axis3() const;
  void setIsScaled(bool is_scaled);
  float value(size_t index1,
              size_t index2,
              size_t index3) const;
  // Table interpolated lookup.
  float findValue(float value1,
		  float value2,
		  float value3) const;
  // Table interpolated lookup with scale factor.
  float findValue(const LibertyLibrary *library,
		  const LibertyCell *cell,
		  const Pvt *pvt,
		  float value1,
		  float value2,
		  float value3) const;
  string reportValue(const char *result_name,
                     const LibertyLibrary *library,
                     const LibertyCell *cell,
                     const Pvt *pvt,
                     float value1,
                     const char *comment1,
                     float value2,
                     float value3,
                     const Unit *table_unit,
                     int digits) const;
  string report(const Units *units,
                Report *report) const;

protected:
  float scaleFactor(const LibertyLibrary *library,
		    const LibertyCell *cell,
		    const Pvt *pvt) const;
  string reportPvtScaleFactor(const LibertyLibrary *library,
                              const LibertyCell *cell,
                              const Pvt *pvt,
                              int digits) const;

  TablePtr table_;
  TableTemplate *tbl_template_;
  // ScaleFactorType gcc barfs if this is dcl'd.
  unsigned scale_factor_type_:scale_factor_bits;
  unsigned rf_index_:RiseFall::index_bit_count;
  bool is_scaled_:1;
};

// Abstract base class for 0, 1, 2, or 3 dimesnion float tables.
class Table
{
public:
  Table() {}
  virtual ~Table() {}
  void setScaleFactorType(ScaleFactorType type);
  virtual int order() const = 0;
  virtual TableAxisPtr axis1() const { return nullptr; }
  virtual TableAxisPtr axis2() const { return nullptr; }
  virtual TableAxisPtr axis3() const { return nullptr; }
  void setIsScaled(bool is_scaled);
  virtual float value(size_t axis_idx1,
                      size_t axis_idx2,
                      size_t axis_idx3) const = 0;
  // Table interpolated lookup.
  virtual float findValue(float axis_value1,
			  float axis_value2,
			  float axis_value3) const = 0;
  // Table interpolated lookup with scale factor.
  float findValue(const LibertyLibrary *library,
		  const LibertyCell *cell,
		  const Pvt *pvt,
		  float axis_value1,
		  float axis_value2,
		  float axis_value3) const;
  virtual string reportValue(const char *result_name,
                             const LibertyLibrary *library,
                             const LibertyCell *cell,
                             const Pvt *pvt,
                             float value1,
                             const char *comment1,
                             float value2,
                             float value3,
                             const Unit *table_unit,
                             int digits) const = 0;
  virtual void report(const Units *units,
		      Report *report) const = 0;
};

// Zero dimension (scalar) table.
class Table0 : public Table
{
public:
  Table0(float value);
  int order() const override { return 0; }
  float value(size_t axis_index1,
              size_t axis_index2,
              size_t axis_index3) const override;
  float findValue(float axis_value1,
                  float axis_value2,
                  float axis_value3) const override;
  string reportValue(const char *result_name,
                     const LibertyLibrary *library,
                     const LibertyCell *cell,
                     const Pvt *pvt,
                     float value1,
                     const char *comment1,
                     float value2,
                     float value3,
                     const Unit *table_unit,
                     int digits) const override;
  void report(const Units *units,
              Report *report) const override;
  using Table::findValue;

private:
  float value_;
};

// One dimensional table.
class Table1 : public Table
{
public:
  Table1();
  Table1(FloatSeq *values,
	 TableAxisPtr axis1);
  virtual ~Table1();
  Table1(Table1 &&table);
  Table1 &operator= (Table1 &&table);
  int order() const override { return 1; }
  TableAxisPtr axis1() const override { return axis1_; }
  float value(size_t axis_index1,
              size_t axis_index2,
              size_t axis_index3) const override;
  float findValue(float value1,
                  float value2,
                  float value3) const override;
  string reportValue(const char *result_name,
                     const LibertyLibrary *library,
                     const LibertyCell *cell,
                     const Pvt *pvt,
                     float value1,
                     const char *comment1,
                     float value2,
                     float value3,
                     const Unit *table_unit,
                     int digits) const override;
  void report(const Units *units,
              Report *report) const override;

  // Table1 specific functions.
  float value(size_t index1) const;
  void findValue(float axis_value1,
                 // Return values.
                 float &value,
                 bool &extrapolated) const;
  float findValue(float axis_value1) const;
  float findValueClip(float axis_value1) const;
  FloatSeq *values() const { return values_; }
  using Table::findValue;

private:
  FloatSeq *values_;
  TableAxisPtr axis1_;
};

// Two dimensional table.
class Table2 : public Table
{
public:
  Table2(FloatTable *values,
	 TableAxisPtr axis1,
	 TableAxisPtr axis2);
  virtual ~Table2();
  int order() const override { return 2; }
  TableAxisPtr axis1() const override { return axis1_; }
  TableAxisPtr axis2() const override { return axis2_; }
  float value(size_t axis_index1,
              size_t axis_index2,
              size_t axis_index3) const override;
  float findValue(float value1,
                  float value2,
                  float value3) const override;
  string reportValue(const char *result_name,
                     const LibertyLibrary *library,
                     const LibertyCell *cell,
                     const Pvt *pvt,
                     float value1,
                     const char *comment1,
                     float value2,
                     float value3,
                     const Unit *table_unit,
                     int digits) const override;
  void report(const Units *units,
              Report *report) const override;

  // Table2 specific functions.
  float value(size_t axis_index1,
              size_t axis_index2) const;
  FloatTable *values3() { return values_; }

  using Table::findValue;

protected:
  FloatTable *values_;
  // Row.
  TableAxisPtr axis1_;
  // Column.
  TableAxisPtr axis2_;
};

// Three dimensional table.
class Table3 : public Table2
{
public:
  Table3(FloatTable *values,
	 TableAxisPtr axis1,
	 TableAxisPtr axis2,
	 TableAxisPtr axis3);
  virtual ~Table3() {}
  int order() const override { return 3; }
  TableAxisPtr axis1() const override { return axis1_; }
  TableAxisPtr axis2() const override { return axis2_; }
  TableAxisPtr axis3() const override { return axis3_; }
  float value(size_t axis_index1,
              size_t axis_index2,
              size_t axis_index3) const override;
  float findValue(float value1,
                  float value2,
                  float value3) const override;
  string reportValue(const char *result_name,
                     const LibertyLibrary *library,
                     const LibertyCell *cell,
                     const Pvt *pvt,
                     float value1,
                     const char *comment1,
                     float value2,
                     float value3,
                     const Unit *table_unit,
                     int digits) const override;
  void report(const Units *units,
              Report *report) const override;
  using Table::findValue;

private:
  TableAxisPtr axis3_;
};

class TableAxis
{
public:
  TableAxis(TableAxisVariable variable,
	    FloatSeq *values);
  ~TableAxis();
  TableAxisVariable variable() const { return variable_; }
  const char *variableString() const;
  const Unit *unit(const Units *units);
  size_t size() const { return values_->size(); }
  float axisValue(size_t index) const { return (*values_)[index]; }
  // Find the index for value such that axis[index] <= value < axis[index+1].
  size_t findAxisIndex(float value) const;
  void findAxisIndex(float value,
                     // Return values.
                     size_t &index,
                     bool &exists) const;
  FloatSeq *values() const { return values_; }
  float min() const { return (*values_)[0]; }
  float max() const { return (*values_)[values_->size() - 1]; }

private:
  TableAxisVariable variable_;
  FloatSeq *values_;
};

////////////////////////////////////////////////////////////////

class ReceiverModel
{
public:
  ReceiverModel();
  ~ReceiverModel();
  void setCapacitanceModel(TableModel *table_model,
                           int index,
                           RiseFall *rf);
  static bool checkAxes(TablePtr table);

private:
  TableModel *capacitance_models_[2][RiseFall::index_count];
};

// Two dimensional (slew/cap) table of one dimensional time/current tables.
class OutputWaveforms
{
public:
  OutputWaveforms(TableAxisPtr slew_axis,
                  TableAxisPtr cap_axis,
                  const RiseFall *rf,
                  Table1Seq &current_waveforms,
                  Table1 *ref_times);
  ~OutputWaveforms();
  const RiseFall *rf() const { return rf_; }
  Table1 voltageWaveform(float in_slew,
                         float load_cap);
  Table1 currentWaveform(float slew,
                         float cap);
  float referenceTime(float slew);
  static bool checkAxes(TableTemplate *tbl_template);

private:
  Table1 *voltageWaveform(size_t wave_index,
                          float cap);

  // Row.
  TableAxisPtr slew_axis_;
  // Column.
  TableAxisPtr cap_axis_;
  const RiseFall *rf_;
  Table1Seq current_waveforms_;
  Table1Seq voltage_waveforms_;
  Table1 *ref_times_;
};

class DriverWaveform
{
public:
  DriverWaveform(const char *name,
                 TablePtr waveforms);
  ~DriverWaveform();
  const char *name() const { return name_; }
  Table1 waveform(float slew);

private:
  const char *name_;
  TablePtr waveforms_;
};

} // namespace
