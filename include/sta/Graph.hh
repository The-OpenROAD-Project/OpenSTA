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

#include <mutex>
#include <atomic>

#include "Iterator.hh"
#include "Map.hh"
#include "Vector.hh"
#include "ObjectTable.hh"
#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "Delay.hh"
#include "GraphClass.hh"
#include "VertexId.hh"
#include "Path.hh"
#include "StaState.hh"

namespace sta {

class MinMax;
class Sdc;

enum class LevelColor { white, gray, black };

typedef ObjectTable<Vertex> VertexTable;
typedef ObjectTable<Edge> EdgeTable;
typedef Map<const Pin*, Vertex*> PinVertexMap;
typedef Iterator<Edge*> VertexEdgeIterator;
typedef Map<const Pin*, float*, PinIdLess> PeriodCheckAnnotations;
typedef ObjectId EdgeId;

static constexpr EdgeId edge_id_null = object_id_null;
static constexpr ObjectIdx edge_idx_null = object_id_null;
static constexpr ObjectIdx vertex_idx_null = object_idx_null;

// The graph acts as a BUILDER for the graph vertices and edges.
class Graph : public StaState
{
public:
  // slew_rf_count is
  //  0 no slews
  //  1 one slew for rise/fall
  //  2 rise/fall slews
  // ap_count is the dcalc analysis point count.
  Graph(StaState *sta,
	int slew_rf_count,
	DcalcAPIndex ap_count);
  void makeGraph();
  ~Graph();

  // Number of arc delays and slews from sdf or delay calculation.
  void setDelayCount(DcalcAPIndex ap_count);
  size_t slewCount();

  // Vertex functions.
  // Bidirect pins have two vertices.
  Vertex *vertex(VertexId vertex_id) const;
  VertexId id(const Vertex *vertex) const;
  void makePinVertices(Pin *pin);
  void makePinVertices(Pin *pin,
		       Vertex *&vertex,
		       Vertex *&bidir_drvr_vertex);
  // Both vertices for bidirects.
  void pinVertices(const Pin *pin,
		   // Return values.
		   Vertex *&vertex,
		   Vertex *&bidirect_drvr_vertex) const;
  // Driver vertex for bidirects.
  Vertex *pinDrvrVertex(const Pin *pin) const;
  // Load vertex for bidirects.
  Vertex *pinLoadVertex(const Pin *pin) const;
  void deleteVertex(Vertex *vertex);
  bool hasFaninOne(Vertex *vertex) const;
  VertexId vertexCount() { return vertices_->size(); }
  Path *makePaths(Vertex *vertex,
                  uint32_t count);
  Path *paths(const Vertex *vertex) const;
  void deletePaths(Vertex *vertex);

  // Reported slew are the same as those in the liberty tables.
  //  reported_slews = measured_slews / slew_derate_from_library
  // Measured slews are between slew_lower_threshold and slew_upper_threshold.
  const Slew &slew(const Vertex *vertex,
                   const RiseFall *rf,
                   DcalcAPIndex ap_index);
  void setSlew(Vertex *vertex,
               const RiseFall *rf,
               DcalcAPIndex ap_index,
               const Slew &slew);

  // Edge functions.
  Edge *edge(EdgeId edge_index) const;
  EdgeId id(const Edge *edge) const;
  Edge *makeEdge(Vertex *from,
                 Vertex *to,
                 TimingArcSet *arc_set);
  void makeWireEdge(const Pin *from_pin,
                    const Pin *to_pin);
  void makePinInstanceEdges(const Pin *pin);
  void makeInstanceEdges(const Instance *inst);
  void makeWireEdgesToPin(const Pin *to_pin);
  void makeWireEdgesThruPin(const Pin *hpin);
  void makeWireEdgesFromPin(const Pin *drvr_pin);
  void deleteEdge(Edge *edge);
  // Find the edge and timing arc on a gate between in_pin and drvr_pin.
  void gateEdgeArc(const Pin *in_pin,
                   const RiseFall *in_rf,
                   const Pin *drvr_pin,
                   const RiseFall *drvr_rf,
                   // Return values.
                   Edge *&edge,
                   const TimingArc *&arc) const;

