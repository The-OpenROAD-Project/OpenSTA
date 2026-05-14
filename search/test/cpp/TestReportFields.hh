// Shared report-path field name constants used by the OpenSTA gtest suites.

#pragma once

#include "sta/StringUtil.hh"

namespace sta {

inline const StringSeq kAllReportFields = {
    "input_pin", "hierarchical_pin", "net",      "capacitance",
    "slew",      "fanout",           "variation", "src_attr"};

}  // namespace sta
