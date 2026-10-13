// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2025-2026, The OpenROAD Authors

#include "LibertyBinaryWriter.hh"
#include "LibertyBinaryCommon.hh"
#include "LibertyParser.hh"
#include "Report.hh"
#include "StringUtil.hh"
#include "sta/Error.hh"

#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "util/gzstream.hh"

namespace {

// Write a scalar's raw little-endian bytes. Centralizes the cast/sizeof
// pairing so a mismatched address/size cannot creep into one call site.
template <typename T>
void
writeRaw(std::ostream *stream,
         const T &val)
{
  stream->write(reinterpret_cast<const char*>(&val), sizeof(val));
}

// Liberty stores numeric index/value sequences as quoted, comma separated
// strings. Match the text reader's tokenization and validation, leaving the
// original string intact when a token is invalid so its diagnostics survive.
bool
parseOptimisticFloatSeq(const std::string &str, std::vector<float> &floats)
{
  floats.clear();
  for (const std::string &token : sta::parseTokens(str, " ,{}")) {
    auto [value, valid] = sta::stringFloat(token);
    if (!valid)
      return false; // Not a float list.
    floats.push_back(value);
  }
  return !floats.empty();
}

// Only table-style attributes are converted to native floats. Converting any
// numeric-looking string would change the type of values whose consumers
// need a string (e.g. a mode named "1" or a bundle member named "INF").
bool
isFloatSeqAttr(const std::string &name)
{
  return name == "values"
    || name.starts_with("index_");
}

// Hash of the source file's bytes as stored on disk; false if it cannot be read.
bool
hashFile(const char *filename,
         uint64_t &hash)
{
  std::ifstream stream(filename, std::ios::binary);
  if (!stream)
    return false;
  hash = sta::LIBERTY_BINARY_HASH_OFFSET;
  char buffer[64 * 1024];
  while (stream.read(buffer, sizeof(buffer)) || stream.gcount() > 0)
    hash = sta::libertyBinaryHash(hash, buffer, stream.gcount());
  return true;
}

// True if the file, after any gzip decompression, starts with the binary
// liberty magic number.
bool
isLibertyBinaryFile(const char *filename)
{
  gzstream::igzstream stream(filename);
  if (!stream.is_open())
    return false;
  char magic[sta::LIBERTY_BINARY_MAGIC_SIZE];
  stream.read(magic, sizeof(magic));
  return std::string_view(magic, stream.gcount()) == sta::LIBERTY_BINARY_MAGIC;
}

} // namespace