  ArcDelay arcDelay(const Edge *edge,
                    const TimingArc *arc,
                    DcalcAPIndex ap_index) const;
  void setArcDelay(Edge *edge,
                   const TimingArc *arc,
                   DcalcAPIndex ap_index,
                   ArcDelay delay);
  // Alias for arcDelays using library wire arcs.
  const ArcDelay &wireArcDelay(const Edge *edge,
                               const RiseFall *rf,
                               DcalcAPIndex ap_index);
  void setWireArcDelay(Edge *edge,
                       const RiseFall *rf,
                       DcalcAPIndex ap_index,
                       const ArcDelay &delay);
  // Is timing arc delay annotated.
  bool arcDelayAnnotated(const Edge *edge,
			 const TimingArc *arc,
			 DcalcAPIndex ap_index) const;
  void setArcDelayAnnotated(Edge *edge,
			    const TimingArc *arc,
			    DcalcAPIndex ap_index,
			    bool annotated);
  bool wireDelayAnnotated(const Edge *edge,
			  const RiseFall *rf,
			  DcalcAPIndex ap_index) const;
  void setWireDelayAnnotated(Edge *edge,
			     const RiseFall *rf,
			     DcalcAPIndex ap_index,
			     bool annotated);
  // True if any edge arc is annotated.
  bool delayAnnotated(Edge *edge);

  void minPulseWidthArc(Vertex *vertex,
                        const RiseFall *hi_low,
                        // Return values.
                        Edge *&edge,
                        TimingArc *&arc);
  // Sdf period check annotation.
  void periodCheckAnnotation(const Pin *pin,
			     DcalcAPIndex ap_index,
			    // Return values.
			    float &period,
			     bool &exists);
  void setPeriodCheckAnnotation(const Pin *pin,
				DcalcAPIndex ap_index,
				float period);

  // Remove all delay and slew annotations.
  void removeDelaySlewAnnotations();
  VertexSet *regClkVertices() { return reg_clk_vertices_; }

  static constexpr int vertex_level_bits = 24;
  static constexpr int vertex_level_max = (1<<vertex_level_bits)-1;

protected:
  void makeVerticesAndEdges();
  Vertex *makeVertex(Pin *pin,
		     bool is_bidirect_drvr,
		     bool is_reg_clk);
  void makeEdgeArcDelays(Edge *edge);
  void makePinVertices(const Instance *inst);
  void makeWireEdgesFromPin(const Pin *drvr_pin,
			    PinSet &visited_drvrs);
  bool isIsolatedNet(PinSeq &drvrs,
                     PinSeq &loads) const;
  void makeWireEdges();
  void makeInstDrvrWireEdges(const Instance *inst,
                             PinSet &visited_drvrs);
  void makePortInstanceEdges(const Instance *inst,
                             LibertyCell *cell,
                             LibertyPort *from_to_port);
  void removePeriodCheckAnnotations();
  void makeVertexSlews(Vertex *vertex);
  void deleteInEdge(Vertex *vertex,
		    Edge *edge);
  void deleteOutEdge(Vertex *vertex,
		     Edge *edge);
  void initSlews();
  void initSlews(Vertex *vertex);
  void initArcDelays(Edge *edge);
  void removeDelayAnnotated(Edge *edge);

  VertexTable *vertices_;
  EdgeTable *edges_;
  // Bidirect pins are split into two vertices:
  //  load/sink (top level output, instance pin input) vertex in pin_vertex_map
  //  driver/source (top level input, instance pin output) vertex
  //  in pin_bidirect_drvr_vertex_map
  PinVertexMap pin_bidirect_drvr_vertex_map_;
  int slew_rf_count_;
  DcalcAPIndex ap_count_;
  // Sdf period check annotations.
  PeriodCheckAnnotations *period_check_annotations_;
  // Register/latch clock vertices to search from.
  VertexSet *reg_clk_vertices_;

