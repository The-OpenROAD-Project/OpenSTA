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
#include "Hash.hh"
#include "PortDirection.hh"
#include "Transition.hh"
#include "MinMax.hh"
#include "TimingRole.hh"
#include "FuncExpr.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "TableModel.hh"
#include "Sequential.hh"
#include "EquivCells.hh"

namespace sta {

using std::max;

static unsigned
hashCell(const LibertyCell *cell);
static unsigned
hashCellPorts(const LibertyCell *cell);
static unsigned
hashCellSequentials(const LibertyCell *cell);
static unsigned
hashFuncExpr(const FuncExpr *expr);
static unsigned
hashPort(const LibertyPort *port);

static float
cellDriveResistance(const LibertyCell *cell)
{
  LibertyCellPortBitIterator port_iter(cell);
  while (port_iter.hasNext()) {
    auto port = port_iter.next();
    if (port->direction()->isOutput())
      return port->driveResistance();
  }
  return 0.0;
}

class CellDriveResistanceGreater
{
public:
  bool operator()(const LibertyCell *cell1,
		  const LibertyCell *cell2) const
  {
    return cellDriveResistance(cell1) > cellDriveResistance(cell2);
  }
};

EquivCells::EquivCells(LibertyLibrarySeq *equiv_libs,
		       LibertyLibrarySeq *map_libs)
{
  LibertyCellHashMap hash_matches;
  for (auto lib : *equiv_libs)
    findEquivCells(lib, hash_matches);
  // Sort the equiv sets by drive resistance.
  for (auto cell : unique_equiv_cells_) {
    auto equivs = equiv_cells_.findKey(cell);
    sort(equivs, CellDriveResistanceGreater());
  }
  if (map_libs) {
    for (auto lib : *map_libs)
      mapEquivCells(lib, hash_matches);
  }
  hash_matches.deleteContents();
}

EquivCells::~EquivCells()
{
  for (auto cell : unique_equiv_cells_)
    delete equiv_cells_.findKey(cell);
}

LibertyCellSeq *
EquivCells::equivs(LibertyCell *cell)
{
  return equiv_cells_.findKey(cell);
}

// Use a comprehensive hash on cell properties to segregate
// cells into groups of potential matches.
void
EquivCells::findEquivCells(const LibertyLibrary *library,
			   LibertyCellHashMap &hash_matches)
{
  LibertyCellIterator cell_iter(library);
  while (cell_iter.hasNext()) {
    LibertyCell *cell = cell_iter.next();
    if (!cell->dontUse()) {
      unsigned hash = hashCell(cell);
      LibertyCellSeq *matches = hash_matches.findKey(hash);
      if (matches) {
	LibertyCellSeq::Iterator match_iter(matches);
	while (match_iter.hasNext()) {
	  LibertyCell *match = match_iter.next();
	  if (equivCells(match, cell)) {
	    LibertyCellSeq *equivs = equiv_cells_.findKey(match);
	    if (equivs == nullptr) {
	      equivs = new LibertyCellSeq;
	      equivs->push_back(match);
	      unique_equiv_cells_.push_back(match);
	      equiv_cells_[match] = equivs;
	    }
	    equivs->push_back(cell);
	    equiv_cells_[cell] = equivs;
	    break;
	  }
	}
	matches->push_back(cell);
      }
      else {
	matches = new LibertyCellSeq;
	hash_matches[hash] = matches;
	matches->push_back(cell);
      }
    }
  }
}

// Map library cells to equiv cells.
void
EquivCells::mapEquivCells(const LibertyLibrary *library,
			  LibertyCellHashMap &hash_matches)
{
  
  LibertyCellIterator cell_iter(library);
  while (cell_iter.hasNext()) {
    LibertyCell *cell = cell_iter.next();
    if (!cell->dontUse()) {
      unsigned hash = hashCell(cell);
      LibertyCellSeq *matches = hash_matches.findKey(hash);
      if (matches) {
	LibertyCellSeq::Iterator match_iter(matches);
	while (match_iter.hasNext()) {
	  LibertyCell *match = match_iter.next();
	  if (equivCells(match, cell)) {
	    LibertyCellSeq *equivs = equiv_cells_.findKey(match);
	    equiv_cells_[cell] = equivs;
	    break;
	  }
	}
      }
    }
  }
}

static unsigned
hashCell(const LibertyCell *cell)
{
  return hashCellPorts(cell) + hashCellSequentials(cell);
}

static unsigned
hashCellPorts(const LibertyCell *cell)
{
  unsigned hash = 0;
  LibertyCellPortIterator port_iter(cell);
  while (port_iter.hasNext()) {
    LibertyPort *port = port_iter.next();
    hash += hashPort(port);
    hash += hashFuncExpr(port->function()) * 3;
    hash += hashFuncExpr(port->tristateEnable()) * 5;
  }
  return hash;
}

static unsigned
hashPort(const LibertyPort *port)
{
  return hashString(port->name()) * 3
    + port->direction()->index() * 5;
}

static unsigned
hashCellSequentials(const LibertyCell *cell)
{
  unsigned hash = 0;
  LibertyCellSequentialIterator seq_iter(cell);
  while (seq_iter.hasNext()) {
    Sequential *seq = seq_iter.next();
    hash += hashFuncExpr(seq->clock()) * 3;
    hash += hashFuncExpr(seq->data()) * 5;
    hash += hashPort(seq->output()) * 7;
    hash += hashPort(seq->outputInv()) * 9;
    hash += hashFuncExpr(seq->clear()) * 11;
    hash += hashFuncExpr(seq->preset()) * 13;
    hash += int(seq->clearPresetOutput()) * 17;
    hash += int(seq->clearPresetOutputInv()) * 19;
  }
  return hash;
}

static unsigned
hashFuncExpr(const FuncExpr *expr)
{
  if (expr == nullptr)
    return 0;
  else {
    switch (expr->op()) {
    case FuncExpr::op_port:
      return hashPort(expr->port()) * 17;
      break;
    case FuncExpr::op_not:
      return hashFuncExpr(expr->left()) * 31;
      break;
    default:
      return (hashFuncExpr(expr->left()) + hashFuncExpr(expr->right()))
	* ((1 << expr->op()) - 1);
    }
  }
}

bool
equivCells(const LibertyCell *cell1,
	   const LibertyCell *cell2)
{
  return equivCellPortsAndFuncs(cell1, cell2)
    && equivCellSequentials(cell1, cell2)
    && equivCellTimingArcSets(cell1, cell2);
}

bool
equivCellPortsAndFuncs(const LibertyCell *cell1,
		       const LibertyCell *cell2)
{
  if (cell1->portCount() != cell2->portCount())
    return false;
  else {
    LibertyCellPortIterator port_iter1(cell1);
    while (port_iter1.hasNext()) {
      LibertyPort *port1 = port_iter1.next();
      const char *name = port1->name();
      LibertyPort *port2 = cell2->findLibertyPort(name);
      if (!(port2
	    && LibertyPort::equiv(port1, port2)
	    && FuncExpr::equiv(port1->function(), port2->function())
	    && FuncExpr::equiv(port1->tristateEnable(),
			       port2->tristateEnable()))){
        return false;
      }
    }
    return true;
  }
}

bool
equivCellPorts(const LibertyCell *cell1,
	       const LibertyCell *cell2)
{
  if (cell1->portCount() != cell2->portCount())
    return false;
  else {
    LibertyCellPortIterator port_iter1(cell1);
    while (port_iter1.hasNext()) {
      LibertyPort *port1 = port_iter1.next();
      const char* name = port1->name();
      LibertyPort *port2 = cell2->findLibertyPort(name);
      if (!(port2 && LibertyPort::equiv(port1, port2)))
        return false;
    }
    return true;
  }
}

bool
equivCellSequentials(const LibertyCell *cell1,
		     const LibertyCell *cell2)
{
  LibertyCellSequentialIterator seq_iter1(cell1);
  LibertyCellSequentialIterator seq_iter2(cell2);
  while (seq_iter1.hasNext() && seq_iter2.hasNext()) {
    Sequential *seq1 = seq_iter1.next();
    Sequential *seq2 = seq_iter2.next();
    if (!(FuncExpr::equiv(seq1->clock(), seq2->clock())
	  && FuncExpr::equiv(seq1->data(), seq2->data())
	  && LibertyPort::equiv(seq1->output(), seq2->output())
	  && LibertyPort::equiv(seq1->outputInv(), seq2->outputInv())
	  && FuncExpr::equiv(seq1->clear(), seq2->clear())
	  && FuncExpr::equiv(seq1->preset(), seq2->preset())))
      return false;
  }
  return !seq_iter1.hasNext() && !seq_iter2.hasNext();
}

bool
equivCellTimingArcSets(const LibertyCell *cell1,
		       const LibertyCell *cell2)
{
  if (cell1->timingArcSetCount() != cell2->timingArcSetCount())
    return false;
  else {
    LibertyCellTimingArcSetIterator set_iter1(cell1);
    while (set_iter1.hasNext()) {
      TimingArcSet *set1 = set_iter1.next();
      TimingArcSet *set2 = cell2->findTimingArcSet(set1);
      if (!(set2 && TimingArcSet::equiv(set1, set2)))
	return false;
    }
    return true;
  }
}

} // namespace
