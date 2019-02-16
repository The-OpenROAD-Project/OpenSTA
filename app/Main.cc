// OpenSTA, Static Timing Analyzer
// Copyright (c) 2019, Parallax Software, Inc.
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <stdio.h>
#include "Machine.hh"
#include "StaConfig.hh"  // STA_VERSION
#include "StringUtil.hh"
#include "Sta.hh"
#include "StaMain.hh"

using sta::stringEq;
using sta::Sta;
using sta::staMain;
using sta::showUseage;

// Swig uses C linkage for init functions.
extern "C" {
extern int Sta_Init(Tcl_Interp *interp);
}

int
main(int argc, char **argv)
{
  if (argc == 2 && stringEq(argv[1], "-help")) {
    showUseage(argv[0]);
    return 0;
  }
  else if (argc == 2 && stringEq(argv[1], "-version")) {
    printf("%s\n", STA_VERSION);
    return 0;
  }
  else {
    Sta *sta = new Sta;
    staMain(sta, argc, argv, Sta_Init);
    return 0;
  }
}
