// OpenSTA, Static Timing Analyzer
// Copyright (c) 2024, Parallax Software, Inc.
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

// Swig TCL input/output type parsers.
%{

#include "Machine.hh"
#include "StringUtil.hh"
#include "StringSet.hh"
#include "StringSeq.hh"
#include "PatternMatch.hh"
#include "Vector.hh"
#include "Network.hh"
#include "Liberty.hh"
#include "FuncExpr.hh"
#include "TimingArc.hh"
#include "TableModel.hh"
#include "TimingRole.hh"
#include "Graph.hh"
#include "NetworkClass.hh"
#include "Clock.hh"
#include "Corner.hh"
#include "Search.hh"
#include "PathRef.hh"
#include "search/Tag.hh"
#include "PathEnd.hh"
#include "SearchClass.hh"
#include "CircuitSim.hh"
#include "ArcDelayCalc.hh"
#include "Property.hh"
#include "Sta.hh"

namespace sta {

typedef MinPulseWidthCheckSeq::Iterator MinPulseWidthCheckSeqIterator;
typedef MinMaxAll MinMaxAllNull;

Network *
cmdNetwork();
Network *
cmdLinkedNetwork();
Graph *
cmdGraph();

template <class TYPE>
Vector<TYPE> *
tclListSeqPtr(Tcl_Obj *const source,
              swig_type_info *swig_type,
              Tcl_Interp *interp)
{
  int argc;
  Tcl_Obj **argv;

  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK
      && argc > 0) {
    Vector<TYPE> *seq = new Vector<TYPE>;
    for (int i = 0; i < argc; i++) {
      void *obj;
      // Ignore returned TCL_ERROR because can't get swig_type_info.
      SWIG_ConvertPtr(argv[i], &obj, swig_type, false);
      seq->push_back(reinterpret_cast<TYPE>(obj));
    }
    return seq;
  }
  else
    return nullptr;
}

template <class TYPE>
std::vector<TYPE>
tclListSeq(Tcl_Obj *const source,
           swig_type_info *swig_type,
           Tcl_Interp *interp)
{
  int argc;
  Tcl_Obj **argv;

  std::vector<TYPE> seq;
  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK
      && argc > 0) {
    for (int i = 0; i < argc; i++) {
      void *obj;
      // Ignore returned TCL_ERROR because can't get swig_type_info.
      SWIG_ConvertPtr(argv[i], &obj, swig_type, false);
      seq.push_back(reinterpret_cast<TYPE>(obj));
    }
  }
  return seq;
}

template <class SET_TYPE, class OBJECT_TYPE>
SET_TYPE *
tclListSetPtr(Tcl_Obj *const source,
              swig_type_info *swig_type,
              Tcl_Interp *interp)
{
  int argc;
  Tcl_Obj **argv;
  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK
      && argc > 0) {
    SET_TYPE *set = new SET_TYPE;
    for (int i = 0; i < argc; i++) {
      void *obj;
      // Ignore returned TCL_ERROR because can't get swig_type_info.
      SWIG_ConvertPtr(argv[i], &obj, swig_type, false);
      set->insert(reinterpret_cast<OBJECT_TYPE*>(obj));
    }
    return set;
  }
  else
    return nullptr;
}

template <class SET_TYPE, class OBJECT_TYPE>
SET_TYPE
tclListSet(Tcl_Obj *const source,
           swig_type_info *swig_type,
           Tcl_Interp *interp)
{
  int argc;
  Tcl_Obj **argv;
  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK
      && argc > 0) {
    SET_TYPE set;
    for (int i = 0; i < argc; i++) {
      void *obj;
      // Ignore returned TCL_ERROR because can't get swig_type_info.
      SWIG_ConvertPtr(argv[i], &obj, swig_type, false);
      set.insert(reinterpret_cast<OBJECT_TYPE>(obj));
    }
    return set;
  }
  else
    return SET_TYPE();
}

template <class SET_TYPE, class OBJECT_TYPE>
SET_TYPE *
tclListNetworkSet(Tcl_Obj *const source,
                  swig_type_info *swig_type,
                  Tcl_Interp *interp,
                  const Network *network)
{
  int argc;
  Tcl_Obj **argv;
  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK
      && argc > 0) {
    SET_TYPE *set = new SET_TYPE(network);
    for (int i = 0; i < argc; i++) {
      void *obj;
      // Ignore returned TCL_ERROR because can't get swig_type_info.
      SWIG_ConvertPtr(argv[i], &obj, swig_type, false);
      set->insert(reinterpret_cast<OBJECT_TYPE*>(obj));
    }
    return set;
  }
  else
    return nullptr;
}

template <class SET_TYPE, class OBJECT_TYPE>
SET_TYPE
tclListNetworkSet1(Tcl_Obj *const source,
                   swig_type_info *swig_type,
                   Tcl_Interp *interp,
                   const Network *network)
{
  int argc;
  Tcl_Obj **argv;
  SET_TYPE set(network);
  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK
      && argc > 0) {
    for (int i = 0; i < argc; i++) {
      void *obj;
      // Ignore returned TCL_ERROR because can't get swig_type_info.
      SWIG_ConvertPtr(argv[i], &obj, swig_type, false);
      set.insert(reinterpret_cast<OBJECT_TYPE*>(obj));
    }
  }
  return set;
}

