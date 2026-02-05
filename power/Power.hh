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

#pragma once

#include <utility>
#include <string>
#include <unordered_map>

#include "StaConfig.hh"  // CUDD
#include "Network.hh"
#include "SdcClass.hh"
#include "PowerClass.hh"
#include "StaState.hh"
#include "Bdd.hh"

struct DdNode;
struct DdManager;

namespace sta {

class Sta;
class Scene;
class PropActivityVisitor;
class BfsFwdIterator;
class Vertex;
class ClkNetwork;

using SeqPin = std::pair<const Instance*, LibertyPort*>;

class SeqPinHash
{
public:
  SeqPinHash(const Network *network);
  size_t operator()(const SeqPin &pin) const;

private:
  const Network *network_;
};

class SeqPinEqual
{
public:
  bool operator()(const SeqPin &pin1,
		  const SeqPin &pin2) const;
};

using PwrActivityMap = std::unordered_map<const Pin*, PwrActivity>;
using PwrSeqActivityMap = std::unordered_map<SeqPin, PwrActivity,
                                             SeqPinHash, SeqPinEqual>;

// The Power class has access to Sta components directly for
// convenience but also requires access to the Sta class member functions.
class Power : public StaState
{
public:
  Power(StaState *sta);
  void clear();
  void activitiesInvalid();
  void reportDesign(const Scene *scene,
                    int digits);
  void reportInsts(const InstanceSeq &insts,
                   const Scene *scene,
                   int digits);
  void reportHighestInsts(size_t count,
                          const Scene *scene,
                          int digits);
  void reportDesignJson(const Scene *scene,
                        int digits);
  void reportInstsJson(const InstanceSeq &insts,
                       const Scene *scene,
                       int digits);
  InstPowers highestInstPowers(size_t count,
                               const Scene *scene);
  InstPowers sortInstsByPower(const InstanceSeq &insts,
                              const Scene *scene);

  void power(const Scene *scene,
	     // Return values.
	     PowerResult &total,
	     PowerResult &sequential,
  	     PowerResult &combinational,
             PowerResult &clock,
	     PowerResult &macro,
	     PowerResult &pad);
  PowerResult power(const Instance *inst,
                    const Scene *scene);

  void setGlobalActivity(float activity,
			 float duty);
  void unsetGlobalActivity();
  void setInputActivity(float activity,
			float duty);
  void unsetInputActivity();
  void setInputPortActivity(const Port *input_port,
			    float activity,
			    float duty);
  void unsetInputPortActivity(const Port *input_port);
  PwrActivity pinActivity(const Pin *pin,
                          const Scene *scene);
  void setUserActivity(const Pin *pin,
		       float activity,
		       float duty,
		       PwrActivityOrigin origin);
  void unsetUserActivity(const Pin *pin);
  void reportActivityAnnotation(bool report_unannotated,
                                bool report_annotated);
  float clockMinPeriod(const Sdc *sdc);
  float clockMinPeriod();
  void powerInvalid();

protected:
  PwrActivity &activity(const Pin *pin);
  bool inClockNetwork(const Instance *inst,
                      const ClkNetwork *clk_network);
  void powerInside(const Instance *hinst,
                   const Scene *scene,
                   PowerResult &result);
  void ensureActivities(const Scene *scene);
  bool hasUserActivity(const Pin *pin);
  PwrActivity &userActivity(const Pin *pin);
  void setSeqActivity(const Instance *reg,
		      LibertyPort *output,
		      PwrActivity &activity);
  bool hasSeqActivity(const Instance *reg,
		      LibertyPort *output);
  PwrActivity &seqActivity(const Instance *reg,
                           LibertyPort *output);
  bool hasActivity(const Pin *pin);
  void setActivity(const Pin *pin,
		   PwrActivity &activity);
  PwrActivity findActivity(const Pin *pin);
  void reportPowerRowJson(const char *name,
                          const PowerResult &power,
                          int digits,
                          const char *separator);
  void reportPowerInstJson(const Instance *inst,
                           const PowerResult &power,
                           int digits);

