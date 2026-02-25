# test
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2025, The OpenROAD Authors

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_hdl//dependency_support/com_github_westes_flex:flex.bzl", "genlex")
load("@rules_hdl//dependency_support/org_gnu_bison:bison.bzl", "genyacc")
load("//bazel:tcl_encode_sta.bzl", "tcl_encode_sta")
load("//bazel:tcl_wrap_cc.bzl", "tcl_wrap_cc")

package(
    default_visibility = ["//:__subpackages__"],
    #    features = ["layering_check"],
)

genlex(
    name = "LibExprLex",
    src = "liberty/LibExprLex.ll",
    out = "liberty/LibExprLex.cc",
    prefix = "LibExprLex_",
)

genyacc(
    name = "LibExprParse",
    src = "liberty/LibExprParse.yy",
    header_out = "liberty/LibExprParse.hh",
    prefix = "LibExprParse_",
    source_out = "liberty/LibExprParse.cc",
)

# Liberty Parser
genlex(
    name = "LibertyLex",
    src = "liberty/LibertyLex.ll",
    out = "liberty/LibertyLex.cc",
    prefix = "LibertyLex_",
)

genyacc(
    name = "LibertyParser",
    src = "liberty/LibertyParse.yy",
    extra_outs = ["liberty/LibertyLocation.hh"],
    header_out = "liberty/LibertyParse.hh",
    prefix = "LibertyParse_",
    source_out = "liberty/LibertyParse.cc",
)

# Spef scan/parse.
genlex(
    name = "SpefLex",
    src = "parasitics/SpefLex.ll",
    out = "parasitics/SpefLex.cc",
    prefix = "SpefLex_",
)

genyacc(
    name = "SpefParser",
    src = "parasitics/SpefParse.yy",
    extra_outs = ["parasitics/SpefLocation.hh"],
    header_out = "parasitics/SpefParse.hh",
    prefix = "SpefParse_",
    source_out = "parasitics/SpefParse.cc",
)

# Verilog scan/parse.
genlex(
    name = "VerilogLex",
    src = "verilog/VerilogLex.ll",
    out = "verilog/VerilogLex.cc",
    prefix = "VerilogLex_",
)

genyacc(
    name = "VerilogParser",
    src = "verilog/VerilogParse.yy",
    extra_outs = ["verilog/VerilogLocation.hh"],
    header_out = "verilog/VerilogParse.hh",
    prefix = "VerilogParse_",
    source_out = "verilog/VerilogParse.cc",
)

# sdf scan/parse.
genlex(
    name = "SdfLex",
    src = "sdf/SdfLex.ll",
    out = "sdf/SdfLex.cc",
    prefix = "SdfLex_",
)

genyacc(
    name = "SdfParser",
    src = "sdf/SdfParse.yy",
    extra_outs = ["sdf/SdfLocation.hh"],
    header_out = "sdf/SdfParse.hh",
    prefix = "SdfParse_",
    source_out = "sdf/SdfParse.cc",
)

genlex(
    name = "SaifLex",
    src = "power/SaifLex.ll",
    out = "power/SaifLex.cc",
    prefix = "SaifLex_",
)

genyacc(
    name = "SaifParser",
    src = "power/SaifParse.yy",
    extra_outs = ["power/SaifLocation.hh"],
    header_out = "power/SaifParse.hh",
    prefix = "SaifParse_",
    source_out = "power/SaifParse.cc",
)

# The order here is very important as the script to encode these relies on it
tcl_srcs = [
    "tcl/Util.tcl",
    "tcl/CmdUtil.tcl",
    "tcl/Init.tcl",
    "tcl/CmdArgs.tcl",
    "tcl/Property.tcl",
    "tcl/Sta.tcl",
    "tcl/Variables.tcl",
    "tcl/Splash.tcl",
    "graph/Graph.tcl",
    "liberty/Liberty.tcl",
    "network/Link.tcl",
    "network/Network.tcl",
    "network/NetworkEdit.tcl",
    "search/Search.tcl",
    "dcalc/DelayCalc.tcl",
    "parasitics/Parasitics.tcl",
    "power/Power.tcl",
    "sdc/Sdc.tcl",
    "sdf/Sdf.tcl",
    "verilog/Verilog.tcl",
]

