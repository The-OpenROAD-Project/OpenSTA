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

#include "ArcDelayCalc.hh"
#include "StringSet.hh"
#include "StringSeq.hh"

#include <tcl.h>

namespace sta {

#if TCL_MAJOR_VERSION < 9
    typedef int Tcl_Size;
#endif

StringSet *
tclListSetConstChar(Tcl_Obj *const source,
		    Tcl_Interp *interp);

StringSeq *
tclListSeqConstChar(Tcl_Obj *const source,
		    Tcl_Interp *interp);

StdStringSet *
tclListSetStdString(Tcl_Obj *const source,
		    Tcl_Interp *interp);

void
tclArgError(Tcl_Interp *interp,
            const char *msg,
            const char *arg);

void
objectListNext(const char *list,
	       const char *type,
	       // Return values.
	       bool &type_match,
	       const char *&next);

Tcl_Obj *
tclArcDcalcArg(ArcDcalcArg &gate,
               Tcl_Interp *interp);

ArcDcalcArg
arcDcalcArgTcl(Tcl_Obj *obj,
               Tcl_Interp *interp);

} // namespace
