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

#include "Bdd.hh"

#include "cudd.h"
#include "StaConfig.hh"
#include "Report.hh"
#include "FuncExpr.hh"

namespace sta {

Bdd::Bdd(const StaState *sta) :
  StaState(sta),
  cudd_mgr_(Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0))
{
}

Bdd::~Bdd()
{
  Cudd_Quit(cudd_mgr_);
}

DdNode *
Bdd::funcBdd(const FuncExpr *expr)
{
  DdNode *left = nullptr;
  DdNode *right = nullptr;
  DdNode *result = nullptr;
  switch (expr->op()) {
  case FuncExpr::op_port: {
    LibertyPort *port = expr->port();
    result = ensureNode(port);
    break;
  }
  case FuncExpr::op_not:
    left = funcBdd(expr->left());
    if (left)
      result = Cudd_Not(left);
    break;
  case FuncExpr::op_or:
    left = funcBdd(expr->left());
    right = funcBdd(expr->right());
    if (left && right)
      result = Cudd_bddOr(cudd_mgr_, left, right);
    else if (left)
      result = left;
    else if (right)
      result = right;
    break;
  case FuncExpr::op_and:
    left = funcBdd(expr->left());
    right = funcBdd(expr->right());
    if (left && right)
      result = Cudd_bddAnd(cudd_mgr_, left, right);
    else if (left)
      result = left;
    else if (right)
      result = right;
    break;
  case FuncExpr::op_xor:
    left = funcBdd(expr->left());
    right = funcBdd(expr->right());
    if (left && right)
      result = Cudd_bddXor(cudd_mgr_, left, right);
    else if (left)
      result = left;
    else if (right)
      result = right;
    break;
  case FuncExpr::op_one:
    result = Cudd_ReadOne(cudd_mgr_);
    break;
  case FuncExpr::op_zero:
    result = Cudd_ReadLogicZero(cudd_mgr_);
    break;
  default:
    report_->critical(1440, "unknown function operator");
  }
  if (result)
    Cudd_Ref(result);
  if (left)
    Cudd_RecursiveDeref(cudd_mgr_, left);
  if (right)
    Cudd_RecursiveDeref(cudd_mgr_, right);
  return result;
}

DdNode *
Bdd::findNode(const LibertyPort *port)
{
  auto port_var = bdd_port_var_map_.find(port);
  if (port_var == bdd_port_var_map_.end())
    return nullptr;
  else
    return port_var->second;
}

DdNode *
Bdd::ensureNode(const LibertyPort *port)
{
  auto port_var = bdd_port_var_map_.find(port);
  DdNode *node = nullptr;
  if (port_var == bdd_port_var_map_.end()) {
    unsigned var_index = bdd_port_var_map_.size();
    node = Cudd_bddIthVar(cudd_mgr_, var_index);
    bdd_port_var_map_[port] = node;
    bdd_var_idx_port_map_[var_index] = port;
    Cudd_Ref(node);
  }
  else
    node = port_var->second;
  return node;
}

const LibertyPort *
Bdd::nodePort(DdNode *node)
{
  auto port_index = bdd_var_idx_port_map_.find(Cudd_NodeReadIndex(node));
  if (port_index == bdd_var_idx_port_map_.end())
    return nullptr;
  else
    return port_index->second;
}

const LibertyPort *
Bdd::varIndexPort(int var_index)
{
  auto index_port = bdd_var_idx_port_map_.find(var_index);
  if (index_port == bdd_var_idx_port_map_.end())
    return nullptr;
  else
    return index_port->second;
}

void
Bdd::clearVarMap()
{
  bdd_port_var_map_.clear();
  bdd_var_idx_port_map_.clear();
}

} // namespace
