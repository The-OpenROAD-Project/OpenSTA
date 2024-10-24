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

#include "power/SaifReader.hh"

#include <algorithm>
#include <cinttypes>

#include "Error.hh"
#include "Debug.hh"
#include "Report.hh"
#include "Network.hh"
#include "Sdc.hh"
#include "Power.hh"
#include "power/SaifReaderPvt.hh"
#include "Sta.hh"

extern int
SaifParse_parse();
extern int SaifParse_debug;

namespace sta {

using std::min;

SaifReader *saif_reader = nullptr;

bool
readSaif(const char *filename,
         const char *scope,
         Sta *sta)
{
  SaifReader reader(filename, scope, sta);
  saif_reader = &reader;
  bool success = reader.read();
  saif_reader = nullptr;
  return success;
}

SaifReader::SaifReader(const char *filename,
                       const char *scope,
                       Sta *sta) :
  StaState(sta),
  filename_(filename),
  scope_(scope),
  stream_(nullptr),
  line_(1),
  divider_('/'),
  escape_('\\'),
  timescale_(1.0E-9F),		// default units of ns
  duration_(0.0),
  clk_period_(0.0),
  in_scope_level_(0),
  power_(sta->power())
{
}

SaifReader::~SaifReader()
{
}

bool
SaifReader::read()
{
  // Use zlib to uncompress gzip'd files automagically.
  stream_ = gzopen(filename_, "rb");
  if (stream_) {
    clk_period_ = INF;
    for (Clock *clk : *sdc_->clocks())
      clk_period_ = min(static_cast<double>(clk->period()), clk_period_);

    saif_scope_.clear();
    in_scope_level_ = 0;
    annotated_pins_.clear();
    //::SaifParse_debug = 1;
    // yyparse returns 0 on success.
    bool success = (::SaifParse_parse() == 0);
    gzclose(stream_);
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
      saifError(180, "TIMESCALE units not us, ns, or ps.");
  }
  else
    saifError(181, "TIMESCALE multiplier not 1, 10, or 100.");
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
        saif_scope += network_->pathDivider();
      saif_scope += inst;
      first = false;
    }
    if (stringEq(saif_scope.c_str(), scope_))
      in_scope_level_ = saif_scope_.size();
  }
  else {
    // Inside annotation scope.
    Instance *parent = path_.empty() ? network_->topInstance() : path_.back();
    Instance *child = network_->findChild(parent, instance_name);
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
    Instance *parent = path_.empty() ? network_->topInstance() : path_.back();
    if (parent) {
      string unescaped_name = unescaped(net_name);
      const Pin *pin = sdc_network_->findPin(parent, unescaped_name.c_str());
      if (pin) {
        double t1 = durations[static_cast<int>(SaifState::T1)];
        float duty = t1 / duration_;
        double tc = durations[static_cast<int>(SaifState::TC)];
        float activity = tc / (duration_ * timescale_ / clk_period_);
        debugPrint(debug_, "read_saif", 2,
                   "%s duty %.0f / %" PRIu64 " = %.2f tc %.0f activity %.2f",
                   sdc_network_->pathName(pin),
                   t1,
                   duration_,
                   duty,
                   tc,
                   activity);
        power_->setUserActivity(pin, activity, duty, PwrActivityOrigin::saif);
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
    if (ch == escape_)
      unescaped += *(t+1);
    else
      // Just the normal noises.
      unescaped += ch;
  }
  debugPrint(debug_, "saif_name", 1, "token %s -> %s", token, unescaped.c_str());
  return unescaped;
}

void
SaifReader::incrLine()
{
  line_++;
}

void
SaifReader::getChars(char *buf,
		    size_t &result,
		    size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_nullptr
  else
    result = strlen(buf);
}

void
SaifReader::getChars(char *buf,
		    int &result,
		    size_t max_size)
{
  char *status = gzgets(stream_, buf, max_size);
  if (status == Z_NULL)
    result = 0;  // YY_nullptr
  else
    result = strlen(buf);
}

void
SaifReader::notSupported(const char *feature)
{
  saifError(193, "%s not supported.", feature);
}

void
SaifReader::saifWarn(int id,
                   const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileWarn(id, filename_, line_, fmt, args);
  va_end(args);
}

void
SaifReader::saifError(int id,
                    const char *fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  report_->vfileError(id, filename_, line_, fmt, args);
  va_end(args);
}

} // namespace

// Global namespace

void saifFlushBuffer();

int
SaifParse_error(const char *msg)
{
  saifFlushBuffer();
  sta::saif_reader->saifError(196, "%s.\n", msg);
  return 0;
}
