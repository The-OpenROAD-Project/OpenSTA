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

#pragma once

namespace sta {

class Power;

enum class PwrActivityOrigin
{
 global,
 input,
 user,
 vcd,
 saif,
 propagated,
 clock,
 constant,
 defaulted,
 unknown
};

class PwrActivity
{
public:
  PwrActivity();
  PwrActivity(float density,
	      float duty,
	      PwrActivityOrigin origin);
  float density() const { return density_; }
  void setDensity(float density);
  float duty() const { return duty_; }
  void setDuty(float duty);
  PwrActivityOrigin origin() const { return origin_; }
  void setOrigin(PwrActivityOrigin origin);
  const char *originName() const;
  void set(float density,
	   float duty,
	   PwrActivityOrigin origin);
  bool isSet() const;

private:
  void check();

  float density_;               // transitions / second
  float duty_;                  // probability signal is high
  PwrActivityOrigin origin_;

  static constexpr float min_density = 1E-10;
};

class PowerResult
{
public:
  PowerResult();
  void clear();
  float internal() const { return internal_; }
  float switching() const { return switching_; }
  float leakage() const { return leakage_; }
  float total() const;
  void incr(PowerResult &result);
  void incrInternal(float pwr);
  void incrSwitching(float pwr);
  void incrLeakage(float pwr);

private:
  float internal_;
  float switching_;
  float leakage_;
};

} // namespace
