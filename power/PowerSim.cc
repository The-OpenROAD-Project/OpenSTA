// OpenSTA, Static Timing Analyzer
// Copyright (c) 2026, Parallax Software, Inc.
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
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// The origin of this software must not be misrepresented; you must not
// claim that you wrote it. If you use this software in a product, an
// acknowledgment in the product documentation would be
// appreciated but is not required.
//
// Altered source versions must be plainly marked as such, and must not be
// misrepresented as being the original software.
//
// This notice may not be removed or altered from any source distribution.

// ---- OpenROAD fork: power activity simulation (whole file) ----

// Vectorless activities of combinational logic from a random zero-delay
// simulation. Activities are first propagated with the formulas. Primary
// inputs and the outputs of registers, latches, clock gates, macros and
// cells on the clock network are then two state processes with that
// activity, and the combinational cells between them are evaluated once per
// clock cycle. Each net holds a few 64 bit words, one bit per independent
// random copy (lane) of the design. Reconvergent fanout and correlation
// between the inputs of a cell are seen by the simulation, which the
// formulas, with independent inputs, do not see. Toggles and ones counted
// over the cycles annotate the driver of each combinational net; source
// drivers are annotated with the activity they were simulated with, so
// propagation keeps register outputs at that activity and copies driver
// activity to the loads.

#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ClkNetwork.hh"
#include "Clock.hh"
#include "FuncExpr.hh"
#include "Liberty.hh"
#include "Mode.hh"
#include "Network.hh"
#include "PortDirection.hh"
#include "Power.hh"
#include "Report.hh"
#include "Scene.hh"
#include "Sdc.hh"
#include "Units.hh"
#include "search/Sim.hh"

namespace sta {

// Helpers private to this file.
namespace {

using Word = uint64_t;

// Portable popcount without depending on -mpopcnt.
inline int
wordOnes(Word x)
{
  x = x - ((x >> 1) & 0x5555555555555555ULL);
  x = (x & 0x3333333333333333ULL) + ((x >> 2) & 0x3333333333333333ULL);
  x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0fULL;
  return static_cast<int>((x * 0x0101010101010101ULL) >> 56);
}

enum class TapeOp : uint8_t
{
  sig,
  one,
  zero,
  not_,
  and_,
  or_,
  xor_,
  store
};

struct TapeInstr
{
  TapeOp op;
  uint32_t arg;
};

// Cell function program; sig arguments are renumbered when the programs are
// lowered to the tape.
using SimProgram = std::vector<TapeInstr>;

struct SimOutput
{
  int signal;
  SimProgram func;
};

// A combinational cell.
struct SimNode
{
  // Signals that must be evaluated before this node.
  std::vector<int> deps;
  std::vector<SimOutput> outputs;
};

// Two state process driving a source signal.
struct SimSource
{
  int signal;
  float duty;
  float rise;  // P(0 -> 1) per cycle
  float fall;  // P(1 -> 0) per cycle
};

class SimRng
{
public:
  explicit SimRng(uint64_t seed) : state_(seed) {}
  Word
  next()
  {
    // splitmix64
    uint64_t z = (state_ += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
  }
  // Each bit is 1 with probability prob (16 bit resolution).
  Word
  bernoulli(float prob)
  {
    if (prob <= 0.0)
      return 0;
    if (prob >= 1.0)
      return ~Word(0);
    unsigned bits = static_cast<unsigned>(prob * 65536.0F + 0.5F);
    if (bits >= 65536)
      return ~Word(0);
    // Bits below the lowest set bit only AND into zero.
    Word word = 0;
    for (int i = std::countr_zero(bits); i < 16; i++)
      word = ((bits >> i) & 1) ? (word | next()) : (word & next());
    return word;
  }

private:
  uint64_t state_;
};

}  // namespace

class ActivitySim
{
public:
  ActivitySim(Power *power,
              const Scene *scene,
              int words,
              uint64_t seed,
              bool verbose,
              const StaState *sta);
  void build(const PwrActivity &input_activity,
             float period);
  void run(int cycles);
  void annotate(int cycles,
                float period);

private:
  int signal(const Pin *drvr);
  int loadSignal(const Pin *load);
  int sourceSignal(const PwrActivity &activity);
  int constOneSignal();
  bool sameActivity(const PwrActivity &activity,
                    const Pin *drvr) const;
  SimProgram compile(const FuncExpr *expr,
                     const Instance *inst,
                     std::vector<int> &deps);
  void addInstance(const Instance *inst);
  void addSource(int sig,
                 const PwrActivity &activity,
                 float period);
  void sortNodes();
  std::vector<int> loopNodes(const std::vector<std::vector<int>> &fanout);
  void lower();
  template <int W>
  void runTape();
  template <int W>
  void runCycles(int cycles);
  Word *sig(int index) { return &signals_[index * words_]; }

