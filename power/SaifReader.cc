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

#include "power/SaifReader.hh"

#include <algorithm>
#include <cinttypes>
#include <string>
#include <utility>

#include "Debug.hh"
#include "Error.hh"
#include "Liberty.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Power.hh"
#include "Report.hh"
#include "Sdc.hh"
#include "Sta.hh"
#include "Stats.hh"
#include "power/SaifReaderPvt.hh"
#include "power/SaifScanner.hh"

namespace sta {

bool
readSaif(const char *filename,
         const char *scope,
         Sta *sta)
{
  SaifReader reader(filename, scope, sta);
  bool success = reader.read();
  return success;
}

SaifReader::SaifReader(const char *filename,
                       const char *scope,
                       Sta *sta) :
  StaState(sta),
  filename_(filename),
  scope_(scope),
  power_(sta->power())
{
}

bool
SaifReader::read()
{
  gzstream::igzstream stream(filename_);
  if (stream.is_open()) {
    Stats stats(debug_, report_);
    SaifScanner scanner(&stream, filename_, this, report_);
    SaifParse parser(&scanner, this);
    // yyparse returns 0 on success.
    bool success = (parser.parse() == 0);
    report_->report("Annotated {} pin activities.", annotated_pins_.size());
    return success;
  }
  else
    throw FileNotReadable(filename_);
}

void
SaifReader::setDivider(char divider)
{
  divider_ = divider;
}

void
SaifReader::setTimescale(uint64_t multiplier,
                         std::string &&units)
{
  if (multiplier == 1 || multiplier == 10 || multiplier == 100) {
    if (stringEqual(units, "us"))
      timescale_ = multiplier * 1E-6;
    else if (stringEqual(units, "ns"))
      timescale_ = multiplier * 1E-9;
    else if (stringEqual(units, "ps"))
      timescale_ = multiplier * 1E-12;
    else if (stringEqual(units, "fs"))
      timescale_ = multiplier * 1E-15;
    else
      report_->error(1861, "SAIF TIMESCALE units not us, ns, or ps.");
  }
  else
    report_->error(1862, "SAIF TIMESCALE multiplier not 1, 10, or 100.");
}

void
SaifReader::setDuration(uint64_t duration)
{
  duration_ = duration;
}

void
SaifReader::instancePush(std::string &&instance_name)
{
  if (in_scope_level_ == 0) {
    // Check for a match to the annotation scope.
    saif_scope_.push_back(std::move(instance_name));

    std::string saif_scope;
    bool first = true;
    for (std::string &inst : saif_scope_) {
      if (!first)
        saif_scope += sdc_network_->pathDivider();
      saif_scope += inst;
      first = false;
    }
    if (saif_scope == scope_)
      in_scope_level_ = saif_scope_.size();
  }
  else {
    // Inside annotation scope.
    Instance *parent = path_.empty() ? sdc_network_->topInstance() : path_.back();
    Instance *child = parent
      ? sdc_network_->findChild(parent, instance_name)
      : nullptr;
    path_.push_back(child);
  }
}

void
SaifReader::instancePop()
{
  if (in_scope_level_ == 0)
    saif_scope_.pop_back();
  if (!path_.empty())
    path_.pop_back();
  if (saif_scope_.size() < in_scope_level_)
    in_scope_level_ = 0;
}

void
SaifReader::setNetDurations(std::string &&net_name,
                            const SaifStateDurations &durations)
{
  if (in_scope_level_ > 0) {
    Instance *parent = path_.empty() ? sdc_network_->topInstance() : path_.back();
    if (parent) {
      std::string unescaped_name = unescaped(net_name);
      const Pin *pin = sdc_network_->findPin(parent, unescaped_name);
      LibertyPort *liberty_port = pin ? sdc_network_->libertyPort(pin) : nullptr;
      if (pin && !sdc_network_->isHierarchical(pin)
          && !sdc_network_->direction(pin)->isInternal()
          && !(liberty_port && liberty_port->isPwrGnd())) {
        double t1 = durations[static_cast<int>(SaifState::T1)];
        float duty = t1 / duration_;
        double tc = durations[static_cast<int>(SaifState::TC)];
        float density = tc / (duration_ * timescale_);
        debugPrint(debug_, "read_saif", 2,
                   "{} duty {:.0f} / {} = {:.2f} tc {:.0f} density {:.2f}",
                   sdc_network_->pathName(pin), t1, duration_, duty, tc, density);
        power_->setUserActivity(pin, density, duty, PwrActivityOrigin::saif);
        annotated_pins_.insert(pin);
      }
    }
  }
}

std::string
SaifReader::unescaped(const std::string &token)
{
  std::string unescaped;
  for (char ch : token) {
    if (ch != escape_)
      unescaped += ch;
  }
  debugPrint(debug_, "saif_name", 1, "token {} -> {}", token, unescaped);
  return unescaped;
}

////////////////////////////////////////////////////////////////

SaifScanner::SaifScanner(std::istream *stream,
                         const std::string &filename,
                         SaifReader *reader,
                         Report *report) :
  yyFlexLexer(stream),
  filename_(filename),
  reader_(reader),
  report_(report)
{
}

void
SaifScanner::error(const char *msg)
{
  report_->fileError(1860, filename_, lineno(), "{}", msg);
}

}  // namespace sta
