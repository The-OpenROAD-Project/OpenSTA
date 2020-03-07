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

#pragma once

#include "Vector.hh"
#include "Set.hh"
#include "Map.hh"
#include "UnorderedMap.hh"
#include "StringSet.hh"
#include "Delay.hh"
#include "NetworkClass.hh"
#include "GraphClass.hh"

namespace sta {

class Search;
class Path;
class PathRep;
class PathVertex;
class PathVertexRep;
class PathRef;
class PathEnumed;
class PathEnd;
class PathGroup;
class Tag;
class TagIndexLess;
class TagMatchLess;
class TagLess;
class TagHash;
class TagEqual;
class TagGroup;
class TagGroupHash;
class TagGroupEqual;
class ClkInfo;
class ClkInfoHash;
class ClkInfoEqual;
class VertexPathIterator;
class PathAnalysisPt;
class PathAnalysisPtIterator;
class MinPulseWidthCheck;
class MinPeriodCheck;
class MaxSkewCheck;
class CharPtrLess;

// Tag compare using tag matching (tagMatch) critera.
class TagMatchLess
{
public:
  explicit TagMatchLess(bool match_crpr_clk_pin,
			const StaState *sta);
  bool operator()(const Tag *tag1,
		  const Tag *tag2) const;

protected:
  bool match_crpr_clk_pin_;
  const StaState *sta_;
};

class TagMatchHash
{
public:
  TagMatchHash(bool match_crpr_clk_pin,
	       const StaState *sta);
  size_t operator()(const Tag *tag) const;

protected:
  bool match_crpr_clk_pin_;
  const StaState *sta_;
};

class TagMatchEqual
{
public:
  TagMatchEqual(bool match_crpr_clk_pin,
		const StaState *sta);
  bool operator()(const Tag *tag1, 
		  const Tag *tag2) const;

protected:
  bool match_crpr_clk_pin_;
  const StaState *sta_;
};

typedef int PathAPIndex;
typedef int TagIndex;
typedef int TagGroupTagIndex;
typedef Vector<Tag*> TagSeq;
typedef Vector<MinPulseWidthCheck*> MinPulseWidthCheckSeq;
typedef Vector<MinPeriodCheck*> MinPeriodCheckSeq;
typedef Vector<MaxSkewCheck*> MaxSkewCheckSeq;
typedef StringSet PathGroupNameSet;
typedef Vector<PathEnd*> PathEndSeq;
typedef Vector<Arrival> ArrivalSeq;
typedef Map<Vertex*, int> VertexPathCountMap;
typedef UnorderedMap<Tag*, int, TagMatchHash, TagMatchEqual> ArrivalMap;
typedef Vector<PathVertex> PathVertexSeq;
typedef Vector<Slack> SlackSeq;
typedef Delay Crpr;
typedef Vector<PathRef> PathRefSeq;

enum class ReportPathFormat { full,
			      full_clock,
			      full_clock_expanded,
			      shorter,
			      endpoint,
			      summary,
			      slack_only
};

static const int tag_index_bits = 24;
static const int tag_index_max = (1 << tag_index_bits) - 1;
static const int tag_index_null = tag_index_max;
static const int path_ap_index_bit_count = 4;

} // namespace