exports_files([
    "etc/TclEncode.tcl",
])

exported_tcl = [
    "tcl/Util.tcl",
    "tcl/CmdUtil.tcl",
    "tcl/CmdArgs.tcl",
    "tcl/Property.tcl",
    "tcl/Splash.tcl",
    "tcl/Sta.tcl",
    "tcl/Variables.tcl",
    "sdc/Sdc.tcl",
    "sdf/Sdf.tcl",
    "search/Search.tcl",
    "dcalc/DelayCalc.tcl",
    "graph/Graph.tcl",
    "liberty/Liberty.tcl",
    "network/Network.tcl",
    "network/NetworkEdit.tcl",
    "parasitics/Parasitics.tcl",
    "power/Power.tcl",
]

filegroup(
    name = "tcl_scripts",
    srcs = exported_tcl,
    visibility = [
        "//:__subpackages__",
    ],
)

tcl_encode_sta(
    name = "StaTclInitVar",
    srcs = tcl_srcs,
    char_array_name = "tcl_inits",
)

genrule(
    name = "StaConfig",
    srcs = [],
    outs = ["util/StaConfig.hh"],
    cmd = """echo -e '
    #define STA_VERSION "2.7.0"
    #define STA_GIT_SHA1 "f21d4a3878e2531e3af4930818d9b5968aad9416"
    #define SSTA 0
    #define ZLIB_FOUND' > \"$@\"
    """,
    visibility = ["//:__subpackages__"],
)

filegroup(
    name = "sta_swig_files",
    srcs = [
        "app/StaApp.i",
        "dcalc/DelayCalc.i",
        "graph/Graph.i",
        "liberty/Liberty.i",
        "network/Network.i",
        "network/NetworkEdit.i",
        "parasitics/Parasitics.i",
        "power/Power.i",
        "sdc/Sdc.i",
        "sdf/Sdf.i",
        "search/Property.i",
        "search/Search.i",
        "spice/WriteSpice.i",
        "tcl/Exception.i",
        "tcl/StaTclTypes.i",
        "util/Util.i",
        "verilog/Verilog.i",
    ],
    visibility = ["//:__subpackages__"],
)

tcl_wrap_cc(
    name = "StaApp",
    srcs = [
        ":sta_swig_files",
    ],
    namespace_prefix = "sta",
    root_swig_src = "app/StaApp.i",
    swig_includes = [
        ".",
    ],
)

parser_cc = [
    # Liberty Expression Parser
    ":liberty/LibExprParse.cc",
    ":liberty/LibExprLex.cc",
    # Liberty Parser
    ":liberty/LibertyLex.cc",
    ":liberty/LibertyParse.cc",
    # Spef scan/parse.
    ":parasitics/SpefLex.cc",
    ":parasitics/SpefParse.cc",
    # Verilog scan/parse.
    ":verilog/VerilogLex.cc",
    ":verilog/VerilogParse.cc",
    # sdf scan/parse.
    ":sdf/SdfLex.cc",
    ":sdf/SdfParse.cc",
    # Saif scan/parse.
    ":power/SaifLex.cc",
    ":power/SaifParse.cc",
]

parser_headers = [
    # Liberty Expression Parser
    ":liberty/LibExprParse.hh",
    # Liberty Parser
    ":liberty/LibertyParse.hh",
    ":liberty/LibertyLocation.hh",
    # Spef scan/parse.
    ":parasitics/SpefParse.hh",
    ":parasitics/SpefLocation.hh",
    # Verilog scan/parse.
    ":verilog/VerilogParse.hh",
    ":verilog/VerilogLocation.hh",
    # sdf scan/parse.
    ":sdf/SdfParse.hh",
    ":sdf/SdfLocation.hh",
    # Saif scan/parse.
    ":power/SaifParse.hh",
    ":power/SaifLocation.hh",
]

