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

#pragma once

#include "MinMax.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "Transition.hh"
#include "GraphClass.hh"
#include "Delay.hh"
#include "StaState.hh"
#include "SearchClass.hh"

namespace sta {

class DcalcAnalysisPt;

class Path
{
public:
  Path();
  Path(Path *path);
  Path(Vertex *vertex,
       Tag *tag,
       const StaState *sta);
  Path(Vertex *vertex,
       Tag *tag,
       Arrival arrival,
       Path *prev_path,
       Edge *prev_edge,
       TimingArc *prev_arc,
       const StaState *sta);
  Path(Vertex *vertex,
       Tag *tag,
       Arrival arrival,
       Path *prev_path,
       Edge *prev_edge,
       TimingArc *prev_arc,
       bool is_enum,
       const StaState *sta);
  ~Path();
  std::string to_string(const StaState *sta) const;
  bool isNull() const;
  // prev_path null 
  void init(Vertex *vertex,
            Arrival arrival,
            const StaState *sta);
  void init(Vertex *vertex,
            Tag *tag,
            Arrival arrival,
            Path *prev_path,
            Edge *prev_edge,
            TimingArc *prev_arc,
            const StaState *sta);
  void init(Vertex *vertex,
            Tag *tag,
            const StaState *sta);
  void init(Vertex *vertex,
            Tag *tag,
            Arrival arrival,
            const StaState *sta);

  Vertex *vertex(const StaState *sta) const;
  VertexId vertexId(const StaState *sta) const;
  Pin *pin(const StaState *sta) const;
  Tag *tag(const StaState *sta) const;
  TagIndex tagIndex(const StaState *sta) const;
  void setTag(Tag *tag);
  size_t pathIndex(const StaState *sta) const;
  ClkInfo *clkInfo(const StaState *sta) const;
  const ClockEdge *clkEdge(const StaState *sta) const;
  const Clock *clock(const StaState *sta) const;
  bool isClock(const StaState *sta) const;
  const RiseFall *transition(const StaState *sta) const;
  int rfIndex(const StaState *sta) const;
  const MinMax *minMax(const StaState *sta) const;
  PathAnalysisPt *pathAnalysisPt(const StaState *sta) const;
  PathAPIndex pathAnalysisPtIndex(const StaState *sta) const;
  DcalcAnalysisPt *dcalcAnalysisPt(const StaState *sta) const;
  Arrival &arrival() { return arrival_; }
  const Arrival &arrival() const { return arrival_; }
  void setArrival(Arrival arrival);
  Required &required() { return required_; }
  const Required &required() const {return required_; }
  void setRequired(const Required &required);
  Slack slack(const StaState *sta) const;
  Slew slew(const StaState *sta) const;
  // This takes the same time as prevPath and prevArc combined.
  Path *prevPath() const;
  void setPrevPath(Path *prev_path);
  void clearPrevPath(const StaState *sta);
  TimingArc *prevArc(const StaState *sta) const;
  Edge *prevEdge(const StaState *sta) const;
  Vertex *prevVertex(const StaState *sta) const;
  void setPrevEdgeArc(Edge *prev_edge,
                      TimingArc *prev_arc,
                      const StaState *sta);
  bool isEnum() const { return is_enum_; }
  void setIsEnum(bool is_enum);
  void checkPrevPath(const StaState *sta) const;

  static Path *vertexPath(const Path *path,
                          const StaState *sta);
  static Path *vertexPath(const Path &path,
                          const StaState *sta);
  static Path *vertexPath(const Vertex *vertex,
                          Tag *tag,
                          const StaState *sta);

  static bool less(const Path *path1,
		   const Path *path2,
		   const StaState *sta);
  static int cmp(const Path *path1,
		 const Path *path2,
		 const StaState *sta);
  // Compare all path attributes (vertex, transition, tag, analysis point).
  static bool equal(const Path *path1,
		    const Path *path2,
		    const StaState *sta);
  // Compare pin name and transition and source clock edge.
  static int cmpPinTrClk(const Path *path1,
			 const Path *path2,
			 const StaState *sta);
  // Compare source clock edge.
  static int cmpClk(const Path *path1,
		    const Path *path2,
		    const StaState *sta);
  // Compare vertex, transition, path ap and tag without crpr clk pin.
  static int cmpNoCrpr(const Path *path1,
		       const Path *path2,
		       const StaState *sta);
  // Search back on each path until finding a difference.
  static int cmpAll(const Path *path1,
		    const Path *path2,
		    const StaState *sta);
  static bool lessAll(const Path *path1,
		      const Path *path2,
		      const StaState *sta);

protected:
  Path *prev_path_;
  Arrival arrival_;
  Required required_;
  union {
    VertexId vertex_id_;
    EdgeId prev_edge_id_;
  };
  TagIndex tag_index_:tag_index_bit_count;
  bool is_enum_:1;
  unsigned prev_arc_idx_:2;
};

// Compare all path attributes (vertex, transition, tag, analysis point).
class PathLess
{
public:
  explicit PathLess(const StaState *sta);
  bool operator()(const Path *path1,
		  const Path *path2) const;

protected:
  const StaState *sta_;
};

// Iterator for paths on a vertex.
class VertexPathIterator : public Iterator<Path*>
{
public:
  // Iterate over all vertex paths.
  VertexPathIterator(Vertex *vertex,
		     const StaState *sta);
  // Iterate over vertex paths with the same transition and
  // analysis pt but different tags.
  VertexPathIterator(Vertex *vertex,
		     const RiseFall *rf,
		     const PathAnalysisPt *path_ap,
		     const StaState *sta);
  // Iterate over vertex paths with the same transition and
  // analysis pt min/max but different tags.
  VertexPathIterator(Vertex *vertex,
		     const RiseFall *rf,
		     const MinMax *min_max,
		     const StaState *sta);
  VertexPathIterator(Vertex *vertex,
                     const RiseFall *rf,
                     const PathAnalysisPt *path_ap,
                     const MinMax *min_max,
                     const StaState *sta);
  virtual ~VertexPathIterator();
  virtual bool hasNext();
  virtual Path *next();

private:
  void findNext();

  const Search *search_;
  bool filtered_;
  const RiseFall *rf_;
  const PathAnalysisPt *path_ap_;
  const MinMax *min_max_;
  Path *paths_;
  size_t path_count_;
  size_t path_index_;
  Path *next_;
};

} // namespace