static StringSet *
tclListSetConstChar(Tcl_Obj *const source,
		    Tcl_Interp *interp)
{
  int argc;
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

static StringSeq *
tclListSeqConstChar(Tcl_Obj *const source,
		    Tcl_Interp *interp)
{
  int argc;
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

#ifdef UNUSED
static StdStringSet *
tclListSetStdString(Tcl_Obj *const source,
		    Tcl_Interp *interp)
{
  int argc;
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
#endif

////////////////////////////////////////////////////////////////

// Sequence out to tcl list.
template <class SEQ_TYPE, class OBJECT_TYPE>
void
seqPtrTclList(SEQ_TYPE *seq,
              swig_type_info *swig_type,
              Tcl_Interp *interp)
{
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  for (const OBJECT_TYPE *obj : *seq) {
    Tcl_Obj *tcl_obj = SWIG_NewInstanceObj(const_cast<OBJECT_TYPE*>(obj),
                                           swig_type, false);
    Tcl_ListObjAppendElement(interp, list, tcl_obj);
  }
  Tcl_SetObjResult(interp, list);
}

template <class SEQ_TYPE, class OBJECT_TYPE>
void
seqTclList(SEQ_TYPE &seq,
           swig_type_info *swig_type,
           Tcl_Interp *interp)
{
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  for (const OBJECT_TYPE *obj : seq) {
    Tcl_Obj *tcl_obj = SWIG_NewInstanceObj(const_cast<OBJECT_TYPE*>(obj),
                                           swig_type, false);
    Tcl_ListObjAppendElement(interp, list, tcl_obj);
  }
  Tcl_SetObjResult(interp, list);
}

template <class SET_TYPE, class OBJECT_TYPE>
void
setTclList(SET_TYPE set,
           swig_type_info *swig_type,
           Tcl_Interp *interp)
{
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  for (const OBJECT_TYPE *obj : set) {
    Tcl_Obj *tcl_obj = SWIG_NewInstanceObj(const_cast<OBJECT_TYPE*>(obj),
                                           swig_type, false);
    Tcl_ListObjAppendElement(interp, list, tcl_obj);
  }
  Tcl_SetObjResult(interp, list);
}

template <class SET_TYPE, class OBJECT_TYPE>
void
setPtrTclList(SET_TYPE *set,
              swig_type_info *swig_type,
              Tcl_Interp *interp)
{
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  for (const OBJECT_TYPE *obj : *set) {
    Tcl_Obj *tcl_obj = SWIG_NewInstanceObj(const_cast<OBJECT_TYPE*>(obj),
                                           swig_type, false);
    Tcl_ListObjAppendElement(interp, list, tcl_obj);
  }
  Tcl_SetObjResult(interp, list);
}

////////////////////////////////////////////////////////////////

static void
tclArgError(Tcl_Interp *interp,
            const char *msg,
            const char *arg)
{
  // Swig does not add try/catch around arg parsing so this cannot use Report::error.
  string error_msg = "Error: ";
  error_msg += msg;
  char *error = stringPrint(error_msg.c_str(), arg);
  Tcl_SetResult(interp, error, TCL_VOLATILE);
  stringDelete(error);
}

static void
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

#ifdef UNUSED
static Tcl_Obj *
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

  const char *from_edge = arc->fromEdge()->asString();
  obj = Tcl_NewStringObj(from_edge, strlen(from_edge));
  Tcl_ListObjAppendElement(interp, list, obj);

  const char *to_name = arc->to()->name();
  obj = Tcl_NewStringObj(to_name, strlen(to_name));
  Tcl_ListObjAppendElement(interp, list, obj);

  const char *to_edge = arc->toEdge()->asString();
  obj = Tcl_NewStringObj(to_edge, strlen(to_edge));
  Tcl_ListObjAppendElement(interp, list, obj);

  const char *input_delay = delayAsString(gate.inputDelay(), sta, 3);
  obj = Tcl_NewStringObj(input_delay, strlen(input_delay));
  Tcl_ListObjAppendElement(interp, list, obj);

  return list;
}

static ArcDcalcArg
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
#endif

} // namespace

using namespace sta;

%}

////////////////////////////////////////////////////////////////
//
// SWIG type definitions.
//
////////////////////////////////////////////////////////////////

// String that is deleted after crossing over to tcland.
%typemap(out) string {
  string &str = $1;
  // String is volatile because it is deleted.
  Tcl_SetResult(interp, const_cast<char*>(str.c_str()), TCL_VOLATILE);
}

// String that is deleted after crossing over to tcland.
%typemap(out) TmpString* {
  string *str = $1;
  if (str) {
    // String is volatile because it is deleted.
    Tcl_SetResult(interp, const_cast<char*>(str->c_str()), TCL_VOLATILE);
    delete str;
  }
  else
    Tcl_SetResult(interp, nullptr, TCL_STATIC);
}

%typemap(in) StringSeq* {
  $1 = tclListSeqConstChar($input, interp);
}

%typemap(in) StdStringSet* {
  $1 = tclListSetStdString($input, interp);
}

