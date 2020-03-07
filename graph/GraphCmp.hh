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

#include "NetworkClass.hh"
#include "NetworkCmp.hh"
#include "GraphClass.hh"

namespace sta {

class VertexNameLess
{
public:
  explicit VertexNameLess(Network *network);
  bool operator()(const Vertex *vertex1,
		  const Vertex *vertex2);

private:
  Network *network_;
};

class EdgeLess
{
public:
  EdgeLess(const Network *network,
	   Graph *graph);
  bool operator()(const Edge *edge1,
		  const Edge *edge2) const;

private:
  const PinPathNameLess pin_less_;
  Graph *graph_;
};

void
sortEdges(EdgeSeq *edges,
	  Network *network,
	  Graph *graph);

} // namespace
