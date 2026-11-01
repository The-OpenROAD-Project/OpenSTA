// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025-2026, The OpenROAD Authors

#include "LibertyBinaryReader.hh"

#include <cstring>
#include <istream>
#include <string_view>
#include <vector>

#include "ContainerHelpers.hh"
#include "Error.hh"
#include "LibertyBinaryCommon.hh"
#include "LibertyParser.hh"
#include "Report.hh"
#include "util/gzstream.hh"

namespace sta {

// Header is magic(8) + version(4) + source path length(4) + source path
// + source hash(8) + string table offset(8).
static constexpr size_t min_header_size = 32;
// The fields after the source path length, not counting the path itself.
static constexpr size_t header_tail_size = 16;
// readLibertyBinaryHeader reads this much of the file, enough for any path.
static constexpr size_t header_prefix_size = 64 * 1024;
// A string table entry is at least length(4) + index(4).
static constexpr size_t min_string_entry_size = 8;
// The smallest encoded value is a type byte + 4 bytes of payload.
static constexpr size_t min_value_size = 5;
// Far above normal Liberty nesting; bounds recursion for malformed input.
static constexpr int max_group_depth = 1000;

BinaryCursor::BinaryCursor(const char *data,
                           size_t size) :
  ptr_(data),
  end_(data + size),
  start_(data)
{
}

uint8_t
BinaryCursor::readU8()
{
  return static_cast<uint8_t>(*ptr_++);
}

uint32_t
BinaryCursor::readU32()
{
  uint32_t val;
  std::memcpy(&val, ptr_, sizeof(val));
  ptr_ += sizeof(val);
  return val;
}

uint64_t
BinaryCursor::readU64()
{
  uint64_t val;
  std::memcpy(&val, ptr_, sizeof(val));
  ptr_ += sizeof(val);
  return val;
}

float
BinaryCursor::readFloat()
{
  float val;
  std::memcpy(&val, ptr_, sizeof(val));
  ptr_ += sizeof(val);
  return val;
}

void
BinaryCursor::readBytes(char *dest,
                        size_t len)
{
  std::memcpy(dest, ptr_, len);
  ptr_ += len;
}

void
BinaryCursor::seek(size_t offset)
{
  ptr_ = start_ + offset;
}

void
BinaryCursor::setPtr(const char *ptr)
{
  ptr_ = ptr;
}

////////////////////////////////////////////////////////////////

LibertyBinaryReader::LibertyBinaryReader(LibertyGroupVisitor *visitor,
                                         std::string_view filename,
                                         Report *report) :
  parser_(filename, visitor, report),
  cursor_(nullptr, 0)
{
}

// A malformed file errors out of read() with groups still open, and the
// parser does not free them. After a complete read none are open.
LibertyBinaryReader::~LibertyBinaryReader()
{
  parser_.deleteGroups();
}

void
LibertyBinaryReader::corruptError()
{
  corruptError(parser_.filename(), parser_.report());
}

void
LibertyBinaryReader::corruptError(std::string_view filename,
                                  Report *report)
{
  report->error(1900, "{} is not a valid binary liberty file.", filename);
}

void
LibertyBinaryReader::versionError(std::string_view filename,
                                  Report *report,
                                  uint32_t version)
{
  report->error(1902,
                "{} is binary liberty version {}; this build reads version {}. "
                "Regenerate it with write_liberty_binary.",
                filename, version, liberty_binary_version);
}

void
LibertyBinaryReader::require(size_t bytes)
{
  if (cursor_.remaining() < bytes)
    corruptError();
}

void
LibertyBinaryReader::read(std::istream *stream)
{
  // Read sequentially so compressed files and pipes need no seeking.
  std::vector<char> buffer;
  char chunk[64 * 1024];
  while (stream->read(chunk, sizeof(chunk)) || stream->gcount() > 0)
    buffer.insert(buffer.end(), chunk, chunk + stream->gcount());
  if (stream->bad() || !stream->eof() || buffer.size() < min_header_size)
    corruptError();

  cursor_ = BinaryCursor(buffer.data(), buffer.size());

  LibertyBinaryHeader header;
  uint64_t string_table_offset =
    readHeader(cursor_, header, parser_.filename(), parser_.report());

  // Reject offsets outside the file or inside the header rather than reading
  // out of bounds.
  if (string_table_offset < cursor_.offset()
      || !cursor_.inBounds(string_table_offset))
    corruptError();

  size_t body_offset = cursor_.offset();
  cursor_.seek(string_table_offset);
  readStringTable();
  // The string table ends the file.
  if (cursor_.remaining() != 0)
    corruptError();
  // Records cannot consume string-table bytes, even if a terminator is missing.
  cursor_ = BinaryCursor(buffer.data() + body_offset,
                         string_table_offset - body_offset);

  // The text grammar, and therefore the writer, emits exactly one root group.
  require(1);
  if (static_cast<LibertyBinaryTag>(cursor_.readU8())
      != LibertyBinaryTag::group_begin)
    corruptError();
  readGroup();
  require(1);
  if (static_cast<LibertyBinaryTag>(cursor_.readU8())
        != LibertyBinaryTag::eof
      || cursor_.remaining() != 0)
    corruptError();
}

uint64_t
LibertyBinaryReader::readHeader(BinaryCursor &cursor,
                                LibertyBinaryHeader &header,
                                std::string_view filename,
                                Report *report)
{
  if (cursor.remaining() < min_header_size)
    corruptError(filename, report);

  char magic[liberty_binary_magic_size];
  cursor.readBytes(magic, liberty_binary_magic_size);
  std::string_view magic_view(magic, liberty_binary_magic_size);
  if (magic_view != liberty_binary_magic)
    corruptError(filename, report);

  header.version = cursor.readU32();
  if (header.version != liberty_binary_version)
    versionError(filename, report, header.version);

  uint32_t path_length = cursor.readU32();
  // Subtract rather than add so a huge path_length cannot wrap the sum.
  if (cursor.remaining() < header_tail_size
      || cursor.remaining() - header_tail_size < path_length)
    corruptError(filename, report);
  header.source_filename.assign(cursor.current(), path_length);
  cursor.setPtr(cursor.current() + path_length);
  header.source_hash = cursor.readU64();
  return cursor.readU64();
}

LibertyBinaryHeader
readLibertyBinaryHeader(const char *filename,
                        Report *report)
{
  gzstream::igzstream stream(filename);
  if (!stream.is_open())
    throw FileNotReadable(filename);
  std::vector<char> buffer(header_prefix_size);
  stream.read(buffer.data(), buffer.size());
  BinaryCursor cursor(buffer.data(), stream.gcount());
  LibertyBinaryHeader header;
  LibertyBinaryReader::readHeader(cursor, header, filename, report);
  return header;
}

void
LibertyBinaryReader::readStatements()
{
  while (true) {
    require(1);
    LibertyBinaryTag tag = static_cast<LibertyBinaryTag>(cursor_.readU8());
    if (tag == LibertyBinaryTag::group_end)
      return;
    if (tag == LibertyBinaryTag::group_begin)
      readGroup();
    else if (tag == LibertyBinaryTag::attr_simple)
      readSimpleAttr();
    else if (tag == LibertyBinaryTag::attr_complex)
      readComplexAttr();
    else if (tag == LibertyBinaryTag::variable)
      readVariable();
    else
      corruptError();
  }
}

void
LibertyBinaryReader::readGroup()
{
  if (++group_depth_ > max_group_depth)
    corruptError();

  int line = readLine();
  std::string type = readString();
  // groupBegin takes ownership of the params (nullptr when there are none).
  LibertyAttrValueSeq *params = readValues();
  parser_.groupBegin(std::move(type), params, line);

  readStatements();

  parser_.groupEnd();
  group_depth_--;
}

void
LibertyBinaryReader::readSimpleAttr()
{
  int line = readLine();
  std::string name = readString();
  // Simple attributes have exactly one value.
  if (readUInt32() != 1)
    corruptError();
  LibertyAttrValue *val = readValue();
  // makeSimpleAttr takes ownership of the value and dispatches to the visitor.
  parser_.makeSimpleAttr(std::move(name), val, line);
}

void
LibertyBinaryReader::readComplexAttr()
{
  int line = readLine();
  std::string name = readString();
  LibertyAttrValueSeq *values = readValues();
  if (!values)
    values = new LibertyAttrValueSeq;
  // makeComplexAttr takes ownership of the values and dispatches to the visitor.
  parser_.makeComplexAttr(std::move(name), values, line);
}

void
LibertyBinaryReader::readVariable()
{
  int line = readLine();
  std::string name = readString();
  float val = readFloat();
  parser_.makeVariable(std::move(name), val, line);
}

int
LibertyBinaryReader::readLine()
{
  last_line_ += zigzagDecode(readVarint());
  return last_line_;
}

uint32_t
LibertyBinaryReader::readVarint()
{
  uint32_t val = 0;
  for (int shift = 0; shift < 32; shift += 7) {
    require(1);
    uint8_t byte = cursor_.readU8();
    val |= static_cast<uint32_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0)
      return val;
  }
  corruptError();
  return 0;  // Unreachable; corruptError throws.
}

