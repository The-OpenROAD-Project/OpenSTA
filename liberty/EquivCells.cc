// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

#include "EquivCells.hh"

#include "Hash.hh"
#include "MinMax.hh"
#include "PortDirection.hh"
#include "Transition.hh"
#include "TimingRole.hh"
#include "FuncExpr.hh"
#include "TimingArc.hh"
#include "Liberty.hh"
#include "TableModel.hh"
#include "Sequential.hh"

namespace sta {

using std::max;

static unsigned
hashCell(const LibertyCell *cell);
static unsigned
hashCellPorts(const LibertyCell *cell);
static unsigned
hashCellSequentials(const LibertyCell *cell);
static unsigned
hashSequential(const Sequential *seq);
bool
equivCellStatetables(const LibertyCell *cell1,
		     const LibertyCell *cell2);
static bool
equivCellPortSeq(const LibertyPortSeq &ports1,
                 const LibertyPortSeq &ports2);
static bool
equivStatetableRows(const StatetableRows &table1,
                    const StatetableRows &table2);
static bool
equivStatetableRow(const StatetableRow &row1,
                   const StatetableRow &row2);
static unsigned
hashStatetable(const Statetable *statetable);
static unsigned
hashStatetableRow(const StatetableRow &row);
static unsigned
hashFuncExpr(const FuncExpr *expr);
static unsigned
hashPort(const LibertyPort *port);
static unsigned
hashCellPgPorts(const LibertyCell *cell);
static unsigned
hashPgPort(const LibertyPgPort *port);

static bool
equivCellPgPorts(const LibertyCell *cell1,
                 const LibertyCell *cell2);

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
  return hashCellPorts(cell)
    + hashCellPgPorts(cell)
    + hashCellSequentials(cell);
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
hashCellPgPorts(const LibertyCell *cell)
{
  unsigned hash = 0;
  LibertyCellPgPortIterator port_iter(cell);
  while (port_iter.hasNext()) {
    LibertyPgPort *port = port_iter.next();
    hash += hashPgPort(port);
  }
  return hash;
}

static unsigned
hashPgPort(const LibertyPgPort *port)
{
  return hashString(port->name()) * 3
    + static_cast<int>(port->pgType()) * 5;
}

static unsigned
hashCellSequentials(const LibertyCell *cell)
{
  unsigned hash = 0;
  for (const Sequential *seq : cell->sequentials())
    hash += hashSequential(seq);
  const Statetable *statetable = cell->statetable();
  if (statetable)
    hash += hashStatetable(statetable);
  return hash;
}

static unsigned
hashSequential(const Sequential *seq)
{
  unsigned hash = 0;
  hash += seq->isRegister() * 3;
  hash += hashFuncExpr(seq->clock()) * 5;
  hash += hashFuncExpr(seq->data()) * 7;
  hash += hashPort(seq->output()) * 9;
  hash += hashPort(seq->outputInv()) * 11;
  hash += hashFuncExpr(seq->clear()) * 13;
  hash += hashFuncExpr(seq->preset()) * 17;
  hash += int(seq->clearPresetOutput()) * 19;
  hash += int(seq->clearPresetOutputInv()) * 23;
  return hash;
}

static unsigned
hashStatetable(const Statetable *statetable)
{
  unsigned hash = 0;
  unsigned hash_ports = 0;
  for (LibertyPort *input_port : statetable->inputPorts())
    hash_ports += hashPort(input_port);
  hash += hash_ports * 3;

  hash_ports = 0;
  for (LibertyPort *internal_port : statetable->internalPorts())
    hash_ports += hashPort(internal_port);
  hash += hash_ports * 5;

  unsigned hash_rows = 0;
  for (const StatetableRow &row : statetable->table())
    hash_rows += hashStatetableRow(row);
  hash += hash_rows * 7;
  return hash;
}

static unsigned
hashStatetableRow(const StatetableRow &row)
{
  unsigned hash  = 0;
  for (StateInputValue input_value : row.inputValues())
    hash += static_cast<int>(input_value) * 9;
  for (StateInternalValue current_value : row.currentValues())
    hash += static_cast<int>(current_value) * 11;
  for (StateInternalValue next_value : row.nextValues())
    hash += static_cast<int>(next_value) * 13;
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
    && equivCellPgPorts(cell1, cell2)
    && equivCellSequentials(cell1, cell2)
    && equivCellStatetables(cell1, cell2)
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
equivCellPgPorts(const LibertyCell *cell1,
                 const LibertyCell *cell2)
{
  if (cell1->pgPortCount() != cell2->pgPortCount())
    return false;
  else {
    LibertyCellPgPortIterator port_iter1(cell1);
    while (port_iter1.hasNext()) {
      LibertyPgPort *port1 = port_iter1.next();
      const char* name = port1->name();
      LibertyPgPort *port2 = cell2->findPgPort(name);
      if (!(port2 && LibertyPgPort::equiv(port1, port2)))
        return false;
    }
    return true;
  }
}

bool
equivCellSequentials(const LibertyCell *cell1,
		     const LibertyCell *cell2)
{
  const SequentialSeq &seqs1 = cell1->sequentials();
  const SequentialSeq &seqs2 = cell2->sequentials();
  auto seq_itr1 = seqs1.begin(), seq_itr2 = seqs2.begin();
  for (;
       seq_itr1 != seqs1.end() && seq_itr2 != seqs2.end();
       seq_itr1++, seq_itr2++) {
    const Sequential *seq1 = *seq_itr1;
    const Sequential *seq2 = *seq_itr2;
    if (!(FuncExpr::equiv(seq1->clock(), seq2->clock())
	  && FuncExpr::equiv(seq1->data(), seq2->data())
	  && LibertyPort::equiv(seq1->output(), seq2->output())
	  && LibertyPort::equiv(seq1->outputInv(), seq2->outputInv())
	  && FuncExpr::equiv(seq1->clear(), seq2->clear())
	  && FuncExpr::equiv(seq1->preset(), seq2->preset())))
      return false;
  }
  return seq_itr1 == seqs1.end() && seq_itr2 == seqs2.end();
}

bool
equivCellStatetables(const LibertyCell *cell1,
		     const LibertyCell *cell2)

{
  const Statetable *statetable1 = cell1->statetable();
  const Statetable *statetable2 = cell2->statetable();
  return (statetable1 == nullptr && statetable2 == nullptr)
    || (statetable1 && statetable2
        && equivCellPortSeq(statetable1->inputPorts(), statetable2->inputPorts())
        && equivCellPortSeq(statetable1->internalPorts(), statetable2->internalPorts())
        && equivStatetableRows(statetable1->table(), statetable2->table()));
}

static bool
equivCellPortSeq(const LibertyPortSeq &ports1,
                 const LibertyPortSeq &ports2)
{
  if (ports1.size() != ports2.size())
    return false;
  
  auto port_itr1 = ports1.begin();
  auto port_itr2 = ports2.begin();
  for (;
       port_itr1 != ports1.end() && port_itr2 != ports2.end();
       port_itr1++, port_itr2++) {
    const LibertyPort *port1 = *port_itr1;
    const LibertyPort *port2 = *port_itr2;
    if (!LibertyPort::equiv(port1, port2))
        return false;
  }
  return true;
}

static bool
equivStatetableRows(const StatetableRows &table1,
                    const StatetableRows &table2)
{
  if (table1.size() != table2.size())
    return false;

  auto row_itr1 = table1.begin();
  auto row_itr2 = table2.begin();
  for (;
       row_itr1 != table1.end() && row_itr2 != table2.end();
       row_itr1++, row_itr2++) {
    const StatetableRow &row1 = *row_itr1;
    const StatetableRow &row2 = *row_itr2;
    if (!equivStatetableRow(row1, row2))
        return false;
  }
  return true;
}

static bool
equivStatetableRow(const StatetableRow &row1,
                   const StatetableRow &row2)
{
  const StateInputValues &input_values1 = row1.inputValues();
  const StateInputValues &input_values2 = row2.inputValues();
  if (input_values1.size() != input_values2.size())
    return false;
  for (auto input_itr1 = input_values1.begin(),
         input_itr2 = input_values2.begin();
       input_itr1 != input_values1.end() && input_itr2 != input_values2.end();
       input_itr1++, input_itr2++) {
    if (*input_itr1 != *input_itr2)
      return false;
  }

  const StateInternalValues &current_values1 = row1.currentValues();
  const StateInternalValues &current_values2 = row2.currentValues();
  if (current_values1.size() != current_values2.size())
    return false;
  for (auto current_itr1 = current_values1.begin(),
         current_itr2 = current_values2.begin();
       current_itr1 != current_values1.end() && current_itr2 != current_values2.end();
       current_itr1++, current_itr2++) {
    if (*current_itr1 != *current_itr2)
      return false;
  }

  const StateInternalValues &next_values1 = row1.nextValues();
  const StateInternalValues &next_values2 = row2.nextValues();
  if (next_values1.size() != next_values2.size())
    return false;
  for (auto next_itr1 = next_values1.begin(),
         next_itr2 = next_values2.begin();
       next_itr1 != next_values1.end() && next_itr2 != next_values2.end();
       next_itr1++, next_itr2++) {
    if (*next_itr1 != *next_itr2)
      return false;
  }
  return true;
}

bool
equivCellTimingArcSets(const LibertyCell *cell1,
		       const LibertyCell *cell2)
{
  if (cell1->timingArcSetCount() != cell2->timingArcSetCount())
    return false;
  else {
    for (TimingArcSet *arc_set1 : cell1->timingArcSets()) {
      TimingArcSet *arc_set2 = cell2->findTimingArcSet(arc_set1);
      if (!(arc_set2 && TimingArcSet::equiv(arc_set1, arc_set2)))
	return false;
    }
    return true;
  }
}

} // namespace