  Power *power_;
  const Scene *scene_;
  Network *network_;
  Report *report_;
  const Sim *sim_;
  const ClkNetwork *clk_network_;
  int words_;
  bool verbose_;
  SimRng rng_;

  std::unordered_map<const Pin *, int> drvr_signal_;
  std::unordered_map<const Pin *, int> load_signal_;
  // Signals not evaluated from a cell function: sources.
  std::vector<bool> pinned_;
  int cell_count_ = 0;
  // Constant 1 for undriven pins tied high, created on first use.
  int const_one_ = -1;
  // Sources for loads without a driver (floating or black box nets).
  std::unordered_map<const Net *, int> undriven_signal_;
  PwrActivity input_activity_;
  float period_ = 0.0;
  std::vector<const Pin *> signal_drvr_;
  std::vector<int8_t> forced_;  // -1 free, 0/1 forced constant
  std::vector<SimNode> nodes_;
  std::vector<int> order_;
  std::vector<SimSource> sources_;
  int loop_count_ = 0;

  std::vector<Word> signals_;
  std::vector<Word> prev_signals_;
  std::vector<Word> stack_;
  std::vector<TapeInstr> tape_;
  size_t max_stack_ = 1;
  std::vector<uint64_t> toggles_;
  std::vector<uint64_t> ones_;
};

ActivitySim::ActivitySim(Power *power,
                         const Scene *scene,
                         int words,
                         uint64_t seed,
                         bool verbose,
                         const StaState *sta) :
  power_(power),
  scene_(scene),
  network_(sta->network()),
  report_(sta->report()),
  sim_(scene->mode()->sim()),
  clk_network_(scene->mode()->clkNetwork()),
  words_(words),
  verbose_(verbose),
  rng_(seed)
{
  // Signal 0 is the constant 0 for undriven pins.
  signal_drvr_.push_back(nullptr);
  forced_.push_back(0);
  pinned_.push_back(false);
}

// Driver signals are sources until addInstance evaluates them.
int
ActivitySim::signal(const Pin *drvr)
{
  auto itr = drvr_signal_.find(drvr);
  if (itr != drvr_signal_.end())
    return itr->second;
  int index = signal_drvr_.size();
  drvr_signal_[drvr] = index;
  signal_drvr_.push_back(drvr);
  LogicValue value = sim_->simValue(drvr);
  forced_.push_back(value == LogicValue::zero ? 0
                    : value == LogicValue::one ? 1 : -1);
  pinned_.push_back(true);
  return index;
}

int
ActivitySim::loadSignal(const Pin *load)
{
  auto itr = load_signal_.find(load);
  if (itr != load_signal_.end())
    return itr->second;
  PinSet *drvrs = network_->drivers(load);
  const Pin *drvr = (drvrs && !drvrs->empty()) ? *drvrs->begin() : nullptr;
  const PwrActivityMap &user_activity = power_->user_activity_map_;
  auto user = user_activity.find(load);
  // Constants on the load itself (case analysis, set_logic, tie cells
  // without a driver cell such as Verilog 1'b1).
  LogicValue value = sim_->simValue(load);
  int sig = 0;
  if (value == LogicValue::zero)
    sig = 0;
  else if (value == LogicValue::one)
    sig = constOneSignal();
  else if (user != user_activity.end() && !sameActivity(user->second, drvr))
    // Annotated apart from its driver (set_power_activity -pins on a load).
    // read_vcd and read_saif annotate a whole net alike.
    sig = sourceSignal(user->second);
  else if (drvr)
    sig = signal(drvr);
  else {
    // Floating or driven by a black box: the default input activity, as
    // the formulas seed it.
    const Net *net = network_->net(load);
    if (net) {
      // One source for the whole flat net across hierarchy.
      net = network_->highestConnectedNet(net);
      auto itr = undriven_signal_.find(net);
      if (itr == undriven_signal_.end())
        itr = undriven_signal_.emplace(net, sourceSignal(input_activity_)).first;
      sig = itr->second;
    }
    else
      sig = sourceSignal(input_activity_);
  }
  load_signal_[load] = sig;
  return sig;
}

bool
ActivitySim::sameActivity(const PwrActivity &activity,
                          const Pin *drvr) const
{
  if (drvr == nullptr)
    return false;
  const PwrActivityMap &user_activity = power_->user_activity_map_;
  auto itr = user_activity.find(drvr);
  return itr != user_activity.end()
    && itr->second.density() == activity.density()
    && itr->second.duty() == activity.duty();
}

// A signal without a driver pin, driven as a source with activity.
int
ActivitySim::sourceSignal(const PwrActivity &activity)
{
  int sig = signal_drvr_.size();
  signal_drvr_.push_back(nullptr);
  forced_.push_back(-1);
  pinned_.push_back(true);
  addSource(sig, activity, period_);
  return sig;
}

int
ActivitySim::constOneSignal()
{
  if (const_one_ < 0) {
    const_one_ = sourceSignal(PwrActivity(0.0, 1.0,
                                          PwrActivityOrigin::constant));
    forced_[const_one_] = 1;
  }
  return const_one_;
}

SimProgram
ActivitySim::compile(const FuncExpr *expr,
                     const Instance *inst,
                     std::vector<int> &deps)
{
  SimProgram prog;
  size_t depth = 0;
  size_t max_depth = 0;
  auto push = [&](TapeInstr instr) {
    prog.push_back(instr);
    depth++;
    max_depth = std::max(max_depth, depth);
  };
  std::function<void(const FuncExpr *)> walk = [&](const FuncExpr *expr) {
    switch (expr->op()) {
    case FuncExpr::Op::port: {
      const Pin *pin = network_->findPin(inst, expr->port());
      int sig = pin ? loadSignal(pin) : 0;
      push({TapeOp::sig, static_cast<uint32_t>(sig)});
      deps.push_back(sig);
      break;
    }
    case FuncExpr::Op::not_:
      walk(expr->left());
      prog.push_back({TapeOp::not_, 0});
      break;
    case FuncExpr::Op::and_:
    case FuncExpr::Op::or_:
    case FuncExpr::Op::xor_:
      walk(expr->left());
      walk(expr->right());
      prog.push_back({expr->op() == FuncExpr::Op::and_ ? TapeOp::and_
                      : expr->op() == FuncExpr::Op::or_ ? TapeOp::or_
                      : TapeOp::xor_, 0});
      depth--;
      break;
    case FuncExpr::Op::one:
      push({TapeOp::one, 0});
      break;
    case FuncExpr::Op::zero:
    default:
      push({TapeOp::zero, 0});
      break;
    }
  };
  walk(expr);
  max_stack_ = std::max(max_stack_, max_depth);
  return prog;
}

// Combinational cells are evaluated. The outputs of registers, latches,
// clock gates and macros, and outputs on the clock network or annotated
// by the user, stay sources.
void
ActivitySim::addInstance(const Instance *inst)
{
  LibertyCell *cell = network_->libertyCell(inst);
  const LibertyCell *test_cell = cell ? cell->testCell() : nullptr;
  bool comb = cell && !cell->isClockGate() && cell->sequentials().empty()
    && !(test_cell && !test_cell->sequentials().empty());
  if (comb) {
    SimNode node;
    InstancePinIterator *pin_iter = network_->pinIterator(inst);
    while (pin_iter->hasNext()) {
      const Pin *pin = pin_iter->next();
      LibertyPort *port = network_->libertyPort(pin);
      if (network_->direction(pin)->isAnyOutput() && port && port->function()
          && !clk_network_->isClock(pin) && !power_->hasUserActivity(pin)) {
        int sig = signal(pin);
        pinned_[sig] = false;
        node.outputs.push_back({sig, compile(port->function(), inst,
                                             node.deps)});
      }
    }
    delete pin_iter;
    if (!node.outputs.empty()) {
      nodes_.push_back(std::move(node));
      cell_count_++;
    }
  }
}

void
ActivitySim::addSource(int sig,
                       const PwrActivity &activity,
                       float period)
{
  float duty = std::clamp(activity.duty(), 0.0F, 1.0F);
  float change = std::min(activity.density() * period,
                          2.0F * std::min(duty, 1.0F - duty));
  float rise = duty < 1.0 ? change / (2.0F * (1.0F - duty)) : 0.0F;
  float fall = duty > 0.0 ? change / (2.0F * duty) : 0.0F;
  sources_.push_back({sig, duty, rise, fall});
}

void
ActivitySim::build(const PwrActivity &input_activity,
                   float period)
{
  input_activity_ = input_activity;
  period_ = period;
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext())
    addInstance(inst_iter->next());
  delete inst_iter;

