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

#pragma once

#include <cstdio>
#include <fstream>
#include <string>
#include <string_view>

#include "StaConfig.hh"

#ifdef ZLIB_FOUND
#include <zlib.h>
#endif

#if HAVE_CXX_STD_FORMAT
#include <format>

namespace sta {

template <typename... Args>
std::string format(std::format_string<Args...> fmt,
                   Args &&...args) {
  return std::format(fmt, std::forward<Args>(args)...);
}

template <typename... Args>
void print(std::ofstream &stream,
           std::format_string<Args...> fmt,
           Args &&...args) {
  stream << std::format(fmt, std::forward<Args>(args)...);
}

#ifdef ZLIB_FOUND
template <typename... Args>
void print(gzFile stream,
           std::format_string<Args...> fmt, 
           Args &&...args) {
  std::string s = sta::format(fmt, std::forward<Args>(args)...);
  gzwrite(stream, s.c_str(), s.size());
}
#endif
template <typename... Args>
void print(FILE *stream,
           std::format_string<Args...> fmt,
           Args &&...args) {
  std::string s = sta::format(fmt, std::forward<Args>(args)...);
  std::fprintf(stream, "%s", s.c_str());
}

inline std::string vformat(std::string_view fmt,
                           std::format_args args) {
  return std::vformat(fmt, args);
}

template <typename... Args>
auto make_format_args(Args &&...args) {
  return std::make_format_args(std::forward<Args>(args)...);
}

// Format with runtime format string - captures args to avoid make_format_args
// rvalue reference issues.
template <typename... Args>
std::string formatRuntime(std::string_view fmt,
                          Args &&...args) {
  auto args_tuple = std::make_tuple(std::forward<Args>(args)...);
  return std::apply(
      [fmt](auto &...a) {
        return std::vformat(fmt, std::make_format_args(a...));
      },
      args_tuple);
}

}  // namespace sta

#else
#include <fmt/core.h>

namespace sta {

template <typename... Args>
std::string format(fmt::format_string<Args...> fmt,
                   Args &&...args) {
  return fmt::format(fmt, std::forward<Args>(args)...);
}
template <typename... Args>
void print(std::ofstream &stream,
           fmt::format_string<Args...> fmt, 
           Args &&...args) {
  stream << fmt::format(fmt, std::forward<Args>(args)...);
}

#ifdef ZLIB_FOUND
template <typename... Args>
void print(gzFile stream,
           fmt::format_string<Args...> fmt,
           Args &&...args) {
  std::string s = sta::format(fmt, std::forward<Args>(args)...);
  gzwrite(stream, s.c_str(), s.size());
}
#endif
template <typename... Args>
void print(FILE *stream,
           fmt::format_string<Args...> fmt,
           Args &&...args) {
  std::string s = sta::format(fmt, std::forward<Args>(args)...);
  std::fprintf(stream, "%s", s.c_str());
}

inline
std::string vformat(std::string_view fmt,
                    fmt::format_args args) {
  return fmt::vformat(fmt, args);
}

template <typename... Args>
auto make_format_args(Args &&...args) {
  return fmt::make_format_args(std::forward<Args>(args)...);
}

template <typename... Args>
std::string formatRuntime(std::string_view fmt,
                          Args &&...args) {
  return fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...);
}

}  // namespace sta
#endif
