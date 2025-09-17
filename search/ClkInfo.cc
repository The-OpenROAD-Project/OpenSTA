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

#include "ClkInfo.hh"

#include <functional>

#include "Units.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Sdc.hh"
#include "Corner.hh"
#include "Search.hh"
#include "Tag.hh"
#include "PathAnalysisPt.hh"

namespace sta {

ClkInfo::ClkInfo(const ClockEdge *clk_edge,
		 const Pin *clk_src,
		 bool is_propagated,
                 const Pin *gen_clk_src,
		 bool is_gen_clk_src_path,
		 const RiseFall *pulse_clk_sense,
		 Arrival insertion,
		 float latency,
		 ClockUncertainties *uncertainties,
                 PathAPIndex path_ap_index,
		 const Path *crpr_clk_path,
		 const StaState *sta) :
  clk_edge_(clk_edge),
  clk_src_(clk_src),
  gen_clk_src_(gen_clk_src),
  crpr_clk_path_(is_propagated ? crpr_clk_path : nullptr),
  uncertainties_(uncertainties),
  insertion_(insertion),
  latency_(latency),
  is_propagated_(is_propagated),
  is_gen_clk_src_path_(is_gen_clk_src_path),
  crpr_path_refs_filter_(crpr_clk_path ? crpr_clk_path->tag(sta)->isFilter() : false),
  is_pulse_clk_(pulse_clk_sense != nullptr),
  pulse_clk_sense_(pulse_clk_sense ? pulse_clk_sense->index() : 0),
  path_ap_index_(path_ap_index)
{
  findHash(sta);
}

ClkInfo::~ClkInfo()
{
}

void
ClkInfo::findHash(const StaState *sta)
{
  hash_ = hash_init_value;
  if (clk_edge_)
    hashIncr(hash_, clk_edge_->index());

  const Network *network = sta->network();
  if (clk_src_)
    hashIncr(hash_, network->vertexId(clk_src_));
  if (gen_clk_src_)
    hashIncr(hash_, network->vertexId(gen_clk_src_));
  if (crpr_clk_path_.isNull())
    hashIncr(hash_, vertex_id_null);
  else {
    hashIncr(hash_, crpr_clk_path_.vertexId(sta));
    hashIncr(hash_, crpr_clk_path_.tag(sta)->hash(false, sta));
  }

  std::hash<float> hash_float;
  if (uncertainties_) {
    float uncertainty;
    bool exists;
    uncertainties_->value(MinMax::min(), uncertainty, exists);
    if (exists)
      hashIncr(hash_, hash_float(uncertainty));
    uncertainties_->value(MinMax::max(), uncertainty, exists);
    if (exists)
      hashIncr(hash_, hash_float(uncertainty));
  }
  hashIncr(hash_, hash_float(latency_));
  hashIncr(hash_, hash_float(delayAsFloat(insertion_)));
  hashIncr(hash_, is_propagated_);
  hashIncr(hash_, is_gen_clk_src_path_);
  hashIncr(hash_, is_pulse_clk_);
  hashIncr(hash_, pulse_clk_sense_);
  hashIncr(hash_, path_ap_index_);
}

VertexId
ClkInfo::crprClkVertexId(const StaState *sta) const
{
  return crpr_clk_path_.vertexId(sta);
}

Path *
ClkInfo::crprClkPath(const StaState *sta)
{
  return Path::vertexPath(crpr_clk_path_, sta);
}

const Path *
ClkInfo::crprClkPath(const StaState *sta) const
{
  if (crpr_clk_path_.isNull())
    return nullptr;
  else
    return Path::vertexPath(crpr_clk_path_, sta);
}

const Path *
ClkInfo::crprClkPathRaw() const
{
  if (crpr_clk_path_.isNull())
    return nullptr;
  else
    return &crpr_clk_path_;
}

std::string
ClkInfo::to_string(const StaState *sta) const
{
  Network *network = sta->network();
  Corners *corners = sta->corners();
  std::string result;

  PathAnalysisPt *path_ap = corners->findPathAnalysisPt(path_ap_index_);
  result += path_ap->pathMinMax()->to_string();
  result += "/";
  result += std::to_string(path_ap_index_);

  result += " ";
  if (clk_edge_)
    result += clk_edge_->name();
  else
    result += "unclocked";

  if (clk_src_) {
    result += " clk_src ";
    result += network->pathName(clk_src_);
  }

  if (!crpr_clk_path_.isNull()) {
    const Pin *crpr_clk_pin = crpr_clk_path_.vertex(sta)->pin();
    result += " crpr ";
    result += network->pathName(crpr_clk_pin);
    result += "/";
    result += std::to_string(crpr_clk_path_.tag(sta)->index());
  }

  if (is_gen_clk_src_path_)
    result += " genclk";
  if (gen_clk_src_) {
    result += " ";
    result += network->pathName(gen_clk_src_);
  }

  if (insertion_ > 0.0) {
    result += " insert";
    result += std::to_string(insertion_);
  }

  if (uncertainties_) {
    result += " uncertain ";
    float uncertainty;
    bool exists;
    uncertainties_->value(MinMax::min(), uncertainty, exists);
    if (exists)
      result += sta->units()->timeUnit()->asString(uncertainty);
    uncertainties_->value(MinMax::max(), uncertainty, exists);
    if (exists) {
      result += ":";
      result += sta->units()->timeUnit()->asString(uncertainty);
    }
  }
  return result;
}

const Clock *
ClkInfo::clock() const
{
  if (clk_edge_)
    return clk_edge_->clock();
  else
    return nullptr;
}

const RiseFall *
ClkInfo::pulseClkSense() const
{
  if (is_pulse_clk_)
    return RiseFall::find(pulse_clk_sense_);
  else
    return nullptr;
}

////////////////////////////////////////////////////////////////

size_t
ClkInfoHash::operator()(const ClkInfo *clk_info) const
{
  return clk_info->hash();
}

////////////////////////////////////////////////////////////////

ClkInfoEqual::ClkInfoEqual(const StaState *sta) :
  sta_(sta)
{
}

bool
ClkInfoEqual::operator()(const ClkInfo *clk_info1,
			 const ClkInfo *clk_info2) const
{
  return ClkInfo::equal(clk_info1, clk_info2, sta_);
}

bool
ClkInfo::equal(const ClkInfo *clk_info1,
	       const ClkInfo *clk_info2,
	       const StaState *sta)
{
  return ClkInfo::cmp(clk_info1, clk_info2, sta) == 0;
}

////////////////////////////////////////////////////////////////

ClkInfoLess::ClkInfoLess(const StaState *sta) :
  sta_(sta)
{
}

bool
ClkInfoLess::operator()(const ClkInfo *clk_info1,
			const ClkInfo *clk_info2) const
{
  return ClkInfo::cmp(clk_info1, clk_info2, sta_) < 0;
}

int
ClkInfo::cmp(const ClkInfo *clk_info1,
	     const ClkInfo *clk_info2,
	     const StaState *sta)
{
  const ClockEdge *clk_edge1 = clk_info1->clkEdge();
  const ClockEdge *clk_edge2 = clk_info2->clkEdge();
  int edge_index1 = clk_edge1 ? clk_edge1->index() : -1;
  int edge_index2 = clk_edge2 ? clk_edge2->index() : -1;
  if (edge_index1 < edge_index2)
    return -1;
  if (edge_index1 > edge_index2)
    return 1;

  PathAPIndex path_ap_index1 = clk_info1->pathAPIndex();
  PathAPIndex path_ap_index2 = clk_info2->pathAPIndex();
  if (path_ap_index1 < path_ap_index2)
    return -1;
  if (path_ap_index1 > path_ap_index2)
    return 1;

  const Network *network = sta->network();
  const Pin *clk_src1 = clk_info1->clkSrc();
  const Pin *clk_src2 = clk_info2->clkSrc();
  int clk_src_id1 = clk_src1 ? network->id(clk_src1) : -1;
  int clk_src_id2 = clk_src2 ? network->id(clk_src2) : -1;
  if (clk_src_id1 < clk_src_id2)
    return -1;
  if (clk_src_id1 > clk_src_id2)
    return 1;

  const Pin *gen_clk_src1 = clk_info1->genClkSrc();
  const Pin *gen_clk_src2 = clk_info2->genClkSrc();
  int gen_clk_src_id1 = gen_clk_src1 ? network->id(gen_clk_src1) : -1;
  int gen_clk_src_id2 = gen_clk_src2 ? network->id(gen_clk_src2) : -1;
  if (gen_clk_src_id1 < gen_clk_src_id2)
    return -1;
  if (gen_clk_src_id1 > gen_clk_src_id2)
    return 1;

  bool crpr_on = sta->crprActive();
  if (crpr_on) {
    const Path *crpr_path1 = clk_info1->crprClkPathRaw();
    const Path *crpr_path2 = clk_info2->crprClkPathRaw();
    int path_cmp = Path::cmp(crpr_path1, crpr_path2, sta);
    if (path_cmp != 0)
      return path_cmp;
  }

  const ClockUncertainties *uncertainties1 = clk_info1->uncertainties();
  const ClockUncertainties *uncertainties2 = clk_info2->uncertainties();
  if (uncertainties1 == nullptr  && uncertainties2)
    return -1;
  if (uncertainties1  && uncertainties2 == nullptr)
    return 1;
  if (uncertainties1 && uncertainties2) {
    int uncertain_cmp = ClockUncertainties::cmp(uncertainties1, uncertainties2);
    if (uncertain_cmp != 0)
      return uncertain_cmp;
  }
  const Arrival &insert1 = clk_info1->insertion();
  const Arrival &insert2 = clk_info2->insertion();
  if (delayLess(insert1, insert2, sta))
    return -1;
  if (delayGreater(insert1, insert2, sta))
    return 1;

  float latency1 = clk_info1->latency();
  float latency2 = clk_info2->latency();
  if (latency1 < latency2)
    return -1;
  if (latency1 > latency2)
    return 1;

  bool is_propagated1 = clk_info1->isPropagated();
  bool is_propagated2 = clk_info2->isPropagated();
  if (!is_propagated1 && is_propagated2)
    return -1;
  if (is_propagated1 && !is_propagated2)
    return 1;

  bool is_gen_clk_src_path1 = clk_info1->isGenClkSrcPath();
  bool is_gen_clk_src_path2 = clk_info2->isGenClkSrcPath();
  if (!is_gen_clk_src_path1 && is_gen_clk_src_path2)
    return -1;
  if (is_gen_clk_src_path1 && !is_gen_clk_src_path2)
    return 1;

  bool is_pulse_clk1 = clk_info1->isPulseClk();
  bool is_pulse_clk2 = clk_info2->isPulseClk();
  if (!is_pulse_clk1 && is_pulse_clk2)
    return -1;
  if (is_pulse_clk1 && !is_pulse_clk2)
    return 1;

  int pulse_clk_sense_index1 = clk_info1->pulseClkSenseRfIndex();
  int pulse_clk_sense_index2 = clk_info2->pulseClkSenseRfIndex();
  if (pulse_clk_sense_index1 < pulse_clk_sense_index2)
    return -1;
  if (pulse_clk_sense_index1 > pulse_clk_sense_index2)
    return 1;
  else
    return 0;
}

} // namespace
