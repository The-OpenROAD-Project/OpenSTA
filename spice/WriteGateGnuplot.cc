// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include "WriteGateGnuplot.hh"

#include <string>

#include "Gnuplot.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Network.hh"
#include "Scene.hh"
#include "StaState.hh"
#include "Xyce.hh"
#include "dcalc/DelayNoise.hh"
#include "dcalc/PrimaDelayCalc.hh"

namespace sta {

class WriteGateGnuplot : public StaState
{
public:
  WriteGateGnuplot(ArcDcalcArgSeq &gates,
                   const PinSet &plot_pins,
                   std::string_view spice_waveform_filename,
                   std::string_view gnuplot_filename,
                   std::string_view gnuplot_csv_filename,
                   const Scene *scene,
                   const MinMax *min_max,
                   StaState *sta);
  void writeFiles();

private:
  void plotWaveforms(Gnuplot &plot);
  bool watchSpiceTitle(std::string &title);

  ArcDcalcArgSeq &gates_;
  const PinSet &plot_pins_;
  const Scene *scene_;
  const MinMax *min_max_;
  std::string_view spice_waveform_filename_;
  std::string_view gnuplot_filename_;
  std::string_view gnuplot_csv_filename_;

  ArcDcalcWaveforms *arc_dcalc_{nullptr};
  PrimaDelayCalc *prima_dcalc_{nullptr};
};

void
writeGateGnuplot(ArcDcalcArgSeq &gates,
                 const PinSet &plot_pins,
                 std::string_view spice_waveform_filename,
                 std::string_view gnuplot_filename,
                 std::string_view gnuplot_csv_filename,
                 const Scene *scene,
                 const MinMax *min_max,
                 StaState *sta)
{
  WriteGateGnuplot writer(gates, plot_pins, spice_waveform_filename,
                          gnuplot_filename, gnuplot_csv_filename,
                          scene, min_max, sta);
  writer.writeFiles();
}

WriteGateGnuplot::WriteGateGnuplot(ArcDcalcArgSeq &gates,
                                   const PinSet &plot_pins,
                                   std::string_view spice_waveform_filename,
                                   std::string_view gnuplot_filename,
                                   std::string_view gnuplot_csv_filename,
                                   const Scene *scene,
                                   const MinMax *min_max,
                                   StaState *sta) :
  StaState(sta),
  gates_(gates),
  plot_pins_(plot_pins),
  scene_(scene),
  min_max_(min_max),
  spice_waveform_filename_(spice_waveform_filename),
  gnuplot_filename_(gnuplot_filename),
  gnuplot_csv_filename_(gnuplot_csv_filename)
{
}

void
WriteGateGnuplot::writeFiles()
{
  arc_dcalc_ = dynamic_cast<ArcDcalcWaveforms*>(arc_delay_calc_);
  prima_dcalc_ = dynamic_cast<PrimaDelayCalc*>(arc_delay_calc_);
  if (prima_dcalc_ == nullptr)
    report_->error(1820, "Delay calculator is not Prima.");

  arc_delay_calc_->setDcalcArgParasiticSlew(gates_, scene_, min_max_);
  ArcDcalcArg &drvr_gate = gates_[0];
  const Pin *drvr_pin = drvr_gate.drvrPin();
  Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
  const LoadPinIndexMap load_pin_index_map =
    graph_delay_calc_->makeLoadPinIndexMap(drvr_vertex);

  arc_dcalc_->clearWatchPins();
  for (const Pin *plot_pin : plot_pins_)
    arc_dcalc_->watchPin(plot_pin);

  size_t prima_order_save = 0;
  if (prima_dcalc_ && delay_noise_) {
    prima_order_save = prima_dcalc_->primaReduceOrder();
    prima_dcalc_->setPrimaReduceOrder(gates_.size()*delay_noise_->primaOrderPerGate());
  }

  arc_delay_calc_->gateDelays(gates_, load_pin_index_map, scene_, min_max_);

  if (prima_dcalc_)
    prima_dcalc_->setPrimaReduceOrder(prima_order_save);

  Gnuplot plot;
  plotWaveforms(plot);
  plot.writeFiles(gnuplot_filename_, gnuplot_csv_filename_);
  arc_dcalc_->clearWatchPins();
}

void
WriteGateGnuplot::plotWaveforms(Gnuplot &plot)
{
  for (const Pin *pin : plot_pins_) {
    Waveform waveform = arc_dcalc_->watchWaveform(pin);
    if (waveform.values() && !waveform.values()->empty()) {
      std::string title = network_->pathName(pin);
      plot.append(title, std::move(waveform));
    }
  }

  // Add spice waveforms.
  if (!spice_waveform_filename_.empty()) {
    StringSeq titles;
    WaveformSeq waveforms;
    readXyceCsv(spice_waveform_filename_, titles, waveforms);
    for (size_t i = 0; i < waveforms.size(); i++) {
      std::string title = titles[i];
      if (watchSpiceTitle(title))
        plot.append(title, std::move(waveforms[i]));
    }
  }
}

static std::string
stripVparens(std::string &title)
{
  return title.substr(2, title.length() - 3);
}

bool
WriteGateGnuplot::watchSpiceTitle(std::string &title)
{
  std::string title_pin_name = stripVparens(title);
  for (const Pin *pin : plot_pins_) {
    std::string watch_name = network_->pathName(pin);
    if (stringEqual(title_pin_name, watch_name))
      return true;
  }
  return false;
}

} // namespace sta
