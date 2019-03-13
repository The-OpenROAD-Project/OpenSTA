// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
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

typedef Map<unsigned,LibertyCellSeq*, std::less<unsigned> > LibertyCellHashMap;
typedef Set<LibertyCellSeq*> LibertyCellSeqSet;

static LibertyCellEquivMap *
findEquivCells1(const LibertyLibrary *library);
static void
sortCellEquivs(LibertyCellEquivMap *cell_equivs);
float
cellDriveResistance(const LibertyCell *cell);

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
static unsigned
hashString(const char *str);

static bool
equivCellSequentials(const LibertyCell *cell1,
		     const LibertyCell *cell2);

LibertyCellEquivMap *
makeEquivCellMap(const LibertyLibrary *library)
{
  LibertyCellEquivMap *cell_equivs = findEquivCells1(library);
  sortCellEquivs(cell_equivs);
  return cell_equivs;
}

void
deleteEquivCellMap(LibertyCellEquivMap *equiv_map)
{
  // Multiple cells can point to the same cell sequence, so collect
  // them into a set so the are only deleted once.
  LibertyCellSeqSet cells_seqs;
  LibertyCellEquivMap::Iterator equiv_iter(equiv_map);
  while (equiv_iter.hasNext()) {
    LibertyCellSeq *cells = equiv_iter.next();
    cells_seqs.insert(cells);
  }
  LibertyCellSeqSet::Iterator cells_iter(cells_seqs);
  while (cells_iter.hasNext()) {
    LibertyCellSeq *cells = cells_iter.next();
    delete cells;
  }
  delete equiv_map;
}

static LibertyCellEquivMap *
findEquivCells1(const LibertyLibrary *library)
{
  LibertyCellHashMap cell_hash;
  LibertyCellEquivMap *cell_equivs = new LibertyCellEquivMap;
  LibertyCellIterator cell_iter(library);
  while (cell_iter.hasNext()) {
    LibertyCell *cell = cell_iter.next();
    if (!cell->dontUse()) {
      bool found_equiv = false;
      unsigned hash = hashCell(cell);
      // Use a comprehensive hash on cell properties to segregate
      // cells into groups of potential matches.
      LibertyCellSeq *matches = cell_hash[hash];
      if (matches) {
	LibertyCellSeq::Iterator match_iter(matches);
	while (match_iter.hasNext()) {
	  LibertyCell *match = match_iter.next();
	  if (equivCells(match, cell)) {
	    LibertyCellSeq *equivs = cell_equivs->findKey(match);
	    equivs->push_back(cell);
	    (*cell_equivs)[cell] = equivs;
	    found_equiv = true;
	    break;
	  }
	}
      }
      else {
	matches = new LibertyCellSeq;
	cell_hash[hash] = matches;
      }
      matches->push_back(cell);
      if (!found_equiv) {
	LibertyCellSeq *equivs = new LibertyCellSeq;
	equivs->push_back(cell);
	(*cell_equivs)[cell] = equivs;
      }
    }
  }

  LibertyCellHashMap::Iterator hash_iter(cell_hash);
  while (hash_iter.hasNext()) {
    LibertyCellSeq *cells = hash_iter.next();
    delete cells;
  }

  return cell_equivs;
}

struct CellDriveResistanceLess
{
  bool operator()(const LibertyCell *cell1,
		  const LibertyCell *cell2) const
  {
    return cellDriveResistance(cell1) > cellDriveResistance(cell2);
  }
};

static void
sortCellEquivs(LibertyCellEquivMap *cell_equivs)
{
  LibertyCellEquivMap::Iterator equivs_iter(cell_equivs);
  while (equivs_iter.hasNext()) {
    LibertyCellSeq *equivs = equivs_iter.next();
    sort(equivs, CellDriveResistanceLess());
  }
}

// Use the worst "drive" for all the timing arcs in the cell.
// Note that this function can NOT be static for sun's compiler to
// be happy with using it in a sort predicate (presumably because the
// template functions are compiled outside the scope of this file).
float
cellDriveResistance(const LibertyCell *cell)
{
  float max_drive = 0.0;
  LibertyCellTimingArcSetIterator set_iter(cell);
  while (set_iter.hasNext()) {
    TimingArcSet *set = set_iter.next();
    if (!set->role()->isTimingCheck()) {
      TimingArcSetArcIterator arc_iter(set);
      while (arc_iter.hasNext()) {
	TimingArc *arc = arc_iter.next();
	GateTimingModel *model = dynamic_cast<GateTimingModel*>(arc->model());
	if (model) {
	  float drive = model->driveResistance(cell, nullptr);
	  if (drive > max_drive)
	    max_drive = drive;
	}
      }
    }
  }
  return max_drive;
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

static unsigned
hashString(const char *str)
{
  unsigned hash = 0;
  size_t length = strlen(str);
  for (size_t i = 0; i < length; i++) {
    hash = str[i] + (hash << 2);
  }
  return hash;
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

static bool
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
