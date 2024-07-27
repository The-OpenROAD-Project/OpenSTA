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

#pragma once

#include <map>

#include "StaState.hh"
#include "LibertyClass.hh"

struct DdNode;
struct DdManager;

namespace sta {

typedef std::map<const LibertyPort*, DdNode*> BddPortVarMap;
typedef std::map<unsigned, const LibertyPort*> BddVarIdxPortMap;

class Bdd : public StaState
{
public:
  Bdd(const StaState *sta);
  ~Bdd();
  DdNode *funcBdd(const FuncExpr *expr);
  DdNode *findNode(const LibertyPort *port);
  const LibertyPort *nodePort(DdNode *node);
  DdNode *ensureNode(const LibertyPort *port);
  const LibertyPort *varIndexPort(int var_index);
  BddPortVarMap &portVarMap() { return bdd_port_var_map_; }

  void clearVarMap();
  DdManager *cuddMgr() const { return cudd_mgr_; }

private:
  DdManager *cudd_mgr_;
  BddPortVarMap bdd_port_var_map_;
  BddVarIdxPortMap bdd_var_idx_port_map_;
};

} // namespace
