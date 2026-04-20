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

#include "DeratingFactors.hh"

namespace sta {

inline size_t
index(TimingDerateType type)
{
  return static_cast<size_t>(type);
}

inline size_t
index(TimingDerateCellType type)
{
  return static_cast<size_t>(type);
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
  for (const RiseFall *rf1 : rf->range())
    factors_[static_cast<int>(clk_data)].setValue(rf1, early_late, factor);
}

void
DeratingFactors::factor(PathClkOrData clk_data,
                        const RiseFall *rf,
                        const EarlyLate *early_late,
                        float &factor,
                        bool &exists) const
{
  factors_[static_cast<int>(clk_data)].value(rf, early_late, factor, exists);
}

void
DeratingFactors::clear()
{
  for (RiseFallMinMax &factors : factors_)
    factors.clear();
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
  is_one_value = factors_[static_cast<int>(clk_data)].isOneValue(early_late, value);
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
  factors_[index(type)].setFactor(clk_data, rf, early_late, factor);
}

void
DeratingFactorsGlobal::factor(TimingDerateType type,
                              PathClkOrData clk_data,
                              const RiseFall *rf,
                              const EarlyLate *early_late,
                              float &factor,
                              bool &exists) const
{
  factors_[index(type)].factor(clk_data, rf, early_late, factor, exists);
}

void
DeratingFactorsGlobal::factor(TimingDerateCellType type,
                              PathClkOrData clk_data,
                              const RiseFall *rf,
                              const EarlyLate *early_late,
                              float &factor,
                              bool &exists) const
{
  factors_[index(type)].factor(clk_data, rf, early_late, factor, exists);
}

void
DeratingFactorsGlobal::clear()
{
  for (DeratingFactors &factors : factors_)
    factors.clear();
}

DeratingFactors *
DeratingFactorsGlobal::factors(TimingDerateType type)
{
  return &factors_[index(type)];
}

////////////////////////////////////////////////////////////////

DeratingFactorsCell::DeratingFactorsCell()
{
  clear();
}

void
DeratingFactorsCell::setFactor(TimingDerateCellType type,
                               PathClkOrData clk_data,
                               const RiseFallBoth *rf,
                               const EarlyLate *early_late,
                               float factor)
{
  factors_[index(type)].setFactor(clk_data, rf, early_late, factor);
}

void
DeratingFactorsCell::factor(TimingDerateCellType type,
                            PathClkOrData clk_data,
                            const RiseFall *rf,
                            const EarlyLate *early_late,
                            float &factor,
                            bool &exists) const
{
  factors_[index(type)].factor(clk_data, rf, early_late, factor, exists);
}

void
DeratingFactorsCell::clear()
{
  for (DeratingFactors &factors : factors_)
    factors.clear();
}

DeratingFactors *
DeratingFactorsCell::factors(TimingDerateCellType type)
{
  return &factors_[index(type)];
}

void
DeratingFactorsCell::isOneValue(const EarlyLate *early_late,
                                bool &is_one_value,
                                float &value) const
{
  bool is_one_value1, is_one_value2;
  float value1, value2;
  factors_[index(TimingDerateType::cell_delay)]
    .isOneValue(early_late, is_one_value1, value1);
  factors_[index(TimingDerateType::cell_check)]
    .isOneValue(early_late, is_one_value2, value2);
  is_one_value = is_one_value1
    && is_one_value2
    && value1 == value2;
  value = value1;
}

} // namespace sta
