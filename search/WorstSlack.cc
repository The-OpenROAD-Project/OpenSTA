// OpenSTA, Static Timing Analyzer
// Copyright (c) 2018, Parallax Software, Inc.
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
#include "Mutex.hh"
#include "Debug.hh"
#include "Report.hh"
#include "Graph.hh"
#include "Search.hh"
#include "WorstSlack.hh"

namespace sta {

using std::min;

////////////////////////////////////////////////////////////////

class WnsSlackLess
{
public:
  WnsSlackLess(const MinMax *min_max,
	       Search *search);
  bool operator()(Vertex *vertex1,
		  Vertex *vertex2);

private:
  const MinMax *min_max_;
  Search *search_;
};

WnsSlackLess::WnsSlackLess(const MinMax *min_max,
			   Search *search) :
  min_max_(min_max),
  search_(search)
{
}

bool
WnsSlackLess::operator()(Vertex *vertex1,
			 Vertex *vertex2)
{
  return delayFuzzyLess(search_->wnsSlack(vertex1, min_max_),
			search_->wnsSlack(vertex2, min_max_));
}

////////////////////////////////////////////////////////////////

class WorstSlack
{
public:
  WorstSlack(const MinMax *min_max,
	     StaState *sta);
  Vertex *worstVertex();
  Slack worstSlack();
  void clear();
  void updateWorstSlack(Vertex *vertex,
			Slack *slacks);
  void deleteVertexBefore(Vertex *vertex);

protected:
  void findWorstSlack();
  void initQueue();
  void findWorstInQueue();
  void setWorstSlack(Vertex *vertex,
		     Slack slack);
  void sortQueue();
  void checkQueue();