  friend class Vertex;
  friend class VertexIterator;
  friend class VertexInEdgeIterator;
  friend class VertexOutEdgeIterator;
  friend class MakeEdgesThruHierPin;
};

// Each Vertex corresponds to one network pin.
class Vertex
{
public:
  Vertex();
  ~Vertex();
  Pin *pin() const { return pin_; }
  // Pin path with load/driver suffix for bidirects.
  const char *name(const Network *network) const;
  bool isBidirectDriver() const { return is_bidirect_drvr_; }
  bool isDriver(const Network *network) const;
  Level level() const { return level_; }
  void setLevel(Level level);
  bool isRoot() const{ return level_ == 0; }
  bool hasFanin() const;
  bool hasFanout() const;
  LevelColor color() const { return static_cast<LevelColor>(color_); }
  void setColor(LevelColor color);
  Slew *slews() { return slews_; }
  const Slew *slews() const { return slews_; }
  Path *paths() const { return paths_; }
  void setPaths(Path *paths);
  TagGroupIndex tagGroupIndex() const;
  void setTagGroupIndex(TagGroupIndex tag_index);
  // Slew is annotated by sdc set_annotated_transition cmd.
  bool slewAnnotated(const RiseFall *rf,
		     const MinMax *min_max) const;
  // True if any rise/fall analysis pt slew is annotated.
  bool slewAnnotated() const;
  void setSlewAnnotated(bool annotated,
			const RiseFall *rf,
			DcalcAPIndex ap_index);
  void removeSlewAnnotated();
  // Constant zero/one from simulation.
  bool isConstant() const;
  LogicValue simValue() const;
  void setSimValue(LogicValue value);
  bool isDisabledConstraint() const { return is_disabled_constraint_; }
  void setIsDisabledConstraint(bool disabled);
  // True when vertex has timing check edges that constrain it.
  bool hasChecks() const  { return has_checks_; }
  void setHasChecks(bool has_checks);
  bool isCheckClk() const { return is_check_clk_; }
  void setIsCheckClk(bool is_check_clk);
  bool isGatedClkEnable() const { return is_gated_clk_enable_; }
  void setIsGatedClkEnable(bool enable);
  bool hasDownstreamClkPin() const { return has_downstream_clk_pin_; }
  void setHasDownstreamClkPin(bool has_clk_pin);
  // Vertices are constrained if they have one or more of the
  // following timing constraints:
  //   output delay constraints
  //   data check constraints
  //   path delay constraints
  bool isConstrained() const { return is_constrained_; }
  void setIsConstrained(bool constrained);
  bool bfsInQueue(BfsIndex index) const;
  void setBfsInQueue(BfsIndex index, bool value);
  bool isRegClk() const { return is_reg_clk_; }
  bool crprPathPruningDisabled() const { return crpr_path_pruning_disabled_;}
  void setCrprPathPruningDisabled(bool disabled);
  
  // ObjectTable interface.
  ObjectIdx objectIdx() const { return object_idx_; }
  void setObjectIdx(ObjectIdx idx);

  static int transitionCount() { return 2; }  // rise/fall

protected:
  void init(Pin *pin,
	    bool is_bidirect_drvr,
	    bool is_reg_clk);
  void clear();
  void setSlews(Slew *slews);

  Pin *pin_;
  EdgeId in_edges_;		// Edges to this vertex.
  EdgeId out_edges_;		// Edges from this vertex.

  // Delay calc
  Slew *slews_;
  // Search
  Path *paths_;

  // These fields are written by multiple threads, so they
  // cannot share the same word as the following bit fields.
  uint32_t tag_group_index_;
  // Each bit corresponds to a different BFS queue.
  std::atomic<uint8_t> bfs_in_queue_; // 8

  unsigned int level_:Graph::vertex_level_bits; // 24
  unsigned int slew_annotated_:slew_annotated_bits;  // 4
  // Levelization search state.
  // LevelColor gcc barfs if this is dcl'd.
  unsigned color_:2;
  // LogicValue gcc barfs if this is dcl'd.
  unsigned sim_value_:3;
  // Bidirect pins have two vertices.
  // This flag distinguishes the driver and load vertices.
  bool is_bidirect_drvr_:1;
  bool is_reg_clk_:1;
  bool is_disabled_constraint_:1;
  bool is_gated_clk_enable_:1;
  // Constrained by timing check edge.
  bool has_checks_:1;
  // Is the clock for a timing check.
  bool is_check_clk_:1;
  bool is_constrained_:1;
  bool has_downstream_clk_pin_:1;
  bool crpr_path_pruning_disabled_:1;
  unsigned object_idx_:VertexTable::idx_bits; // 7

private:
  friend class Graph;
  friend class Edge;
  friend class VertexInEdgeIterator;
  friend class VertexOutEdgeIterator;
};

// There is one Edge between each pair of pins that has a timing
// path between them.
class Edge
{
public:
  Edge();
  ~Edge();
  Vertex *to(const Graph *graph) const { return graph->vertex(to_); }
  VertexId to() const { return to_; }
  Vertex *from(const Graph *graph) const { return graph->vertex(from_); }
  VertexId from() const { return from_; }
  TimingRole *role() const;
  bool isWire() const;
  TimingSense sense() const;
  TimingArcSet *timingArcSet() const { return arc_set_; }
  void setTimingArcSet(TimingArcSet *set);
  ArcDelay *arcDelays() const { return arc_delays_; }
  void setArcDelays(ArcDelay *arc_delays);
  bool delay_Annotation_Is_Incremental() const {return delay_annotation_is_incremental_;};
  void setDelayAnnotationIsIncremental(bool is_incr);
  // Edge is disabled by set_disable_timing constraint.
  bool isDisabledConstraint() const;
  void setIsDisabledConstraint(bool disabled);
  // Timing sense for the to_pin function after simplifying the
  // function based constants on the instance pins.
  TimingSense simTimingSense() const;
  void setSimTimingSense(TimingSense sense);
  // Edge is disabled by constants in condition (when) function.
  bool isDisabledCond() const { return is_disabled_cond_; }
  void setIsDisabledCond(bool disabled);
  // Edge is disabled to break combinational loops.
  bool isDisabledLoop() const { return is_disabled_loop_; }
  void setIsDisabledLoop(bool disabled);
  // Edge is disabled to prevent converging clocks from merging (Xilinx).
  bool isBidirectInstPath() const { return is_bidirect_inst_path_; }
  void setIsBidirectInstPath(bool is_bidir);
  bool isBidirectNetPath() const { return is_bidirect_net_path_; }
  void setIsBidirectNetPath(bool is_bidir);
  void removeDelayAnnotated();

