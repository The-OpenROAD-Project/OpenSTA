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

#include "FilterObjects.hh"

#include <regex>
#include <stack>
#include <functional>
#include <memory>

#include "Property.hh"
#include "PatternMatch.hh"
#include "Sta.hh"

namespace sta {

class FilterExpr
{
public:
  struct Token
  {
    enum class Kind {
      skip = 0,
      predicate,
      op_lparen,
      op_rparen,
      op_or,
      op_and,
      op_inv,
      defined,
      undefined
    };
        
    Token(std::string text,
          Kind kind);
        
    std::string text;
    Kind kind;
  };
    
  struct PredicateToken : public Token
  {
    PredicateToken(std::string property,
                   std::string op,
                   std::string arg);
      
    std::string property;
    std::string op;
    std::string arg;
  };
    
  FilterExpr(std::string_view expression,
             Report *report);
    
  std::vector<std::unique_ptr<Token>> postfix();
private:
  std::vector<std::unique_ptr<Token>> lex();
  std::vector<std::unique_ptr<Token>> shuntingYard(std::vector<std::unique_ptr<Token>> &infix);
    
  std::string raw_;
  Report *report_;
};

FilterExpr::Token::Token(std::string text,
                         Token::Kind kind) :
  text (text),
  kind(kind)
{
}

FilterExpr::PredicateToken::PredicateToken(std::string property,
                                           std::string op,
                                           std::string arg) :
  Token(property + " " + op + " " + arg,
        Token::Kind::predicate),
  property(property), op(op), arg(arg)
{
}

FilterExpr::FilterExpr(std::string_view expression,
                       Report *report) :
  raw_(expression),
  report_(report)
{
}

std::vector<std::unique_ptr<FilterExpr::Token>>
FilterExpr::postfix()
{
  auto infix = lex();
  return shuntingYard(infix);
}

std::vector<std::unique_ptr<FilterExpr::Token>>
FilterExpr::lex()
{
  std::vector<std::pair<std::regex, Token::Kind>> token_regexes = {
    {std::regex("^\\s+"), Token::Kind::skip},
    {std::regex("^defined\\(([a-zA-Z_]+)\\)"), Token::Kind::defined},
    {std::regex("^undefined\\(([a-zA-Z_]+)\\)"), Token::Kind::undefined},
    {std::regex("^@?([a-zA-Z_]+) *((==|!=|=~|!~) *([0-9a-zA-Z_\\/$\\[\\]*?.]+))?"),
     Token::Kind::predicate},
    {std::regex("^(&&)"), Token::Kind::op_and},
    {std::regex("^(\\|\\|)"), Token::Kind::op_or},
    {std::regex("^(!)"), Token::Kind::op_inv},
    {std::regex("^(\\()"), Token::Kind::op_lparen},
    {std::regex("^(\\))"), Token::Kind::op_rparen},
  };
    
  std::vector<std::unique_ptr<Token>> result;
  const char* ptr = &raw_[0];
  bool match = false;
  while (*ptr != '\0') {
    match = false;
    for (auto& [regex, kind]: token_regexes) {
      std::cmatch token_match;
      if (std::regex_search(ptr, token_match, regex)) {
        if (kind == Token::Kind::predicate) {
          std::string property = token_match[1].str();
                    
          // The default operation on a predicate if an op and arg are
          // omitted is 'prop == 1 || true'.
          std::string op = "==";
          std::string arg = "1";
                    
          if (token_match[2].length() != 0) {
            op = token_match[3].str();
            arg = token_match[4].str();
          }
          result.push_back(std::make_unique<PredicateToken>(property, op, arg));
        }
        else if (kind == Token::Kind::defined)
          result.push_back(std::make_unique<Token>(token_match[1].str(), kind));
        else if (kind == Token::Kind::undefined)
          result.push_back(std::make_unique<Token>(token_match[1].str(), kind));
        else if (kind != Token::Kind::skip)
          result.push_back(std::make_unique<Token>(std::string(ptr,token_match.length()),
                                                   kind));
        ptr += token_match.length();
        match = true;
        break;
      };
    }
    if (!match)
      report_->error(2600, "-filter parsing failed at '{}'.", ptr);
  }
  return result;
}

std::vector<std::unique_ptr<FilterExpr::Token>>
FilterExpr::shuntingYard(std::vector<std::unique_ptr<Token>> &infix)
{
  std::vector<std::unique_ptr<Token>> output;
  std::stack<std::unique_ptr<Token>> operator_stack;

  for (auto &token : infix) {
    switch (token->kind) {
    case Token::Kind::predicate:
      output.push_back(std::move(token));
      break;
    case Token::Kind::op_or:
      [[fallthrough]];
    case Token::Kind::op_and:
      // The operators' enum values are ascending by precedence:
      //   inv > and > or
      while (operator_stack.size()
             && operator_stack.top()->kind > token->kind) {
        output.push_back(std::move(operator_stack.top()));
        operator_stack.pop();
      }
      operator_stack.push(std::move(token));
      break;
    case Token::Kind::op_inv:
      // Unary with highest precedence, no need for the while loop.
      operator_stack.push(std::move(token));
      break;
    case Token::Kind::defined:
      operator_stack.push(std::move(token));
      break;
    case Token::Kind::undefined:
      operator_stack.push(std::move(token));
      break;
    case Token::Kind::op_lparen:
      operator_stack.push(std::move(token));
      break;
    case Token::Kind::op_rparen:
      if (operator_stack.empty())
        report_->error(2601, "-filter extraneous ).");
      while (operator_stack.size()
             && operator_stack.top()->kind != Token::Kind::op_lparen) {
        output.push_back(std::move(operator_stack.top()));
        operator_stack.pop();
        if (operator_stack.empty())
          report_->error(2602, "-filter extraneous ).");
      }
      // Guaranteed to be lparen at this point.
      operator_stack.pop();
      break;
    default:
      // Unhandled/skip.
      break;
    }
  }

  while (operator_stack.size()) {
    if (operator_stack.top()->kind == Token::Kind::op_lparen)
      report_->error(2603, "-filter unmatched (.");
    output.push_back(std::move(operator_stack.top()));
    operator_stack.pop();
  }

  return output;
}

////////////////////////////////////////////////////////////////

template <typename T> std::set<T*>
filterObjects(const char *property,
              const char *op,
              const char *pattern,
              std::set<T*> &all,
              Sta *sta)
{
  Properties &properties = sta->properties();
  Network *network = sta->network();
  auto filtered_objects = std::set<T*>();
  bool exact_match = stringEq(op, "==");
  bool pattern_match = stringEq(op, "=~");
  bool not_match = stringEq(op, "!=");
  bool not_pattern_match = stringEq(op, "!~");
  for (T *object : all) {
    PropertyValue value = properties.getProperty(object, property);
    std::string prop = value.to_string(network);
    if (value.type() == PropertyValue::Type::bool_) {
      // Canonicalize bool true/false to 1/0.
      if (stringEqual(pattern, "true"))
        pattern = "1";
      else if (stringEqual(pattern, "false"))
        pattern = "0";
    }
    if ((exact_match && stringEq(prop.c_str(), pattern))
        || (not_match && !stringEq(prop.c_str(), pattern))
        || (pattern_match && patternMatch(pattern, prop))
        || (not_pattern_match && !patternMatch(pattern, prop)))
      filtered_objects.insert(object);
  }
  return filtered_objects;
}

template <typename T> std::vector<T*>
filterObjects(std::string_view filter_expression,
              std::vector<T*> *objects,
              Sta *sta)
{
  Report *report = sta->report();
  Network *network = sta->network();
  Properties &properties = sta->properties();
  std::vector<T*> result;
  if (objects) {
    std::set<T*> all;
    for (auto object: *objects)
      all.insert(object);

    FilterExpr filter(filter_expression, report);
    auto postfix = filter.postfix();
    std::stack<std::set<T*>> eval_stack;
    for (auto &token : postfix) {
      if (token->kind == FilterExpr::Token::Kind::op_or) {
        if (eval_stack.size() < 2)
          report->error(2604, "-filter logical OR requires at least two operands.");
        auto arg0 = eval_stack.top();
        eval_stack.pop();
        auto arg1 = eval_stack.top();
        eval_stack.pop();
        auto union_result = std::set<T*>();
        std::set_union(arg0.cbegin(), arg0.cend(), arg1.cbegin(), arg1.cend(),
                       std::inserter(union_result, union_result.begin()));
        eval_stack.push(union_result);
      }
      else if (token->kind == FilterExpr::Token::Kind::op_and) {
        if (eval_stack.size() < 2) {
          report->error(2605, "-filter logical AND requires two operands.");
        }
        auto arg0 = eval_stack.top();
        eval_stack.pop();
        auto arg1 = eval_stack.top();
        eval_stack.pop();
        auto intersection_result = std::set<T*>();
        std::set_intersection(arg0.cbegin(), arg0.cend(),
                              arg1.cbegin(), arg1.cend(),
                              std::inserter(intersection_result,
                                            intersection_result.begin()));
        eval_stack.push(intersection_result);
      }
      else if (token->kind == FilterExpr::Token::Kind::op_inv) {
        if (eval_stack.size() < 1) {
          report->error(2606, "-filter NOT missing operand.");
        }
        auto arg0 = eval_stack.top();
        eval_stack.pop();
        
        auto difference_result = std::set<T*>();
        std::set_difference(all.cbegin(), all.cend(),
                            arg0.cbegin(), arg0.cend(),
                            std::inserter(difference_result,
                                          difference_result.begin()));
        eval_stack.push(difference_result);
      }
      else if (token->kind == FilterExpr::Token::Kind::defined
               || token->kind == FilterExpr::Token::Kind::undefined) {
        bool should_be_defined =
          (token->kind == FilterExpr::Token::Kind::defined);
        auto result = std::set<T*>();
        for (auto object : all) {
          PropertyValue value = properties.getProperty(object, token->text);
          bool is_defined = false;
          switch (value.type()) {
          case PropertyValue::Type::float_:
            is_defined = value.floatValue() != 0;
            break;
          case PropertyValue::Type::bool_:
            is_defined = value.boolValue();
            break;
          case PropertyValue::Type::string:
          case PropertyValue::Type::liberty_library:
          case PropertyValue::Type::liberty_cell:
          case PropertyValue::Type::liberty_port:
          case PropertyValue::Type::library:
          case PropertyValue::Type::cell:
          case PropertyValue::Type::port:
          case PropertyValue::Type::instance:
          case PropertyValue::Type::pin:
          case PropertyValue::Type::net:
          case PropertyValue::Type::clk:
            is_defined = value.to_string(network) != "";
            break;
          case PropertyValue::Type::none:
            is_defined = false;
            break;
          case PropertyValue::Type::pins:
            is_defined = value.pins()->size() > 0;
            break;
          case PropertyValue::Type::clks:
            is_defined = value.clocks()->size() > 0;
            break;
          case PropertyValue::Type::paths:
            is_defined = value.paths()->size() > 0;
            break;
          case PropertyValue::Type::pwr_activity:
            is_defined = value.pwrActivity().isSet();
            break;
          }
          if (is_defined == should_be_defined) {
            result.insert(object);
          }
        }
        eval_stack.push(result);
      }
      else if (token->kind == FilterExpr::Token::Kind::predicate) {
        auto *predicate_token =
          static_cast<FilterExpr::PredicateToken *>(token.get());
        auto result = filterObjects<T>(predicate_token->property.c_str(),
                                       predicate_token->op.c_str(),
                                       predicate_token->arg.c_str(),
                                       all, sta);
        eval_stack.push(result);
      }
    }
    if (eval_stack.size() == 0)
      report->error(2607, "-filter expression is empty.");
    if (eval_stack.size() > 1)
      // huh?
      report->error(2608, "-filter expression evaluated to multiple sets.");
    auto result_set = eval_stack.top();
    result.resize(result_set.size());
    std::copy(result_set.begin(), result_set.end(), result.begin());
    std::map<T*, int> objects_i;
    for (size_t i = 0; i < objects->size(); ++i)
      objects_i[objects->at(i)] = i;
    std::sort(result.begin(), result.end(),
              [&](T* a, T* b) {
                return objects_i[a] < objects_i[b];
              });
    delete objects;
  }
  return result;
}

PortSeq
filterPorts(std::string_view filter_expression,
            PortSeq *objects,
            Sta *sta)
{
  return filterObjects<const Port>(filter_expression, objects, sta);
}

InstanceSeq
filterInstances(std::string_view filter_expression,
                InstanceSeq *objects,
                Sta *sta)
{
  return filterObjects<const Instance>(filter_expression, objects, sta);
}

PinSeq
filterPins(std::string_view filter_expression,
           PinSeq *objects,
           Sta *sta)
{
  return filterObjects<const Pin>(filter_expression, objects, sta);
}

NetSeq
filterNets(std::string_view filter_expression,
           NetSeq *objects,
           Sta *sta)
{
  return filterObjects<const Net>(filter_expression, objects, sta);
}

ClockSeq
filterClocks(std::string_view filter_expression,
             ClockSeq *objects,
             Sta *sta)
{
  return filterObjects<Clock>(filter_expression, objects, sta);
}

LibertyCellSeq
filterLibCells(std::string_view filter_expression,
               LibertyCellSeq *objects,
               Sta *sta)
{
  return filterObjects<LibertyCell>(filter_expression, objects, sta);
}

LibertyPortSeq
filterLibPins(std::string_view filter_expression,
              LibertyPortSeq *objects,
              Sta *sta)
{
  return filterObjects<LibertyPort>(filter_expression, objects, sta);
}

LibertyLibrarySeq
filterLibertyLibraries(std::string_view filter_expression,
                       LibertyLibrarySeq *objects,
                       Sta *sta)
{
  return filterObjects<LibertyLibrary>(filter_expression, objects, sta);
}

EdgeSeq
filterTimingArcs(std::string_view filter_expression,
                 EdgeSeq *objects,
                 Sta *sta)
{
  return filterObjects<Edge>(filter_expression, objects, sta);
}

PathEndSeq
filterPathEnds(std::string_view filter_expression,
               PathEndSeq *objects,
               Sta *sta)
{
  return filterObjects<PathEnd>(filter_expression, objects, sta);
}

StringSeq
filterExprToPostfix(std::string_view expr,
                    Report *report)
{
  FilterExpr filter(expr, report);
  auto postfix = filter.postfix();
  StringSeq result;
  for (auto &token : postfix)
    result.push_back(token->text);
  return result;
}

} // namespace sta