%typemap(out) StringSeq* {
  StringSeq *strs = $1;
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  for (const char *str : *strs) {
    Tcl_Obj *obj = Tcl_NewStringObj(str, strlen(str));
    Tcl_ListObjAppendElement(interp, list, obj);
  }
  Tcl_SetObjResult(interp, list);
}

%typemap(out) StringSeq {
  StringSeq &strs = $1;
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  for (const char *str : strs) {
    Tcl_Obj *obj = Tcl_NewStringObj(str, strlen(str));
    Tcl_ListObjAppendElement(interp, list, obj);
  }
  Tcl_SetObjResult(interp, list);
}

%typemap(out) Library* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) LibraryIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) LibertyLibraryIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) Cell* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) CellSeq* {
  $1 = tclListSeqPtr<Cell*>($input, SWIGTYPE_p_Cell, interp);
}

%typemap(out) CellSeq {
  seqTclList<CellSeq, Cell>($1, SWIGTYPE_p_Cell, interp);
}

%typemap(in) LibertyCellSeq* {
  $1 = tclListSeqPtr<LibertyCell*>($input, SWIGTYPE_p_LibertyCell, interp);
}

%typemap(out) LibertyCellSeq * {
  seqPtrTclList<LibertyCellSeq, LibertyCell>($1, SWIGTYPE_p_LibertyCell, interp);
}

%typemap(out) LibertyCellSeq {
  seqTclList<LibertyCellSeq, LibertyCell>($1, SWIGTYPE_p_LibertyCell, interp);
}

%typemap(in) LibertyPortSeq* {
  $1 = tclListSeqPtr<LibertyPort*>($input, SWIGTYPE_p_LibertyPort, interp);
}

%typemap(out) LibertyPortSeq {
  seqTclList<LibertyPortSeq, LibertyPort>($1, SWIGTYPE_p_LibertyPort, interp);
}

%typemap(out) CellPortIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) LibertyCellPortIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) Port* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) PortSeq* {
  $1 = tclListSeqPtr<const Port*>($input, SWIGTYPE_p_Port, interp);
}

%typemap(out) PortSeq {
  seqTclList<PortSeq, Port>($1, SWIGTYPE_p_Port, interp);
}

%typemap(out) PortMemberIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) LibertyCell* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) LibertyPort* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) LibertyPortMemberIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, SWIGTYPE_p_LibertyPortMemberIterator, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) TimingArc* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) TimingArcSetSeq& {
  seqPtrTclList<TimingArcSetSeq, TimingArcSet>($1, SWIGTYPE_p_TimingArcSet, interp);
}

%typemap(out) TimingArcSeq& {
  seqPtrTclList<TimingArcSeq, TimingArc>($1, SWIGTYPE_p_TimingArc, interp);
}

%typemap(out) Wireload* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) WireloadSelection* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) Transition* {
  int length;
  const char *arg = Tcl_GetStringFromObj($input, &length);
  Transition *tr = Transition::find(arg);
  if (tr == nullptr) {
    Tcl_SetResult(interp,const_cast<char*>("Error: transition not found."),
		  TCL_STATIC);
    return TCL_ERROR;
  }
  else
    $1 = tr;
}

%typemap(out) Transition* {
  Transition *tr = $1;
  const char *str = "";
  if (tr)
    str = tr->asString();
  Tcl_SetResult(interp, const_cast<char*>(str), TCL_STATIC);
}

%typemap(in) RiseFall* {
  int length;
  const char *arg = Tcl_GetStringFromObj($input, &length);
  RiseFall *rf = RiseFall::find(arg);
  if (rf == nullptr) {
    Tcl_SetResult(interp,const_cast<char*>("Error: unknown rise/fall edge."),
		  TCL_STATIC);
    return TCL_ERROR;
  }
  $1 = rf;
}

%typemap(out) RiseFall* {
  const RiseFall *tr = $1;
  const char *str = "";
  if (tr)
    str = tr->asString();
  Tcl_SetResult(interp, const_cast<char*>(str), TCL_STATIC);
}

%typemap(in) RiseFallBoth* {
  int length;
  const char *arg = Tcl_GetStringFromObj($input, &length);
  RiseFallBoth *tr = RiseFallBoth::find(arg);
  if (tr == nullptr) {
    Tcl_SetResult(interp,const_cast<char*>("Error: unknown transition name."),
		  TCL_STATIC);
    return TCL_ERROR;
  }
  $1 = tr;
}

%typemap(out) RiseFallBoth* {
  RiseFallBoth *tr = $1;
  const char *str = "";
  if (tr)
    str = tr->asString();
  Tcl_SetResult(interp, const_cast<char*>(str), TCL_STATIC);
}

%typemap(in) TimingRole* {
  int length;
  const char *arg = Tcl_GetStringFromObj($input, &length);
  TimingRole *role = TimingRole::find(arg);
  if (role)
    $1 = TimingRole::find(arg);
  else {
    Tcl_SetResult(interp,const_cast<char*>("Error: unknown timing role."),
		  TCL_STATIC);
    return TCL_ERROR;
  }
}

%typemap(out) TimingRole* {
  Tcl_SetResult(interp, const_cast<char*>($1->asString()), TCL_STATIC);
}

