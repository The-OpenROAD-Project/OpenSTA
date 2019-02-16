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

#ifndef STA_WRITE_SDC_H
#define STA_WRITE_SDC_H

#include "NetworkClass.hh"
#include "SdcClass.hh"

namespace sta {

// Write constraints to a file.
// Allow contraints to apply to an instance to support write_context.
void
writeSdc(Instance *instance,
	 const char *filename,
	 const char *creator,
	 bool compatible,
	 bool no_timestamp,
	 int digits,
	 Sdc *sdc);

} // namespace
#endif