namespace sta {

void
writeLibertyBinary(const char *in_filename,
                   const char *out_filename,
                   Report *report)
{
  // Opening the output truncates it, so writing over the source would destroy
  // it before it is parsed. equivalent() also catches a different spelling of
  // the same path or a link to it, and is false when the output does not exist.
  std::error_code error;
  if (std::filesystem::equivalent(in_filename, out_filename, error))
    report->error(1899, "output {} is the input liberty file.", out_filename);
  // parseLibertyFile reads binary liberty too, so a .blib input would be
  // re-encoded with the .blib itself recorded as the source.
  if (isLibertyBinaryFile(in_filename))
    report->error(1898, "{} is already a binary liberty file.", in_filename);

  // Hash the source before creating the output so an unreadable input leaves
  // nothing behind.
  uint64_t source_hash;
  if (!hashFile(in_filename, source_hash))
    throw FileNotReadable(in_filename);
  // Keep directory paths out of the metadata for reproducible, shareable files.
  std::string source_filename =
    std::filesystem::path(in_filename).filename().string();

  std::ofstream out_stream(out_filename, std::ios::binary);
  if (!out_stream)
    throw FileNotWritable(out_filename);

  try {
    LibertyBinaryWriter writer(&out_stream);
    // Write Magic and Version.
    out_stream.write(LIBERTY_BINARY_MAGIC, LIBERTY_BINARY_MAGIC_SIZE);
    uint32_t version = LIBERTY_BINARY_VERSION;
    writeRaw(&out_stream, version);
    // Source file and its hash, for traceability.
    uint32_t source_filename_length = source_filename.size();
    writeRaw(&out_stream, source_filename_length);
    out_stream.write(source_filename.data(), source_filename.size());
    writeRaw(&out_stream, source_hash);
    uint64_t string_table_offset = 0;
    uint64_t string_table_offset_location = out_stream.tellp();
    writeRaw(&out_stream, string_table_offset);

    parseLibertyFile(in_filename, &writer, report);

    // Write EOF tag.
    writeRaw(&out_stream, static_cast<uint8_t>(LibertyBinaryTag::EOF_TAG));

    // Backpatch the string table offset.
    string_table_offset = out_stream.tellp();
    out_stream.seekp(string_table_offset_location, out_stream.beg);
    writeRaw(&out_stream, string_table_offset);

    // Write the string table at the end of the file.
    out_stream.seekp(0, out_stream.end);
    uint32_t string_table_size = writer.string_table().size();
    writeRaw(&out_stream, string_table_size);
    for (const auto &entry : writer.string_table()) {
      uint32_t string_length = entry.first.size();
      writeRaw(&out_stream, string_length);
      out_stream.write(entry.first.c_str(), entry.first.size());
      writeRaw(&out_stream, entry.second);
    }
  }
  catch (...) {
    // A parse error leaves a partial file with a valid magic number behind.
    out_stream.close();
    std::remove(out_filename);
    throw;
  }

  // A failed write (e.g. disk full) silently poisons the stream, and the last
  // buffer is not flushed until close(), so check afterwards and remove the
  // corrupt file rather than reporting success.
  out_stream.close();
  if (out_stream.fail()) {
    std::remove(out_filename);
    report->error(1901, "error writing {}.", out_filename);
  }
}

LibertyBinaryWriter::LibertyBinaryWriter(std::ostream *stream) :
  stream_(stream)
{
}

void
LibertyBinaryWriter::begin(const LibertyGroup *group,
                           LibertyGroup *)
{
  depth_++;
  writeTag(LibertyBinaryTag::GROUP_BEGIN);
  writeLine(group->line());
  writeString(group->type());

  const LibertyAttrValueSeq &params = group->params();
  uint32_t param_count = params.size();
  writeRaw(stream_, param_count);
  for (const LibertyAttrValue *val : params)
    writeValue(val);
}

void
LibertyBinaryWriter::end(const LibertyGroup *group,
                         LibertyGroup *parent_group)
{
  writeTag(LibertyBinaryTag::GROUP_END);
  depth_--;
  // LibertyParser retains the whole group tree as it parses. Once a top-level
  // group (a cell, table template, etc. directly under the library) has been
  // fully serialized, its subtree is no longer needed, so release it to bound
  // peak memory. This mirrors LibertyReader::endCell clearing library_group,
  // and lets large libraries stream with ~one-cell memory instead of holding
  // the entire (uncompressed) file in RAM.
  if (depth_ == 1)
    parent_group->clear();
  // The parser pops the library group with no owner and the visitor is the
  // last to see it; the text reader's endLibrary deletes it the same way.
  else if (!parent_group)
    delete group;
}

void
LibertyBinaryWriter::visitAttr(const LibertySimpleAttr *attr)
{
  writeTag(LibertyBinaryTag::ATTR_SIMPLE);
  writeLine(attr->line());
  writeString(attr->name());
  uint32_t count = 1;
  writeRaw(stream_, count);
  writeValue(&attr->value());
}

void
LibertyBinaryWriter::visitAttr(const LibertyComplexAttr *attr)
{
  writeTag(LibertyBinaryTag::ATTR_COMPLEX);
  writeLine(attr->line());
  writeString(attr->name());

  const LibertyAttrValueSeq &values = attr->values();
  uint32_t count = values.size();
  writeRaw(stream_, count);
  bool float_seq_attr = isFloatSeqAttr(attr->name());
  std::vector<float> float_values;
  for (const LibertyAttrValue *val : values) {
    if (float_seq_attr
        && val->isString()
        && parseOptimisticFloatSeq(val->stringValue(), float_values)) {
      if (float_values.size() == 1)
        writeFloat(float_values[0]);
      else
        writeFloatSeq(float_values);
    }
    else
      writeValue(val);
  }
}

void
LibertyBinaryWriter::visitVariable(LibertyVariable *variable)
{
  writeTag(LibertyBinaryTag::VARIABLE);
  writeLine(variable->line());
  writeString(variable->variable());
  writeFloat(variable->value());
}

void
LibertyBinaryWriter::writeTag(LibertyBinaryTag tag)
{
  writeRaw(stream_, static_cast<uint8_t>(tag));
}

void
LibertyBinaryWriter::writeType(LibertyBinaryValueType type)
{
  writeRaw(stream_, static_cast<uint8_t>(type));
}

void
LibertyBinaryWriter::writeLine(int line)
{
  writeVarint(zigzagEncode(line - last_line_));
  last_line_ = line;
}

// Little-endian base 128; the high bit of each byte marks another byte.
void
LibertyBinaryWriter::writeVarint(std::uint32_t val)
{
  while (val >= 0x80) {
    writeRaw(stream_, static_cast<uint8_t>(val | 0x80));
    val >>= 7;
  }
  writeRaw(stream_, static_cast<uint8_t>(val));
}

void
LibertyBinaryWriter::writeString(std::string_view str)
{
  writeType(LibertyBinaryValueType::STRING);

  // Heterogeneous find so table hits (the common case) don't allocate a key.
  auto it = string_table_.find(str);
  uint32_t offset;
  if (it != string_table_.end())
    offset = it->second;
  else {
    offset = string_table_.size();
    string_table_.emplace(std::string(str), offset);
  }
  writeRaw(stream_, offset);
}

void
LibertyBinaryWriter::writeFloat(float val)
{
  writeType(LibertyBinaryValueType::FLOAT);
  writeRaw(stream_, val);
}

void
LibertyBinaryWriter::writeValue(const LibertyAttrValue *value)
{
  if (value->isString())
    writeString(value->stringValue());
  else if (value->isFloatSeq())
    writeFloatSeq(value->floatSeq());
  else if (value->isFloat())
    writeFloat(value->floatValue().first);
}

void
LibertyBinaryWriter::writeFloatSeq(const std::vector<float> &floats)
{
  writeType(LibertyBinaryValueType::FLOAT_SEQ);
  uint32_t count = floats.size();
  writeRaw(stream_, count);
  if (count > 0)
    stream_->write(reinterpret_cast<const char*>(floats.data()),
                   count * sizeof(float));
}

} // namespace sta