%typemap(in) LogicValue {
  int length;
  const char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEq(arg, "0") || stringEq(arg, "zero"))
    $1 = LogicValue::zero;
  else if (stringEq(arg, "1") || stringEq(arg, "one"))
    $1 = LogicValue::one;
  else if (stringEq(arg, "X"))
    $1 = LogicValue::unknown;
  else if (stringEq(arg, "rise") || stringEq(arg, "rising"))
    $1 = LogicValue::rise;
  else if (stringEq(arg, "fall") || stringEq(arg, "falling"))
    $1 = LogicValue::fall;
  else {
    Tcl_SetResult(interp,const_cast<char*>("Error: unknown logic value."),
		  TCL_STATIC);
    return TCL_ERROR;
  }
}

%typemap(in) AnalysisType {
  int length;
  const char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEqual(arg, "single"))
    $1 = AnalysisType::single;
  else if (stringEqual(arg, "bc_wc"))
    $1 = AnalysisType::bc_wc;
  else if (stringEq(arg, "on_chip_variation"))
    $1 = AnalysisType::ocv;
  else {
    Tcl_SetResult(interp,const_cast<char*>("Error: unknown analysis type."),
		  TCL_STATIC);

    return TCL_ERROR;
  }
}

%typemap(out) Instance* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) InstanceSeq* {
  $1 = tclListSeqPtr<const Instance*>($input, SWIGTYPE_p_Instance, interp);
}

%typemap(out) InstanceSeq {
  seqTclList<InstanceSeq, Instance>($1, SWIGTYPE_p_Instance, interp);
}

%typemap(out) InstanceChildIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) LeafInstanceIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) InstancePinIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) InstanceNetIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) LibertyLibrarySeq* {
  $1 = tclListSeqPtr<LibertyLibrary*>($input, SWIGTYPE_p_LibertyLibrary, interp);
}

%typemap(out) LibertyLibrarySeq {
  seqTclList<LibertyLibrarySeq, LibertyLibrary>($1, SWIGTYPE_p_LibertyLibrary, interp);
}

%typemap(out) Pin* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) PinSeq* {
  seqPtrTclList<PinSeq, Pin>($1, SWIGTYPE_p_Pin, interp);
}

%typemap(out) PinSeq {
  seqTclList<PinSeq, Pin>($1, SWIGTYPE_p_Pin, interp);
}

%typemap(out) Net* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) NetSeq* {
  $1 = tclListSeqPtr<const Net*>($input, SWIGTYPE_p_Net, interp);
}

%typemap(out) NetSeq* {
  seqPtrTclList<NetSeq, Net>($1, SWIGTYPE_p_Net, interp);
}

%typemap(in) ConstNetSeq {
  $1 = tclListSeq<const Net*>($input, SWIGTYPE_p_Net, interp);
}

%typemap(out) NetSeq {
  seqTclList<NetSeq, Net>($1, SWIGTYPE_p_Net, interp);
}

%typemap(out) NetPinIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) NetTermIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) NetConnectedPinIterator* {
  Tcl_Obj *obj=SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) PinConnectedPinIterator* {
  Tcl_Obj *obj=SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) Clock* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) ConstClockSeq {
  $1 = tclListSeq<const Clock*>($input, SWIGTYPE_p_Clock, interp);
}

%typemap(in) ClockSeq* {
  $1 = tclListSeqPtr<Clock*>($input, SWIGTYPE_p_Clock, interp);
}

%typemap(out) ClockSeq* {
  seqPtrTclList<ClockSeq, Clock>($1, SWIGTYPE_p_Clock, interp);
}

%typemap(out) ClockSeq {
  seqTclList<ClockSeq, Clock>($1, SWIGTYPE_p_Clock, interp);
}

%typemap(out) ClockEdge* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1,$1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) PinSeq* {
  $1 = tclListSeqPtr<const Pin*>($input, SWIGTYPE_p_Pin, interp);
}

%typemap(in) PinSet {
  Network *network = cmdNetwork();
  $1 = tclListNetworkSet1<PinSet, Pin>($input, SWIGTYPE_p_Pin, interp, network);
}

%typemap(in) PinSet* {
  Network *network = cmdNetwork();
  $1 = tclListNetworkSet<PinSet, Pin>($input, SWIGTYPE_p_Pin, interp, network);
}

%typemap(out) PinSet* {
  setPtrTclList<PinSet, Pin>($1, SWIGTYPE_p_Pin, interp);
}

%typemap(out) PinSet {
  setTclList<PinSet, Pin>($1, SWIGTYPE_p_Pin, interp);
}

%typemap(out) const PinSet& {
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  // A swig bug sets the result to PinSet* rather than const PinSet&.
  PinSet *pins = $1;
  for (const Pin *pin : *pins) {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Pin*>(pin), SWIGTYPE_p_Pin, false);
    Tcl_ListObjAppendElement(interp, list, obj);
  }
  Tcl_SetObjResult(interp, list);
}

%typemap(in) ConstClockSet {
  $1 = tclListSet<ConstClockSet, Clock>($input, SWIGTYPE_p_Clock, interp);
}

%typemap(in) ClockSet* {
  $1 = tclListSetPtr<ClockSet, Clock>($input, SWIGTYPE_p_Clock, interp);
}

%typemap(out) ClockSet* {
  setPtrTclList<ClockSet, Clock>($1, SWIGTYPE_p_Clock, interp);
}

