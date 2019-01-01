#! /bin/sh
# The next line is executed by /bin/sh, but not Tcl \
exec tclsh $0 ${1+"$@"}

# OpenSTA, Static Timing Analyzer
# Copyright (c) 2019, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

# Clean up compiler warnings in swig generated code.

# Version is on stderr so redirect stderr to stdout and use cat to catch it.
set swig_version_str [exec "swig" "-version" |& cat]
regexp "(\\d+\\.\\d+\\.\\d+)" [lindex $swig_version_str 2] swig_version
regexp "(\\d+)\\.(\\d+)\\.(\\d+)" [lindex $swig_version_str 2] \
  ignore swig_version1 swig_version2 swig_version3
set swig_file [lindex $argv 0]
set backup_file "$swig_file.backup"

# Copy to the side before munging.
file rename -force $swig_file $backup_file

set in_stream [open $backup_file r]
set out_stream [open $swig_file w]

# include "Machine.hh" happens too late to define this gcc pragma.
puts $out_stream "#ifndef __GNUC__"
puts $out_stream "#define __attribute__(x)"
puts $out_stream "#endif"

proc func_unused_proto { ret_type func line out_stream } {
  if {[regexp "$func\\\((.*)\\\) \\\{" $line ignore args]} {
    puts $out_stream "${func}($args)"
    puts $out_stream "__attribute__((unused));"
    puts $out_stream "SWIGRUNTIME $ret_type"
  }
}

# No curly bracket on the function line.
proc func_unused_proto2 { ret_type func line out_stream } {
  if {[regexp "$func\\\((.*)\\\)" $line ignore args]} {
    puts $out_stream "${func}($args)"
    puts $out_stream "__attribute__((unused));"
    puts $out_stream "SWIGRUNTIME $ret_type"
  }
}

# Add initializers to swig type statements
proc swig1 { line_var } {
  upvar 1 $line_var line
  regsub "^static swig_type_info (\\w+)\\\[\\\] = \{\{\"(\\w+)\", 0, \"(.+)\", (&.+)\},\{\"(\\w+)\"\},\{" $line \
    [concat {static swig_type_info \1[] = }\
    "\{\{"\
	 {"\2", 0, "\3", \4,0,0,0}\
	 "\},\{"\
	 {"\5",0,0,0,0,0,0} "\},\{0,0,0,0,0,0,"] line
  regsub "^static swig_type_info (\\w+)\\\[\\\] = \{\{\"(\\w+)\", 0, \"(.+)\", 0\},\{\"(\\w+)\"\},\{" $line \
    [concat {static swig_type_info \1[] = }\
    "\{\{"\
	 {"\2", 0, "\3", 0,0,0,0}\
	 "\},\{"\
	 {"\4",0,0,0,0,0,0} "\},\{0,0,0,0,0,0,"] line
}

proc swig3 { line_var } {
  upvar 1 $line_var line
  regsub "\\\(char \\\*\\\) Tcl_GetStringResult\\\(interp\\\)" $line \
    "const_cast<char*>(Tcl_GetStringResult(interp))" line
}

# The functions SWIG_Tcl_TypeName, SWIG_Tcl_PointerTypeFromString, 
# and SWIG_Tcl_ConvertPacked are not used, so gcc complains about
# them being defined and not used.
proc swig4 { line_var } {
  upvar 1 $line_var line
  regsub "\#ifdef SWIG_GLOBAL" $line "\#if 1" line
}

proc swig5 { line_var } {
  upvar 1 $line_var line
  regsub ", *\\\(char *\\\*\\\) ptr" $line \
    {, reinterpret_cast<char*>(ptr)} line
  
  regsub ", *\\\(char *\\\*\\\) NULL" $line \
    {, reinterpret_cast<char*>(NULL)} line
    
  regsub ", ?\\\(char ?\\\*\\\)(\[^,\\\)\]+)(\[,\\\)\])" $line \
    {, const_cast<char*>(\1)\2} line
    
  regsub ", ?\\\(char ?\\\*\\\)(\[^,\\\)\]+)(\[,\\\)\])" $line \
    {, const_cast<char*>(\1)\2} line
    
  regsub "\\\(char \\\*\\\)\"\"" $line {""} line
    
  # Tcl_SetResult missing const_cast<char*> for literal string.
  regsub "Tcl_SetResult\\\(interp,(\"\\\[^\"\\\]*\")" $line \
    "Tcl_SetResult(interp, const_cast<char*>(\"\1\")" line
    
  # Misguided (char*) cast in SWIG_MakePtr.
  regsub "\\\(char \\\*\\\)\"NULL\"" $line {"NULL"} line
    
  regsub "objv = \\\(Tcl_Obj \\\*\\\*\\\) _objv;" $line \
    "objv = const_cast<Tcl_Obj**>(_objv);" line
    
  regsub "result = \\\(char \\\*\\\)(.*);" $line \
    {result = const_cast<char*>(\1);} line
}

