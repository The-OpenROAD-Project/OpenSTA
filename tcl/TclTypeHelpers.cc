// OpenSTA, Static Timing Analyzer
// Copyright (c) 2025, Parallax Software, Inc.
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

StringSet *
tclListSetConstChar(Tcl_Obj *const source,
		    Tcl_Interp *interp)
{
  Tcl_Size argc;
  Tcl_Obj **argv;

  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK) {
    StringSet *set = new StringSet;
    for (int i = 0; i < argc; i++) {
      int length;
      const char *str = Tcl_GetStringFromObj(argv[i], &length);
      set->insert(str);
    }
    return set;
  }
  else
    return nullptr;
}

StringSeq *
tclListSeqConstChar(Tcl_Obj *const source,
		    Tcl_Interp *interp)
{
  Tcl_Size argc;
  Tcl_Obj **argv;

  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK) {
    StringSeq *seq = new StringSeq;
    for (int i = 0; i < argc; i++) {
      int length;
      const char *str = Tcl_GetStringFromObj(argv[i], &length);
      seq->push_back(str);
    }
    return seq;
  }
  else
    return nullptr;
}

StdStringSet *
tclListSetStdString(Tcl_Obj *const source,
		    Tcl_Interp *interp)
{
  Tcl_Size argc;
  Tcl_Obj **argv;

  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK) {
    StdStringSet *set = new StdStringSet;
    for (int i = 0; i < argc; i++) {
      int length;
      const char *str = Tcl_GetStringFromObj(argv[i], &length);
      set->insert(str);
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
    Tcl_SetResult(interp, const_cast<char*>(e.what()), TCL_STATIC);
  }
}

void
objectListNext(const char *list,
	       const char *type,
	       // Return values.
	       bool &type_match,
	       const char *&next)
{
  // Default return values (failure).
  type_match = false;
  next = nullptr;
  // _hexaddress_p_type
  const char *s = list;
  char ch = *s++;
  if (ch == '_') {
    while (*s && isxdigit(*s))
      s++;
    if ((s - list - 1) == sizeof(void*) * 2
	&& *s && *s++ == '_'
	&& *s && *s++ == 'p'
	&& *s && *s++ == '_') {
      const char *t = type;
      while (*s && *s != ' ') {
	if (*s != *t)
	  return;
	s++;
	t++;
      }
      type_match = true;
      if (*s)
	next = s + 1;
      else
	next = nullptr;
    }
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

  const char *inst_name = network->pathName(drvr);
  obj = Tcl_NewStringObj(inst_name, strlen(inst_name));
  Tcl_ListObjAppendElement(interp, list, obj);

  const char *from_name = arc->from()->name();
  obj = Tcl_NewStringObj(from_name, strlen(from_name));
  Tcl_ListObjAppendElement(interp, list, obj);

  const char *from_edge = arc->fromEdge()->to_string().c_str();
  obj = Tcl_NewStringObj(from_edge, strlen(from_edge));
  Tcl_ListObjAppendElement(interp, list, obj);

  const char *to_name = arc->to()->name();
  obj = Tcl_NewStringObj(to_name, strlen(to_name));
  Tcl_ListObjAppendElement(interp, list, obj);

  const char *to_edge = arc->toEdge()->to_string().c_str();
  obj = Tcl_NewStringObj(to_edge, strlen(to_edge));
  Tcl_ListObjAppendElement(interp, list, obj);

  const char *input_delay = delayAsString(gate.inputDelay(), sta, 3);
  obj = Tcl_NewStringObj(input_delay, strlen(input_delay));
  Tcl_ListObjAppendElement(interp, list, obj);

  return list;
}

ArcDcalcArg
arcDcalcArgTcl(Tcl_Obj *obj,
               Tcl_Interp *interp)
{
  Sta *sta = Sta::sta();
  sta->ensureGraph();
  int list_argc;
  Tcl_Obj **list_argv;
  if (Tcl_ListObjGetElements(interp, obj, &list_argc, &list_argv) == TCL_OK) {
    const char *input_delay = "0.0";
    int length;
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

} // namespace
