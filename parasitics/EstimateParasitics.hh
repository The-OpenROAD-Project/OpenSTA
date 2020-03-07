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

#include "LibertyClass.hh"
#include "NetworkClass.hh"
#include "SdcClass.hh"
#include "ParasiticsClass.hh"

namespace sta {

class EstimateParasitics
{
public:

protected:
  // Helper function for wireload estimation.
  void estimatePiElmore(const Pin *drvr_pin,
			const RiseFall *rf,
			const Wireload *wireload,
			float fanout,
			float net_pin_cap,
			const OperatingConditions *op_cond,
			const Corner *corner,
			const MinMax *min_max,
			const StaState *sta,
			// Return values.
			float &c2,
			float &rpi,
			float &c1,
			float &elmore_res,
			float &elmore_cap,
			bool &elmore_use_load_cap);
  void estimatePiElmoreBest(const Pin *drvr_pin,
			    float net_pin_cap,
			    float wireload_cap,
			    const RiseFall *rf,
			    const OperatingConditions *op_cond,
			    const Corner *corner,
			    const MinMax *min_max,
			    // Return values.
			    float &c2,
			    float &rpi,
			    float &c1,
			    float &elmore_res,
			    float &elmore_cap,
			    bool &elmore_use_load_cap) const;
  void estimatePiElmoreWorst(const Pin *drvr_pin,
			     float wireload_cap,
			     float wireload_res,
			     float fanout,
			     float net_pin_cap,
			     const RiseFall *rf,
			     const OperatingConditions *op_cond,
			     const Corner *corner,
			     const MinMax *min_max,
			     const StaState *sta,
			     // Return values.
			     float &c2, float &rpi, float &c1,
			     float &elmore_res, float &elmore_cap,
			     bool &elmore_use_load_cap);
  void estimatePiElmoreBalanced(const Pin *drvr_pin,
				float wireload_cap,
				float wireload_res,
				float fanout,
				float net_pin_cap,
				const RiseFall *rf,
				const OperatingConditions *op_cond,
				const Corner *corner,
				const MinMax *min_max,
				const StaState *sta,
				// Return values.
				float &c2, float &rpi, float &c1,
				float &elmore_res, float &elmore_cap,
				bool &elmore_use_load_cap);
};

} // namespace