  void ensureInstPowers();
  void findInstPowers();
  PowerResult power(const Instance *inst,
                    LibertyCell *cell,
                    const Scene *scene);
  void findInternalPower(const Instance *inst,
                         LibertyCell *cell,
                         const Scene *scene,
                         // Return values.
                         PowerResult &result);
  void findInputInternalPower(const Pin *to_pin,
			      LibertyPort *to_port,
			      const Instance *inst,
			      LibertyCell *cell,
			      PwrActivity &to_activity,
			      float load_cap,
                              const Scene *scene,
			      // Return values.
			      PowerResult &result);
  void findOutputInternalPower(const LibertyPort *to_port,
			       const Instance *inst,
			       LibertyCell *cell,
			       PwrActivity &to_activity,
			       float load_cap,
                               const Scene *scene,
			       // Return values.
			       PowerResult &result);
  void findLeakagePower(const Instance *inst,
			LibertyCell *cell,
                        const Scene *scene,
			// Return values.
			PowerResult &result);
  void findSwitchingPower(const Instance *inst,
                          LibertyCell *cell,
                          const Scene *scene,
                          // Return values.
                          PowerResult &result);
  float getSlew(Vertex *vertex,
                const RiseFall *rf,
                const Scene *scene);
  float getMinRfSlew(const Pin *pin);
  const Clock *findInstClk(const Instance *inst);
  const Clock *findClk(const Pin *to_pin);
  float clockDuty(const Clock *clk);
  PwrActivity findSeqActivity(const Instance *inst,
			      LibertyPort *port);
  float portVoltage(LibertyCell *cell,
		    const LibertyPort *port,
                    const Scene *scene,
                    const MinMax *min_max);
  float pgNameVoltage(LibertyCell *cell,
		      const char *pg_port_name,
                      const Scene *scene,
                      const MinMax *min_max);
  void seedActivities(BfsFwdIterator &bfs);
  void seedRegOutputActivities(const Instance *reg,
			       const Sequential &seq,
			       LibertyPort *output,
			       bool invert);
  void seedRegOutputActivities(const Instance *inst,
			       BfsFwdIterator &bfs);
  void seedRegOutputActivities(const Instance *inst,
                               const LibertyCell *test_cell,
                               const SequentialSeq &seqs,
                               BfsFwdIterator &bfs);
  PwrActivity evalActivity(FuncExpr *expr,
			   const Instance *inst);
  PwrActivity evalActivity(FuncExpr *expr,
			   const Instance *inst,
			   const LibertyPort *cofactor_port,
			   bool cofactor_positive);
  LibertyPort *findExprOutPort(FuncExpr *expr);
  float findInputDuty(const Instance *inst,
		      FuncExpr *func,
		      const InternalPower *pwr);
  float evalDiffDuty(FuncExpr *expr,
                     LibertyPort *from_port,
                     const Instance *inst);
  LibertyPort *findLinkPort(const LibertyCell *cell,
                            const LibertyPort *scene_port);
  Pin *findLinkPin(const Instance *inst,
                   const LibertyPort *scene_port);
  void clockGatePins(const Instance *inst,
                     // Return values.
                     const Pin *&enable,
                     const Pin *&clk,
                     const Pin *&gclk) const;
  float evalBddActivity(DdNode *bdd,
                        const Instance *inst);
  float evalBddDuty(DdNode *bdd,
                    const Instance *inst);
  void findUnannotatedPins(const Instance *inst,
                           PinSeq &unannotated_pins);
  size_t pinCount();

private:
  const Scene *scene_;
  // Port/pin activities set by set_pin_activity.
  // set_pin_activity -global
  PwrActivity global_activity_;
  // set_pin_activity -input
  PwrActivity input_activity_;
  // set_pin_activity -input_ports -pins
  PwrActivityMap user_activity_map_;
  // Propagated activities.
  PwrActivityMap activity_map_;
  PwrSeqActivityMap seq_activity_map_;
  bool activities_valid_;
  Bdd bdd_;
  std::map<const Instance*, PowerResult, InstanceIdLess> instance_powers_;
  bool instance_powers_valid_;

  static constexpr int max_activity_passes_ = 50;

  friend class PropActivityVisitor;
};

} // namespace
