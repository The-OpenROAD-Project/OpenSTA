// OpenSTA, Static Timing Analyzer
// Copyright (c) 2020, Parallax Software, Inc.
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

#pragma once

// The zlib package is optional.
// Define stdio based macros if it is missing.

#include "StaConfig.hh"  // ZLIB

#if ZLIB

#include <zlib.h>

#else // ZLIB

#include <stdio.h>

#define gzFile FILE*
#define gzopen fopen
#define gzclose fclose
#define gzgets(stream,s,size) fgets(s,size,stream)
#define gzprintf fprintf
#define Z_NULL nullptr

#endif // ZLIB