%typemap(in) InstanceSet* {
  Network *network = cmdNetwork();
  $1 = tclListNetworkSet<InstanceSet, Instance>($input, SWIGTYPE_p_Instance,
                                                interp, network);
}

%typemap(out) InstanceSet {
  setTclList<InstanceSet, Instance>($1, SWIGTYPE_p_Instance, interp);
}

%typemap(in) NetSet* {
  Network *network = cmdNetwork();
  $1 = tclListNetworkSet<NetSet, Net>($input, SWIGTYPE_p_Net, interp, network);
}

%typemap(in) FloatSeq* {
  int argc;
  Tcl_Obj **argv;
  FloatSeq *floats = nullptr;

  if (Tcl_ListObjGetElements(interp, $input, &argc, &argv) == TCL_OK) {
    if (argc)
      floats = new FloatSeq;
    for (int i = 0; i < argc; i++) {
      char *arg = Tcl_GetString(argv[i]);
      double value;
      if (Tcl_GetDouble(interp, arg, &value) == TCL_OK)
	floats->push_back(static_cast<float>(value));
      else {
	delete floats;
	tclArgError(interp, "%s is not a floating point number.", arg);
	return TCL_ERROR;
      }
    }
  }
  $1 = floats;
}

%typemap(out) FloatSeq* {
  FloatSeq *floats = $1;
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  if (floats) {
    for (float f : *floats) {
      Tcl_Obj *obj = Tcl_NewDoubleObj(f);
      Tcl_ListObjAppendElement(interp, list, obj);
    }
  }
  Tcl_SetObjResult(interp, list);
}

%typemap(out) FloatSeq {
  FloatSeq &floats = $1;
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  for (float f : floats) {
    Tcl_Obj *obj = Tcl_NewDoubleObj(f);
    Tcl_ListObjAppendElement(interp, list, obj);
  }
  Tcl_SetObjResult(interp, list);
}

%typemap(in) IntSeq* {
  int argc;
  Tcl_Obj **argv;
  IntSeq *ints = nullptr;

  if (Tcl_ListObjGetElements(interp, $input, &argc, &argv) == TCL_OK) {
    if (argc)
      ints = new IntSeq;
    for (int i = 0; i < argc; i++) {
      char *arg = Tcl_GetString(argv[i]);
      int value;
      if (Tcl_GetInt(interp, arg, &value) == TCL_OK)
	ints->push_back(value);
      else {
	delete ints;
	tclArgError(interp, "%s is not an integer.", arg);
	return TCL_ERROR;
      }
    }
  }
  $1 = ints;
}

%typemap(out) Table1 {
  Table1 &table = $1;
  if (table.axis1()) {
    Tcl_Obj *list3 = Tcl_NewListObj(0, nullptr);
    Tcl_Obj *list1 = Tcl_NewListObj(0, nullptr);
    for (float f : *table.axis1()->values()) {
      Tcl_Obj *obj = Tcl_NewDoubleObj(f);
      Tcl_ListObjAppendElement(interp, list1, obj);
    }
    Tcl_Obj *list2 = Tcl_NewListObj(0, nullptr);
    for (float f : *table.values()) {
      Tcl_Obj *obj = Tcl_NewDoubleObj(f);
      Tcl_ListObjAppendElement(interp, list2, obj);
    }
    Tcl_ListObjAppendElement(interp, list3, list1);
    Tcl_ListObjAppendElement(interp, list3, list2);
    Tcl_SetObjResult(interp, list3);
  }
}

%typemap(out) const Table1* {
  const Table1 *table = $1;
  Tcl_Obj *list3 = Tcl_NewListObj(0, nullptr);
  if (table) {
    Tcl_Obj *list1 = Tcl_NewListObj(0, nullptr);
    for (float f : *table->axis1()->values()) {
      Tcl_Obj *obj = Tcl_NewDoubleObj(f);
      Tcl_ListObjAppendElement(interp, list1, obj);
    }
    Tcl_Obj *list2 = Tcl_NewListObj(0, nullptr);
    for (float f : *table->values()) {
      Tcl_Obj *obj = Tcl_NewDoubleObj(f);
      Tcl_ListObjAppendElement(interp, list2, obj);
    }
    Tcl_ListObjAppendElement(interp, list3, list1);
    Tcl_ListObjAppendElement(interp, list3, list2);
  }
  Tcl_SetObjResult(interp, list3);
}

%typemap(in) MinMax* {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  MinMax *min_max = MinMax::find(arg);
  if (min_max)
    $1 = min_max;
  else {
    tclArgError(interp, "%s not min or max.", arg);
    return TCL_ERROR;
  }
}

%typemap(out) MinMax* {
  Tcl_SetResult(interp, const_cast<char*>($1->asString()), TCL_STATIC);
}

%typemap(out) MinMax* {
  Tcl_SetResult(interp, const_cast<char*>($1->asString()), TCL_STATIC);
}

%typemap(in) MinMaxAll* {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  MinMaxAll *min_max = MinMaxAll::find(arg);
  if (min_max)
    $1 = min_max;
  else {
    tclArgError(interp, "%s not min, max or min_max.", arg);
    return TCL_ERROR;
  }
}

