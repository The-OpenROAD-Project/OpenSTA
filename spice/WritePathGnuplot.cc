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

#include "WritePathGnuplot.hh"

#include "ArcDelayCalc.hh"
#include "Gnuplot.hh"
#include "Graph.hh"
#include "GraphDelayCalc.hh"
#include "Network.hh"
#include "PathExpanded.hh"
#include "Scene.hh"
#include "StaState.hh"
#include "Units.hh"
#include "Xyce.hh"
#include "dcalc/DelayNoise.hh"
#include "dcalc/PrimaDelayCalc.hh"
#include "spice/PathStages.hh"
#include "spice/WriteSpice.hh"

namespace sta {

class WritePathGnuplot : public StaState
{
public:
  WritePathGnuplot(const Path *path,
                   const PinSet &plot_pins,
                   std::string_view spice_waveform_filename,
                   std::string_view gnuplot_filename,
                   std::string_view gnuplot_csv_filename,
                   StaState *sta);
  void writeFiles();

private:
  void plotSpiceWaveforms(Gnuplot &plot);
  bool watchSpiceTitle(std::string &title);
  float spiceWaveformOffset();

  const Path *path_;
  PathExpanded path_expanded_;
  PathStages stages_;

  const PinSet &plot_pins_;
  std::string_view spice_waveform_filename_;
  std::string_view gnuplot_filename_;
  std::string_view gnuplot_csv_filename_;

  ArcDcalcWaveforms *arc_dcalc_{nullptr};
  PrimaDelayCalc *prima_dcalc_{nullptr};
};

void
writePathGnuplot(const Path *path,
                 const PinSet &plot_pins,
                 std::string_view spice_waveform_filename,
                 std::string_view gnuplot_filename,
                 std::string_view gnuplot_csv_filename,
                 StaState *sta)
{
  WritePathGnuplot writer(path, plot_pins, spice_waveform_filename,
                          gnuplot_filename, gnuplot_csv_filename, sta);
  writer.writeFiles();
}

WritePathGnuplot::WritePathGnuplot(const Path *path,
                                   const PinSet &plot_pins,
                                   std::string_view spice_waveform_filename,
                                   std::string_view gnuplot_filename,
                                   std::string_view gnuplot_csv_filename,
                                   StaState *sta) :
  StaState(sta),
  path_(path),
  path_expanded_(sta),
  stages_(path_expanded_, sta),
  plot_pins_(plot_pins),
  spice_waveform_filename_(spice_waveform_filename),
  gnuplot_filename_(gnuplot_filename),
  gnuplot_csv_filename_(gnuplot_csv_filename)
{
  path_expanded_.expand(path_, true);
}

void
WritePathGnuplot::writeFiles()
{
  arc_dcalc_ = dynamic_cast<ArcDcalcWaveforms*>(arc_delay_calc_);
  prima_dcalc_ = dynamic_cast<PrimaDelayCalc*>(arc_delay_calc_);
  if (prima_dcalc_ == nullptr)
    report_->error(1830, "Delay calculator is not Prima.");
  
  Gnuplot plot;
  for (Stage stage : stages_) {
    const Pin *drvr_pin = stages_.stageDrvrPin(stage);
    const Pin *load_pin = stages_.stageLoadPin(stage);
    arc_dcalc_->clearWatchPins();
    if (plot_pins_.contains(drvr_pin))
      arc_dcalc_->watchPin(drvr_pin);
    if (plot_pins_.contains(load_pin))
      arc_dcalc_->watchPin(load_pin);

    const TimingArc *arc = stages_.stageGateArc(stage);
    if (arc) {
      const Scene *scene = path_->scene(this);
      const MinMax *min_max = path_->minMax(this);
      const Pin *input_pin = stages_.stageGateInputPin(stage);
      Edge *edge = stages_.stageGateEdge(stage);
      Vertex *drvr_vertex = graph_->pinDrvrVertex(drvr_pin);
      const LoadPinIndexMap load_pin_index_map =
        graph_delay_calc_->makeLoadPinIndexMap(drvr_vertex);
      const Path *input_path = stages_.stageGateInputPath(stage);
      float input_delay = 0.0;
      if (input_path) {
        const Slew &in_slew =
          graph_delay_calc_->edgeFromSlew(edge->from(graph_),
                                          arc->fromEdge()->asRiseFall(),
                                          edge, scene, min_max);
        input_delay = delayAsFloat(input_path->arrival())
          - in_slew * 0.5 + spiceWaveformOffset();
      }

      // Path gate delay calculation to collect waveforms.
      ArcDcalcArgSeq gates;
      ArcDcalcArg gate(input_pin, drvr_pin, edge, arc, input_delay);
      arc_delay_calc_->setDcalcArgParasiticSlew(gate, scene, min_max);
      gates.push_back(gate);
      arc_delay_calc_->gateDelays(gates, load_pin_index_map, scene, min_max);

      for (const Pin *pin : arc_dcalc_->watchPins()) {
        Waveform waveform = arc_dcalc_->watchWaveform(pin);
        if (waveform.values() && !waveform.values()->empty()) {
          std::string title = network_->pathName(pin);
          plot.append(title, std::move(waveform));
        }
      }
    }
  }

  plotSpiceWaveforms(plot);
  plot.writeFiles(gnuplot_filename_, gnuplot_csv_filename_);
  arc_dcalc_->clearWatchPins();
}

// There is no way to align the first input ramp to spice so it
// occurs at t=0. This moves the dcalc waveforms to align with the
// spice results.
// WriteSpice::findSlew
float
WritePathGnuplot::spiceWaveformOffset()
{
  const Path *input_path = path_expanded_.path(0);
  const Scene *scene = input_path->scene(this);
  const MinMax *min_max = input_path->minMax(this);
  DcalcAPIndex ap_index = scene->dcalcAnalysisPtIndex(min_max);
  const Vertex *input_vertex = input_path->vertex(this);
  const RiseFall *input_rf = input_path->transition(this);
  float slew = delayAsFloat(graph_->slew(input_vertex, input_rf, ap_index));
  if (slew == 0.0)
    slew = units_->timeUnit()->scale();
  return slew * 0.5;
}

void
WritePathGnuplot::plotSpiceWaveforms(Gnuplot &plot)
{
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
WritePathGnuplot::watchSpiceTitle(std::string &title)
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
