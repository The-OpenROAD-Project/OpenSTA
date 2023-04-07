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

#include <map>

#include "LibertyClass.hh"
#include "SdcClass.hh"
#include "SearchClass.hh"
#include "StaState.hh"
#include "RiseFallMinMax.hh"

namespace sta {

class Sta;
class LibertyBuilder;

class OutputDelays
{
public:
  OutputDelays();
  TimingSense timingSense() const;

  RiseFallMinMax delays;
  // input edge -> output edge path exists for unateness
  bool rf_path_exists[RiseFall::index_count][RiseFall::index_count];
};

typedef std::map<const ClockEdge*, RiseFallMinMax> ClockEdgeDelays;
typedef std::map<const Pin *, OutputDelays> OutputPinDelays;

class MakeTimingModel : public StaState
{
public:
  MakeTimingModel(const char *lib_name,
                  const char *cell_name,
                  const char *filename,
                  const Corner *corner,
                  Sta *sta);
  ~MakeTimingModel();
  LibertyLibrary *makeTimingModel();

private:
  void makeLibrary();
  void makeCell();
  void makePorts();
  void checkClock(Clock *clk);
  void findTimingFromInputs();
  void findTimingFromInput(Port *input_port);
  void findClkedOutputPaths();
  void findOutputDelays(const RiseFall *input_rf,
                        OutputPinDelays &output_pin_delays);
  void makeSetupHoldTimingArcs(const Pin *input_pin,
                               const ClockEdgeDelays &clk_margins);
  void makeInputOutputTimingArcs(const Pin *input_pin,
                                 OutputPinDelays &output_pin_delays);
  TimingModel *makeScalarCheckModel(float value,
                                    ScaleFactorType scale_factor_type,
                                    RiseFall *rf);
  TimingModel *makeGateModelScalar(Delay delay,
                                   Slew slew,
                                   RiseFall *rf);
  TimingModel *makeGateModelTable(const Pin *output_pin,
                                  Delay delay,
                                  RiseFall *rf);
  TableTemplate *ensureTableTemplate(const TableTemplate *drvr_template,
                                     TableAxisPtr load_axis);
  TableAxisPtr loadCapacitanceAxis(const TableModel *table);
  LibertyPort *modelPort(const Pin *pin);

  void saveSdc();
  void restoreSdc();

  const char *lib_name_;
  const char *cell_name_;
  const char *filename_;
  const Corner *corner_;
  LibertyLibrary *library_;
  LibertyCell *cell_;
  MinMax *min_max_;
  LibertyBuilder *lib_builder_;
  // Output driver table model template to model template.
  Map<const TableTemplate*, TableTemplate*> template_map_;
  int tbl_template_index_;
  Sdc *sdc_backup_;
  Sta *sta_;
};

} // namespace
