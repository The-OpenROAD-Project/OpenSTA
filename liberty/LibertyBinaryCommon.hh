// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025-2026, The OpenROAD Authors

#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

namespace sta {

// Multi-byte values are written and read in the host's byte order.
static_assert(std::endian::native == std::endian::little,
              "binary liberty format assumes a little-endian host");

// Magic number for binary liberty files.
constexpr char liberty_binary_magic[] = "STALIB01";
constexpr size_t liberty_binary_magic_size = sizeof(liberty_binary_magic) - 1;
// File layout: magic, version, source filename (u32 length + bytes), source hash
// (u64), string table offset (u64), records, string table.
// Bumped when the layout changes; the reader rejects other versions.
// 1: initial layout.
// 2: group, attribute and variable records carry the source line of the
//    statement after the tag, as a zigzag varint delta from the previous
//    statement's line.
// 3: the header records the source filename and the FNV-1a 64-bit hash of its
//    bytes, for traceability. Older writers stored an absolute path; new writers
//    store only the basename. The field encoding is unchanged.
constexpr uint32_t liberty_binary_version = 3;

// FNV-1a 64-bit hash of the source file's bytes as stored on disk (compressed
// when it is gzipped), so a .blib can be traced to the exact file it came from.
constexpr uint64_t liberty_binary_hash_offset = 14695981039346656037ull;
constexpr uint64_t liberty_binary_hash_prime = 1099511628211ull;

inline uint64_t
libertyBinaryHash(uint64_t hash,
                  const char *bytes,
                  size_t size)
{
  for (size_t i = 0; i < size; i++) {
    hash ^= static_cast<unsigned char>(bytes[i]);
    hash *= liberty_binary_hash_prime;
  }
  return hash;
}

// Zigzag mapping so the (rare) negative line delta also encodes compactly.
constexpr uint32_t
zigzagEncode(int32_t val)
{
  return (static_cast<uint32_t>(val) << 1) ^ static_cast<uint32_t>(val >> 31);
}

constexpr int32_t
zigzagDecode(uint32_t val)
{
  return static_cast<int32_t>(val >> 1) ^ -static_cast<int32_t>(val & 1);
}

enum class LibertyBinaryTag : uint8_t {
  group_begin = 1,
  group_end = 2,
  attr_simple = 3,
  attr_complex = 4,
  define = 5, // Reserved; no writer emits it.
  variable = 6,
  eof = 0
};

enum class LibertyBinaryValueType : uint8_t {
  value_string = 1,
  value_float = 2,
  value_int = 3,     // Reserved; no writer emits it.
  value_boolean = 4, // Reserved; no writer emits it.
  value_float_seq = 5
};

} // namespace sta
