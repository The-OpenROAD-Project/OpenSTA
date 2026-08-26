// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026, The OpenROAD Authors

// OpenROAD fork: analysis_corner support.

%{

#include "AnalysisCorner.hh"
#include "Property.hh"
#include "Scene.hh"
#include "Sta.hh"
#include "TclTypeHelpers.hh"

using namespace sta;

%}

%typemap(in) AnalysisCorner* {
  Tcl_Size length;
  const char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEqual(arg, "NULL"))
    $1 = nullptr;
  else {
    void *obj;
    if (SWIG_ConvertPtr($input, &obj, SWIGTYPE_p_AnalysisCorner, false) != TCL_OK) {
      tclArgError(interp, 3701, "{} is not an analysis_corner object.", arg);
      return TCL_ERROR;
    }
    $1 = reinterpret_cast<AnalysisCorner*>(obj);
  }
}

%typemap(out) AnalysisCorner* {
  AnalysisCorner *corner = $1;
  if (corner) {
    Tcl_Obj *obj = SWIG_NewInstanceObj(corner, SWIGTYPE_p_AnalysisCorner, false);
    Tcl_SetObjResult(interp, obj);
  }
  else
    Tcl_SetResult(interp, const_cast<char*>("NULL"), TCL_STATIC);
}

%typemap(out) AnalysisCornerSeq {
  seqTclList<AnalysisCornerSeq, AnalysisCorner>($1, SWIGTYPE_p_AnalysisCorner, interp);
}

%typemap(in) AnalysisCornerSeq* {
  $1 = tclListSeqPtr<AnalysisCorner*>($input, SWIGTYPE_p_AnalysisCorner, interp);
}

%inline %{

void
define_analysis_corner_cmd(const char *name,
                           StringSeq liberty_min_files,
                           StringSeq liberty_max_files,
                           const char *spef_min_name,
                           const char *spef_max_name,
                           StringSeq sdc_files)
{
  AnalysisCorner *corner = Sta::sta()->makeAnalysisCorner(name);
  // Redefining with any data replaces the whole bundle; a bare redefine
  // preserves it (makeAnalysisCorner is find-or-create). Min/max always
  // arrive as a pair, so checking min suffices.
  if (!liberty_min_files.empty() || spef_min_name[0] != '\0'
      || !sdc_files.empty()) {
    corner->setLiberty(liberty_min_files, liberty_max_files);
    corner->setSpef(spef_min_name, spef_max_name);
    corner->setSdc(sdc_files);
  }
}

StringSeq
analysis_corner_liberty_min(AnalysisCorner *corner)
{
  return corner->libertyMinFiles();
}

StringSeq
analysis_corner_liberty_max(AnalysisCorner *corner)
{
  return corner->libertyMaxFiles();
}

const char *
analysis_corner_spef_min(AnalysisCorner *corner)
{
  return corner->spefMinName().c_str();
}

const char *
analysis_corner_spef_max(AnalysisCorner *corner)
{
  return corner->spefMaxName().c_str();
}

StringSeq
analysis_corner_sdc(AnalysisCorner *corner)
{
  return corner->sdcFiles();
}

AnalysisCorner *
find_analysis_corner(const char *name)
{
  return Sta::sta()->findAnalysisCorner(name);
}

AnalysisCornerSeq
find_analysis_corners_matching(const char *pattern)
{
  return Sta::sta()->findAnalysisCorners(pattern);
}

const char *
analysis_corner_name(AnalysisCorner *corner)
{
  return corner->name().c_str();
}

void
set_scene_analysis_corner_cmd(Scene *scene,
                              AnalysisCorner *corner)
{
  Sta::sta()->setSceneAnalysisCorner(scene, corner);
}

// Scope SDC commands to a corner overlay Sdc (NULL restores mode scope).
void
set_cmd_analysis_corner_cmd(AnalysisCorner *corner)
{
  Sta::sta()->setCmdAnalysisCorner(corner);
}

AnalysisCorner *
cmd_analysis_corner()
{
  return Sta::sta()->cmdAnalysisCorner();
}

// Property system integration: built-in and user-defined properties on
// analysis_corner objects (get_property / define_property / set_property /
// get_analysis_corners -filter). The generic property storage in
// Properties is object-type keyed, so corners reuse it unmodified.
PropertyValue
analysis_corner_property(AnalysisCorner *corner,
                         const char *property)
{
  Properties &properties = Sta::sta()->properties();
  return properties.getProperty(corner, property);
}

void
define_analysis_corner_property_cmd(const char *property,
                                    const char *type)
{
  Properties &properties = Sta::sta()->properties();
  properties.defineProperty<AnalysisCorner>("analysis_corner", property, type);
}

void
set_analysis_corner_property_cmd(AnalysisCorner *corner,
                                 const char *property,
                                 const char *value)
{
  Properties &properties = Sta::sta()->properties();
  properties.setProperty(corner, "analysis_corner", property, value);
}

AnalysisCornerSeq
filter_analysis_corners(const char *filter_expression,
                        AnalysisCornerSeq *corners)
{
  return filterAnalysisCorners(filter_expression, corners, Sta::sta());
}

%} // inline
