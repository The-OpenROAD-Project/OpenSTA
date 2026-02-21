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

#pragma once

#include <vector>
#include <algorithm>
#include <utility>
#include <iterator>
#include <stdexcept>
#include <type_traits>

namespace sta {

// Proxy reference type that mimics std::pair<const Key, Value>
// and supports structured bindings via tuple interface
template <typename Key, typename Value>
class VectorMapReferenceProxy {
public:
  // Constructor that stores pointer to underlying pair (non-const)
  VectorMapReferenceProxy(std::pair<Key, Value>* ptr) : pair_ptr_(ptr) {}
  
  // Constructor that stores pointer to underlying pair (const)
  VectorMapReferenceProxy(const std::pair<Key, Value>* ptr) 
    : pair_ptr_(const_cast<std::pair<Key, Value>*>(ptr)) {}
  
  // Access members like std::pair (for compatibility)
  const Key& first() const { return pair_ptr_->first; }
  Value& second() { return pair_ptr_->second; }
  const Value& second() const { return pair_ptr_->second; }
  
  // Tuple-like access for structured bindings
  template<std::size_t I>
  auto get() const {
    if constexpr (I == 0) return pair_ptr_->first;
    if constexpr (I == 1) return pair_ptr_->second;
  }
  
  template<std::size_t I>
  auto get() {
    if constexpr (I == 0) return pair_ptr_->first;
    if constexpr (I == 1) return pair_ptr_->second;
  }
  
  // Assignment (only second can be assigned)
  VectorMapReferenceProxy& operator=(const std::pair<const Key, Value>& other) {
    pair_ptr_->second = other.second;
    return *this;
  }
  
private:
  std::pair<Key, Value>* pair_ptr_;
};

// VectorMap: A map implementation using a sorted vector with binary search.
// This provides O(log n) lookup, O(n) insertion/deletion, but better cache
// locality than std::map for small to medium-sized maps.
template <typename Key, typename Value, typename Compare = std::less<Key>>
class VectorMap {
public:
  using key_type = Key;
  using mapped_type = Value;
  using value_type = std::pair<const Key, Value>;
  using key_compare = Compare;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using reference = value_type&;
  using const_reference = const value_type&;
  using pointer = value_type*;
  using const_pointer = const value_type*;

private:
  // Internal storage: vector of pairs
  // We use std::pair<Key, Value> internally, but expose it as
  // std::pair<const Key, Value> through iterators
  using storage_type = std::vector<std::pair<Key, Value>>;
  storage_type data_;
  Compare comp_;

  // Helper to find insertion point using binary search
  typename storage_type::iterator findInsertPos(const Key& key) {
    return std::lower_bound(data_.begin(), data_.end(), key,
                            [this](const auto& pair, const Key& k) {
                              return comp_(pair.first, k);
                            });
  }

  typename storage_type::const_iterator findInsertPos(const Key& key) const {
    return std::lower_bound(data_.begin(), data_.end(), key,
                            [this](const auto& pair, const Key& k) {
                              return comp_(pair.first, k);
                            });
  }

  // Iterator adapter to expose const Key in pair
  // Uses a proxy reference to safely expose std::pair<const Key, Value>
  template <typename BaseIter>
  class IteratorAdapter {
  public:
    using iterator_category = std::random_access_iterator_tag;
    using value_type = std::pair<const Key, Value>;
    using difference_type = ptrdiff_t;
    using proxy_type = VectorMapReferenceProxy<Key, Value>;
    
    using reference = proxy_type;
    using pointer = proxy_type*;

    IteratorAdapter() = default;
    explicit IteratorAdapter(BaseIter it) : it_(it) {}
    
    // Conversion constructor: allow conversion from non-const to const iterator
    template <typename OtherIter>
    IteratorAdapter(const IteratorAdapter<OtherIter>& other,
                    typename std::enable_if<
                      std::is_convertible_v<OtherIter, BaseIter>
                    >::type* = nullptr)
      : it_(other.base()) {}

    reference operator*() {
      return proxy_type(&*it_);
    }

    reference operator*() const {
      return proxy_type(&*it_);
    }