  // ObjectTable interface.
  ObjectIdx objectIdx() const { return object_idx_; }
  void setObjectIdx(ObjectIdx idx);

protected:
  void init(VertexId from,
	    VertexId to,
	    TimingArcSet *arc_set);
  void clear();
  bool arcDelayAnnotated(const TimingArc *arc,
                         DcalcAPIndex ap_index,
                         DcalcAPIndex ap_count) const;
  void setArcDelayAnnotated(const TimingArc *arc,
                            DcalcAPIndex ap_index,
                            DcalcAPIndex ap_count,
                            bool annotated);

  TimingArcSet *arc_set_;
  VertexId from_;
  VertexId to_;
  EdgeId vertex_in_link_;		// Vertex in edges list.
  EdgeId vertex_out_next_;		// Vertex out edges doubly linked list.
  EdgeId vertex_out_prev_;
  ArcDelay *arc_delays_;
  union {
    uintptr_t bits_;
    vector<bool> *seq_;
  } arc_delay_annotated_;
  bool arc_delay_annotated_is_bits_:1;
  bool delay_annotation_is_incremental_:1;
  bool is_bidirect_inst_path_:1;
  bool is_bidirect_net_path_:1;
  // Timing sense from function and constants on edge instance.
  unsigned sim_timing_sense_:timing_sense_bit_count;
  bool is_disabled_constraint_:1;
  bool is_disabled_cond_:1;
  bool is_disabled_loop_:1;
  unsigned object_idx_:VertexTable::idx_bits;

private:
  friend class Graph;
  friend class GraphDelays1;
  friend class GraphSlewsDelays1;
  friend class GraphSlewsDelays2;
  friend class Vertex;
  friend class VertexInEdgeIterator;
  friend class VertexOutEdgeIterator;
};

// Iterate over all graph vertices.
class VertexIterator : public Iterator<Vertex*>
{
public:
  explicit VertexIterator(Graph *graph);
  virtual bool hasNext() { return vertex_ || bidir_vertex_; }
  virtual Vertex *next();

private:
  bool findNextPin();
  void findNext();

  Graph *graph_;
  Network *network_;
  Instance *top_inst_;
  LeafInstanceIterator *inst_iter_;
  InstancePinIterator *pin_iter_;
  Vertex *vertex_;
  Vertex *bidir_vertex_;
};

class VertexInEdgeIterator : public VertexEdgeIterator
{
public:
  VertexInEdgeIterator(Vertex *vertex,
		       const Graph *graph);
  VertexInEdgeIterator(VertexId vertex_id,
		       const Graph *graph);
  bool hasNext() { return (next_ != nullptr); }
  Edge *next();

private:
  Edge *next_;
  const Graph *graph_;
};

class VertexOutEdgeIterator : public VertexEdgeIterator
{
public:
  VertexOutEdgeIterator(Vertex *vertex,
			const Graph *graph);
  bool hasNext() { return (next_ != nullptr); }
  Edge *next();

private:
  Edge *next_;
  const Graph *graph_;
};

// Iterate over the edges through a hierarchical pin.
class EdgesThruHierPinIterator : public Iterator<Edge*>
{
public:
  EdgesThruHierPinIterator(const Pin *hpin,
			   Network *network,
			   Graph *graph);
  virtual bool hasNext() { return edge_iter_.hasNext(); }
  virtual Edge *next() { return edge_iter_.next(); }

private:
  EdgeSet edges_;
  EdgeSet::Iterator edge_iter_;
};

class VertexIdLess
{
public:
  VertexIdLess(Graph *&graph);
  bool operator()(const Vertex *vertex1,
		  const Vertex *vertex2) const;

private:
  Graph *&graph_;
};

class VertexSet : public Set<Vertex*, VertexIdLess>
{
public:
  VertexSet(Graph *&graph);
};

} // namespace