  // Drivers that are not evaluated are sources with their propagated
  // activity.
  for (size_t sig = 1; sig < signal_drvr_.size(); sig++) {
    const Pin *drvr = signal_drvr_[sig];
    if (pinned_[sig] && drvr)
      addSource(sig, power_->findActivity(drvr), period);
  }
  sortNodes();
  lower();

  size_t signal_count = signal_drvr_.size();
  signals_.assign(signal_count * words_, 0);
  prev_signals_.assign(signal_count * words_, 0);
  stack_.assign(max_stack_ * words_, 0);
  toggles_.assign(signal_count, 0);
  ones_.assign(signal_count, 0);
  for (const SimSource &source : sources_) {
    Word *value = sig(source.signal);
    int8_t forced = forced_[source.signal];
    for (int w = 0; w < words_; w++)
      value[w] = forced >= 0 ? (forced ? ~Word(0) : Word(0))
                             : rng_.bernoulli(source.duty);
  }
  if (verbose_)
    report_->report("Simulation: {} combinational cells {} sources "
                    "{} signals, {} lanes.",
                    cell_count_, sources_.size(), signal_count, words_ * 64);
  if (loop_count_ > 0)
    report_->warn(2410, "{} cells in combinational loops are evaluated in "
                  "netlist order.", loop_count_);
}

void
ActivitySim::sortNodes()
{
  // Signal -> producing node.
  std::vector<int> producer(signal_drvr_.size(), -1);
  for (size_t i = 0; i < nodes_.size(); i++)
    for (const SimOutput &output : nodes_[i].outputs)
      producer[output.signal] = i;
  std::vector<int> indegree(nodes_.size(), 0);
  std::vector<std::vector<int>> fanout(nodes_.size());
  for (size_t i = 0; i < nodes_.size(); i++) {
    std::vector<int> &deps = nodes_[i].deps;
    std::sort(deps.begin(), deps.end());
    deps.erase(std::unique(deps.begin(), deps.end()), deps.end());
    for (int sig : deps) {
      int from = producer[sig];
      if (from >= 0 && from != static_cast<int>(i)) {
        fanout[from].push_back(i);
        indegree[i]++;
      }
    }
  }
  std::vector<int> loop_nodes = loopNodes(fanout);
  order_.clear();
  std::vector<bool> placed(nodes_.size(), false);
  for (size_t i = 0; i < nodes_.size(); i++)
    if (indegree[i] == 0) {
      order_.push_back(i);
      placed[i] = true;
    }
  size_t k = 0;
  size_t next_loop = 0;
  while (order_.size() < nodes_.size() || k < order_.size()) {
    for (; k < order_.size(); k++)
      for (int to : fanout[order_[k]])
        if (--indegree[to] == 0 && !placed[to]) {
          order_.push_back(to);
          placed[to] = true;
        }
    if (order_.size() < nodes_.size()) {
      // Break the first unsorted combinational loop at one of its cells and
      // continue the sort.
      while (placed[loop_nodes[next_loop]])
        next_loop++;
      int node = loop_nodes[next_loop];
      order_.push_back(node);
      placed[node] = true;
      loop_count_++;
    }
  }
}

// Cells on combinational loops (strongly connected components with more
// than one cell, Tarjan), loops upstream first.
std::vector<int>
ActivitySim::loopNodes(const std::vector<std::vector<int>> &fanout)
{
  size_t node_count = fanout.size();
  std::vector<int> index(node_count, -1);
  std::vector<int> low(node_count, 0);
  std::vector<int> comp(node_count, -1);
  std::vector<int> comp_size;
  std::vector<bool> on_stack(node_count, false);
  std::vector<int> stack;
  std::vector<std::pair<int, size_t>> calls;  // node, next fanout
  int next_index = 0;
  for (size_t root = 0; root < node_count; root++) {
    if (index[root] >= 0)
      continue;
    index[root] = low[root] = next_index++;
    stack.push_back(root);
    on_stack[root] = true;
    calls.emplace_back(root, 0);
    while (!calls.empty()) {
      int v = calls.back().first;
      size_t &edge = calls.back().second;
      if (edge < fanout[v].size()) {
        int w = fanout[v][edge++];
        if (index[w] < 0) {
          index[w] = low[w] = next_index++;
          stack.push_back(w);
          on_stack[w] = true;
          calls.emplace_back(w, 0);
        }
        else if (on_stack[w])
          low[v] = std::min(low[v], index[w]);
      }
      else {
        calls.pop_back();
        if (!calls.empty()) {
          int u = calls.back().first;
          low[u] = std::min(low[u], low[v]);
        }
        if (low[v] == index[v]) {
          int c = comp_size.size();
          int size = 0;
          int w;
          do {
            w = stack.back();
            stack.pop_back();
            on_stack[w] = false;
            comp[w] = c;
            size++;
          } while (w != v);
          comp_size.push_back(size);
        }
      }
    }
  }
  // Tarjan finds downstream components first.
  std::vector<int> loop_nodes;
  for (size_t i = 0; i < node_count; i++)
    if (comp_size[comp[i]] > 1)
      loop_nodes.push_back(i);
  std::stable_sort(loop_nodes.begin(), loop_nodes.end(),
                   [&](int a, int b) { return comp[a] > comp[b]; });
  return loop_nodes;
}

// Lower the sorted nodes to a flat instruction tape with signals renumbered
// in evaluation order, so a cycle streams through memory.
void
ActivitySim::lower()
{
  size_t signal_count = signal_drvr_.size();
  std::vector<int> sperm(signal_count, -1);
  int next_sig = 0;
  auto mapSig = [&](int old) {
    if (sperm[old] < 0)
      sperm[old] = next_sig++;
    return sperm[old];
  };
  mapSig(0);
  for (const SimSource &source : sources_)
    mapSig(source.signal);
  for (int n : order_)
    for (const SimOutput &output : nodes_[n].outputs)
      mapSig(output.signal);
  for (size_t old = 0; old < signal_count; old++)
    mapSig(old);

  for (int n : order_) {
    for (const SimOutput &output : nodes_[n].outputs) {
      int8_t forced = forced_[output.signal];
      if (forced >= 0)
        tape_.push_back({forced ? TapeOp::one : TapeOp::zero, 0});
      else {
        for (TapeInstr instr : output.func) {
          if (instr.op == TapeOp::sig)
            instr.arg = sperm[instr.arg];
          tape_.push_back(instr);
        }
      }
      tape_.push_back({TapeOp::store,
                       static_cast<uint32_t>(sperm[output.signal])});
    }
  }

  // Renumber the per-signal tables.
  std::vector<const Pin *> drvrs(signal_count);
  std::vector<int8_t> forced(signal_count);
  std::vector<bool> pinned(signal_count);
  for (size_t old = 0; old < signal_count; old++) {
    drvrs[sperm[old]] = signal_drvr_[old];
    forced[sperm[old]] = forced_[old];
    pinned[sperm[old]] = pinned_[old];
  }
  signal_drvr_ = std::move(drvrs);
  forced_ = std::move(forced);
  pinned_ = std::move(pinned);
  for (SimSource &source : sources_)
    source.signal = sperm[source.signal];
  nodes_ = {};
  drvr_signal_ = {};
  load_signal_ = {};
}

template <int W>
void
ActivitySim::runTape()
{
  Word *sigs = signals_.data();
  Word *base = stack_.data();
  Word *top = base - W;
  for (const TapeInstr &instr : tape_) {
    switch (instr.op) {
    case TapeOp::sig: {
      top += W;
      const Word *src = sigs + size_t(instr.arg) * W;
      for (int w = 0; w < W; w++)
        top[w] = src[w];
      break;
    }
    case TapeOp::one:
      top += W;
      for (int w = 0; w < W; w++)
        top[w] = ~Word(0);
      break;
    case TapeOp::zero:
      top += W;
      for (int w = 0; w < W; w++)
        top[w] = 0;
      break;
    case TapeOp::not_:
      for (int w = 0; w < W; w++)
        top[w] = ~top[w];
      break;
    case TapeOp::and_:
      top -= W;
      for (int w = 0; w < W; w++)
        top[w] &= top[w + W];
      break;
    case TapeOp::or_:
      top -= W;
      for (int w = 0; w < W; w++)
        top[w] |= top[w + W];
      break;
    case TapeOp::xor_:
      top -= W;
      for (int w = 0; w < W; w++)
        top[w] ^= top[w + W];
      break;
    case TapeOp::store: {
      Word *dst = sigs + size_t(instr.arg) * W;
      for (int w = 0; w < W; w++)
        dst[w] = top[w];
      top -= W;
      break;
    }
    }
  }
}

// Sources start in their stationary distribution, so counting starts at
// the first cycle.
template <int W>
void
ActivitySim::runCycles(int cycles)
{
  size_t signal_count = signal_drvr_.size();
  runTape<W>();
  prev_signals_ = signals_;
  for (int cycle = 0; cycle < cycles; cycle++) {
    for (const SimSource &source : sources_) {
      if (forced_[source.signal] < 0
          && (source.rise > 0.0 || source.fall > 0.0)) {
        Word *value = sig(source.signal);
        for (int w = 0; w < W; w++) {
          Word rise = rng_.bernoulli(source.rise);
          Word fall = rng_.bernoulli(source.fall);
          value[w] = (value[w] & ~fall) | (~value[w] & rise);
        }
      }
    }
    runTape<W>();
    Word *value = signals_.data() + W;
    Word *prev = prev_signals_.data() + W;
    for (size_t s = 1; s < signal_count; s++) {
      uint64_t tog = 0, one = 0;
      for (int w = 0; w < W; w++) {
        tog += wordOnes(value[w] ^ prev[w]);
        one += wordOnes(value[w]);
        prev[w] = value[w];
      }
      toggles_[s] += tog;
      ones_[s] += one;
      value += W;
      prev += W;
    }
  }
}

void
ActivitySim::run(int cycles)
{
  switch (words_) {
  case 1: runCycles<1>(cycles); break;
  case 2: runCycles<2>(cycles); break;
  case 4: runCycles<4>(cycles); break;
  case 8: runCycles<8>(cycles); break;
  default: runCycles<16>(cycles); break;
  }
  // Only the counts are needed to annotate.
  signals_ = {};
  prev_signals_ = {};
  stack_ = {};
  tape_ = {};
}

void
ActivitySim::annotate(int cycles,
                      float period)
{
  double samples = static_cast<double>(cycles) * words_ * 64;
  PwrActivityMap &user_activity = power_->user_activity_map_;
  user_activity.reserve(user_activity.size() + signal_drvr_.size());
  size_t annotated = 0;
  for (size_t s = 1; s < signal_drvr_.size(); s++) {
    const Pin *drvr = signal_drvr_[s];
    // Primary inputs keep their set or default activity, the clock network
    // its clock activity and constants their value.
    if (drvr && !network_->isTopLevelPort(drvr)
        && !clk_network_->isClock(drvr) && forced_[s] < 0) {
      // Sources keep the activity they were simulated with.
      PwrActivity activity = pinned_[s]
        ? power_->findActivity(drvr)
        : PwrActivity(toggles_[s] / samples / period, ones_[s] / samples,
                      PwrActivityOrigin::simulation);
      activity.setOrigin(PwrActivityOrigin::simulation);
      // Keep set_power_activity, read_saif and read_vcd annotations.
      annotated += user_activity.try_emplace(drvr, activity).second;
    }
  }
  power_->propagationInvalid();
  if (verbose_)
    report_->report("Simulation annotated {} pins.", annotated);
}

void
Power::simulateActivities(const Scene *scene,
                          int cycles,
                          int lanes,
                          uint64_t seed,
                          bool command)
{
  clearActivitySimResults();
  activity_sim_valid_ = true;
  activity_sim_mode_ = scene->mode();

  // Clocks with source pins. Generated clocks on cells with a function
  // (dividers, clock gates) are derived from the design logic, so their
  // period does not matter; those on macro outputs (PLLs), ports or
  // undriven nets do.
  const Sdc *sdc = scene->mode()->sdc();
  std::vector<const Clock *> clocks;
  float period = 0.0;
  for (const Clock *clk : sdc->clocks()) {
    if (!clk->isVirtual() && !clk->leafPins().empty() && clk->period() > 0.0
        && !(clk->isGenerated() && simulatedClockSource(clk))) {
      clocks.push_back(clk);
      if (period == 0.0 || clk->period() < period)
        period = clk->period();
    }
  }
  if (period == 0.0) {
    // Without a clock the activities are propagated with formulas.
    if (command)
      report_->warn(2411, "power activity simulation needs a clock with a "
                    "source pin.");
    else if (!activity_sim_fallback_warned_) {
      report_->warn(2414, "power activity simulation needs a clock with a "
                    "source pin; activities are propagated with formulas.");
      activity_sim_fallback_warned_ = true;
    }
    return;
  }
  for (const Clock *clk : clocks) {
    if (clk->period() != period) {
      // Clocks with different periods are not simulated yet. The default
      // (automatic) run leaves activities to the formulas instead.
      if (!command) {
        if (!activity_sim_fallback_warned_) {
          report_->warn(2415, "power activity simulation does not support "
                        "clocks with different periods; activities are "
                        "propagated with formulas.");
          activity_sim_fallback_warned_ = true;
        }
        return;
      }
      report_->warn(2412, "clock {} period differs from the fastest clock; "
                    "simulation runs all clocks at the fastest period.",
                    clk->name());
    }
  }
  PwrActivity input_activity = input_activity_;
  if (input_activity.origin() == PwrActivityOrigin::unknown) {
    // As ensureActivities seeds it, so inputs match their propagated values.
    float min_period = clockMinPeriod(sdc);
    if (min_period == 0.0)
      min_period = units_->timeUnit()->scale();
    input_activity.set(0.1 / min_period, 0.5, PwrActivityOrigin::input);
  }
  // The sources take their activity from the formulas. The simulation is
  // valid, so this does not simulate again.
  ensureActivities(scene);
  // 64, 128, 256, 512 or 1024 lanes.
  int words = 1;
  while (words * 64 < lanes && words < 16)
    words *= 2;
  ActivitySim sim(this, scene, words, seed, command, this);
  sim.build(input_activity, period);
  sim.run(cycles);
  sim.annotate(cycles, period);
}

bool
Power::simulatedClockSource(const Clock *clk)
{
  for (const Pin *pin : clk->leafPins()) {
    const Pin *drvr = pin;
    if (!network_->isDriver(pin)) {
      PinSet *drvrs = network_->drivers(pin);
      drvr = (drvrs && !drvrs->empty()) ? *drvrs->begin() : nullptr;
    }
    if (drvr && !network_->isTopLevelPort(drvr)) {
      const LibertyCell *cell = network_->libertyCell(network_->instance(drvr));
      const LibertyPort *port = network_->libertyPort(drvr);
      const LibertyCell *test_cell = cell ? cell->testCell() : nullptr;
      const LibertyPort *test_port = (test_cell && port)
        ? test_cell->findLibertyPort(port->name()) : nullptr;
      if (cell && (cell->isClockGate() || (port && port->function())
                   || (test_port && test_port->function())))
        return true;
    }
  }
  return false;
}

void
Power::clearActivitySimResults()
{
  std::erase_if(user_activity_map_, [](const auto &pin_activity) {
    return pin_activity.second.origin() == PwrActivityOrigin::simulation;
  });
  propagationInvalid();
}

void
Power::setSimulateActivities(bool simulate)
{
  if (simulate != simulate_activities_) {
    simulate_activities_ = simulate;
    if (!simulate)
      clearActivitySimResults();
    activitiesInvalid();
  }
}

}  // namespace sta

// ---- OpenROAD fork: power activity simulation (end) ----