proc swig6 { line_var out_stream } {
  upvar 1 $line_var line
  # SWIG_UnpackData signed/unsigned comparison.
  regsub "strlen\\\(c\\\) < \\\(2\\\*sz\\\)" $line \
    "(int)strlen(c) < (2*sz)" line
  
  # Add pragmas for unused functions.
  if {[regexp "SWIG_TypeDynamicCast\\\(swig_type_info \\\*ty, void \\\*\\\*ptr\\\)" $line]} {
    puts $out_stream "SWIG_TypeDynamicCast(swig_type_info *ty, void **ptr)"
    puts $out_stream "__attribute__((unused));"
    puts $out_stream "SWIGRUNTIME(swig_type_info *)"
  }
  
  # Add pragmas for unused functions.
  if {[regexp "SWIG_TypeQuery\\\(const char \\\*name\\\) \\\{" $line]} {
    puts $out_stream "SWIG_TypeQuery(const char *name)"
    puts $out_stream "__attribute__((unused));"
    puts $out_stream "SWIGRUNTIME(swig_type_info *)"
  }
  
  # Add pragmas for unused functions.
  if {[regexp "SWIG_PointerTypeFromString\\\(char \\\*c\\\) \\\{" $line]} {
    puts $out_stream "SWIG_PointerTypeFromString(char *c)"
    puts $out_stream "__attribute__((unused));"
    puts $out_stream "SWIGRUNTIME(char *)"
  }
  
  # Add pragmas for unused functions.
  if {[regexp "SWIG_ConvertPacked\\\(Tcl_Interp \\\*interp, Tcl_Obj \\\*obj, void \\\*ptr, int sz, swig_type_info \\\*ty, int flags\\\) \\\{" $line]} {
    puts $out_stream "    SWIG_ConvertPacked(Tcl_Interp *interp, Tcl_Obj *obj, void *ptr, int sz, swig_type_info *ty, int flags)"
    puts $out_stream "__attribute__((unused));"
    puts $out_stream "SWIGRUNTIME(int)"
  }
}

proc swig7 { line_var out_stream } {
  upvar 1 $line_var line
  # Add pragmas for unused functions.
  func_unused_proto "int" "SWIG_TypeCompare" $line $out_stream
  func_unused_proto "swig_cast_info*" "SWIG_TypeCheckStruct" $line $out_stream
  func_unused_proto "swig_type_info*" "SWIG_TypeDynamicCast" $line $out_stream
  func_unused_proto "const char*" "SWIG_TypePrettyName" $line $out_stream
  func_unused_proto "void" "SWIG_TypeNewClientData" $line $out_stream
  func_unused_proto "char*" "SWIG_PackVoidPtr" $line $out_stream
  func_unused_proto "const char*" "SWIG_UnpackVoidPtr" $line $out_stream
  func_unused_proto "char*" "SWIG_PackDataName" $line $out_stream
  func_unused_proto "const char*" "SWIG_UnpackDataName" $line $out_stream
  func_unused_proto2 "void" "SWIG_Tcl_SetErrorObj" $line $out_stream
  func_unused_proto "char*" "SWIG_Tcl_PointerTypeFromString" $line $out_stream
  func_unused_proto "int" "SWIG_Tcl_ConvertPacked" $line $out_stream
}

proc swig8 { line_var } {
  upvar 1 $line_var line
  regsub "\\\(char \\\*\\\) Tcl_GetVar\\\((.*)\\\)" $line \
    "const_cast<char*>(Tcl_GetVar(\\1))" line
}

