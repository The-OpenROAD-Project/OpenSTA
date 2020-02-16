// Parallax Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// All rights reserved.
// 
// No part of this document may be copied, transmitted or
// disclosed in any form or fashion without the express
// written consent of Parallax Software, Inc.

#pragma once

#include <cstdint>

namespace sta {

// ObjectId is block index and object index within the block.
typedef uint32_t ObjectId;
// Block index.
typedef uint32_t BlockIdx;
// Object index within a block.
typedef uint32_t ObjectIdx;

static constexpr BlockIdx block_idx_null = 0;
static constexpr ObjectId object_id_null = 0;
static constexpr ObjectIdx object_idx_null = 0;

} // Namespace
