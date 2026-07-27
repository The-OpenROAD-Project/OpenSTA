// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// The origin of this software must not be misrepresented; you must not
// claim that you wrote the original software.
//
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// This notice may not be removed or altered from any source distribution.

// OpenROAD fork: analysis_corner support.

%{

#include "AnalysisCorner.hh"
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

%inline %{

void
define_analysis_corner_cmd(const char *name,
                           StringSeq liberty_min_files,
                           StringSeq liberty_max_files,
                           const char *spef_min_name,
                           const char *spef_max_name)
{
  AnalysisCorner *corner = Sta::sta()->makeAnalysisCorner(name);
  // Redefining with any data replaces the whole bundle; a bare redefine
  // preserves it (makeAnalysisCorner is find-or-create). Min/max always
  // arrive as a pair, so checking min suffices.
  if (!liberty_min_files.empty() || spef_min_name[0] != '\0') {
    corner->setLiberty(liberty_min_files, liberty_max_files);
    corner->setSpef(spef_min_name, spef_max_name);
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

%} // inline