    // For operator->, we return a pointer to a temporary
    // This is not ideal but necessary for compatibility
    class arrow_proxy {
    public:
      explicit arrow_proxy(const proxy_type& ref) 
        : value_(ref.first(), ref.second()) {}
      value_type* operator->() { return &value_; }
    private:
      value_type value_;
    };

    arrow_proxy operator->() {
      return arrow_proxy(operator*());
    }

    arrow_proxy operator->() const {
      return arrow_proxy(operator*());
    }

    // Assignment operator: allow assignment from non-const to const iterator
    template <typename OtherIter>
    typename std::enable_if<
      std::is_convertible_v<OtherIter, BaseIter>,
      IteratorAdapter&
    >::type
    operator=(const IteratorAdapter<OtherIter>& other) {
      it_ = other.base();
      return *this;
    }

    IteratorAdapter& operator++() {
      ++it_;
      return *this;
    }

    IteratorAdapter operator++(int) {
      IteratorAdapter tmp = *this;
      ++it_;
      return tmp;
    }

    IteratorAdapter& operator--() {
      --it_;
      return *this;
    }

    IteratorAdapter operator--(int) {
      IteratorAdapter tmp = *this;
      --it_;
      return tmp;
    }

    IteratorAdapter& operator+=(difference_type n) {
      it_ += n;
      return *this;
    }

    IteratorAdapter& operator-=(difference_type n) {
      it_ -= n;
      return *this;
    }

    IteratorAdapter operator+(difference_type n) const {
      return IteratorAdapter(it_ + n);
    }

    IteratorAdapter operator-(difference_type n) const {
      return IteratorAdapter(it_ - n);
    }

    difference_type operator-(const IteratorAdapter& other) const {
      return it_ - other.it_;
    }

    reference operator[](difference_type n) {
      return proxy_type(&it_[n]);
    }

    reference operator[](difference_type n) const {
      return proxy_type(&it_[n]);
    }

    bool operator==(const IteratorAdapter& other) const {
      return it_ == other.it_;
    }

    bool operator!=(const IteratorAdapter& other) const {
      return it_ != other.it_;
    }

    bool operator<(const IteratorAdapter& other) const {
      return it_ < other.it_;
    }

    bool operator<=(const IteratorAdapter& other) const {
      return it_ <= other.it_;
    }

    bool operator>(const IteratorAdapter& other) const {
      return it_ > other.it_;
    }

    bool operator>=(const IteratorAdapter& other) const {
      return it_ >= other.it_;
    }

    BaseIter base() const { return it_; }

  private:
    BaseIter it_;
  };

public:
  using iterator = IteratorAdapter<typename storage_type::iterator>;
  using const_iterator = IteratorAdapter<typename storage_type::const_iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;

  VectorMap() = default;
  explicit VectorMap(const Compare& comp) : comp_(comp) {}

  template <typename InputIt>
  VectorMap(InputIt first, InputIt last, const Compare& comp = Compare())
    : comp_(comp) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  VectorMap(std::initializer_list<value_type> init,
            const Compare& comp = Compare())
    : comp_(comp) {
    for (const auto& pair : init) {
      insert(pair);
    }
  }

  // Element access
  Value& operator[](const Key& key) {
    auto it = findInsertPos(key);
    if (it != data_.end() && !comp_(key, it->first)) {
      // Key exists
      return it->second;
    } else {
      // Key doesn't exist, insert it
      return data_.insert(it, std::make_pair(key, Value{}))->second;
    }
  }

  Value& at(const Key& key) {
    auto it = findInsertPos(key);
    if (it != data_.end() && !comp_(key, it->first)) {
      return it->second;
    }
    throw std::out_of_range("VectorMap::at");
  }

  const Value& at(const Key& key) const {
    auto it = findInsertPos(key);
    if (it != data_.end() && !comp_(key, it->first)) {
      return it->second;
    }
    throw std::out_of_range("VectorMap::at");
  }

  // Lookup
  bool contains(const Key& key) const {
    auto it = findInsertPos(key);
    return it != data_.end() && !comp_(key, it->first);
  }

