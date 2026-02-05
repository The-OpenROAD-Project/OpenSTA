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

#include "FuncExpr.hh"

#include "StringUtil.hh"
#include "Liberty.hh"
#include "Network.hh"

namespace sta {

using std::string;

FuncExpr *
FuncExpr::makePort(LibertyPort *port)
{
  return new FuncExpr(Op::port, nullptr, nullptr, port);
}

FuncExpr *
FuncExpr::makeNot(FuncExpr *expr)
{
  return new FuncExpr(Op::not_, expr, nullptr, nullptr);
}


FuncExpr *
FuncExpr::makeAnd(FuncExpr *left,
                  FuncExpr *right)
{
  return new FuncExpr(Op::and_, left, right, nullptr);
}

FuncExpr *
FuncExpr::makeOr(FuncExpr *left,
                 FuncExpr *right)
{
  return new FuncExpr(Op::or_, left, right, nullptr);
}

FuncExpr *
FuncExpr::makeXor(FuncExpr *left,
                  FuncExpr *right)
{
  return new FuncExpr(Op::xor_, left, right, nullptr);
}

FuncExpr *
FuncExpr::makeZero()
{
  return new FuncExpr(Op::zero, nullptr, nullptr, nullptr);
}

FuncExpr *
FuncExpr::makeOne()
{
  return new FuncExpr(Op::one, nullptr, nullptr, nullptr);
}

FuncExpr::FuncExpr(Op op,
                   FuncExpr *left,
                   FuncExpr *right,
                   LibertyPort *port) :
  op_(op),
  left_(left),
  right_(right),
  port_(port)
{
}

FuncExpr::~FuncExpr()
{
  delete left_;
  delete right_;
}

void
FuncExpr::shallowDelete()
{
  left_ = nullptr;
  right_ = nullptr;
  delete this;
}

FuncExpr *
FuncExpr::copy()
{
  FuncExpr *left = left_ ? left_->copy() : nullptr;
  FuncExpr *right = right_ ? right_->copy() : nullptr;
  return new FuncExpr(op_, left, right, port_);
}

LibertyPort *
FuncExpr::port() const
{
  if (op_ == Op::port)
    return port_;
  else
    return nullptr;
}

// Protect against null sub-expressions caused by unknown port refs.
TimingSense
FuncExpr::portTimingSense(const LibertyPort *port) const
{
  TimingSense left_sense, right_sense;

  switch (op_) {
  case Op::port:
    if (port == port_)
      return TimingSense::positive_unate;
    else
      return TimingSense::none;
  case Op::not_:
    if (left_) {
      switch (left_->portTimingSense(port)) {
      case TimingSense::positive_unate:
        return TimingSense::negative_unate;
      case TimingSense::negative_unate:
        return TimingSense::positive_unate;
      case TimingSense::non_unate:
        return TimingSense::non_unate;
      case TimingSense::none:
        return TimingSense::none;
      case TimingSense::unknown:
        return TimingSense::unknown;
      }
    }
    return TimingSense::unknown;
  case Op::or_:
  case Op::and_:
    left_sense = TimingSense::unknown;
    right_sense = TimingSense::unknown;
    if (left_)
      left_sense = left_->portTimingSense(port);
    if (right_)
      right_sense = right_->portTimingSense(port);

    if (left_sense == right_sense)
      return left_sense;
    else if (left_sense == TimingSense::non_unate
             || right_sense == TimingSense::non_unate
             || (left_sense == TimingSense::positive_unate
                 && right_sense == TimingSense::negative_unate)
             || (left_sense == TimingSense::negative_unate
                 && right_sense == TimingSense::positive_unate))
      return TimingSense::non_unate;
    else if (left_sense == TimingSense::none
             || left_sense == TimingSense::unknown)
      return right_sense;
    else if (right_sense == TimingSense::none
             || right_sense == TimingSense::unknown)
      return left_sense;
    else
      return TimingSense::unknown;
  case Op::xor_:
    left_sense = TimingSense::unknown;
    right_sense = TimingSense::unknown;
    if (left_)
      left_sense = left_->portTimingSense(port);
    if (right_)
      right_sense = right_->portTimingSense(port);
    if (left_sense == TimingSense::positive_unate
        || left_sense == TimingSense::negative_unate
        || left_sense == TimingSense::non_unate
        || right_sense == TimingSense::positive_unate
        || right_sense == TimingSense::negative_unate
        || right_sense == TimingSense::non_unate)
      return TimingSense::non_unate;
    else
      return TimingSense::unknown;
  case Op::one:
    return TimingSense::none;
  case Op::zero:
    return TimingSense::none;
  }
  // Prevent warnings from lame compilers.
  return TimingSense::unknown;
}

string
FuncExpr::to_string() const
{
  return to_string(false);
}

string
FuncExpr::to_string(bool with_parens) const
{
  switch (op_) {
  case Op::port:
    return port_->name();
  case Op::not_: {
    string result = "!";
    result += left_ ? left_->to_string(true) : "?";
    return result;
  }
  case Op::or_:
    return to_string(with_parens, '+');
  case Op::and_:
    return to_string(with_parens, '*');
  case Op::xor_:
    return to_string(with_parens, '^');
  case Op::one:
    return "1";
  case Op::zero:
    return "0";
  default:
    return "?";
  }
}

string
FuncExpr::to_string(bool with_parens,
                    char op) const
{
  string right = right_->to_string(true);
  string result;
  if (with_parens)
    result += '(';
  result += left_ ? left_->to_string(true) : "?";
  result += op;
  result += right_ ? right_->to_string(true) : "?";
  if (with_parens)
    result += ')';
  return result;
}

FuncExpr *
FuncExpr::bitSubExpr(int bit_offset)
{
  switch (op_) {
  case Op::port:
    if (port_->hasMembers()) {
      if (port_->size() == 1) {
        LibertyPort *port = port_->findLibertyMember(0);
        return makePort(port);
      }
      else {
        if (bit_offset < port_->size()) {
          LibertyPort *port = port_->findLibertyMember(bit_offset);
          return makePort(port);
        }
        else
          return nullptr;
      }
    }
    else
      // Always copy so the subexpr doesn't share memory.
      return makePort(port_);
  case Op::not_:
    return makeNot(left_->bitSubExpr(bit_offset));
  case Op::or_:
    return makeOr(left_->bitSubExpr(bit_offset),
                  right_->bitSubExpr(bit_offset));
  case Op::and_:
    return makeAnd(left_->bitSubExpr(bit_offset),
                   right_->bitSubExpr(bit_offset));
  case Op::xor_:
    return makeXor(left_->bitSubExpr(bit_offset),
                   right_->bitSubExpr(bit_offset));
  case Op::one:
    return makeOne();
  case Op::zero:
    return makeZero();
  }
  // Prevent warnings from lame compilers.
  return nullptr;
}

bool
FuncExpr::hasPort(const LibertyPort *port) const
{
  switch (op_) {
  case Op::port:
    return (port_ == port);
  case Op::not_:
    return left_ && left_->hasPort(port);
  case Op::or_:
  case Op::and_:
  case Op::xor_:
    return (left_ && left_->hasPort(port))
      || (right_ && right_->hasPort(port));
  case Op::one:
  case Op::zero:
    return false;
  }
  // Prevent warnings from lame compilers.
  return false;
}

bool
FuncExpr::checkSize(LibertyPort *port)
{
  return checkSize(port->size());
}

bool
FuncExpr::checkSize(size_t size)
{
  size_t port_size;
  switch (op_) {
  case Op::port:
    port_size = port_->size();
    return !(port_size == size
             || port_size == 1);
  case Op::not_:
    return left_->checkSize(size);
  case Op::or_:
  case Op::and_:
  case Op::xor_:
    return left_->checkSize(size) || right_->checkSize(size);
  case Op::one:
  case Op::zero:
    return false;
  }
  // Prevent warnings from lame compilers.
  return false;
}

FuncExpr *
FuncExpr::invert()
{
  if (op_ == FuncExpr::Op::not_) {
    FuncExpr *inv = left_;
    shallowDelete() ;
    return inv;
  }
  else
    return FuncExpr::makeNot(this);
}

LibertyPortSet
FuncExpr::ports() const
{
  LibertyPortSet ports;
  findPorts(this, ports);
  return ports;
}

void
FuncExpr::findPorts(const FuncExpr *expr,
                    LibertyPortSet &ports) const
{
  if (expr) {
    if (expr->op() == FuncExpr::Op::port)
      ports.insert(expr->port());
    else {
      findPorts(expr->left(), ports);
      findPorts(expr->right(), ports);
    }
  }
}

bool
FuncExpr::equiv(const FuncExpr *expr1,
                const FuncExpr *expr2)
{
  if (expr1 == nullptr && expr2 == nullptr)
    return true;
  else if (expr1 != nullptr && expr2 != nullptr
           && expr1->op() == expr2->op()) {
    switch (expr1->op()) {
    case FuncExpr::Op::port:
      return LibertyPort::equiv(expr1->port(), expr2->port());
    case FuncExpr::Op::not_:
      return equiv(expr1->left(), expr2->left());
    default:
      return equiv(expr1->left(), expr2->left())
        && equiv(expr1->right(), expr2->right());
    }
  }
  else
    return false;
}

bool
FuncExpr::less(const FuncExpr *expr1,
               const FuncExpr *expr2)
{
  if (expr1 != nullptr && expr2 != nullptr) {
    Op op1 = expr1->op();
    Op op2 = expr2->op();
    if (op1 == op2) {
      switch (expr1->op()) {
      case FuncExpr::Op::port:
        return LibertyPort::less(expr1->port(), expr2->port());
      case FuncExpr::Op::not_:
        return less(expr1->left(), expr2->left());
      default:
        if (equiv(expr1->left(), expr2->left()))
          return less(expr1->right(), expr2->right());
        else
          return less(expr1->left(), expr2->left());
      }
    }
    else
      return op1 < op2;
  }
  else if (expr1 == nullptr && expr2 == nullptr)
    return false;
  else
    return (expr1 == nullptr && expr2 != nullptr);
}

} // namespace
