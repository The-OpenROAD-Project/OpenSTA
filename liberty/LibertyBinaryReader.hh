// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025-2026, The OpenROAD Authors

#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "LibertyParser.hh"

namespace sta {

// Unchecked read primitives; LibertyBinaryReader bounds-checks with
// remaining()/inBounds() before every read so a malformed or foreign-format
// file is rejected rather than read out of bounds. Multi-byte values are in
// the host's byte order (LibertyBinaryCommon.hh asserts it is little-endian).
class BinaryCursor
{
public:
  BinaryCursor(const char *data,
               size_t size);
  uint8_t readU8();
  uint32_t readU32();
  uint64_t readU64();
  float readFloat();
  void readBytes(char *dest,
                 size_t len);
  void seek(size_t offset);
  void setPtr(const char *ptr);
  const char *current() const { return ptr_; }
  size_t offset() const { return size_t(ptr_ - start_); }
  size_t remaining() const { return ptr_ < end_ ? size_t(end_ - ptr_) : 0; }
  bool inBounds(size_t offset) const { return offset <= size_t(end_ - start_); }

private:
  const char *ptr_;
  const char *end_;
  const char *start_;
};

// Header fields recorded by write_liberty_binary for traceability.
struct LibertyBinaryHeader
{
  uint32_t version;
  std::string source_filename;
  uint64_t source_hash;
};

// Reads only the header. Errors like read_liberty on a file that is not
// binary liberty or is another format version.
LibertyBinaryHeader
readLibertyBinaryHeader(const char *filename,
                        Report *report);

class LibertyBinaryReader
{
public:
  LibertyBinaryReader(LibertyGroupVisitor *visitor,
                      std::string_view filename,
                      Report *report);
  ~LibertyBinaryReader();

  // Errors on a malformed file rather than returning.
  void read(std::istream *stream);
  // Parses the header at the cursor and returns the string table offset.
  // Errors on a bad magic, another version or a truncated header.
  static uint64_t readHeader(BinaryCursor &cursor,
                             LibertyBinaryHeader &header,
                             std::string_view filename,
                             Report *report);

private:
  // Walk the binary stream, driving the parser's builder methods so the
  // group/attribute ownership and visitor dispatch match the text reader.
  void readStatements();
  void readGroup();
  void readSimpleAttr();
  void readComplexAttr();
  void readVariable();
  // Source line of the statement being read (zigzag varint delta).
  int readLine();
  uint32_t readVarint();

  void readStringTable();
  // Type-tagged reads for structural fields.
  std::string readString();
  float readFloat();
  uint32_t readUInt32();
  // Payload reads; readValue has already consumed the type tag.
  std::string readStringIndex();
  float readFloatValue();
  LibertyAttrValue *readValue();
  // Counted value sequence; returns nullptr instead of allocating when empty.
  LibertyAttrValueSeq *readValues();
  // Error unless bytes remain before the end of the buffer.
  void require(size_t bytes);
  // Report::error throws, so these do not return.
  void corruptError();
  static void corruptError(std::string_view filename,
                           Report *report);
  static void versionError(std::string_view filename,
                           Report *report,
                           uint32_t version);

  LibertyParser parser_;
  BinaryCursor cursor_;
  std::vector<std::string> string_table_;
  // Nesting depth of the group being read.
  int group_depth_ = 0;
  // Line of the previous statement; statement lines are stored as deltas.
  int last_line_ = 0;
};

} // namespace sta