  iterator find(const Key& key) {
    auto it = findInsertPos(key);
    if (it != data_.end() && !comp_(key, it->first)) {
      return iterator(it);
    }
    return end();
  }

  const_iterator find(const Key& key) const {
    auto it = findInsertPos(key);
    if (it != data_.end() && !comp_(key, it->first)) {
      return const_iterator(it);
    }
    return end();
  }

  size_type count(const Key& key) const {
    return contains(key) ? 1 : 0;
  }

  // Modifiers
  std::pair<iterator, bool> insert(const value_type& value) {
    auto it = findInsertPos(value.first);
    if (it != data_.end() && !comp_(value.first, it->first)) {
      // Key already exists
      return std::make_pair(iterator(it), false);
    } else {
      // Insert new key-value pair
      it = data_.insert(it, std::make_pair(value.first, value.second));
      return std::make_pair(iterator(it), true);
    }
  }

  std::pair<iterator, bool> insert(value_type&& value) {
    auto it = findInsertPos(value.first);
    if (it != data_.end() && !comp_(value.first, it->first)) {
      // Key already exists
      return std::make_pair(iterator(it), false);
    } else {
      // Insert new key-value pair
      it = data_.insert(it, std::make_pair(std::move(value.first),
                                            std::move(value.second)));
      return std::make_pair(iterator(it), true);
    }
  }

  template <typename P>
  std::pair<iterator, bool> insert(P&& value) {
    return insert(value_type(std::forward<P>(value)));
  }

  template <typename InputIt>
  void insert(InputIt first, InputIt last) {
    for (; first != last; ++first) {
      insert(*first);
    }
  }

  void insert(std::initializer_list<value_type> ilist) {
    for (const auto& pair : ilist) {
      insert(pair);
    }
  }

  iterator erase(const_iterator pos) {
    return iterator(data_.erase(pos.base()));
  }

  iterator erase(const_iterator first, const_iterator last) {
    return iterator(data_.erase(first.base(), last.base()));
  }

  size_type erase(const Key& key) {
    auto it = findInsertPos(key);
    if (it != data_.end() && !comp_(key, it->first)) {
      data_.erase(it);
      return 1;
    }
    return 0;
  }

  void clear() {
    data_.clear();
  }

  // Capacity
  bool empty() const {
    return data_.empty();
  }

  size_type size() const {
    return data_.size();
  }

  size_type max_size() const {
    return data_.max_size();
  }

  // Iterators
  iterator begin() {
    return iterator(data_.begin());
  }

  const_iterator begin() const {
    return const_iterator(data_.begin());
  }

  const_iterator cbegin() const {
    return const_iterator(data_.begin());
  }

  iterator end() {
    return iterator(data_.end());
  }

  const_iterator end() const {
    return const_iterator(data_.end());
  }

  const_iterator cend() const {
    return const_iterator(data_.end());
  }

  reverse_iterator rbegin() {
    return reverse_iterator(end());
  }

  const_reverse_iterator rbegin() const {
    return const_reverse_iterator(end());
  }

  const_reverse_iterator crbegin() const {
    return const_reverse_iterator(end());
  }

  reverse_iterator rend() {
    return reverse_iterator(begin());
  }

  const_reverse_iterator rend() const {
    return const_reverse_iterator(begin());
  }

  const_reverse_iterator crend() const {
    return const_reverse_iterator(begin());
  }

  // Observers
  key_compare key_comp() const {
    return comp_;
  }
};

} // namespace sta

// Specializations for structured bindings support
namespace std {

template <typename Key, typename Value>
struct tuple_size<sta::VectorMapReferenceProxy<Key, Value>>
  : std::integral_constant<size_t, 2> {};

template <size_t I, typename Key, typename Value>
struct tuple_element<I, sta::VectorMapReferenceProxy<Key, Value>> {
  static_assert(I < 2, "Index out of bounds for pair");
  using type = std::conditional_t<I == 0, const Key, Value>;
};

template <size_t I, typename Key, typename Value>
struct tuple_element<I, const sta::VectorMapReferenceProxy<Key, Value>> {
  static_assert(I < 2, "Index out of bounds for pair");
  using type = std::conditional_t<I == 0, const Key, const Value>;
};

} // namespace std

