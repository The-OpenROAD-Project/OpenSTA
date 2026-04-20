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

#include "TclTypeHelpers.hh"

#include "Liberty.hh"
#include "Network.hh"
#include "Sta.hh"

namespace sta {

StringSeq
tclListStringSeq(Tcl_Obj *const source,
                 Tcl_Interp *interp)
{
  Tcl_Size argc;
  Tcl_Obj **argv;

  StringSeq seq;
  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK) {
    for (int i = 0; i < argc; i++) {
      Tcl_Size length;
      const char *arg = Tcl_GetStringFromObj(argv[i], &length);
      seq.emplace_back(arg);
    }
  }
  return seq;
}

StringSeq *
tclListStringSeqPtr(Tcl_Obj *const source,
                    Tcl_Interp *interp)
{
  Tcl_Size argc;
  Tcl_Obj **argv;

  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK) {
    StringSeq *seq = new StringSeq;
    for (int i = 0; i < argc; i++) {
      Tcl_Size length;
      const char *arg = Tcl_GetStringFromObj(argv[i], &length);
      seq->emplace_back(arg);
    }
    return seq;
  }
  else
    return nullptr;
}

StringSet *
tclListStringSet(Tcl_Obj *const source,
                 Tcl_Interp *interp)
{
  Tcl_Size argc;
  Tcl_Obj **argv;

  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK) {
    StringSet *set = new StringSet;
    for (int i = 0; i < argc; i++) {
      Tcl_Size length;
      const char *arg = Tcl_GetStringFromObj(argv[i], &length);
      set->insert(arg);
    }
    return set;
  }
  else
    return nullptr;
}

void
tclArgError(Tcl_Interp *interp,
            int id,
            const char *msg,
            const char *arg)
{
  // Swig does not add try/catch around arg parsing so this cannot use Report::error.
  try {
    Sta::sta()->report()->error(id, msg, arg);
  } catch (const std::exception &e) {
    Tcl_SetResult(interp, const_cast<char*>(e.what()), TCL_VOLATILE);
  }
}

Tcl_Obj *
tclArcDcalcArg(ArcDcalcArg &gate,
               Tcl_Interp *interp)
{
  Sta *sta = Sta::sta();
  const Network *network = sta->network();
  const Instance *drvr = network->instance(gate.drvrPin());
  const TimingArc *arc = gate.arc();

  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  Tcl_Obj *obj;

  std::string inst_name = network->pathName(drvr);
  obj = Tcl_NewStringObj(inst_name.data(), inst_name.size());
  Tcl_ListObjAppendElement(interp, list, obj);

  const std::string &from_name = arc->from()->name();
  obj = Tcl_NewStringObj(from_name.c_str(), from_name.size());
  Tcl_ListObjAppendElement(interp, list, obj);

  const std::string from_edge(arc->fromEdge()->to_string());
  obj = Tcl_NewStringObj(from_edge.c_str(), from_edge.size());
  Tcl_ListObjAppendElement(interp, list, obj);

  const std::string to_name = arc->to()->name();
  obj = Tcl_NewStringObj(to_name.c_str(), to_name.size());
  Tcl_ListObjAppendElement(interp, list, obj);

  const std::string to_edge(arc->toEdge()->to_string());
  obj = Tcl_NewStringObj(to_edge.c_str(), to_edge.size());
  Tcl_ListObjAppendElement(interp, list, obj);

  std::string input_delay = delayAsString(gate.inputDelay(), sta);
  obj = Tcl_NewStringObj(input_delay.c_str(), input_delay.size());
  Tcl_ListObjAppendElement(interp, list, obj);

  return list;
}

ArcDcalcArg
arcDcalcArgTcl(Tcl_Obj *obj,
               Tcl_Interp *interp)
{
  Sta *sta = Sta::sta();
  sta->ensureGraph();
  Tcl_Size list_argc;
  Tcl_Obj **list_argv;
  if (Tcl_ListObjGetElements(interp, obj, &list_argc, &list_argv) == TCL_OK) {
    const char *input_delay = "0.0";
    Tcl_Size length;
    if (list_argc == 6)
      input_delay = Tcl_GetStringFromObj(list_argv[5], &length);
    if (list_argc == 5 || list_argc == 6) {
      return makeArcDcalcArg(Tcl_GetStringFromObj(list_argv[0], &length),
                             Tcl_GetStringFromObj(list_argv[1], &length),
                             Tcl_GetStringFromObj(list_argv[2], &length),
                             Tcl_GetStringFromObj(list_argv[3], &length),
                             Tcl_GetStringFromObj(list_argv[4], &length),
                             input_delay, sta);
    }
    else
      sta->report()->warn(2140, "Delay calc arg requires 5 or 6 args.");
  }
  return ArcDcalcArg();
}

} // namespace sta
