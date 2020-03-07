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
#include "StringUtil.hh"
#include "Network.hh"
#include "NetworkCmp.hh"
#include "Graph.hh"
#include "GraphCmp.hh"

namespace sta {

VertexNameLess::VertexNameLess(Network *network) :
  network_(network)
{
}

bool
VertexNameLess::operator()(const Vertex *vertex1,
			   const Vertex *vertex2)
{
  return network_->pathNameLess(vertex1->pin(), vertex2->pin());
}

////////////////////////////////////////////////////////////////

EdgeLess::EdgeLess(const Network *network,
		   Graph *graph) :
  pin_less_(network),
  graph_(graph)
{
}

bool
EdgeLess::operator()(const Edge *edge1,
		     const Edge *edge2) const
{
  const Pin *from1 = edge1->from(graph_)->pin();
  const Pin *from2 = edge2->from(graph_)->pin();
  const Pin *to1 = edge1->to(graph_)->pin();
  const Pin *to2 = edge2->to(graph_)->pin();
  return pin_less_(from1, from2)
    || (from1 == from2
	&& pin_less_(to1, to2));
}

void
sortEdges(EdgeSeq *edges,
	  Network *network,
	  Graph *graph)
{
  sort(edges, EdgeLess(network, graph));
}

}