proc swig9 { line_var } {
  upvar 1 $line_var line
  regsub "_wrap_(.*)\\\(ClientData clientData," $line \
    "_wrap_\\1(ClientData," line
}

# (char *)"string" -> "string"
proc swig10 { line_var } {
  upvar 1 $line_var line
  regsub "\\\(char \\\*\\\)\"" $line \" line
  regsub "\\\(char \\\*\\\) \"" $line \" line
}  

# SWIG_Tcl_MakePtr flags arg is set but not ref'd
proc swig11 { line_var } {
  upvar 1 $line_var line
  regsub "SWIG_Tcl_MakePtr\\\(char \\\*c, void \\\*ptr, swig_type_info \\\*ty, int flags\\\)"\
    $line "SWIG_Tcl_MakePtr(char *c, void *ptr, swig_type_info *ty, int)" line
  regexp "flags = 0;" $line "" line
}

# SWIG_Tcl_MethodCommand Tcl_Obj *CONST _objv[] should not have CONST
proc swig12 { line_var } {
  upvar 1 $line_var line
  regsub "SWIG_Tcl_MethodCommand\\\(ClientData clientData, Tcl_Interp \\\*interp, int objc, Tcl_Obj \\\*CONST _objv\\\[\\\]\\\)"\
    $line "SWIG_Tcl_MethodCommand(ClientData clientData, Tcl_Interp *interp, int objc, Tcl_Obj *_objv[])" line
}

# Tcl_SetResult(interp, "string"
# Tcl_SetResult(interp, const_cast<char*>("string")
proc swig13 { line_var } {
  upvar 1 $line_var line
  regsub "Tcl_SetResult\\\(interp, *(\"\[^\"\]*\")" $line \
    "Tcl_SetResult(interp, const_cast<char*>(\"\1\")" line
}

# Tcl_SetResult (char *)"string"
# Tcl_SetResult const_cast<char*>("string")
proc swig14 { line_var } {
  upvar 1 $line_var line
  regsub "Tcl_SetResult\\\(interp,\\\(char\\\*\\\)(\"\[^\"\]*\")" $line \
    "Tcl_SetResult(interp,const_cast<char*>(\"\1\")" line
}

# (char *) Tcl_GetStringResult(interp)
# const_cast<char *>(Tcl_GetStringResult(interp))
proc swig15 { line_var } {
  upvar 1 $line_var line
  regsub "\\\(char \\\*\\\) Tcl_GetStringResult\\\(interp\\\)" $line \
    "const_cast<char *>(Tcl_GetStringResult(interp))" line
}

# (char *) meth->name
# const_cast<char *>(meth->name)
proc swig16 { line_var } {
  upvar 1 $line_var line
  regsub "\\\(char \\\*\\\) meth->name" $line \
    "const_cast<char *>(meth->name)" line
}

# result = (char *)
# result = const_cast<char *>
proc swig17 { line_var } {
  upvar 1 $line_var line
  regsub "result = \\\(char \\\*\\\)(.*);" $line \
    "result = const_cast<char *>(\\1);" line
}

# swig_class missing hashtable initializer
proc swig18 { line_var } {
  upvar 1 $line_var line
  regsub "base_names, &swig_module" $line \
    {base_names, \&swig_module, {0,{NULL,NULL,NULL,NULL},0,0,0,0,0,0,NULL,NULL,NULL}} line
}

# (char*)SWIG_name
# const_cast<char*>(SWIG_name)
# (char*)SWIG_version
# const_cast<char*>(SWIG_version)
proc swig19 { line_var } {
  upvar 1 $line_var line
  regsub -all "\\\(char\\\*\\\)(SWIG_name|SWIG_version)" $line \
    "const_cast<char *>(\\1)" line
}

# (char*)swig_commands[i].name|swig_variables[i].name
# const_cast<char*>(swig_commands[i].name|swig_variables[i].name)
proc swig20 { line_var } {
  upvar 1 $line_var line
  regsub "\\\(char \\\*\\\) (swig_\[^,\]+)," $line \
    "const_cast<char *>(\\1)," line
}

