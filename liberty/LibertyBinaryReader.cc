// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025-2026, The OpenROAD Authors

#include "LibertyBinaryReader.hh"
#include "ContainerHelpers.hh"
#include "LibertyBinaryCommon.hh"
#include "LibertyParser.hh"
#include "Report.hh"
#include "sta/Error.hh"

#include <cstring>
#include <istream>
#include <string_view>
#include <vector>

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
                                  std::uint32_t version)
{
  report->error(1902,
                "{} is binary liberty version {}; this build reads version {}. "
                "Regenerate it with write_liberty_binary.",
                filename, version, LIBERTY_BINARY_VERSION);
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
  std::uint64_t string_table_offset =
    readHeader(cursor_, header, parser_.filename(), parser_.report());

  // Reject offsets outside the file or inside the header rather than reading
  // out of bounds.
  if (string_table_offset < cursor_.offset()
      || !cursor_.inBounds(string_table_offset))
    corruptError();

  size_t body_offset = cursor_.offset();
  cursor_.seek(string_table_offset);
  readStringTable();
  // Records cannot consume string-table bytes, even if a terminator is missing.
  cursor_ = BinaryCursor(buffer.data() + body_offset,
                         string_table_offset - body_offset);

  // The text grammar, and therefore the writer, emits exactly one root group.
  require(1);
  if (static_cast<LibertyBinaryTag>(cursor_.readU8())
      != LibertyBinaryTag::GROUP_BEGIN)
    corruptError();
  readGroup();
  require(1);
  if (static_cast<LibertyBinaryTag>(cursor_.readU8())
        != LibertyBinaryTag::EOF_TAG
      || cursor_.remaining() != 0)
    corruptError();
}

std::uint64_t
LibertyBinaryReader::readHeader(BinaryCursor &cursor,
                                LibertyBinaryHeader &header,
                                std::string_view filename,
                                Report *report)
{
  if (cursor.remaining() < min_header_size)
    corruptError(filename, report);

  char magic[LIBERTY_BINARY_MAGIC_SIZE];
  cursor.readBytes(magic, LIBERTY_BINARY_MAGIC_SIZE);
  std::string_view magic_view(magic, LIBERTY_BINARY_MAGIC_SIZE);
  if (magic_view != LIBERTY_BINARY_MAGIC)
    corruptError(filename, report);

  header.version = cursor.readU32();
  if (header.version != LIBERTY_BINARY_VERSION)
    versionError(filename, report, header.version);

  std::uint32_t path_length = cursor.readU32();
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
    if (tag == LibertyBinaryTag::GROUP_END)
      return;
    if (tag == LibertyBinaryTag::GROUP_BEGIN)
      readGroup();
    else if (tag == LibertyBinaryTag::ATTR_SIMPLE)
      readSimpleAttr();
    else if (tag == LibertyBinaryTag::ATTR_COMPLEX)
      readComplexAttr();
    else if (tag == LibertyBinaryTag::VARIABLE)
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
  readUInt32(); // Consume count (always 1 in the format).
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

std::uint32_t
LibertyBinaryReader::readVarint()
{
  std::uint32_t val = 0;
  for (int shift = 0; shift < 32; shift += 7) {
    require(1);
    std::uint8_t byte = cursor_.readU8();
    val |= static_cast<std::uint32_t>(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0)
      return val;
  }
  corruptError();
  return 0;  // Unreachable; corruptError throws.
}

LibertyAttrValueSeq *
LibertyBinaryReader::readValues()
{
  std::uint32_t count = readUInt32();
  if (count == 0)
    return nullptr;
  if (count > cursor_.remaining() / min_value_size)
    corruptError();

  LibertyAttrValueSeq *values = new LibertyAttrValueSeq;
  values->reserve(count);
  try {
    for (std::uint32_t i = 0; i < count; i++)
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
      != LibertyBinaryValueType::STRING)
    corruptError();
  return readStringIndex();
}

std::string
LibertyBinaryReader::readStringIndex()
{
  require(4);
  std::uint32_t index = cursor_.readU32();
  if (index >= string_table_.size())
    corruptError();
  return string_table_[index];
}

void
LibertyBinaryReader::readStringTable()
{
  require(4);
  std::uint32_t size = cursor_.readU32();
  // Validate the count before allocating; each entry needs at least 8 bytes.
  if (size > cursor_.remaining() / min_string_entry_size)
    corruptError();
  string_table_.resize(size);
  std::vector<bool> filled(size, false);
  for (std::uint32_t i = 0; i < size; i++) {
    require(4);
    std::uint32_t len = cursor_.readU32();
    // len for the string bytes plus the 4-byte index that follows, compared
    // without adding so a huge len cannot wrap the sum.
    if (cursor_.remaining() < 4 || cursor_.remaining() - 4 < len)
      corruptError();
    const char *bytes = cursor_.current();
    cursor_.setPtr(bytes + len);
    // The writer stores each string's dense index; place the string there
    // because the writer iterates an unordered_map in arbitrary order.
    std::uint32_t index = cursor_.readU32();
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
      != LibertyBinaryValueType::FLOAT)
    corruptError();
  return readFloatValue();
}

float
LibertyBinaryReader::readFloatValue()
{
  require(4);
  return cursor_.readFloat();
}

std::uint32_t
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

  if (val_type == LibertyBinaryValueType::STRING)
    return parser_.makeAttrValueString(readStringIndex());
  else if (val_type == LibertyBinaryValueType::FLOAT)
    return parser_.makeAttrValueFloat(readFloatValue());
  else if (val_type == LibertyBinaryValueType::FLOAT_SEQ) {
    std::uint32_t count = readUInt32();
    // Divide rather than multiply so a huge count cannot wrap the size.
    if (count > cursor_.remaining() / sizeof(float))
      corruptError();
    size_t bytes = static_cast<size_t>(count) * sizeof(float);
    // The floats follow 1-byte tags, so they are neither aligned nor float
    // objects; copy the bytes out instead of reading through a float pointer.
    std::vector<float> seq(count);
    if (count > 0)
      std::memcpy(seq.data(), cursor_.current(), bytes);
    cursor_.setPtr(cursor_.current() + bytes);
    return parser_.makeAttrValueFloatSeq(std::move(seq));
  }
  corruptError();
  return nullptr; // Unreachable; corruptError throws.
}

} // namespace sta
