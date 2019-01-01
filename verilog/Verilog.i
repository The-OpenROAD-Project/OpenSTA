%module verilog

%{

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

#include "VerilogReader.hh"
#include "Sta.hh"

using sta::Sta;
using sta::NetworkReader;
using sta::readVerilogFile;

%}

%inline %{

bool
read_verilog(const char *filename)
{
  Sta *sta = Sta::sta();
  NetworkReader *network = sta->networkReader();
  if (network) {
    sta->readNetlistBefore();
    return readVerilogFile(filename, sta->report(), sta->debug(), network);
  }
  else
    return false;
}

void
delete_verilog_reader()
{
  deleteVerilogReader();
}

%} // inline
