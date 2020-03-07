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

namespace sta {

#include <mutex>
#include "MinMax.hh"
#include "Vector.hh"
#include "GraphClass.hh"
#include "SearchClass.hh"

class StaState;
class WorstSlack;
class WnsSlackLess;

typedef Vector<WorstSlack> WorstSlackSeq;

class WorstSlacks
{
public:
  WorstSlacks(StaState *sta);
  void worstSlack(const MinMax *min_max,
		  // Return values.
		  Slack &worst_slack,
		  Vertex *&worst_vertex);
  void worstSlack(const Corner *corner,
		  const MinMax *min_max,
		  // Return values.
		  Slack &worst_slack,
		  Vertex *&worst_vertex);
  void updateWorstSlacks(Vertex *vertex,
			 SlackSeq &slacks);
  void worstSlackNotifyBefore(Vertex *vertex);

protected:
  WorstSlackSeq worst_slacks_;
  const StaState *sta_;
};

class WnsSlackLess
{
public:
  WnsSlackLess(PathAPIndex path_ap_index,
	       const StaState *sta);
  bool operator()(Vertex *vertex1,
		  Vertex *vertex2);

private:
  PathAPIndex path_ap_index_;
  Search *search_;
};

class WorstSlack
{
public:
  WorstSlack();
  WorstSlack(const WorstSlack &);
  void worstSlack(PathAPIndex path_ap_index,
		  const StaState *sta,
		  // Return values.
		  Slack &worst_slack,
		  Vertex *&worst_vertex);
  void updateWorstSlack(Vertex *vertex,
			SlackSeq &slacks,
			PathAPIndex path_ap_index,
			const StaState *sta);
  void deleteVertexBefore(Vertex *vertex);

protected:
  void findWorstSlack(PathAPIndex path_ap_index,
		      const StaState *sta);
  void initQueue(PathAPIndex path_ap_index,
		 const StaState *sta);
  void findWorstInQueue(PathAPIndex path_ap_index,
			const StaState *sta);
  void setWorstSlack(Vertex *vertex,
		     Slack slack,
		     const StaState *sta);
  void sortQueue(PathAPIndex path_ap_index,
		 const StaState *sta);
  void checkQueue(PathAPIndex path_ap_index,
		  const StaState *sta);

  Slack slack_init_;
  // Vertex with the worst slack.
  // When nullptr the worst slack is unknown but in the queue.
  Vertex *worst_vertex_;
  Slack worst_slack_;
  Slack slack_threshold_;
  // Vertices with slack < threshold_
  VertexSet queue_;
  // Queue is sorted and pruned to min_queue_size_ vertices when it
  // reaches max_queue_size_.
  int min_queue_size_;
  int max_queue_size_;
  std::mutex lock_;
};

} // namespace
