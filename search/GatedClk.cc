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

#include "GatedClk.hh"

#include "FuncExpr.hh"
#include "Liberty.hh"
#include "PortDirection.hh"
#include "Network.hh"
#include "Graph.hh"
#include "Mode.hh"
#include "Sdc.hh"
#include "Search.hh"
#include "ClkNetwork.hh"

namespace sta {

GatedClk::GatedClk(const StaState *sta) :
  StaState(sta)
{
}

bool
GatedClk::isGatedClkEnable(Vertex *vertex,
                           const Mode *mode) const
{
  bool is_gated_clk_enable;
  const Pin *clk_pin;
  LogicValue logic_active_value;
  isGatedClkEnable(vertex, mode,
                   is_gated_clk_enable, clk_pin, logic_active_value);
  return is_gated_clk_enable;
}

void
GatedClk::isGatedClkEnable(Vertex *enable_vertex,
                           const Mode *mode,
                           // Return values.
                           bool &is_gated_clk_enable,
                           const Pin *&clk_pin,
                           LogicValue &logic_active_value) const
{
  is_gated_clk_enable = false;
  const Pin *enable_pin = enable_vertex->pin();
  const Instance *inst = network_->instance(enable_pin);
  LibertyPort *enable_port = network_->libertyPort(enable_pin);
  EvalPred *eval_pred = search_->evalPred();
  ClkNetwork *clk_network = mode->clkNetwork();
  if (enable_port
      && !clk_network->isClock(enable_vertex)
      && enable_port->direction()->isInput()
      && eval_pred->searchFrom(enable_vertex, mode)) {
    const Sdc *sdc = mode->sdc();
    FuncExpr *func = nullptr;
    Vertex *gclk_vertex = nullptr;
    VertexOutEdgeIterator edge_iter(enable_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      gclk_vertex = edge->to(graph_);
      const Pin *gclk_pin = gclk_vertex->pin();
      if (edge->role() == TimingRole::combinational()
          && eval_pred->searchTo(gclk_vertex, mode)
          && eval_pred->searchThru(edge, mode)) {
        LibertyPort *gclk_port = network_->libertyPort(gclk_pin);
        func = gclk_port->function();
        if (gclk_port
            && func
            && clk_network->isClock(gclk_vertex)) {
          LibertyPortSet clk_ports = func->ports();
          for (LibertyPort *clk_port : clk_ports) {
            if (clk_port != enable_port)  {
              bool is_clk_gate = false;
              isClkGatingFunc(func, enable_port, clk_port,
                              is_clk_gate, logic_active_value);
              if (is_clk_gate) {
                clk_pin = network_->findPin(inst, clk_port);
                if (clk_pin
                    && !sdc->isDisableClockGatingCheck(clk_pin)
                    && clk_network->isClock(graph_->pinLoadVertex(clk_pin))) {
                  is_gated_clk_enable = true;
                  return;
                }
              }
            }
          }
        }
      }
    }
    
  }
}

PinSet
GatedClk::gatedClkEnables(Vertex *clk_vertex,
                          const Mode *mode)
{
  PinSet enable_pins(network_);
  const Pin *clk_pin = clk_vertex->pin();
  const Instance *inst = network_->instance(clk_pin);
  LibertyPort *clk_port = network_->libertyPort(clk_pin);
  EvalPred *eval_pred = search_->evalPred();
  if (clk_port
      && eval_pred->searchFrom(clk_vertex, mode)) {
    ClkNetwork *clk_network = mode->clkNetwork();
    FuncExpr *func = nullptr;
    Vertex *gclk_vertex = nullptr;
    VertexOutEdgeIterator edge_iter(clk_vertex, graph_);
    while (edge_iter.hasNext()) {
      Edge *edge = edge_iter.next();
      gclk_vertex = edge->to(graph_);
      if (edge->role() == TimingRole::combinational()
          && eval_pred->searchTo(gclk_vertex, mode)
          && eval_pred->searchThru(edge, mode)) {
        const Pin *gclk_pin = gclk_vertex->pin();
        LibertyPort *gclk_port = network_->libertyPort(gclk_pin);
        if (gclk_port) {
          func = gclk_port->function();
          if (func) {
            if (clk_network->isClock(gclk_vertex)) {
              LibertyPortSet enable_ports = func->ports();
              for (LibertyPort *enable_port : enable_ports) {
                if (enable_port != clk_port)  {
                  bool is_clk_gate;
                  LogicValue logic_value;
                  isClkGatingFunc(func, enable_port, clk_port,
                                  is_clk_gate, logic_value);
                  if (is_clk_gate) {
                    Pin *enable_pin = network_->findPin(inst, enable_port);
                    if (enable_pin
                        && !clk_network->isClock(graph_->pinLoadVertex(enable_pin))) {
                      enable_pins.insert(enable_pin);
                    }
                  }
                }
              }
            }
            break;
          }
        }
      }
    }
  }
  return enable_pins;
}

void
GatedClk::isClkGatingFunc(FuncExpr *func,
                          LibertyPort *enable_port,
                          LibertyPort *clk_port,
                          bool &is_clk_gate,
                          LogicValue &logic_value) const
{
  // The function should be in two-level SOP or POS form depending on "cost".
  // We need to apply literal cofactor if any input port is constant and
  // do "simple" logic minimization based on SOP and POS.
  while (func->op() == FuncExpr::Op::not_)
    func = func->left();
  if (func->op() == FuncExpr::Op::and_)
    logic_value = LogicValue::one;
  else if (func->op() == FuncExpr::Op::or_)
    logic_value = LogicValue::zero;
  else {
    is_clk_gate = false;
    return;
  }

  FuncExprSet funcs;
  functionClkOperands(func, func->left(), funcs);
  functionClkOperands(func, func->right(), funcs);

  bool need_gating_check = false;
  for (FuncExpr *expr : funcs) {
    if (expr->op() == FuncExpr::Op::not_) {
      if (expr->left()->op() == FuncExpr::Op::port
          && expr->left()->port() == clk_port) {
        need_gating_check = true;
        logic_value = (logic_value == LogicValue::one)
          ? LogicValue::zero
          : LogicValue::one;
      }
    }
    else {
      if (expr->op() == FuncExpr::Op::port
          && expr->port() == clk_port) {
        need_gating_check = true;
      }
    }
  }

  if (need_gating_check) {
    for (FuncExpr *expr : funcs) {
      LibertyPortSet ports = expr->ports();
      for (LibertyPort *port : ports) {
        if (port == enable_port) {
          is_clk_gate = true;
          return;
        }
      }
    }
  }
  is_clk_gate = false;
}

void
GatedClk::functionClkOperands(FuncExpr *root_expr,
                              FuncExpr *expr,
                              FuncExprSet &funcs) const
{
  if (expr->op() != root_expr->op())
    funcs.insert(expr);
  else {
    functionClkOperands(root_expr, expr->left(), funcs);
    functionClkOperands(root_expr, expr->right(), funcs);
  }
}

const RiseFall *
GatedClk::gatedClkActiveTrans(LogicValue active_value,
                              const MinMax *min_max) const
{
  const RiseFall *leading_rf;
  switch (active_value) {
  case LogicValue::one:
  case LogicValue::unknown:
    leading_rf = RiseFall::rise();
    break;
  case LogicValue::zero:
    leading_rf = RiseFall::fall();
    break;
  default:
    criticalError(249, "illegal gated clock active value");
    leading_rf = RiseFall::rise();
    break;
  }
  if (min_max == MinMax::max())
    return leading_rf;
  else
    return leading_rf->opposite();
}

} // namespace
