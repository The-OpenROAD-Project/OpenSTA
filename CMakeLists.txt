# OpenSTA, Static Timing Analyzer
# Copyright (c) 2024, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required (VERSION 3.9)
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
# Use standard target names
cmake_policy(SET CMP0078 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.14)
# Allows SWIG_MODULE_NAME to be set
cmake_policy(SET CMP0086 NEW)
endif()

project(STA VERSION 2.6.0
  LANGUAGES CXX
)

option(CUDD_DIR "CUDD BDD package directory")
option(USE_TCL_READLINE "Use TCL readline package" ON)
option(USE_SANITIZE "Compile with santize address enabled")

# Turn on to debug compiler args.
set(CMAKE_VERBOSE_MAKEFILE OFF)

set(STA_HOME ${PROJECT_SOURCE_DIR})
message(STATUS "STA version: ${PROJECT_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC STA_GIT_SHA1)
message(STATUS "STA git sha: ${STA_GIT_SHA1}")

# Default to bulding optimnized/release executable.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RELEASE)
endif()

message(STATUS "System name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build CXX_FLAGS: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

################################################################
#
# Source files.
#
################################################################

set(STA_SOURCE
  app/StaMain.cc
  
  dcalc/ArcDelayCalc.cc
  dcalc/ArcDcalcWaveforms.cc
  dcalc/ArnoldiDelayCalc.cc
  dcalc/ArnoldiReduce.cc
  dcalc/CcsCeffDelayCalc.cc
  dcalc/DcalcAnalysisPt.cc
  dcalc/DelayCalc.cc
  dcalc/DelayCalcBase.cc
  dcalc/DmpCeff.cc
  dcalc/DmpDelayCalc.cc
  dcalc/FindRoot.cc
  dcalc/GraphDelayCalc.cc
  dcalc/LumpedCapDelayCalc.cc
  dcalc/NetCaps.cc
  dcalc/ParallelDelayCalc.cc
  dcalc/PrimaDelayCalc.cc
  dcalc/UnitDelayCalc.cc
  
  graph/DelayFloat.cc
  graph/DelayNormal1.cc
  graph/DelayNormal2.cc
  graph/Graph.cc
  graph/GraphCmp.cc
  
  liberty/EquivCells.cc
  liberty/FuncExpr.cc
  liberty/InternalPower.cc
  liberty/LeakagePower.cc
  liberty/Liberty.cc
  liberty/LibertyBuilder.cc
  liberty/LibertyExpr.cc
  liberty/LibertyExprPvt.hh
  liberty/LibertyParser.cc
  liberty/LibertyReader.cc
  liberty/LibertyWriter.cc
  liberty/LinearModel.cc
  liberty/Sequential.cc
  liberty/TableModel.cc
  liberty/TimingArc.cc
  liberty/TimingModel.cc
  liberty/TimingRole.cc
  liberty/Units.cc
  liberty/Wireload.cc
  
  network/ConcreteLibrary.cc
  network/ConcreteNetwork.cc
  network/HpinDrvrLoad.cc
  network/Network.cc
  network/NetworkCmp.cc
  network/ParseBus.cc
  network/PortDirection.cc
  network/SdcNetwork.cc
  network/VerilogNamespace.cc
  
  parasitics/ConcreteParasitics.cc
  parasitics/EstimateParasitics.cc
  parasitics/Parasitics.cc
  parasitics/ReduceParasitics.cc
  parasitics/ReportParasiticAnnotation.cc
  parasitics/SpefNamespace.cc
  parasitics/SpefReader.cc
  parasitics/SpefReaderPvt.hh
  
  sdc/Clock.cc
  sdc/ClockGatingCheck.cc
  sdc/ClockGroups.cc
  sdc/ClockInsertion.cc
  sdc/ClockLatency.cc
  sdc/CycleAccting.cc
  sdc/DataCheck.cc
  sdc/DeratingFactors.cc
  sdc/DisabledPorts.cc
  sdc/ExceptionPath.cc
  sdc/InputDrive.cc
  sdc/PinPair.cc
  sdc/PortDelay.cc
  sdc/PortExtCap.cc
  sdc/Sdc.cc
  sdc/SdcGraph.cc
  sdc/SdcCmdComment.cc
  sdc/WriteSdc.cc
  
  sdf/ReportAnnotation.cc
  sdf/SdfReader.cc
  sdf/SdfWriter.cc
  
  search/Bdd.cc
  search/Bfs.cc
  search/CheckMaxSkews.cc
  search/CheckMinPeriods.cc
  search/CheckMinPulseWidths.cc
  search/CheckCapacitanceLimits.cc
  search/CheckFanoutLimits.cc
  search/CheckSlewLimits.cc
  search/CheckTiming.cc
  search/ClkInfo.cc
  search/ClkLatency.cc
  search/ClkNetwork.cc
  search/ClkSkew.cc
  search/Corner.cc
  search/Crpr.cc
  search/FindRegister.cc
  search/GatedClk.cc
  search/Genclks.cc
  search/Latches.cc
  search/Levelize.cc
  search/MakeTimingModel.cc
  search/Path.cc
  search/PathAnalysisPt.cc
  search/PathEnd.cc
  search/PathEnum.cc
  search/PathEnumed.cc
  search/PathExpanded.cc
  search/PathGroup.cc
  search/PathRef.cc
  search/PathVertex.cc
  search/PathVertexRep.cc
  search/Property.cc
  search/ReportPath.cc
  search/Search.cc
  search/SearchPred.cc
  search/Sim.cc
  search/Sta.cc
  search/StaState.cc
  search/Tag.cc
  search/TagGroup.cc
  search/VertexVisitor.cc
  search/VisitPathEnds.cc
  search/VisitPathGroupVertices.cc
  search/WorstSlack.cc

  spice/WritePathSpice.cc
  spice/WriteSpice.cc
  spice/Xyce.cc

  power/Power.cc
  power/ReadVcdActivities.cc
  power/SaifReader.cc
  power/Vcd.cc
  power/VcdReader.cc

  util/Debug.cc
  util/DispatchQueue.cc
  util/Error.cc
  util/Fuzzy.cc
  util/Hash.cc
  util/Machine.cc
  util/MinMax.cc
  util/PatternMatch.cc
  util/Report.cc
  util/ReportStd.cc
  util/ReportTcl.cc
  util/RiseFallMinMax.cc
  util/RiseFallValues.cc
  util/Stats.cc
  util/StringSeq.cc
  util/StringSet.cc
  util/StringUtil.cc
  util/TokenParser.cc
  util/Transition.cc
  
  verilog/VerilogReader.cc
  verilog/VerilogWriter.cc
  )

# Source files.
set(STA_TCL_FILES
  tcl/Init.tcl
  tcl/Util.tcl
  tcl/CmdArgs.tcl
  tcl/CmdUtil.tcl
  tcl/Property.tcl
  tcl/Sta.tcl
  tcl/Splash.tcl
  tcl/Variables.tcl
  dcalc/DelayCalc.tcl
  graph/Graph.tcl
  liberty/Liberty.tcl
  network/Link.tcl
  network/Network.tcl
  network/NetworkEdit.tcl
  parasitics/Parasitics.tcl
  power/Power.tcl
  sdc/Sdc.tcl
  sdf/Sdf.tcl
  search/Search.tcl
  spice/WriteSpice.tcl
  verilog/Verilog.tcl
  )

################################################################
#
# Flex/bison scanner/parsers
#
################################################################

# Earlier versions of flex use 'register' declarations that are illegal in c++17.
#find_package(FLEX 2.6.4)
find_package(FLEX)
find_package(BISON)

# LibertyExpr scan/parse.
flex_target(LibertyExprLex ${STA_HOME}/liberty/LibertyExprLex.ll ${CMAKE_CURRENT_BINARY_DIR}/LibertyExprLex.cc
  COMPILE_FLAGS --prefix=LibertyExprLex_)
bison_target(LibertyExprParser ${STA_HOME}/liberty/LibertyExprParse.yy ${CMAKE_CURRENT_BINARY_DIR}/LibertyExprParse.cc
  COMPILE_FLAGS --name-prefix=LibertyExprParse_
  )
add_flex_bison_dependency(LibertyExprLex LibertyExprParser)

# Liberty scan/parse.
flex_target(LibertyLex ${STA_HOME}/liberty/LibertyLex.ll ${CMAKE_CURRENT_BINARY_DIR}/LibertyLex.cc
  COMPILE_FLAGS --prefix=LibertyLex_
  )
bison_target(LibertyParser ${STA_HOME}/liberty/LibertyParse.yy ${CMAKE_CURRENT_BINARY_DIR}/LibertyParse.cc
  COMPILE_FLAGS "--name-prefix=LibertyParse_ -v"
  )
add_flex_bison_dependency(LibertyLex LibertyParser)

# Spef scan/parse.
flex_target(SpefLex ${STA_HOME}/parasitics/SpefLex.ll ${CMAKE_CURRENT_BINARY_DIR}/SpefLex.cc
  COMPILE_FLAGS --prefix=SpefLex_
  )
bison_target(SpefParser ${STA_HOME}/parasitics/SpefParse.yy ${CMAKE_CURRENT_BINARY_DIR}/SpefParse.cc
  COMPILE_FLAGS --name-prefix=SpefParse_
  )
add_flex_bison_dependency(SpefLex SpefParser)

# Verilog scan/parse.
flex_target(VerilogLex ${STA_HOME}/verilog/VerilogLex.ll ${CMAKE_CURRENT_BINARY_DIR}/VerilogLex.cc
  COMPILE_FLAGS --prefix=VerilogLex_
  )
bison_target(VerilogParser ${STA_HOME}/verilog/VerilogParse.yy ${CMAKE_CURRENT_BINARY_DIR}/VerilogParse.cc
  COMPILE_FLAGS --name-prefix=VerilogParse_
  )
add_flex_bison_dependency(VerilogLex VerilogParser)

# Sdf scan/parse.
flex_target(SdfLex ${STA_HOME}/sdf/SdfLex.ll ${CMAKE_CURRENT_BINARY_DIR}/SdfLex.cc
  COMPILE_FLAGS --prefix=SdfLex_
  )
bison_target(SdfParser ${STA_HOME}/sdf/SdfParse.yy ${CMAKE_CURRENT_BINARY_DIR}/SdfParse.cc
  COMPILE_FLAGS --name-prefix=SdfParse_
  )
add_flex_bison_dependency(SdfLex SdfParser)


# Saif scan/parse.
flex_target(SaifLex ${STA_HOME}/power/SaifLex.ll ${CMAKE_CURRENT_BINARY_DIR}/SaifLex.cc
  COMPILE_FLAGS --prefix=SaifLex_
  )
bison_target(SaifParser ${STA_HOME}/power/SaifParse.yy ${CMAKE_CURRENT_BINARY_DIR}/SaifParse.cc
  COMPILE_FLAGS --name-prefix=SaifParse_
  )
add_flex_bison_dependency(SaifLex SaifParser)

################################################################

set(STA_TCL_INIT ${CMAKE_CURRENT_BINARY_DIR}/StaTclInitVar.cc)

# TCL files included as part of the executable are shoved into StaTclInitVar.cc.
# These files are encoded and shipped as part of the executable 
# so that they do not have to be installed on the client host.
add_custom_command(OUTPUT ${STA_TCL_INIT}
  COMMAND etc/TclEncode.tcl ${STA_TCL_INIT} tcl_inits ${STA_TCL_FILES}
  WORKING_DIRECTORY ${STA_HOME}
  DEPENDS ${STA_TCL_FILES} etc/TclEncode.tcl
  )

################################################################
#
# Library dependencies
#
################################################################

# OSX tcl is deprecated and prints a warning, so look for a user installed
# version before using the system version.
# I tried to override the library search order instead but failed.
# CMAKE_FIND_FRAMEWORK LAST bypasses the version in the framework directory
# but not the one in /usr/lib.
# This calls cmake/FindTCL.cmake
# Do not use REQUIRED because it also requires TK, which is not used by OpenSTA.
find_package(TCL)

# Referenced by util/StaConfig.hh.cmake
set(TCL_READLINE 0)
# check for tclReadline
if (USE_TCL_READLINE)
  set(TCL_READLINE_POSSIBLE_NAMES
    tclreadline-2.1.0
    tclreadline-2.3.2
    tclreadline-2.3.6
    tclreadline-2.3.7
    tclreadline-2.3.8
    )
  find_library(TCL_READLINE_LIBRARY
    NAMES tclreadline ${TCL_READLINE_POSSIBLE_NAMES}
    PATHS ${TCL_LIB_PATHS}
    )
  if (TCL_READLINE_LIBRARY)
    message(STATUS "TCL readline library: ${TCL_READLINE_LIBRARY}")
  else()
    message(STATUS "TCL readline library: NOT FOUND")
  endif()

  find_path(TCL_READLINE_INCLUDE tclreadline.h)
  if (TCL_READLINE_INCLUDE)
    message(STATUS "TCL readline header: ${TCL_READLINE_INCLUDE}/tclreadline.h")
  else()
    message(STATUS "TCL readline header: NOT FOUND")
  endif()

  if (TCL_READLINE_LIBRARY AND TCL_READLINE_INCLUDE)
    set(TCL_READLINE 1)
  endif()
endif()

# Zlib
include(FindZLIB)

find_package(Threads)

find_package(Eigen3 REQUIRED)

################################################################
#
# Locate CUDD bdd package.
#
find_library(CUDD_LIB
  NAME cudd
  PATHS ${CUDD_DIR}
  PATH_SUFFIXES lib lib/cudd cudd/.libs
  )
if (CUDD_LIB)
  message(STATUS "CUDD library: ${CUDD_LIB}")
  get_filename_component(CUDD_LIB_DIR "${CUDD_LIB}" PATH)
  get_filename_component(CUDD_LIB_PARENT1 "${CUDD_LIB_DIR}" PATH)
  find_file(CUDD_HEADER cudd.h
    PATHS ${CUDD_LIB_PARENT1} ${CUDD_LIB_PARENT1}/include ${CUDD_LIB_PARENT1}/include/cudd)
  if (CUDD_HEADER)
    get_filename_component(CUDD_INCLUDE "${CUDD_HEADER}" PATH)
    message(STATUS "CUDD header: ${CUDD_HEADER}")
  else()
    message(STATUS "CUDD header: not found")
  endif()
else()
  message(STATUS "CUDD library: not found")
endif()

if("${SSTA}" STREQUAL "")
  set(SSTA 0)
endif()
message(STATUS "SSTA: ${SSTA}")

# configure a header file to pass some of the CMake settings
configure_file(${STA_HOME}/util/StaConfig.hh.cmake
  ${STA_HOME}/include/sta/StaConfig.hh
  )

###########################################################
# Swig
###########################################################

find_package(SWIG 3.0 REQUIRED)
include(UseSWIG)

set(STA_SWIG_FILE app/StaApp.i)

set_property(SOURCE ${STA_SWIG_FILE}
  PROPERTY CPLUSPLUS ON
)
# Ubuntu 18.04 cmake version 3.10.2 does not support the
# COMPILE_OPTIONS and INCLUDE_DIRECTORIES properties, so cram
# them into SWIG_FLAGS for the time being.
set_property(SOURCE ${STA_SWIG_FILE}
  PROPERTY SWIG_FLAGS
  -module sta
  -namespace -prefix sta
  -I${STA_HOME}
)

set(SWIG_FILES
  ${STA_HOME}/dcalc/DelayCalc.i
  ${STA_HOME}/graph/Graph.i
  ${STA_HOME}/liberty/Liberty.i
  ${STA_HOME}/network/Network.i
  ${STA_HOME}/network/NetworkEdit.i
  ${STA_HOME}/parasitics/Parasitics.i
  ${STA_HOME}/power/Power.i
  ${STA_HOME}/sdc/Sdc.i
  ${STA_HOME}/sdf/Sdf.i
  ${STA_HOME}/search/Search.i
  ${STA_HOME}/spice/WriteSpice.i
  ${STA_HOME}/tcl/Exception.i
  ${STA_HOME}/tcl/StaTclTypes.i
  ${STA_HOME}/util/Util.i
  ${STA_HOME}/verilog/Verilog.i
  )

set_property(SOURCE ${STA_SWIG_FILE}
  PROPERTY DEPENDS ${SWIG_FILES}
)

swig_add_library(sta_swig
  LANGUAGE tcl
  TYPE STATIC
  SOURCES ${STA_SWIG_FILE}
)

get_target_property(STA_SWIG_CXX_FILE sta_swig SOURCES)

set_source_files_properties(${STA_SWIG_CXX_FILE}
  PROPERTIES
  # No simple way to modify the swig template that emits code full of warnings
  # so suppress them.
  COMPILE_OPTIONS "-Wno-cast-qual;-Wno-missing-braces;-Wno-deprecated-declarations"
  )

target_link_libraries(sta_swig
  PUBLIC
  OpenSTA
)

# result build/CMakeFiles/sta_swig.dir/StaAppTCL_wrap.cxx
target_include_directories(sta_swig
  PUBLIC
  include/sta

  PRIVATE
  ${STA_HOME}
  ${TCL_INCLUDE_PATH}
)

###########################################################
# Library
###########################################################

# compatibility with configure
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${STA_HOME}/app)

add_library(OpenSTA)

target_sources(OpenSTA
  PRIVATE
  ${STA_SOURCE}

  ${STA_TCL_INIT}

  ${FLEX_LibertyExprLex_OUTPUTS}
  ${BISON_LibertyExprParser_OUTPUTS}
  ${FLEX_LibertyLex_OUTPUTS}
  ${BISON_LibertyParser_OUTPUTS}

  ${FLEX_SpefLex_OUTPUTS}
  ${BISON_SpefParser_OUTPUTS}

  ${FLEX_SdfLex_OUTPUTS}
  ${BISON_SdfParser_OUTPUTS}

  ${FLEX_VerilogLex_OUTPUTS}
  ${BISON_VerilogParser_OUTPUTS}

  ${FLEX_SaifLex_OUTPUTS}
  ${BISON_SaifParser_OUTPUTS}
)

target_link_libraries(OpenSTA
  Eigen3::Eigen
  ${TCL_LIBRARY}
  ${CMAKE_THREAD_LIBS_INIT}
  ${CUDD_LIB}
  )

if (ZLIB_LIBRARIES)
  target_link_libraries(OpenSTA ${ZLIB_LIBRARIES})
endif()

target_include_directories(OpenSTA
  PUBLIC
  include
  ${TCL_INCLUDE_PATH}

  PRIVATE
  include/sta
  ${STA_HOME}
  ${CUDD_INCLUDE}
  )

if (TCL_READLINE)
  target_link_libraries(OpenSTA ${TCL_READLINE_LIBRARY})
  target_include_directories(OpenSTA
    PUBLIC
    ${TCL_READLINE_INCLUDE})
endif()

# common to gcc/clang
set(CXX_FLAGS -Wall -Wextra -pedantic -Wcast-qual -Wredundant-decls -Wformat-security)

if(USE_SANITIZE)
  message(STATUS "Sanitize: ${USE_SANITIZE}")
  set(CXX_FLAGS "${CXX_FLAGS};-fsanitize=address;-fno-omit-frame-pointer;-fno-common")
  set(CMAKE_EXE_LINKER_FLAGS "-fsanitize=address")
endif()

target_compile_options(OpenSTA
  PRIVATE
  $<$<CXX_COMPILER_ID:GNU>:${CXX_FLAGS}>
  $<$<CXX_COMPILER_ID:Clang>:${CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments>
  $<$<CXX_COMPILER_ID:AppleClang>:${CXX_FLAGS} -Wno-gnu-zero-variadic-macro-arguments>
  )

# Disable compiler specific extensions like gnu++11.
set_target_properties(OpenSTA PROPERTIES CXX_EXTENSIONS OFF)
target_compile_features(OpenSTA PUBLIC cxx_std_17)

###########################################################
# Executable
###########################################################

# compatibility with configure
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${STA_HOME}/app)

# Note executable and lib name cannot be the same because
# on osx something is case insensitive. Using STA for the
# lib name results in "No rule to make target ../depend.
add_executable(sta app/Main.cc)

target_link_libraries(sta
  sta_swig
  OpenSTA
  )

message(STATUS "STA executable: ${STA_HOME}/app/sta")

################################################################
# Install
# cmake .. -DCMAKE_INSTALL_PREFIX=<prefix_path>

# executable
install(TARGETS sta DESTINATION bin)

# library
install(TARGETS OpenSTA DESTINATION lib)

# include
install(DIRECTORY include/sta  DESTINATION include)

################################################################

add_custom_target(sta_tags etags -o TAGS
  ${STA_SOURCE}
  */*.hh
  include/sta/*.hh
  ${SWIG_FILES}
  ${STA_TCL_FILES}
  ${SWIG_TCL_FILES}
  WORKING_DIRECTORY ${STA_HOME}
  )
