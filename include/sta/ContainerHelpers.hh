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

#include <algorithm>
#include <functional>
#include <map>
#include <ranges>
#include <set>
#include <string_view>
#include <type_traits>
#include <utility>   // for std::declval
#include <vector>

namespace sta {

// C++ kung foo courtesy of chat gtp.

// ------------------------------------------------------------
// 1. Sequence containers (vector<T*>, list<T*>, deque<T*>, …)
// ------------------------------------------------------------
template <typename Container>
requires std::is_pointer_v<typename Container::value_type>
void
deleteContents(Container& c)
{
  for (auto ptr : c)
    delete ptr;
  c.clear();
}

template <typename Container>
requires std::is_pointer_v<typename Container::value_type>
void
deleteContents(Container *c)
{
  for (auto ptr : *c)
    delete ptr;
  c->clear();
}

// ------------------------------------------------------------
// 2. Maps (map<K, T*>, unordered_map<K, T*>)
// ------------------------------------------------------------
template <typename Map>
requires std::is_pointer_v<typename Map::mapped_type>
void
deleteContents(Map& m)
{
  for (auto& kv : m)
    delete kv.second;
  m.clear();
}

template <typename Map>
requires std::is_pointer_v<typename Map::mapped_type>
void
deleteContents(Map *m)
{
  for (auto& kv : *m)
    delete kv.second;
  m->clear();
}

// ------------------------------------------------------------
// 3. Sets (set<T*>, unordered_set<T*>)
// ------------------------------------------------------------
template <typename Set>
requires (std::is_pointer_v<typename Set::value_type> &&
          requires { typename Set::mapped_type; } &&
          !std::is_same_v<typename Set::value_type, typename Set::mapped_type>)
void
deleteContents(Set& s)
{
  for (auto ptr : s)
    delete ptr;
  s.clear();
}

////////////////////////////////////////////////////////////////

// detect whether container has mapped_type
template<typename, typename = void>
struct has_mapped_type : std::false_type {};

template<typename T>
struct has_mapped_type<T, std::void_t<typename T::mapped_type>>
    : std::true_type {};

// handle pointer types
template<typename T>
struct has_mapped_type<T*, void> : has_mapped_type<T> {};

// return-type chooser: use struct, NOT alias template
template<typename C, bool = has_mapped_type<C>::value>
struct find_return;

// pointer to map
template<typename C>
struct find_return<C*, true>
{
  using type = C::mapped_type;
};

// pointer to set
template<typename C>
struct find_return<C*, false>
{
  using type = C::key_type;
};

// map ref
template<typename C>
struct find_return<C, true>
{
  using type = C::mapped_type;
};

// set ref
template<typename C>
struct find_return<C, false>
{
  using type = C::key_type;
};


// Find a pointer value in a reference to a contaiiner of pointers.
// Return nullptr if not found.
template<typename AssocContainer>
auto
findKey(const AssocContainer& c,
        const typename AssocContainer::key_type& key)
    -> find_return<AssocContainer>::type
{
  using ReturnType = find_return<AssocContainer>::type;

  static_assert(std::is_pointer_v<ReturnType>,
                "findKey requires pointer types");

  auto it = c.find(key);
  if (it == c.end())
    return nullptr;

  if constexpr (has_mapped_type<AssocContainer>::value)
    return it->second;  // map
  else
    return *it;         // set
}

// Find a pointer value in a reference to a map that uses strings as keys.
// Return nullptr if not found.
template<typename AssocContainer>
auto
findStringKey(const AssocContainer& c,
              std::string_view key)
    -> find_return<AssocContainer>::type
{
  using ReturnType = find_return<AssocContainer>::type;

  static_assert(std::is_pointer_v<ReturnType>,
                "findStringKey requires pointer types");

  auto it = c.find(key);
  if (it == c.end())
    return nullptr;
  else
    return it->second;
}

////////////////////////////////////////////////////////////////

// Find a value reference in a container. Returns reference to the value if found,
// otherwise returns reference to a static empty value of the same type.
template<typename AssocContainer>
auto
findKeyValue(const AssocContainer& c,
             const typename AssocContainer::key_type& key)
  -> const find_return<AssocContainer>::type &
{
  auto it = c.find(key);
  if (it != c.end()) {
    if constexpr (has_mapped_type<AssocContainer>::value)
      return it->second;
    else
      return *it;
  }
  static const typename find_return<AssocContainer>::type empty{};
  return empty;
}

// Find a value reference in a reference to a contaiiner of objects.
// Return exists.
template<typename AssocContainer>
void
findKeyValue(const AssocContainer& c,
             const typename AssocContainer::key_type& key,
             typename find_return<AssocContainer>::type &value,
             bool &exists)
{
  auto it = c.find(key);
  if (it == c.end()) {
    exists = false;
    return;
  }

  if constexpr (has_mapped_type<AssocContainer>::value) {
    // map
    value = it->second;
    exists = true;
  }
  else {
    // set
    value = *it;
    exists = true;
  }
}

// Find a value reference in a pointer to a contaiiner of objects.
// Return exists.
template<typename AssocContainer>
void
findKeyValue(const AssocContainer *c,
             const typename AssocContainer::key_type& key,
             typename find_return<AssocContainer>::type &value,
             bool &exists)
{
  auto it = c->find(key);
  if (it == c->end()) {
    exists = false;
    return;
  }

  if constexpr (has_mapped_type<AssocContainer>::value) {
    // map
    value = it->second;
    exists = true;
  }
  else {
    // set
    value = *it;
    exists = true;
  }
}

////////////////////////////////////////////////////////////////

// Find a value pointer in a reference to a contaiiner of objects.
// Return nullptr if not found.
template<typename AssocContainer>
auto
findKeyValuePtr(AssocContainer& c,
                 const typename AssocContainer::key_type& key)
  -> find_return<AssocContainer>::type*
{
  auto it = c.find(key);
  if (it == c.end())
    return nullptr;

  if constexpr (has_mapped_type<AssocContainer>::value)
    // map
    return &it->second;
  else
    // sett
    return *it;
}

// Find a value pointer in a reference to a contaiiner of objects.
// Return nullptr if not found.
template<typename AssocContainer>
auto
findKeyValuePtr(const AssocContainer& c,
                 const typename AssocContainer::key_type& key)
  -> find_return<AssocContainer>::type const*
{
  auto it = c.find(key);
  if (it == c.end())
    return nullptr;

  if constexpr (has_mapped_type<AssocContainer>::value)
    // map
    return &it->second;
  else
    // set
    return *it;
}

// Find a pointer to a value in a reference to a contaiiner of objects
// using std::string as the key.
// Return nullptr if not found.
template<typename AssocContainer>
auto
findStringValuePtr(AssocContainer& c,
                    std::string_view key)
  -> find_return<AssocContainer>::type*
{
  auto it = c.find(key);
  if (it == c.end())
    return nullptr;
  else
    return &it->second;
}

// Find a const pointer to a value in a const reference to a contaiiner objects
// using std::string as the key.
// Return nullptr if not found.
template<typename AssocContainer>
auto
findStringValuePtr(const AssocContainer& c,
                    std::string_view key)
  -> find_return<AssocContainer>::type const*
{
  auto it = c.find(key);
  if (it == c.end())
    return nullptr;
  else
    return &it->second;
}

////////////////////////////////////////////////////////////////

// Determine if two std::set's intersect.
// Returns true if there is at least one common element.
template <typename Set>
bool
intersects(const Set &set1,
           const Set &set2,
           typename Set::key_compare key_less)
{
  auto iter1 = set1.begin();
  auto end1 = set1.end();
  auto iter2 = set2.begin();
  auto end2 = set2.end();
  
  while (iter1 != end1 && iter2 != end2) {
    if (key_less(*iter1, *iter2))
      iter1++;
    else if (key_less(*iter2, *iter1))
      iter2++;
    else
      return true;
  }
  return false;
}

// Determine if two std::set's intersect (pointer version).
// Returns true if there is at least one common element.
template <typename Set>
bool
intersects(const Set *set1,
           const Set *set2,
           typename Set::key_compare key_less)
{
  if (set1 && set2) {
    auto iter1 = set1->begin();
    auto end1 = set1->end();
    auto iter2 = set2->begin();
    auto end2 = set2->end();
    
    while (iter1 != end1 && iter2 != end2) {
      if (key_less(*iter1, *iter2))
        iter1++;
      else if (key_less(*iter2, *iter1))
        iter2++;
      else
        return true;
    }
  }
  return false;
}

////////////////////////////////////////////////////////////////

// Compare set contents.
template <typename Set>
int
compare(const Set *set1,
        const Set *set2,
        typename Set::key_compare key_less)
{
  size_t size1 = set1 ? set1->size() : 0;
  size_t size2 = set2 ? set2->size() : 0;
  if (size1 == size2) {
    if (set1 == nullptr || set2 == nullptr) {
      // Both are null or empty, so they're equal
      return 0;
    }
    auto iter1 = set1->begin();
    auto iter2 = set2->begin();
    auto end1 = set1->end();
    auto end2 = set2->end();
    while (iter1 != end1 && iter2 != end2) {
      if (key_less(*iter1, *iter2))
        return -1;
      else if (key_less(*iter2, *iter1))
        return 1;
      ++iter1;
      ++iter2;
    }
    // Sets are equal.
    return 0;
  }
  else
    return (size1 > size2) ? 1 : -1;
}

////////////////////////////////////////////////////////////////

// Sort functions that do not require begin()/end() range.

// reference arg
template<std::ranges::random_access_range Range,
         typename Comp = std::less<>>
requires std::predicate<Comp&,
                        std::ranges::range_reference_t<Range>,
                        std::ranges::range_reference_t<Range>>
void
sort(Range& r,
     const Comp &comp = Comp{})
{
  std::sort(std::ranges::begin(r), std::ranges::end(r), comp);
}


// pointer arg
template<typename Range,
         typename Comp = std::less<>>
requires std::ranges::random_access_range<Range> &&
         std::predicate<Comp&,
                        std::ranges::range_reference_t<Range>,
                        std::ranges::range_reference_t<Range>>
void
sort(Range* r,
     const Comp &comp = Comp{})
{
  std::sort(std::ranges::begin(*r), std::ranges::end(*r), comp);
}

} // namespace sta