# #define SWIG_TCL_HASHTABLE_INIT {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
# to
# #define SWIG_TCL_HASHTABLE_INIT {0, {0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, 0, 0, 0}
proc swig21 { line_var } {
  upvar 1 $line_var line
  regsub "#define SWIG_TCL_HASHTABLE_INIT \\\{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0\\\}" $line \
    "#define SWIG_TCL_HASHTABLE_INIT {0, {0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, 0, 0, 0}" line
}

proc swig_sun { line_var out_stream } {
  upvar 1 $line_var line
  # Sun compiler complains about string constants passed as char* args.
  regsub "Tcl_Eval\\\(interp, \\\"namespace eval \\\" SWIG_namespace \\\" \\\{ \\\}\\\"\\\);" $line \
    "Tcl_Eval(interp, const_cast<char*>(\"namespace eval \" SWIG_namespace \" { }\"));" line
  
  # Add extern "C" around wrap functions to make sun compiler happy.
  if { !$in_extern_c && [regexp "^static int" $line] } {
    gets $in_stream next_line
    if { [regexp "^_wrap_" $next_line] } {
      puts $out_stream "extern \"C\" \{"
      puts $out_stream "static int"
      set extern_c_last_line "\}"
      set in_extern_c 1
    } else {
      puts $out_stream $line
    }
    set line $next_line
  }
  if { !$in_extern_c && [regexp "^typedef int \\(\\*swig_wrapper\\)" $line] } {
    puts $out_stream "extern \"C\" \{"
    set extern_c_last_line "\} swig_instance;"
    set in_extern_c 1
  }
}

set in_extern_c 0
while { ! [eof $in_stream] } {
  gets $in_stream line
  
  if { $swig_version1 == 1 && $swig_version2 == 3 } {
    if { $swig_version3 >= 40 } {
      swig3 line
      swig5 line
      swig9 line
      swig11 line
    } elseif { $swig_version3 > 29 && $swig_version3 < 40 } {
      swig3 line
      swig5 line
      swig9 line
    } elseif { $swig_version3 == 29 } {
      swig3 line
      swig5 line
      swig7 line $out_stream
      swig9 line
    } elseif { $swig_version3 == 28 } {
      swig5 line
      swig7 line $out_stream
      swig9 line
    } elseif { $swig_version3 == 27 } {
      swig3 line
      swig5 line
      swig7 line $out_stream
      swig8 line
      swig9 line
    } elseif { $swig_version3 == 25 } {
      swig1 line
      swig3 line
      swig5 line
      swig7 line $out_stream
      swig8 line
      swig9 line
    } elseif { $swig_version3 == 21 } {
      swig1 line
      swig3 line
      swig4 line
      swig5 line
      swig6 line $out_stream
      swig9 line
    } elseif { $swig_version3 == 19 } {
      swig1 line
      swig3 line
      swig4 line
      swig5 line
      swig6 line $out_stream
      swig9 line
    } elseif { $swig_version3 == 14 } {
      swig1 line
      swig6 line $out_stream
      swig9 line
    }
  } elseif { $swig_version1 == 2 && $swig_version2 == 0 } {
    if { $swig_version3 >= 4 } {
      swig10 line
      swig11 line
      swig12 line
      swig13 line
      swig14 line
      swig15 line
      swig16 line
      swig17 line
      swig18 line
      swig19 line
      swig20 line
    }
  } elseif { $swig_version1 == 3 \
	       && $swig_version2 == 0 \
	       && $swig_version3 == 8 } {
    swig3 line
    swig5 line
    swig11 line
  } elseif { $swig_version1 == 3 \
	       && $swig_version2 == 0 \
	       && $swig_version3 == 10 } {
    swig21 line
  } elseif { $swig_version1 == 3 \
	       && $swig_version2 == 0 \
	       && ($swig_version3 == 11 \
		   || $swig_version3 == 12) } {
    swig3 line
    swig5 line
    swig21 line
  } elseif { $swig_version1 == 3 && $swig_version2 == 0 } {
    swig18 line
  }
  # Close the extern "C".
  if { $in_extern_c && $line == $extern_c_last_line } {
    puts $out_stream $line
    set line "\} // extern \"C\""
    set in_extern_c 0
  }
  puts $out_stream $line
}

close $in_stream

# Disable emacs syntax highlighting.
puts $out_stream "// Local Variables:"
puts $out_stream "// mode:c++"
puts $out_stream "// End:"

close $out_stream

file delete $backup_file
