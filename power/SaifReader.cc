// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

#include "Error.hh"
#include "Debug.hh"
#include "Stats.hh"
#include "Report.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Sdc.hh"
#include "Power.hh"
#include "power/SaifReaderPvt.hh"
#include "power/SaifScanner.hh"
#include "Sta.hh"

namespace sta {

using std::min;

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
  divider_('/'),
  escape_('\\'),
  timescale_(1.0E-9F),		// default units of ns
  duration_(0.0),
  in_scope_level_(0),
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
    report_->reportLine("Annotated %zu pin activities.", annotated_pins_.size());
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
                         const char *units)
{
  if (multiplier == 1
      || multiplier == 10
      || multiplier == 100) {
    if (stringEq(units, "us"))
      timescale_ = multiplier * 1E-6;
    else if (stringEq(units, "ns"))
      timescale_ = multiplier * 1E-9;
    else if (stringEq(units, "ps"))
      timescale_ = multiplier * 1E-12;
    else if (stringEq(units, "fs"))
      timescale_ = multiplier * 1E-15;
    else
      report_->error(180, "SAIF TIMESCALE units not us, ns, or ps.");
  }
  else
    report_->error(181, "SAIF TIMESCALE multiplier not 1, 10, or 100.");
  stringDelete(units);
}

void
SaifReader::setDuration(uint64_t duration)
{
  duration_ = duration;
}

void
SaifReader::instancePush(const char *instance_name)
{
  if (in_scope_level_ == 0) {
    // Check for a match to the annotation scope.
    saif_scope_.push_back(instance_name);

    string saif_scope;
    bool first = true;
    for (string &inst : saif_scope_) {
      if (!first)
        saif_scope += sdc_network_->pathDivider();
      saif_scope += inst;
      first = false;
    }
    if (stringEq(saif_scope.c_str(), scope_))
      in_scope_level_ = saif_scope_.size();
  }
  else {
    // Inside annotation scope.
    Instance *parent = path_.empty() ? sdc_network_->topInstance() : path_.back();
    Instance *child = sdc_network_->findChild(parent, instance_name);
    path_.push_back(child);
  }
  stringDelete(instance_name);
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
SaifReader::setNetDurations(const char *net_name,
                            SaifStateDurations &durations)
{
  if (in_scope_level_ > 0) {
    Instance *parent = path_.empty() ? sdc_network_->topInstance() : path_.back();
    if (parent) {
      string unescaped_name = unescaped(net_name);
      const Pin *pin = sdc_network_->findPin(parent, unescaped_name.c_str());
      if (pin
          && !sdc_network_->isHierarchical(pin)
          && !sdc_network_->direction(pin)->isInternal()) {
        double t1 = durations[static_cast<int>(SaifState::T1)];
        float duty = t1 / duration_;
        double tc = durations[static_cast<int>(SaifState::TC)];
        float density = tc / (duration_ * timescale_);
        debugPrint(debug_, "read_saif", 2,
                   "%s duty %.0f / %" PRIu64 " = %.2f tc %.0f density %.2f",
                   sdc_network_->pathName(pin),
                   t1,
                   duration_,
                   duty,
                   tc,
                   density);
        power_->setUserActivity(pin, density, duty, PwrActivityOrigin::saif);
        annotated_pins_.insert(pin);
      }
    }
  }
  stringDelete(net_name);
}

string
SaifReader::unescaped(const char *token)
{
  string unescaped;
  for (const char *t = token; *t; t++) {
    char ch = *t;
    if (ch != escape_)
      // Just the normal noises.
      unescaped += ch;
  }
  debugPrint(debug_, "saif_name", 1, "token %s -> %s", token, unescaped.c_str());
  return unescaped;
}

////////////////////////////////////////////////////////////////

SaifScanner::SaifScanner(std::istream *stream,
                         const string &filename,
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
  report_->fileError(1868, filename_.c_str(), lineno(), "%s", msg);
}

} // namespace
