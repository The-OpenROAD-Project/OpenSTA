// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "Machine.hh"
#include "DeratingFactors.hh"

namespace sta {

inline int
TimingDerateIndex(TimingDerateType type)
{
  return int(type);
}

DeratingFactors::DeratingFactors()
{
  clear();
}

void
DeratingFactors::setFactor(PathClkOrData clk_data,
			   const RiseFallBoth *rf,
			   const EarlyLate *early_late,
			   float factor)
{
  for (auto tr1 : rf->range())
    factors_[int(clk_data)].setValue(tr1, early_late, factor);
}

void
DeratingFactors::factor(PathClkOrData clk_data,
			const RiseFall *rf,
			const EarlyLate *early_late,
			float &factor,
			bool &exists) const
{
  factors_[int(clk_data)].value(rf, early_late, factor, exists);
}

void
DeratingFactors::clear()
{
  for (int clk_data = 0; clk_data < path_clk_or_data_count;clk_data++)
    factors_[int(clk_data)].clear();
}

void
DeratingFactors::isOneValue(const EarlyLate *early_late,
			    bool &is_one_value,
			    float &value) const
{
  bool is_one_value0, is_one_value1;
  float value0, value1;
  is_one_value0 = factors_[0].isOneValue(early_late, value0);
  is_one_value1 = factors_[1].isOneValue(early_late, value1);
  is_one_value = is_one_value0
    && is_one_value1
    && value0 == value1;
  value = value1;
}

void
DeratingFactors::isOneValue(PathClkOrData clk_data,
			    const EarlyLate *early_late,
			    bool &is_one_value,
			    float &value) const
{
  is_one_value = factors_[int(clk_data)].isOneValue(early_late, value);
}

bool
DeratingFactors::hasValue() const
{
  return factors_[0].hasValue()
    || factors_[1].hasValue();
}

////////////////////////////////////////////////////////////////

DeratingFactorsGlobal::DeratingFactorsGlobal()
{
  clear();
}

void
DeratingFactorsGlobal::setFactor(TimingDerateType type,
				 PathClkOrData clk_data,
				 const RiseFallBoth *rf,
				 const EarlyLate *early_late,
				 float factor)
{
  factors_[TimingDerateIndex(type)].setFactor(clk_data, rf, early_late, factor);
}

void
DeratingFactorsGlobal::factor(TimingDerateType type,
			      PathClkOrData clk_data,
			      const RiseFall *rf,
			      const EarlyLate *early_late,
			      float &factor,
			      bool &exists) const
{
  factors_[TimingDerateIndex(type)].factor(clk_data, rf, early_late, factor, exists);
}

void
DeratingFactorsGlobal::clear()
{
  for (int type = 0; type < timing_derate_type_count; type++)
    factors_[type].clear();
}

DeratingFactors *
DeratingFactorsGlobal::factors(TimingDerateType type)
{
  return &factors_[TimingDerateIndex(type)];
}

////////////////////////////////////////////////////////////////

DeratingFactorsCell::DeratingFactorsCell()
{
  clear();
}

void
DeratingFactorsCell::setFactor(TimingDerateType type,
			       PathClkOrData clk_data,
			       const RiseFallBoth *rf,
			       const EarlyLate *early_late,
			       float factor)
{
  factors_[TimingDerateIndex(type)].setFactor(clk_data, rf, early_late, factor);
}

void
DeratingFactorsCell::factor(TimingDerateType type,
			    PathClkOrData clk_data,
			    const RiseFall *rf,
			    const EarlyLate *early_late,
			    float &factor,
			    bool &exists) const
{
  factors_[TimingDerateIndex(type)].factor(clk_data, rf, early_late, factor, exists);
}

void
DeratingFactorsCell::clear()
{
  for (int type = 0; type < timing_derate_cell_type_count; type++)
    factors_[type].clear();
}

DeratingFactors *
DeratingFactorsCell::factors(TimingDerateType type)
{
  return &factors_[TimingDerateIndex(type)];
}

void
DeratingFactorsCell::isOneValue(const EarlyLate *early_late,
				bool &is_one_value,
				float &value) const
{
  bool is_one_value1, is_one_value2;
  float value1, value2;
  factors_[TimingDerateIndex(TimingDerateType::cell_delay)]
    .isOneValue(early_late, is_one_value1, value1);
  factors_[TimingDerateIndex(TimingDerateType::cell_check)]
    .isOneValue(early_late, is_one_value2, value2);
  is_one_value = is_one_value1
    && is_one_value2
    && value1 == value2;
  value = value1;
}

////////////////////////////////////////////////////////////////

DeratingFactorsNet::DeratingFactorsNet()
{
}

} // namespace
