// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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

#include <sstream>
#include <string>
#include <string_view>

#include "Report.hh"
#include "Liberty.hh"
#include "LibExprReaderPvt.hh"
#include "LibExprScanner.hh"

namespace sta {

FuncExpr *
parseFuncExpr(std::string_view func,
              const LibertyCell *cell,
              std::string_view error_msg,
              Report *report)
{
  if (!func.empty()) {
    std::string func_str(func);
    std::istringstream stream(func_str);
    LibExprReader reader(func, cell, error_msg, report);
    LibExprScanner scanner(stream);
    LibExprParse parser(&scanner, &reader);
    parser.parse();
    FuncExpr *expr = reader.result();
    return expr;
  }
  else
    return nullptr;
}

LibExprReader::LibExprReader(std::string_view func,
                             const LibertyCell *cell,
                             std::string_view error_msg,
                             Report *report) :
  func_(func),
  cell_(cell),
  error_msg_(error_msg),
  report_(report)
{
}

// defined in LibertyReader.cc
LibertyPort *
libertyReaderFindPort(const LibertyCell *cell,
                      std::string_view port_name);

FuncExpr *
LibExprReader::makeFuncExprPort(std::string &&port_name)
{
  FuncExpr *expr = nullptr;
  const std::string_view port_view(port_name);
  LibertyPort *port = libertyReaderFindPort(cell_, port_view);
  if (port)
    expr = FuncExpr::makePort(port);
  else
    report_->warn(1130, "{} references unknown port {}.", error_msg_, port_view);
  return expr;
}

FuncExpr *
LibExprReader::makeFuncExprNot(FuncExpr *arg)
{
  if (arg)
    return FuncExpr::makeNot(arg);
  else
    return nullptr;
}

FuncExpr *
LibExprReader::makeFuncExprXor(FuncExpr *arg1,
                               FuncExpr *arg2)
{
  if (arg1 && arg2)
    return FuncExpr::makeXor(arg1, arg2);
  else
    return nullptr;
}

FuncExpr *
LibExprReader::makeFuncExprAnd(FuncExpr *arg1,
                               FuncExpr *arg2)
{
  if (arg1 && arg2)
    return FuncExpr::makeAnd(arg1, arg2);
  else
    return nullptr;
}

FuncExpr *
LibExprReader::makeFuncExprOr(FuncExpr *arg1,
                              FuncExpr *arg2)
{
  if (arg1 && arg2)
    return FuncExpr::makeOr(arg1, arg2);
  else
    return nullptr;
}

void
LibExprReader::setResult(FuncExpr *result)
{
  result_ = result;
}

void
LibExprReader::parseError(std::string_view msg)
{
  report_->error(1131, "{} {}.", error_msg_, msg);
}

////////////////////////////////////////////////////////////////

LibExprScanner::LibExprScanner(std::istringstream &stream) :
  yyFlexLexer(&stream)
{
}

}  // namespace sta
