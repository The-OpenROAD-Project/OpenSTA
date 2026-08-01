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

// OpenROAD-fork: tag-match
// Unit test for the VectorMap insert-position hint API
// (findInsertHint / insertAtPos) added to skip the redundant binary search
// on the TagGroupBldr tag-insert hot path. Verifies that building a map with
// the hint API yields exactly the same contents and order as operator[].

#include "VectorMap.hh"

#include <random>
#include <vector>

#include "gtest/gtest.h"

namespace {

using sta::VectorMap;

using IntMap = VectorMap<int, int, std::less<int>>;

// Build a reference map with operator[] and a candidate map with the
// findInsertHint()/insertAtPos() fast path, then assert identical contents.
static void
checkHintMatchesOperatorBracket(const std::vector<int> &keys)
{
  IntMap ref{std::less<int>()};
  IntMap hint{std::less<int>()};

  for (int k : keys) {
    int value = k * 10 + 1;
    // Reference: plain operator[] insert (does its own binary search).
    ref[k] = value;

    // Candidate: mimic TagGroupBldr's tagMatchPath()+insertPath() pattern.
    size_t pos = 0;
    auto it = hint.findInsertHint(k, pos);
    if (it != hint.end())
      // Already present: overwrite, same as operator[].
      (*it).second() = value;
    else
      hint.insertAtPos(pos, k, value);
  }

  ASSERT_EQ(ref.size(), hint.size());
  // VectorMap iterates in sorted key order; compare element by element.
  auto ri = ref.begin();
  auto hi = hint.begin();
  for (; ri != ref.end() && hi != hint.end(); ++ri, ++hi) {
    EXPECT_EQ((*ri).first(), (*hi).first());
    EXPECT_EQ((*ri).second(), (*hi).second());
  }
  EXPECT_EQ(ri, ref.end());
  EXPECT_EQ(hi, hint.end());
}

TEST(VectorMapHint, InsertAscending)
{
  checkHintMatchesOperatorBracket({0, 1, 2, 3, 4, 5, 6, 7});
}

TEST(VectorMapHint, InsertDescending)
{
  checkHintMatchesOperatorBracket({7, 6, 5, 4, 3, 2, 1, 0});
}

TEST(VectorMapHint, InsertWithDuplicates)
{
  // Duplicate keys must take the "found" branch and overwrite, not reinsert.
  checkHintMatchesOperatorBracket({3, 1, 3, 2, 1, 0, 2, 3, 5, 5});
}

TEST(VectorMapHint, InsertRandomOrders)
{
  std::mt19937 rng(12345);
  for (int trial = 0; trial < 200; trial++) {
    std::vector<int> keys;
    int n = rng() % 30;
    for (int i = 0; i < n; i++)
      keys.push_back(static_cast<int>(rng() % 20));
    checkHintMatchesOperatorBracket(keys);
  }
}

TEST(VectorMapHint, HintFindAgreesWithFind)
{
  IntMap m{std::less<int>()};
  for (int k : {10, 20, 30, 40}) {
    size_t pos;
    auto it = m.findInsertHint(k, pos);
    EXPECT_EQ(it, m.end());
    m.insertAtPos(pos, k, k);
  }
  // Present keys: findInsertHint must agree with find().
  for (int k : {10, 20, 30, 40}) {
    size_t pos;
    auto hint_it = m.findInsertHint(k, pos);
    auto find_it = m.find(k);
    ASSERT_NE(hint_it, m.end());
    EXPECT_EQ((*hint_it).second(), (*find_it).second());
  }
  // Absent keys: end() and a valid insertion position keeping order sorted.
  for (int k : {5, 15, 25, 35, 45}) {
    size_t pos;
    auto hint_it = m.findInsertHint(k, pos);
    EXPECT_EQ(hint_it, m.end());
    EXPECT_LE(pos, m.size());
  }
}

}  // namespace
