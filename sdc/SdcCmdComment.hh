// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
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

#pragma once

namespace sta {

class SdcCmdComment
{
public:
  SdcCmdComment();
  SdcCmdComment(const char *comment);
  const char *comment() { return comment_; }
  void setComment(const char *comment);

protected:
  // Destructor is protected to prevent deletion of a derived
  // class with a pointer to this base class.
  ~SdcCmdComment();

  char *comment_;
};

} // namespace
