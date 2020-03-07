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

#include "Machine.hh"
#include "StringUtil.hh"
#include "SdcCmdComment.hh"

namespace sta {

SdcCmdComment::SdcCmdComment() :
  comment_(nullptr)
{
}

SdcCmdComment::SdcCmdComment(const char *comment)
{
  comment_ = nullptr;
  if (comment && comment[0] != '\0')
    comment_ = stringCopy(comment);
}
  
SdcCmdComment::~SdcCmdComment()
{
  stringDelete(comment_);
}
void
SdcCmdComment::setComment(const char *comment)
{
  if (comment_)
    stringDelete(comment_);
  comment_ = nullptr;
  if (comment && comment[0] != '\0')
    comment_ = stringCopy(comment);
}

} // namespace