  Slack slack_init_;
  // Vertex with the worst slack.
  // When NULL the worst slack is unknown but in the queue.
  Vertex *worst_vertex_;
  Slack worst_slack_;
  Slack slack_threshold_;
  // Vertices with slack < threshold_
  VertexSet queue_;
  // Queue is sorted and pruned to min_queue_size_ vertices when it
  // reaches max_queue_size_.
  int min_queue_size_;
  int max_queue_size_;
  Mutex lock_;
  const MinMax *min_max_;
  WnsSlackLess slack_less_;
  StaState *sta_;
};

////////////////////////////////////////////////////////////////


WorstSlacks::WorstSlacks(StaState *sta)
{
  worst_slacks_[MinMax::minIndex()] = new WorstSlack(MinMax::min(), sta);
  worst_slacks_[MinMax::maxIndex()] = new WorstSlack(MinMax::max(), sta);
}

WorstSlacks::~WorstSlacks()
{
  delete worst_slacks_[MinMax::minIndex()];
  delete worst_slacks_[MinMax::maxIndex()];
}

void
WorstSlacks::clear()
{
  worst_slacks_[MinMax::minIndex()]->clear();
  worst_slacks_[MinMax::maxIndex()]->clear();
}

Slack
WorstSlacks::worstSlack(const MinMax *min_max)
{
  return worst_slacks_[min_max->index()]->worstSlack();
}

Vertex *
WorstSlacks::worstSlackVertex(const MinMax *min_max)
{
  return worst_slacks_[min_max->index()]->worstVertex();
}

void
WorstSlacks::updateWorstSlacks(Vertex *vertex,
			       Slack *slacks)
{
  worst_slacks_[MinMax::minIndex()]->updateWorstSlack(vertex, slacks);
  worst_slacks_[MinMax::maxIndex()]->updateWorstSlack(vertex, slacks);
}

void
WorstSlacks::worstSlackNotifyBefore(Vertex *vertex)
{
  worst_slacks_[MinMax::minIndex()]->deleteVertexBefore(vertex);
  worst_slacks_[MinMax::maxIndex()]->deleteVertexBefore(vertex);
}

////////////////////////////////////////////////////////////////

WorstSlack::WorstSlack(const MinMax *min_max,
		       StaState *sta) :
  slack_init_(MinMax::min()->initValue()),
  worst_vertex_(NULL),
  worst_slack_(slack_init_),
  slack_threshold_(slack_init_),
  min_queue_size_(10),
  max_queue_size_(20),
  min_max_(min_max),
  slack_less_(min_max, sta->search()),
  sta_(sta)
{
}

void
WorstSlack::clear()
{
  queue_.clear();
  worst_vertex_ = NULL;
  worst_slack_ = slack_init_;
}

void
WorstSlack::deleteVertexBefore(Vertex *vertex)
{
  lock_.lock();
  if (vertex == worst_vertex_) {
    worst_vertex_ = NULL;
    worst_slack_ = slack_init_;
  }
  queue_.eraseKey(vertex);
  lock_.unlock();
}

Vertex *
WorstSlack::worstVertex()
{
  findWorstSlack();
  return worst_vertex_;
}

Slack
WorstSlack::worstSlack()
{
  findWorstSlack();
  return worst_slack_;
}

void
WorstSlack::findWorstSlack()
{
  if (worst_vertex_ == NULL) {
    if (queue_.empty())
      initQueue();
    else
      findWorstInQueue();
  }
}

void
WorstSlack::initQueue()
{
  Search *search = sta_->search();
  const Debug *debug = sta_->debug();
  debugPrint0(debug, "wns", 3, "init queue\n");

  queue_.clear();
  worst_vertex_ = NULL;
  worst_slack_ = slack_init_;
  slack_threshold_ = slack_init_;
  VertexSet::Iterator end_iter(search->endpoints());
  while (end_iter.hasNext()) {
    Vertex *vertex = end_iter.next();
    Slack slack = search->wnsSlack(vertex, min_max_);
    if (!delayFuzzyEqual(slack, slack_init_)) {
      if (delayFuzzyLess(slack, worst_slack_))
	setWorstSlack(vertex, slack);
      if (delayFuzzyLessEqual(slack, slack_threshold_))
	queue_.insert(vertex);
      int queue_size = queue_.size();
      if (queue_size >= max_queue_size_)
	sortQueue();
    }
  }
  debugPrint1(debug, "wns", 3, "threshold %s\n",
	      delayAsString(slack_threshold_, sta_->units()));
//  checkQueue();
}

void
WorstSlack::sortQueue()
{
  Search *search = sta_->search();
  const Debug *debug = sta_->debug();
  debugPrint0(debug, "wns", 3, "sort queue\n");

  VertexSeq vertices;
  vertices.reserve(queue_.size());
  VertexSet::Iterator queue_iter(queue_);
  while (queue_iter.hasNext()) {
    Vertex *vertex = queue_iter.next();
    vertices.push_back(vertex);
  }
  sort(vertices, slack_less_);

  int vertex_count = vertices.size();
  int threshold_index = min(min_queue_size_, vertex_count - 1);
  Vertex *threshold_vertex = vertices[threshold_index];
  slack_threshold_ = search->wnsSlack(threshold_vertex, min_max_);
  debugPrint1(debug, "wns", 3, "threshold %s\n",
	      delayAsString(slack_threshold_, sta_->units()));

  // Reinsert vertices with slack < threshold.
  queue_.clear();
  VertexSeq::Iterator queue_iter2(vertices);
  while (queue_iter2.hasNext()) {
    Vertex *vertex = queue_iter2.next();
    Slack slack = search->wnsSlack(vertex, min_max_);
    if (delayFuzzyGreater(slack, slack_threshold_))
      break;
    queue_.insert(vertex);
  }
  max_queue_size_ = queue_.size() * 2;
  Vertex *worst_slack_vertex = vertices[0];
  Slack worst_slack_slack = search->wnsSlack(worst_slack_vertex, min_max_);
  setWorstSlack(worst_slack_vertex, worst_slack_slack);
}

void
WorstSlack::findWorstInQueue()
{
  Search *search = sta_->search();
  const Debug *debug = sta_->debug();
  debugPrint0(debug, "wns", 3, "find worst in queue\n");

  worst_vertex_ = NULL;
  worst_slack_ = slack_init_;
  VertexSet::Iterator queue_iter(queue_);
  while (queue_iter.hasNext()) {
    Vertex *vertex = queue_iter.next();
    Slack slack = search->wnsSlack(vertex, min_max_);
    if (slack < worst_slack_)
      setWorstSlack(vertex, slack);
  }
}

void
WorstSlack::checkQueue()
{
  Search *search = sta_->search();
  Report *report = sta_->report();
  Units *units = sta_->units();
  const Network *network = sta_->network();

  VertexSeq ends;
  VertexSet::Iterator end_iter(search->endpoints());
  while (end_iter.hasNext()) {
    Vertex *end = end_iter.next();
    if (delayFuzzyLessEqual(search->wnsSlack(end, min_max_),
			    slack_threshold_))
      ends.push_back(end);
  }
  sort(ends, slack_less_);

  VertexSet end_set;
  VertexSeq::Iterator end_iter2(ends);
  while (end_iter2.hasNext()) {
    Vertex *end = end_iter2.next();
    end_set.insert(end);
    if (!queue_.hasKey(end)
	&& delayFuzzyLessEqual(search->wnsSlack(end, min_max_),
			       slack_threshold_))
      report->print("WorstSlack queue missing %s %s < %s\n",
		    end->name(network),
		    delayAsString(search->wnsSlack(end, min_max_), units),
		    delayAsString(slack_threshold_, units));
  }

  VertexSet::Iterator queue_iter(queue_);
  while (queue_iter.hasNext()) {
    Vertex *end = queue_iter.next();
    if (!end_set.hasKey(end))
      report->print("WorstSlack queue extra %s %s > %s\n",
		    end->name(network),
		    delayAsString(search->wnsSlack(end, min_max_),
				  units),
		    delayAsString(slack_threshold_, units));
  }
}

void
WorstSlack::updateWorstSlack(Vertex *vertex,
			     Slack *slacks)
{
  const Debug *debug = sta_->debug();
  const Network *network = sta_->network();
  Slack slack = slacks[min_max_->index()];

  // Locking is required because ArrivalVisitor called by multiple
  // threads.
  lock_.lock();
  if (worst_vertex_
      && delayFuzzyLess(slack, worst_slack_))
    setWorstSlack(vertex, slack);
  else if (vertex == worst_vertex_)
    // Mark worst slack as unknown (updated by findWorstSlack().
    worst_vertex_ = NULL;

  if (!delayFuzzyEqual(slack, slack_init_)
      && delayFuzzyLessEqual(slack, slack_threshold_)) {
    debugPrint2(debug, "wns", 3, "insert %s %s\n",
		vertex->name(network),
		delayAsString(slack, sta_->units()));
    queue_.insert(vertex);
  }
  else {
    debugPrint2(debug, "wns", 3, "delete %s %s\n",
		vertex->name(network),
		delayAsString(slack, sta_->units()));
    queue_.eraseKey(vertex);
  }
  lock_.unlock();
  //  checkQueue();
}

void
WorstSlack::setWorstSlack(Vertex *vertex,
			  Slack slack)
{
  debugPrint2(sta_->debug(), "wns", 3, "%s %s\n",
	      vertex->name(sta_->network()),
	      delayAsString(slack, sta_->units()));
  worst_vertex_ = vertex;
  worst_slack_ = slack;
}

} // namespace
