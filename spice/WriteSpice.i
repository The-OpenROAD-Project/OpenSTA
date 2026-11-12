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
// 
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
// 
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
// 
// This notice may not be removed or altered from any source distribution.

%{

#include "spice/WritePathSpice.hh"
#include "spice/WriteGateSpice.hh"
#include "spice/WriteGateGnuplot.hh"
#include "spice/WritePathGnuplot.hh"

%}

%inline %{

void
write_path_spice_cmd(const Path *path,
                     const char *spice_filename,
                     const char *subckt_filename,
                     StringSeq lib_subckt_filenames,
                     const char *model_filename,
                     const char *power_name,
                     const char *gnd_name,
                     CircuitSim ckt_sim)
{
  Sta *sta = Sta::sta();
  sta->writePathSpice(path, spice_filename, subckt_filename,
                      std::move(lib_subckt_filenames), model_filename,
                      power_name, gnd_name, ckt_sim);
}

void
write_path_gnuplot_cmd(const Path *path,
                       PinSet plot_pins,
                       const char *spice_waveform_filename,
                       const char *gnuplot_filename,
                       const char *gnuplot_csv_filename)
{
  Sta *sta = Sta::sta();
  sta->writePathGnuplot(path, plot_pins, spice_waveform_filename,
                        gnuplot_filename, gnuplot_csv_filename);
}

void
write_gate_spice_cmd(ArcDcalcArgSeq gates,
                     const char *spice_filename,
                     const char *subckt_filename,
                     StringSeq lib_subckt_filenames,
                     const char *model_filename,
                     const char *power_name,
                     const char *gnd_name,
                     CircuitSim ckt_sim,
                     const Scene *scene,
                     const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  if (!gates.empty())
    sta->writeGateSpice(gates, spice_filename, subckt_filename,
                        lib_subckt_filenames, model_filename,
                        power_name, gnd_name, ckt_sim, scene, min_max);
}

void
write_gate_gnuplot_cmd(ArcDcalcArgSeq gates,
                       PinSet plot_pins,
                       const char *spice_waveform_filename,
                       const char *gnuplot_filename,
                       const char *gnuplot_csv_filename,
                       Scene *scene,
                       const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  if (!gates.empty())
    sta->writeGateGnuplot(gates, plot_pins, spice_waveform_filename,
                          gnuplot_filename, gnuplot_csv_filename, scene, min_max);
}

%} // inline
