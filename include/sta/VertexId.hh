// Parallax Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// All rights reserved.
// 
// No part of this document may be copied, transmitted or
// disclosed in any form or fashion without the express
// written consent of Parallax Software, Inc.

#pragma once

#include "ObjectId.hh"

// VertexId typedef for Networks to get/set on pins.

namespace sta {

typedef ObjectId VertexId;

static constexpr VertexId vertex_id_null = object_id_null;

} // Namespace
#endif