%typemap(in) MinMaxAllNull* {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEqual(arg, "NULL"))
    $1 = nullptr;
  else {
    MinMaxAll *min_max = MinMaxAll::find(arg);
    if (min_max)
      $1 = min_max;
    else {
      tclArgError(interp, "%s not min, max or min_max.", arg);
      return TCL_ERROR;
    }
  }
}

%typemap(out) MinMaxAll* {
  Tcl_SetResult(interp, const_cast<char*>($1->asString()), TCL_STATIC);
}

// SetupHold is typedef'd to MinMax.
%typemap(in) SetupHold* {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEqual(arg, "hold")
      || stringEqual(arg, "min"))
    $1 = MinMax::min();
  else if (stringEqual(arg, "setup")
	   || stringEqual(arg, "max"))
    $1 = MinMax::max();
  else {
    tclArgError(interp, "%s not setup, hold, min or max.", arg);
    return TCL_ERROR;
  }
}

// SetupHoldAll is typedef'd to MinMaxAll.
%typemap(in) SetupHoldAll* {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEqual(arg, "hold")
      || stringEqual(arg, "min"))
    $1 = SetupHoldAll::min();
  else if (stringEqual(arg, "setup")
	   || stringEqual(arg, "max"))
    $1 = SetupHoldAll::max();
  else if (stringEqual(arg, "setup_hold")
	   || stringEqual(arg, "min_max"))
    $1 = SetupHoldAll::all();
  else {
    tclArgError(interp, "%s not setup, hold, setup_hold, min, max or min_max.", arg);
    return TCL_ERROR;
  }
}

// EarlyLate is typedef'd to MinMax.
%typemap(in) EarlyLate* {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  EarlyLate *early_late = EarlyLate::find(arg);
  if (early_late)
    $1 = early_late;
  else {
    tclArgError(interp, "%s not early/min, late/max or early_late/min_max.", arg);
    return TCL_ERROR;
  }
}

// EarlyLateAll is typedef'd to MinMaxAll.
%typemap(in) EarlyLateAll* {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  EarlyLateAll *early_late = EarlyLateAll::find(arg);
  if (early_late)
    $1 = early_late;
  else {
    tclArgError(interp, "%s not early/min, late/max or early_late/min_max.", arg);
    return TCL_ERROR;
  }
}

%typemap(in) TimingDerateType {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEq(arg, "net_delay"))
    $1 = TimingDerateType::net_delay;
  else if (stringEq(arg, "cell_delay"))
    $1 = TimingDerateType::cell_delay;
  else if (stringEq(arg, "cell_check"))
    $1 = TimingDerateType::cell_check;
  else {
    tclArgError(interp, "%s not net_delay, cell_delay or cell_check.", arg);
    return TCL_ERROR;
  }
}

%typemap(in) TimingDerateCellType {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEq(arg, "cell_delay"))
    $1 = TimingDerateCellType::cell_delay;
  else if (stringEq(arg, "cell_check"))
    $1 = TimingDerateCellType::cell_check;
  else {
    tclArgError(interp, "%s not cell_delay or cell_check.", arg);
    return TCL_ERROR;
  }
}

%typemap(in) PathClkOrData {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEq(arg, "clk"))
    $1 = PathClkOrData::clk;
  else if (stringEq(arg, "data"))
    $1 = PathClkOrData::data;
  else {
    tclArgError(interp, "%s not clk or data.", arg);
    return TCL_ERROR;
  }
}

%typemap(in) ReportSortBy {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEq(arg, "group"))
    $1 = sort_by_group;
  else if (stringEq(arg, "slack"))
    $1 = sort_by_slack;
  else {
    tclArgError(interp, "%s not group or slack.", arg);
    return TCL_ERROR;
  }
}

%typemap(in) ReportPathFormat {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEq(arg, "full"))
    $1 = ReportPathFormat::full;
  else if (stringEq(arg, "full_clock"))
    $1 = ReportPathFormat::full_clock;
  else if (stringEq(arg, "full_clock_expanded"))
    $1 = ReportPathFormat::full_clock_expanded;
  else if (stringEq(arg, "short"))
    $1 = ReportPathFormat::shorter;
  else if (stringEq(arg, "end"))
    $1 = ReportPathFormat::endpoint;
  else if (stringEq(arg, "summary"))
    $1 = ReportPathFormat::summary;
  else if (stringEq(arg, "slack_only"))
    $1 = ReportPathFormat::slack_only;
  else if (stringEq(arg, "json"))
    $1 = ReportPathFormat::json;
  else {
    tclArgError(interp, "unknown path type %s.", arg);
    return TCL_ERROR;
  }
}

%typemap(in) ExceptionThruSeq* {
  $1 = tclListSeqPtr<ExceptionThru*>($input, SWIGTYPE_p_ExceptionThru, interp);
}

%typemap(out) Vertex* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) Vertex** {
  int i = 0;
  Tcl_ResetResult(interp);
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  while ($1[i]) {
    Tcl_Obj *obj = SWIG_NewInstanceObj($1[i], SWIGTYPE_p_Vertex,false);
    Tcl_ListObjAppendElement(interp, list, obj);
    i++;
  }
  Tcl_SetObjResult(interp, list);
}

%typemap(out) Edge* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(in) EdgeSeq* {
  $1 = tclListSeqPtr<Edge*>($input, SWIGTYPE_p_Edge, interp);
}

