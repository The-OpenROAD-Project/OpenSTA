// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

%module delay_noise

%{

#include "Parasitics.hh"
#include "Sta.hh"
#include "dcalc/DelayNoise.hh"

%}

%inline %{

void
annotate_noise_delays_cmd(float min_coupling_sum_ratio,
                          float min_coupling_ratio,
                          const Scene *scene,
                          const MinMaxAll *min_maxs)
{
  Sta *sta = Sta::sta();
  sta->annotateNoiseDelays(min_coupling_sum_ratio, min_coupling_ratio,
                           scene, min_maxs);
}

void
report_noise_delays_cmd(size_t max_count,
                        float min_coupling_sum_ratio,
                        float min_coupling_ratio,
                        const Scene *scene,
                        const MinMaxAll *min_maxs,
                        int digits)
{
  Sta *sta = Sta::sta();
  sta->reportNoiseDelays(max_count, min_coupling_sum_ratio,
                         min_coupling_ratio,
                         scene, min_maxs, digits);
}

void
report_net_noise_delays_cmd(const Net *net,
                            const RiseFallBoth *drvr_rfs,
                            float min_coupling_ratio,
                            const Scene *scene,
                            const MinMaxAll *min_maxs,
                            int digits)
{
  Sta *sta = Sta::sta();
  sta->reportNoiseDelays(net, drvr_rfs, min_coupling_ratio, scene, min_maxs,
                         digits);
}

void
report_gate_noise_delays_cmd(ArcDcalcArgSeq gates,
                             const Scene *scene,
                             const MinMax *min_max,
                             int digits)
{
  Sta *sta = Sta::sta();
  if (!gates.empty())
    sta->reportNoiseDelays(gates, scene, min_max, digits);
}

void
report_victim_nets_cmd(float min_coupling_ratio,
                       const Scene *scene,
                       const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  sta->reportVictimNets(min_coupling_ratio, scene, min_max);
}

void
report_aggressors_cmd(const Net *victim,
                      float min_coupling_ratio,
                      const Scene *scene,
                      const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  sta->reportAggressors(victim, min_coupling_ratio, scene, min_max);
}

void
write_noise_delay_case_cmd(const Net *net,
                           const RiseFall *drvr_rf,
                           std::string_view file_basename,
                           float min_coupling_sum_ratio,
                           float min_coupling_ratio,
                           bool no_timestamp,
                           const Scene *scene,
                           const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  sta->writeNoiseDelayCase(net, drvr_rf, file_basename,
                           min_coupling_sum_ratio, min_coupling_ratio,
                           no_timestamp, scene, min_max);
}

////////////////////////////////////////////////////////////////
//
// Debugging accessors
//
////////////////////////////////////////////////////////////////

NetSeq
net_aggressors(const Net *net,
               float min_coupling_ratio,
               const Scene *scene,
               const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  DelayNoise *delay_noise = sta->delayNoise();
  return delay_noise->aggressorNetsSorted(net, min_coupling_ratio, scene, min_max);
}

ArcDcalcArgSeq
align_aggressors(ArcDcalcArgSeq gates,
                 const Scene *scene,
                 const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  DelayNoise *delay_noise = sta->delayNoise();
  sta->findDelays();
  delay_noise->alignAggressorsCmd(gates, scene, min_max);
  return gates;
}

ArcDcalcArgSeq
make_gate_args(const Net *net,
               const RiseFall *drvr_rf,
               float min_coupling_ratio,
               const Scene *scene,
               const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  DelayNoise *delay_noise = sta->delayNoise();
  ArcDcalcArgSeq gates = delay_noise->makeArcDcalcArgs(net, drvr_rf,
                                                       min_coupling_ratio,
                                                       scene, min_max);
  if (!gates.empty())
    ArcDcalcArg::sort(gates, sta->network());
  return gates;
}

ArcDcalcArgSeq
make_drvr_gate_args(ArcDcalcArg drvr,
                    ConstNetSeq aggressors,
                    const Scene *scene,
                    const MinMax *min_max)
{
  Sta *sta = Sta::sta();
  DelayNoise *delay_noise = sta->delayNoise();
  ArcDcalcArgSeq gates = delay_noise->makeArcDcalcArgs(drvr, aggressors,
                                                       scene, min_max);
  if (!gates.empty())
    ArcDcalcArg::sort(gates, sta->network());
  return gates;
}

ArcDcalcArg
make_net_gate_drvr(const Net *net,
                   const RiseFall *drvr_rf)
{
  Sta *sta = Sta::sta();
  DelayNoise *delay_noise = sta->delayNoise();
  sta->ensureGraph();
  ArcDcalcArg gate = delay_noise->makeNetGateDrvr(net, drvr_rf);
  return gate;
}

%} // inline