LibertyAttrValueSeq *
LibertyBinaryReader::readValues()
{
  uint32_t count = readUInt32();
  if (count == 0)
    return nullptr;
  if (count > cursor_.remaining() / min_value_size)
    corruptError();

  LibertyAttrValueSeq *values = new LibertyAttrValueSeq;
  values->reserve(count);
  try {
    for (uint32_t i = 0; i < count; i++)
      values->push_back(readValue());
  }
  catch (...) {
    // A corrupt value errors before the caller takes ownership of the list.
    deleteContents(values);
    delete values;
    throw;
  }
  return values;
}

std::string
LibertyBinaryReader::readString()
{
  require(1);
  if (static_cast<LibertyBinaryValueType>(cursor_.readU8())
      != LibertyBinaryValueType::value_string)
    corruptError();
  return readStringIndex();
}

std::string
LibertyBinaryReader::readStringIndex()
{
  require(4);
  uint32_t index = cursor_.readU32();
  if (index >= string_table_.size())
    corruptError();
  return string_table_[index];
}

void
LibertyBinaryReader::readStringTable()
{
  require(4);
  uint32_t size = cursor_.readU32();
  // Validate the count before allocating; each entry needs at least 8 bytes.
  if (size > cursor_.remaining() / min_string_entry_size)
    corruptError();
  string_table_.resize(size);
  std::vector<bool> filled(size, false);
  for (uint32_t i = 0; i < size; i++) {
    require(4);
    uint32_t len = cursor_.readU32();
    // len for the string bytes plus the 4-byte index that follows, compared
    // without adding so a huge len cannot wrap the sum.
    if (cursor_.remaining() < 4 || cursor_.remaining() - 4 < len)
      corruptError();
    const char *bytes = cursor_.current();
    cursor_.setPtr(bytes + len);
    // The writer stores each string's dense index; place the string there
    // because the writer iterates an unordered_map in arbitrary order.
    uint32_t index = cursor_.readU32();
    // Each index appears exactly once. With size entries and no repeats every
    // slot is filled, so a damaged index cannot silently rename strings.
    if (index >= string_table_.size() || filled[index])
      corruptError();
    filled[index] = true;
    string_table_[index].assign(bytes, len);
  }
}

