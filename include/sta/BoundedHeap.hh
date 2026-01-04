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
#include <functional>

namespace sta {

// BoundedHeap: A container that maintains the top N elements using a min-heap.
// This provides O(log n) insertion when the heap is full, O(1) when not full,
// and O(n log n) extraction of all elements. Useful for maintaining top K
// elements without storing all elements.
//
// The heap maintains the "worst" (minimum according to Compare) element at
// the root, so new elements that are better than the worst can replace it.
// For example, with Compare = std::greater<int>, this maintains the N largest
// values (greater values are "better").
//
// Template parameters:
//   T: The element type
//   Compare: Comparison function object type (default: std::less<T>)
//            For top N largest, use std::greater<T>
//            For top N smallest, use std::less<T>
template <typename T, typename Compare = std::less<T>>
class BoundedHeap {
public:
  using value_type = T;
  using size_type = size_t;
  using const_reference = const T&;
  using compare_type = Compare;

  // Constructors
  explicit BoundedHeap(size_type max_size,
                       const Compare& comp = Compare()) :
    max_size_(max_size),
    comp_(comp),
    min_heap_comp_(comp)
  {
    heap_.reserve(max_size);
  }

  // Copy constructor
  BoundedHeap(const BoundedHeap& other) :
    heap_(other.heap_),
    max_size_(other.max_size_),
    comp_(other.comp_),
    min_heap_comp_(other.comp_)
  {}

  // Assignment operator
  BoundedHeap& operator=(const BoundedHeap& other)
  {
    if (this != &other) {
      heap_ = other.heap_;
      max_size_ = other.max_size_;
      comp_ = other.comp_;
      min_heap_comp_ = MinHeapCompare(other.comp_);
    }
    return *this;
  }

  // Move constructor
  BoundedHeap(BoundedHeap&& other) noexcept :
    heap_(std::move(other.heap_)),
    max_size_(other.max_size_),
    comp_(std::move(other.comp_)),
    min_heap_comp_(comp_)
  {}

  // Move assignment operator
  BoundedHeap& operator=(BoundedHeap&& other) noexcept
  {
    if (this != &other) {
      heap_ = std::move(other.heap_);
      max_size_ = other.max_size_;
      comp_ = std::move(other.comp_);
      min_heap_comp_ = MinHeapCompare(comp_);
    }
    return *this;
  }

  void
  setMaxSize(size_t max_size)
  {
    max_size_ = max_size;
    heap_.reserve(max_size);
  }

  // Insert an element into the heap.
  // If the heap is not full, the element is added.
  // If the heap is full and the new element is better than the worst element,
  // the worst element is replaced. Otherwise, the element is ignored.
  // Returns true if the element was inserted, false if it was ignored.
  bool
  insert(const T& value) {
    if (heap_.size() < max_size_) {
      heap_.push_back(value);
      std::push_heap(heap_.begin(), heap_.end(), min_heap_comp_);
      return true;
    }
    else if (!heap_.empty()) {
      // When keeping N worst (smallest) values: if new value is smaller than worst,
      // we should keep it and remove the largest element to make room.
      // If new value is larger than worst, we reject it (already have worse values).
      // comp_(value, worst) is true when value < worst (value is smaller/worse)
      if (comp_(value, heap_.front())) {
        // New value is smaller than worst - find and replace the largest element
        auto max_it = std::max_element(heap_.begin(), heap_.end(), comp_);
        *max_it = value;
        // Rebuild heap since we modified an internal element
        std::make_heap(heap_.begin(), heap_.end(), min_heap_comp_);
        return true;
      }
      // Otherwise, new value is >= worst, so we already have worse values - reject it
    }
    return false;
  }

  // Insert an element using move semantics
  bool insert(T&& value)
  {
    if (heap_.size() < max_size_) {
      heap_.push_back(std::move(value));
      std::push_heap(heap_.begin(), heap_.end(), min_heap_comp_);
      return true;
    }
    else if (!heap_.empty()) {
      // When keeping N worst (smallest) values: if new value is smaller than worst,
      // we should keep it and remove the largest element to make room.
      // If new value is larger than worst, we reject it (already have worse values).
      // comp_(value, worst) is true when value < worst (value is smaller/worse)
      if (comp_(value, heap_.front())) {
        // New value is smaller than worst - find and replace the largest element
        auto max_it = std::max_element(heap_.begin(), heap_.end(), comp_);
        *max_it = std::move(value);
        // Rebuild heap since we modified an internal element
        std::make_heap(heap_.begin(), heap_.end(), min_heap_comp_);
        return true;
      }
      // Otherwise, new value is >= worst, so we already have worse values - reject it
    }
    return false;
  }

  // Extract all elements sorted from best to worst.
  // This destroys the heap structure but preserves the elements.
  std::vector<T> extract()
  {
    // Convert heap to sorted vector (best to worst)
    std::sort_heap(heap_.begin(), heap_.end(), min_heap_comp_);
    // Reverse to get best first (according to user's comparison)
    std::reverse(heap_.begin(), heap_.end());
    std::vector<T> result = std::move(heap_);
    heap_.clear();
    return result;
  }

  // Extract all elements sorted from best to worst (const version).
  // Creates a copy since we can't modify the heap.
  std::vector<T> extract() const
  {
    std::vector<T> temp_heap = heap_;
    std::sort_heap(temp_heap.begin(), temp_heap.end(), min_heap_comp_);
    std::reverse(temp_heap.begin(), temp_heap.end());
    return temp_heap;
  }

  // Get the worst element (the one that would be replaced next).
  // Requires !empty()
  const_reference worst() const
  {
    return heap_.front();
  }

  // Check if the heap is empty
  bool empty() const
  {
    return heap_.empty();
  }

  // Get the current number of elements in the heap
  size_type size() const
  {
    return heap_.size();
  }

  // Get the maximum size of the heap
  size_type max_size() const
  {
    return max_size_;
  }

  // Check if the heap is full
  bool full() const
  {
    return heap_.size() >= max_size_;
  }

  // Clear all elements from the heap
  void clear()
  {
    heap_.clear();
  }

  // Get the comparison function
  Compare compare() const
  {
    return comp_;
  }

private:
  std::vector<T> heap_;
  size_type max_size_;
  Compare comp_;

  // Helper comparator for min-heap: we want the worst element at root
  // so we can easily remove it when adding better elements.
  // This is the inverse of the user's comparison.
  struct MinHeapCompare
  {
    Compare comp_;
    explicit MinHeapCompare(const Compare& c) : comp_(c) {}
    bool operator()(const T& a, const T& b) const {
      return comp_(b, a);  // Inverted: worst is at root
    }
  };

  MinHeapCompare min_heap_comp_;
};

} // namespace sta