%typemap(out) EdgeSeq {
  seqTclList<EdgeSeq, Edge>($1, SWIGTYPE_p_Edge, interp);
}

%typemap(out) VertexIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) VertexInEdgeIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) VertexOutEdgeIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) CheckErrorSeq & {
  Tcl_Obj *error_list = Tcl_NewListObj(0, nullptr);
  CheckErrorSeq *check_errors = $1;
  CheckErrorSeq::Iterator check_iter(check_errors);
  while (check_iter.hasNext()) {
    CheckError *error = check_iter.next();
    Tcl_Obj *string_list = Tcl_NewListObj(0, nullptr);
    CheckError::Iterator string_iter(error);
    while (string_iter.hasNext()) {
      const char *str = string_iter.next();
      size_t str_len = strlen(str);
      Tcl_Obj *obj = Tcl_NewStringObj(const_cast<char*>(str),
				      static_cast<int>(str_len));
      Tcl_ListObjAppendElement(interp, string_list, obj);
    }
    Tcl_ListObjAppendElement(interp, error_list, string_list);
  }
  Tcl_SetObjResult(interp, error_list);
}

%typemap(out) PathEnd* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) PathEndSeq* {
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  const PathEndSeq *path_ends = $1;
  PathEndSeq::ConstIterator end_iter(path_ends);
  while (end_iter.hasNext()) {
    PathEnd *path_end = end_iter.next();
    Tcl_Obj *obj = SWIG_NewInstanceObj(path_end, SWIGTYPE_p_PathEnd, false);
    Tcl_ListObjAppendElement(interp, list, obj);
  }
  // Delete the PathEndSeq, not the ends.
  delete path_ends;
  Tcl_SetObjResult(interp, list);
}

%typemap(out) PathEndSeq {
  seqTclList<PathEndSeq, PathEnd>($1, SWIGTYPE_p_PathEnd, interp);
}

%typemap(out) MinPulseWidthCheckSeqIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) PathRefSeq* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);

  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  PathRefSeq *paths = $1;
  PathRefSeq::Iterator path_iter(paths);
  while (path_iter.hasNext()) {
    PathRef *path = &path_iter.next();
    PathRef *copy = new PathRef(path);
    Tcl_Obj *obj = SWIG_NewInstanceObj(copy, SWIGTYPE_p_PathRef, false);
    Tcl_ListObjAppendElement(interp, list, obj);
  }
  Tcl_SetObjResult(interp, list);
}

%typemap(out) MinPulseWidthCheck* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) MinPulseWidthCheckSeq & {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) MinPulseWidthCheckSeqIterator & {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) VertexPathIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) SlowDrvrIterator* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) ExceptionFrom* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) ExceptionTo* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) ExceptionThru* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) OperatingConditions* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) Delay {
  Tcl_SetObjResult(interp,Tcl_NewDoubleObj(delayAsFloat($1)));
}

%typemap(out) Arrival {
  Tcl_SetObjResult(interp,Tcl_NewDoubleObj(delayAsFloat($1)));
}

%typemap(out) Required {
  Tcl_SetObjResult(interp,Tcl_NewDoubleObj(delayAsFloat($1)));
}

%typemap(out) Slack {
  Tcl_SetObjResult(interp,Tcl_NewDoubleObj(delayAsFloat($1)));
}

%typemap(out) ArcDelay {
  Tcl_SetObjResult(interp,Tcl_NewDoubleObj(delayAsFloat($1)));
}

%typemap(out) Slew {
  Tcl_SetObjResult(interp,Tcl_NewDoubleObj(delayAsFloat($1)));
}

%typemap(out) Crpr {
  Tcl_SetObjResult(interp,Tcl_NewDoubleObj(delayAsFloat($1)));
}

%typemap(in) PathGroupNameSet* {
  $1 = tclListSetConstChar($input, interp);
}

%typemap(in) StringSet* {
  $1 = tclListSetConstChar($input, interp);
}

%typemap(out) Corner* {
  Tcl_Obj *obj = SWIG_NewInstanceObj($1, $1_descriptor, false);
  Tcl_SetObjResult(interp, obj);
}

%typemap(out) Corners* {
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  Corners *corners = $1;
  for (Corner *corner : *corners) {
    Tcl_Obj *obj = SWIG_NewInstanceObj(corner, SWIGTYPE_p_Corner, false);
    Tcl_ListObjAppendElement(interp, list, obj);
  }
  Tcl_SetObjResult(interp, list);
}