float
LibertyBinaryReader::readFloat()
{
  require(1);
  if (static_cast<LibertyBinaryValueType>(cursor_.readU8())
      != LibertyBinaryValueType::value_float)
    corruptError();
  return readFloatValue();
}

float
LibertyBinaryReader::readFloatValue()
{
  require(4);
  return cursor_.readFloat();
}

uint32_t
LibertyBinaryReader::readUInt32()
{
  require(4);
  return cursor_.readU32();
}

LibertyAttrValue *
LibertyBinaryReader::readValue()
{
  require(1);
  LibertyBinaryValueType val_type =
    static_cast<LibertyBinaryValueType>(cursor_.readU8());

  if (val_type == LibertyBinaryValueType::value_string)
    return parser_.makeAttrValueString(readStringIndex());
  else if (val_type == LibertyBinaryValueType::value_float)
    return parser_.makeAttrValueFloat(readFloatValue());
  else if (val_type == LibertyBinaryValueType::value_float_seq) {
    uint32_t count = readUInt32();
    // An empty sequence would read back as a float. Divide rather than
    // multiply so a huge count cannot wrap the size.
    if (count == 0
        || count > cursor_.remaining() / sizeof(float))
      corruptError();
    size_t bytes = static_cast<size_t>(count) * sizeof(float);
    // The floats follow 1-byte tags, so they are neither aligned nor float
    // objects; copy the bytes out instead of reading through a float pointer.
    std::vector<float> seq(count);
    std::memcpy(seq.data(), cursor_.current(), bytes);
    cursor_.setPtr(cursor_.current() + bytes);
    return parser_.makeAttrValueFloatSeq(std::move(seq));
  }
  corruptError();
  return nullptr; // Unreachable; corruptError throws.
}

} // namespace sta
