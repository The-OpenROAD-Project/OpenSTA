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

#include <iostream>
#include <sstream>

#include "Report.hh"
#include "StringUtil.hh"
#include "Liberty.hh"
#include "LibExprReaderPvt.hh"
#include "LibExprScanner.hh"

namespace sta {

FuncExpr *
parseFuncExpr(const char *func,
	      LibertyCell *cell,
	      const char *error_msg,
	      Report *report)
{
  if (func != nullptr && func[0] != '\0') {
    std::string func1(func);
    std::istringstream stream(func);
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

LibExprReader::LibExprReader(const char *func,
			     LibertyCell *cell,
			     const char *error_msg,
			     Report *report) :
  func_(func),
  cell_(cell),
  error_msg_(error_msg),
  report_(report),
  result_(nullptr)
{
}

// defined in LibertyReader.cc
LibertyPort *
libertyReaderFindPort(LibertyCell *cell,
                      const char *port_name);

FuncExpr *
LibExprReader::makeFuncExprPort(const char *port_name)
{
  FuncExpr *expr = nullptr;
  LibertyPort *port = libertyReaderFindPort(cell_, port_name);
  if (port)
    expr = FuncExpr::makePort(port);
  else
    report_->warn(1130, "%s references unknown port %s.",
                  error_msg_, port_name);
  stringDelete(port_name);
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
LibExprReader::parseError(const char *msg)
{
  report_->error(1131, "%s %s.", error_msg_, msg);
}

////////////////////////////////////////////////////////////////

LibExprScanner::LibExprScanner(std::istringstream &stream) :
  yyFlexLexer(&stream)
{
}

} // namespace