%typemap(out) PropertyValue {
  PropertyValue value = $1;
  switch (value.type()) {
  case PropertyValue::Type::type_none:
    Tcl_SetResult(interp, const_cast<char*>(""), TCL_STATIC);
    break;
  case PropertyValue::Type::type_string:
    Tcl_SetResult(interp, const_cast<char*>(value.stringValue()), TCL_VOLATILE);
    break;
  case PropertyValue::Type::type_float: {
    const Unit *unit = value.unit();
    const char *float_string = unit->asString(value.floatValue(), 6);
    Tcl_SetResult(interp, const_cast<char*>(float_string), TCL_VOLATILE);
  }
    break;
  case PropertyValue::Type::type_bool: {
    const char *bool_string = value.boolValue() ? "1" : "0";
    Tcl_SetResult(interp, const_cast<char*>(bool_string), TCL_STATIC);
  }
    break;
  case PropertyValue::Type::type_library: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Library*>(value.library()),
				       SWIGTYPE_p_Library, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_cell: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Cell*>(value.cell()),
				       SWIGTYPE_p_Cell, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_port: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Port*>(value.port()),
				       SWIGTYPE_p_Port, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_liberty_library: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<LibertyLibrary*>(value.libertyLibrary()),
				       SWIGTYPE_p_LibertyLibrary, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_liberty_cell: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<LibertyCell*>(value.libertyCell()),
				       SWIGTYPE_p_LibertyCell, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_liberty_port: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<LibertyPort*>(value.libertyPort()),
				       SWIGTYPE_p_LibertyPort, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_instance: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Instance*>(value.instance()),
				       SWIGTYPE_p_Instance, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_pin: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Pin*>(value.pin()),
                                       SWIGTYPE_p_Pin, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_pins: {
    Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
    PinSeq *pins = value.pins();
    PinSeq::Iterator pin_iter(pins);
    while (pin_iter.hasNext()) {
      const Pin *pin = pin_iter.next();
      Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Pin*>(pin), SWIGTYPE_p_Pin, false);
      Tcl_ListObjAppendElement(interp, list, obj);
    }
    Tcl_SetObjResult(interp, list);
  }
    break;
  case PropertyValue::Type::type_net: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Net*>(value.net()),
				       SWIGTYPE_p_Net, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_clk: {
    Tcl_Obj *obj = SWIG_NewInstanceObj(const_cast<Clock*>(value.clock()),
				       SWIGTYPE_p_Clock, false);
    Tcl_SetObjResult(interp, obj);
  }
    break;
  case PropertyValue::Type::type_clks: {
    Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
    ClockSeq *clks = value.clocks();
    ClockSeq::Iterator clk_iter(clks);
    while (clk_iter.hasNext()) {
      Clock *clk = clk_iter.next();
      Tcl_Obj *obj = SWIG_NewInstanceObj(clk, SWIGTYPE_p_Clock, false);
      Tcl_ListObjAppendElement(interp, list, obj);
    }
    Tcl_SetObjResult(interp, list);
  }
    break;
  case PropertyValue::Type::type_path_refs: {
    Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
    for (PathRef &path : *value.pathRefs()) {
      PathRef *copy = new PathRef(path);
      Tcl_Obj *obj = SWIG_NewInstanceObj(copy, SWIGTYPE_p_PathRef, false);
      Tcl_ListObjAppendElement(interp, list, obj);
    }
    Tcl_SetObjResult(interp, list);
  }
    break;
  case PropertyValue::Type::type_pwr_activity: {
    PwrActivity activity = value.pwrActivity();
    Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
    Tcl_Obj *obj;
    const char *str;

    str = stringPrintTmp("%.5e", activity.activity());
    obj = Tcl_NewStringObj(str, strlen(str));
    Tcl_ListObjAppendElement(interp, list, obj);

    str = stringPrintTmp("%.3f", activity.duty());
    obj = Tcl_NewStringObj(str, strlen(str));
    Tcl_ListObjAppendElement(interp, list, obj);

    str = activity.originName();
    obj = Tcl_NewStringObj(str, strlen(str));
    Tcl_ListObjAppendElement(interp, list, obj);

    Tcl_SetObjResult(interp, list);
  }
    break;
  }
}

%typemap(in) CircuitSim {
  int length;
  char *arg = Tcl_GetStringFromObj($input, &length);
  if (stringEq(arg, "hspice"))
    $1 = CircuitSim::hspice;
  else if (stringEq(arg, "ngspice"))
    $1 = CircuitSim::ngspice;
  else if (stringEq(arg, "xyce"))
    $1 = CircuitSim::xyce;
  else {
    tclArgError(interp, "unknown circuit simulator %s.", arg);
    return TCL_ERROR;
  }
}

%typemap(in) ArcDcalcArg {
  Tcl_Obj *const source = $input;
  $1 = arcDcalcArgTcl(source, interp);
}

%typemap(out) ArcDcalcArg {
  Tcl_Obj *tcl_obj = tclArcDcalcArg($1, interp);
  Tcl_SetObjResult(interp, tcl_obj);
}

%typemap(in) ArcDcalcArgSeq {
  Tcl_Obj *const source = $input;
  int argc;
  Tcl_Obj **argv;

  Sta *sta = Sta::sta();
  ArcDcalcArgSeq seq;
  if (Tcl_ListObjGetElements(interp, source, &argc, &argv) == TCL_OK
      && argc > 0) {
    for (int i = 0; i < argc; i++) {
      ArcDcalcArg gate = arcDcalcArgTcl(argv[i], interp);
      if (gate.drvrPin())
        seq.push_back(gate);
    }
  }
  $1 = seq;
}

%typemap(out) ArcDcalcArgSeq {
  Tcl_Obj *list = Tcl_NewListObj(0, nullptr);
  for (ArcDcalcArg &gate : $1) {
    Tcl_Obj *tcl_obj = tclArcDcalcArg(gate, interp);
    Tcl_ListObjAppendElement(interp, list, tcl_obj);
  }
  Tcl_SetObjResult(interp, list);
}