cc_binary(
    name = "opensta",
    srcs = [
        "app/Main.cc",
        ":StaApp",
        ":StaTclInitVar",
        "//bazel:runfiles",
    ],
    copts = [
        "-Wno-error",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-Wno-cast-qual",  # typically from TCL swigging
        "-Wno-missing-braces",  # typically from TCL swigging
        "-Wredundant-decls",
        "-Wformat-security",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
    ],
    features = ["-use_header_modules"],
    includes = [
        "",
        "dcalc",
        "include/sta",
        "util",
    ],
    malloc = "@tcmalloc//tcmalloc",
    visibility = ["//visibility:public"],
    deps = [
        ":opensta_lib",
        "@rules_cc//cc/runfiles",
        "@tk_tcl//:tcl",
    ],
)

cc_library(
    name = "opensta_lib",
    srcs = parser_cc +
           parser_headers + glob(
        include = [
            "dcalc/*.cc",
            "dcalc/*.hh",
            "graph/*.cc",
            "liberty/*.cc",
            "liberty/*.hh",
            "network/*.cc",
            "parasitics/*.cc",
            "parasitics/*.hh",
            "power/*.cc",
            "power/*.hh",
            "sdc/*.cc",
            "sdc/*.hh",
            "sdf/*.cc",
            "sdf/*.hh",
            "search/*.cc",
            "search/*.hh",
            "spice/*.cc",
            "spice/*.hh",
            "tcl/*.cc",
            "util/*.cc",
            "util/*.hh",
            "verilog/*.cc",
            "verilog/*.hh",
        ],
        exclude = [
            "graph/Delay.cc",
            "liberty/LibertyExt.cc",
            "util/Machine*.cc",
        ],
    ) + [
        "app/StaMain.cc",
        "util/Machine.cc",
        ":StaConfig",
    ],
    #+ select({
    #        "@bazel_tools//src/conditions:windows": ["util/MachineWin32.cc"],
    #        "@bazel_tools//src/conditions:darwin": ["util/MachineApple.cc"],
    #        "@bazel_tools//src/conditions:linux": ["util/MachineLinux.cc"],
    #        "//conditions:default": ["util/MachineUnknown.cc"],
    #    })
    hdrs = glob(
        include = ["include/sta/*.hh"],
    ) + [
        # Required for swig
        "search/Tag.hh",
        "dcalc/ArcDcalcWaveforms.hh",
        "power/Power.hh",
        "power/VcdReader.hh",
        "power/SaifReader.hh",
        "sdf/SdfReader.hh",
        "sdf/ReportAnnotation.hh",
        "sdf/SdfWriter.hh",
        "search/Levelize.hh",
        "search/ReportPath.hh",
        "spice/WritePathSpice.hh",
        "dcalc/PrimaDelayCalc.hh",
    ],
    copts = [
        "-Wno-error",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-Wno-cast-qual",  # typically from TCL swigging
        "-Wno-missing-braces",  # typically from TCL swigging
        "-Wredundant-decls",
        "-Wformat-security",
        "-Wno-unused-parameter",
        "-Wno-sign-compare",
        "-fopenmp",
    ],
    features = [
        "-use_header_modules",
    ],
    includes = [
        ".",
        "dcalc",
        "include",
        "include/sta",
        "liberty",
        "parasitics",
        "power",
        "sdf",
        "util",
        "verilog",
    ],
    textual_hdrs = ["util/MachineLinux.cc"],
    visibility = ["//:__subpackages__"],
    deps = [
        "@cudd",
        "@eigen",
        "@openmp",
        "@rules_flex//flex:current_flex_toolchain",
        "@tk_tcl//:tcl",
        "@zlib",
    ],
)

filegroup(
    name = "tcl_util",
    srcs = [
        "tcl/Util.tcl",
    ],
    visibility = ["//visibility:public"],
)
