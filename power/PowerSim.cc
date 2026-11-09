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

// Vectorless activities from a random zero-delay simulation of the netlist.
// Each net holds a few 64 bit words, one bit per independent random copy
// (lane) of the design. Primary inputs and outputs of cells without a
// function (macros) are two state processes with their set or default
// activity; clock ports follow their clock. Every clock cycle is evaluated
// as a high phase then a low phase so flops capture on a rising edge of
// their own clock function and latches are transparent while enabled.
// Clock gate cells are a latch on the enable, transparent while the clock
// is low (high for negative edge gates). Toggles and ones
// counted over the measured cycles annotate every pin of each net like
// read_saif, so report_power uses them unchanged.

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>
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
#include "Sequential.hh"
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
  state,
  state_inv,
  one,
  zero,
  not_,
  and_,
  or_,
  xor_,
  store,
  store_next,
  flop,
  latch,
  clear,
  preset
};

struct TapeInstr
{
  TapeOp op;
  uint32_t arg;
};

// Cell function program; sig and state arguments are renumbered when the
// programs are lowered to tapes.
using SimProgram = std::vector<TapeInstr>;

enum class SimNodeKind
{
  comb,
  flop,
  latch
};

struct SimOutput
{
  int signal;
  SimProgram func;
};

struct SimNode
{
  SimNodeKind kind;
  // Signals that must be evaluated before this node in a phase.
  std::vector<int> deps;
  std::vector<SimOutput> outputs;
  int state = -1;
  // Flop clock, latch enable.
  SimProgram clock;
  // Flop next_state, latch data_in.
  SimProgram next;
  SimProgram clear;
  SimProgram preset;
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
    Word word = 0;
    for (int i = 0; i < 16; i++)
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
             float period,
             const std::vector<const Clock *> &clocks);
  void run(int warmup,
           int cycles);
  void annotate(int cycles,
                float period);

private:
  int signal(const Pin *drvr);
  int loadSignal(const Pin *load);
  SimProgram compile(const FuncExpr *expr,
                     const Instance *inst,
                     const std::map<const LibertyPort *, int> &states,
                     std::vector<int> *deps);
  void addInstance(const Instance *inst);
  void addSource(int sig,
                 const PwrActivity &activity,
                 float period);
  void sortNodes();
  void lower();
  template <int W>
  void runTape(const std::vector<TapeInstr> &tape);
  template <int W>
  void runCycles(int warmup,
                 int cycles);
  Word *sig(int index) { return &signals_[index * words_]; }
  Word *state(int index) { return &states_[index * words_]; }

  Power *power_;
  const Scene *scene_;
  Network *network_;
  Report *report_;
  const Sim *sim_;
  int words_;
  bool verbose_;
  SimRng rng_;

  std::unordered_map<const Pin *, int> drvr_signal_;
  std::unordered_map<const Pin *, int> load_signal_;
  // Signals driven from outside the netlist logic: annotated pins and
  // clocks defined on cell pins. Their drivers are not evaluated.
  std::vector<bool> pinned_;
  int clock_gate_other_ = 0;
  // Constant 1 for undriven pins tied high, created on first use.
  int const_one_ = -1;
  std::vector<const Pin *> signal_drvr_;
  std::vector<std::vector<const Pin *>> signal_loads_;
  std::vector<int8_t> forced_;  // -1 free, 0/1 forced constant
  std::vector<SimNode> nodes_;
  std::vector<int> order_;
  std::vector<SimSource> sources_;
  std::vector<int> clock_signals_;
  int state_count_ = 0;
  int macro_count_ = 0;
  int loop_count_ = 0;

  std::vector<Word> signals_;
  std::vector<Word> prev_signals_;
  std::vector<Word> states_;
  std::vector<Word> next_states_;  // flop next_state sampled at phase start
  std::vector<Word> prev_clocks_;  // flop clock function in previous phase
  std::vector<Word> stack_;
  std::vector<TapeInstr> eval_tape_;
  std::vector<TapeInstr> next_tape_;
  size_t node_count_ = 0;
  std::vector<int> macro_signals_;
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
  words_(words),
  verbose_(verbose),
  rng_(seed)
{
  // Signal 0 is the constant 0 for undriven pins.
  signal_drvr_.push_back(nullptr);
  signal_loads_.emplace_back();
  forced_.push_back(0);
  pinned_.push_back(false);
}

int
ActivitySim::signal(const Pin *drvr)
{
  auto itr = drvr_signal_.find(drvr);
  if (itr != drvr_signal_.end())
    return itr->second;
  int index = signal_drvr_.size();
  drvr_signal_[drvr] = index;
  signal_drvr_.push_back(drvr);
  signal_loads_.emplace_back();
  LogicValue value = sim_->simValue(drvr);
  forced_.push_back(value == LogicValue::zero ? 0
                    : value == LogicValue::one ? 1 : -1);
  pinned_.push_back(false);
  return index;
}

int
ActivitySim::loadSignal(const Pin *load)
{
  auto itr = load_signal_.find(load);
  if (itr != load_signal_.end())
    return itr->second;
  PinSet *drvrs = network_->drivers(load);
  int sig = 0;
  if (drvrs && !drvrs->empty()) {
    sig = signal(*drvrs->begin());
    signal_loads_[sig].push_back(load);
  }
  else if (sim_->simValue(load) == LogicValue::one) {
    // Tied high without a driver cell (Verilog 1'b1).
    if (const_one_ < 0) {
      const_one_ = signal_drvr_.size();
      signal_drvr_.push_back(nullptr);
      signal_loads_.emplace_back();
      forced_.push_back(1);
      pinned_.push_back(true);
      sources_.push_back({const_one_, 1.0F, 0.0F, 0.0F});
    }
    sig = const_one_;
  }
  load_signal_[load] = sig;
  return sig;
}

SimProgram
ActivitySim::compile(const FuncExpr *expr,
                     const Instance *inst,
                     const std::map<const LibertyPort *, int> &states,
                     std::vector<int> *deps)
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
      LibertyPort *port = expr->port();
      auto state_itr = states.find(port);
      if (state_itr != states.end()) {
        int state_index = state_itr->second;
        push({state_index < 0 ? TapeOp::state_inv : TapeOp::state,
              static_cast<uint32_t>(state_index < 0 ? -state_index - 1
                                    : state_index)});
      }
      else {
        const Pin *pin = network_->findPin(inst, port);
        int sig = pin ? loadSignal(pin) : 0;
        push({TapeOp::sig, static_cast<uint32_t>(sig)});
        if (deps)
          deps->push_back(sig);
      }
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
  if (expr)
    walk(expr);
  else
    push({TapeOp::zero, 0});
  max_stack_ = std::max(max_stack_, max_depth);
  return prog;
}

void
ActivitySim::addInstance(const Instance *inst)
{
  LibertyCell *cell = network_->libertyCell(inst);
  // Output pins and their liberty ports.
  std::vector<std::pair<const Pin *, LibertyPort *>> outputs;
  InstancePinIterator *pin_iter = network_->pinIterator(inst);
  while (pin_iter->hasNext()) {
    const Pin *pin = pin_iter->next();
    if (network_->direction(pin)->isAnyOutput()) {
      LibertyPort *port = network_->libertyPort(pin);
      if (!(port && port->isPwrGnd()))
        outputs.emplace_back(pin, port);
    }
  }
  delete pin_iter;
  if (outputs.empty())
    return;

  const LibertyCell *seq_cell = cell;
  if (cell && cell->sequentials().empty() && cell->testCell()
      && !cell->testCell()->sequentials().empty())
    seq_cell = cell->testCell();
  auto seqPort = [&](LibertyPort *port) {
    return seq_cell == cell ? port : seq_cell->findLibertyPort(port->name());
  };

  if (cell && cell->isClockGate()) {
    const Pin *enable = nullptr, *clk = nullptr, *gclk = nullptr;
    power_->clockGatePins(inst, enable, clk, gclk);
    if (clk && gclk) {
      bool negedge = cell->isClockGateLatchNegedge();
      if (cell->isClockGateOther())
        clock_gate_other_++;
      // A latch on the enable, transparent while the clock is low (high
      // for negative edge gates).
      SimNode node;
      node.kind = SimNodeKind::latch;
      node.state = state_count_++;
      int clk_sig = loadSignal(clk);
      node.clock = {{TapeOp::sig, static_cast<uint32_t>(clk_sig)}};
      if (!negedge)
        node.clock.push_back({TapeOp::not_, 0});
      node.deps.push_back(clk_sig);
      if (enable)
        node.next = {{TapeOp::sig,
                      static_cast<uint32_t>(loadSignal(enable))}};
      else
        node.next = {{TapeOp::one, 0}};
      // Positive edge: GCLK = CLK & latched enable. Negative edge:
      // GCLK = CLK | !latched enable.
      uint32_t st = node.state;
      node.outputs.push_back({signal(gclk),
                              {{TapeOp::sig, static_cast<uint32_t>(clk_sig)},
                               {negedge ? TapeOp::state_inv : TapeOp::state,
                                st},
                               {negedge ? TapeOp::or_ : TapeOp::and_, 0}}});
      nodes_.push_back(std::move(node));
      return;
    }
  }

  const SequentialSeq *seqs = seq_cell ? &seq_cell->sequentials() : nullptr;
  if (seqs && !seqs->empty()) {
    // One node per sequential; state ports map to their state index.
    std::map<const LibertyPort *, int> states;
    int first_state = state_count_;
    state_count_ += seqs->size();
    for (size_t i = 0; i < seqs->size(); i++) {
      const Sequential &seq = (*seqs)[i];
      int index = first_state + i;
      if (seq.output())
        states[seq.output()] = index;
      if (seq.outputInv())
        states[seq.outputInv()] = -index - 1;
    }
    size_t first_node = nodes_.size();
    for (size_t i = 0; i < seqs->size(); i++) {
      const Sequential &seq = (*seqs)[i];
      SimNode node;
      node.kind = seq.isRegister() ? SimNodeKind::flop : SimNodeKind::latch;
      node.state = first_state + i;
      // Flops depend on their clock and async controls; latches also on
      // their data.
      node.clock = compile(seq.clock(), inst, states, &node.deps);
      node.next = compile(seq.data(), inst, states,
                          seq.isLatch() ? &node.deps : nullptr);
      if (seq.clear())
        node.clear = compile(seq.clear(), inst, states, &node.deps);
      if (seq.preset())
        node.preset = compile(seq.preset(), inst, states, &node.deps);
      nodes_.push_back(std::move(node));
    }
    // Outputs go to the node of the state their function reads.
    for (auto [pin, port] : outputs) {
      LibertyPort *sport = port ? seqPort(port) : nullptr;
      FuncExpr *func = sport ? sport->function() : nullptr;
      size_t owner = first_node;
      if (func) {
        for (size_t i = 0; i < seqs->size(); i++) {
          const Sequential &seq = (*seqs)[i];
          if ((seq.output() && func->hasPort(seq.output()))
              || (seq.outputInv() && func->hasPort(seq.outputInv())))
            owner = first_node + i;
        }
      }
      SimNode &node = nodes_[owner];
      node.outputs.push_back({signal(pin),
                              compile(func, inst, states, &node.deps)});
    }
    return;
  }

  bool has_func = false;
  for (auto [pin, port] : outputs)
    if (port && port->function())
      has_func = true;
  if (cell && has_func) {
    SimNode node;
    node.kind = SimNodeKind::comb;
    std::map<const LibertyPort *, int> no_states;
    for (auto [pin, port] : outputs)
      node.outputs.push_back({signal(pin),
                              compile(port ? port->function() : nullptr,
                                      inst, no_states, &node.deps)});
    nodes_.push_back(std::move(node));
  }
  else {
    // Macros, memories and cells without a function drive their outputs
    // like primary inputs.
    macro_count_++;
    for (auto [pin, port] : outputs)
      macro_signals_.push_back(signal(pin));
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
                   float period,
                   const std::vector<const Clock *> &clocks)
{
  LeafInstanceIterator *inst_iter = network_->leafInstanceIterator();
  while (inst_iter->hasNext())
    addInstance(inst_iter->next());
  delete inst_iter;

  // Clocks drive the net of each source pin (port, cell output or input).
  for (const Clock *clk : clocks) {
    for (const Pin *pin : clk->leafPins()) {
      int sig = 0;
      auto itr = drvr_signal_.find(pin);
      if (itr != drvr_signal_.end())
        sig = itr->second;
      else if (network_->isLoad(pin))
        sig = loadSignal(pin);
      if (sig > 0 && !pinned_[sig]) {
        pinned_[sig] = true;
        clock_signals_.push_back(sig);
      }
    }
  }

  // Primary inputs, and cell outputs already annotated by set_power_activity,
  // read_saif or read_vcd, are sources with that activity. Signals pinned
  // so far are clocks.
  const Instance *top = network_->topInstance();
  for (size_t sig = 1; sig < signal_drvr_.size(); sig++) {
    if (pinned_[sig])
      continue;
    const Pin *drvr = signal_drvr_[sig];
    auto user = power_->user_activity_map_.find(drvr);
    bool annotated = user != power_->user_activity_map_.end();
    if (network_->instance(drvr) == top) {
      if (network_->direction(drvr)->isAnyInput())
        addSource(sig, annotated ? user->second : input_activity, period);
    }
    else if (annotated) {
      addSource(sig, user->second, period);
      pinned_[sig] = true;
    }
  }
  // Other macro outputs use the default input activity; annotated ones and
  // clocks are pinned.
  for (int sig : macro_signals_)
    if (!pinned_[sig])
      addSource(sig, input_activity, period);
  sortNodes();
  lower();

  size_t signal_count = signal_drvr_.size();
  signals_.assign(signal_count * words_, 0);
  prev_signals_.assign(signal_count * words_, 0);
  states_.resize(state_count_ * words_);
  for (Word &word : states_)
    word = rng_.next();
  next_states_.assign(state_count_ * words_, 0);
  prev_clocks_.assign(state_count_ * words_, 0);
  // Two programs can be stacked (clock and data of a latch).
  stack_.assign((max_stack_ + 2) * words_, 0);
  toggles_.assign(signal_count, 0);
  ones_.assign(signal_count, 0);
  for (const SimSource &source : sources_) {
    Word *value = sig(source.signal);
    for (int w = 0; w < words_; w++)
      value[w] = rng_.bernoulli(source.duty);
  }
  if (verbose_)
    report_->report("Simulation: {} cells {} sequential states {} sources "
                    "{} macros {} signals, {} lanes.",
                    node_count_, state_count_, sources_.size(), macro_count_,
                    signal_count, words_ * 64);
  if (loop_count_ > 0)
    report_->warn(2410, "{} cells in combinational loops are evaluated in "
                  "netlist order.", loop_count_);
  if (clock_gate_other_ > 0)
    report_->warn(2413, "{} clock gate cells of type other are simulated as "
                  "positive edge clock gates.", clock_gate_other_);
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
      // Break a combinational loop at one cell and continue the sort.
      while (placed[next_loop])
        next_loop++;
      order_.push_back(next_loop);
      placed[next_loop] = true;
      loop_count_++;
    }
  }
}

// Lower the sorted nodes to two flat instruction tapes with signals and
// states renumbered in evaluation order, so a phase streams through memory.
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
  for (int clk_sig : clock_signals_)
    mapSig(clk_sig);
  for (int n : order_)
    for (const SimOutput &output : nodes_[n].outputs)
      mapSig(output.signal);
  for (size_t old = 0; old < signal_count; old++)
    mapSig(old);

  // Each state belongs to one node.
  std::vector<int> stperm(state_count_, -1);
  int next_state = 0;
  for (int n : order_)
    if (nodes_[n].state >= 0)
      stperm[nodes_[n].state] = next_state++;

  auto emit = [&](std::vector<TapeInstr> &tape, const SimProgram &prog) {
    for (TapeInstr instr : prog) {
      if (instr.op == TapeOp::sig)
        instr.arg = sperm[instr.arg];
      else if (instr.op == TapeOp::state || instr.op == TapeOp::state_inv)
        instr.arg = stperm[instr.arg];
      tape.push_back(instr);
    }
  };
  for (int n : order_) {
    const SimNode &node = nodes_[n];
    uint32_t st = node.state >= 0 ? stperm[node.state] : 0;
    switch (node.kind) {
    case SimNodeKind::comb:
      break;
    case SimNodeKind::flop:
      emit(eval_tape_, node.clock);
      eval_tape_.push_back({TapeOp::flop, st});
      emit(next_tape_, node.next);
      next_tape_.push_back({TapeOp::store_next, st});
      break;
    case SimNodeKind::latch:
      emit(eval_tape_, node.clock);
      emit(eval_tape_, node.next);
      eval_tape_.push_back({TapeOp::latch, st});
      break;
    }
    if (!node.clear.empty()) {
      emit(eval_tape_, node.clear);
      eval_tape_.push_back({TapeOp::clear, st});
    }
    if (!node.preset.empty()) {
      emit(eval_tape_, node.preset);
      eval_tape_.push_back({TapeOp::preset, st});
    }
    for (const SimOutput &output : node.outputs) {
      if (pinned_[output.signal])
        continue;
      int8_t forced = forced_[output.signal];
      if (forced >= 0)
        eval_tape_.push_back({forced ? TapeOp::one : TapeOp::zero, 0});
      else
        emit(eval_tape_, output.func);
      eval_tape_.push_back({TapeOp::store,
                            static_cast<uint32_t>(sperm[output.signal])});
    }
  }

  // Renumber the per-signal tables.
  std::vector<const Pin *> drvrs(signal_count);
  std::vector<std::vector<const Pin *>> loads(signal_count);
  std::vector<int8_t> forced(signal_count);
  std::vector<bool> pinned(signal_count);
  for (size_t old = 0; old < signal_count; old++) {
    drvrs[sperm[old]] = signal_drvr_[old];
    loads[sperm[old]] = std::move(signal_loads_[old]);
    forced[sperm[old]] = forced_[old];
    pinned[sperm[old]] = pinned_[old];
  }
  signal_drvr_ = std::move(drvrs);
  signal_loads_ = std::move(loads);
  forced_ = std::move(forced);
  pinned_ = std::move(pinned);
  for (SimSource &source : sources_)
    source.signal = sperm[source.signal];
  for (int &clk_sig : clock_signals_)
    clk_sig = sperm[clk_sig];
  node_count_ = nodes_.size();
  nodes_ = {};
  drvr_signal_ = {};
  load_signal_ = {};
}

template <int W>
void
ActivitySim::runTape(const std::vector<TapeInstr> &tape)
{
  Word *sigs = signals_.data();
  Word *sts = states_.data();
  Word *nexts = next_states_.data();
  Word *prevs = prev_clocks_.data();
  Word *base = stack_.data();
  Word *top = base - W;
  for (const TapeInstr &instr : tape) {
    switch (instr.op) {
    case TapeOp::sig: {
      top += W;
      const Word *src = sigs + size_t(instr.arg) * W;
      for (int w = 0; w < W; w++)
        top[w] = src[w];
      break;
    }
    case TapeOp::state: {
      top += W;
      const Word *src = sts + size_t(instr.arg) * W;
      for (int w = 0; w < W; w++)
        top[w] = src[w];
      break;
    }
    case TapeOp::state_inv: {
      top += W;
      const Word *src = sts + size_t(instr.arg) * W;
      for (int w = 0; w < W; w++)
        top[w] = ~src[w];
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
    case TapeOp::store_next: {
      Word *dst = nexts + size_t(instr.arg) * W;
      for (int w = 0; w < W; w++)
        dst[w] = top[w];
      top -= W;
      break;
    }
    case TapeOp::flop: {
      // Capture on a rising edge of the clock function.
      size_t off = size_t(instr.arg) * W;
      Word *st = sts + off;
      Word *prev = prevs + off;
      const Word *next = nexts + off;
      for (int w = 0; w < W; w++) {
        Word rise = top[w] & ~prev[w];
        st[w] = (rise & next[w]) | (st[w] & ~rise);
        prev[w] = top[w];
      }
      top -= W;
      break;
    }
    case TapeOp::latch: {
      // Stack: enable, data.
      Word *st = sts + size_t(instr.arg) * W;
      const Word *en = top - W;
      for (int w = 0; w < W; w++)
        st[w] = (en[w] & top[w]) | (st[w] & ~en[w]);
      top -= 2 * W;
      break;
    }
    case TapeOp::clear: {
      Word *st = sts + size_t(instr.arg) * W;
      for (int w = 0; w < W; w++)
        st[w] &= ~top[w];
      top -= W;
      break;
    }
    case TapeOp::preset: {
      Word *st = sts + size_t(instr.arg) * W;
      for (int w = 0; w < W; w++)
        st[w] |= top[w];
      top -= W;
      break;
    }
    }
  }
}

template <int W>
void
ActivitySim::runCycles(int warmup,
                       int cycles)
{
  size_t signal_count = signal_drvr_.size();
  // Settle before the first edge with the clock low.
  runTape<W>(eval_tape_);
  if (warmup == 0)
    prev_signals_ = signals_;
  for (int cycle = 0; cycle < warmup + cycles; cycle++) {
    bool count = cycle >= warmup;
    for (int phase = 0; phase < 2; phase++) {
      bool high = phase == 0;
      // Flops capture the next_state of the settled previous phase.
      runTape<W>(next_tape_);
      if (high) {
        for (const SimSource &source : sources_) {
          Word *value = sig(source.signal);
          int8_t forced = forced_[source.signal];
          if (forced >= 0) {
            for (int w = 0; w < W; w++)
              value[w] = forced ? ~Word(0) : Word(0);
          }
          else if (source.rise > 0.0 || source.fall > 0.0) {
            for (int w = 0; w < W; w++) {
              Word rise = rng_.bernoulli(source.rise);
              Word fall = rng_.bernoulli(source.fall);
              value[w] = (value[w] & ~fall) | (~value[w] & rise);
            }
          }
        }
      }
      for (int clk_sig : clock_signals_) {
        Word *value = sig(clk_sig);
        for (int w = 0; w < W; w++)
          value[w] = high ? ~Word(0) : Word(0);
      }
      runTape<W>(eval_tape_);
      Word *value = signals_.data() + W;
      Word *prev = prev_signals_.data() + W;
      if (count) {
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
      else if (cycle == warmup - 1 && !high)
        // Counting starts from the last warmup phase.
        prev_signals_ = signals_;
    }
  }
}

void
ActivitySim::run(int warmup,
                 int cycles)
{
  switch (words_) {
  case 1: runCycles<1>(warmup, cycles); break;
  case 2: runCycles<2>(warmup, cycles); break;
  case 4: runCycles<4>(warmup, cycles); break;
  case 8: runCycles<8>(warmup, cycles); break;
  default: runCycles<16>(warmup, cycles); break;
  }
  // Only the counts are needed to annotate.
  signals_ = {};
  prev_signals_ = {};
  states_ = {};
  next_states_ = {};
  prev_clocks_ = {};
  stack_ = {};
  eval_tape_ = {};
  next_tape_ = {};
}

void
ActivitySim::annotate(int cycles,
                      float period)
{
  const ClkNetwork *clk_network = scene_->mode()->clkNetwork();
  double samples = static_cast<double>(cycles) * words_ * 64;
  PwrActivityMap &user_activity = power_->user_activity_map_;
  size_t pin_count = 0;
  for (const std::vector<const Pin *> &loads : signal_loads_)
    pin_count += 1 + loads.size();
  user_activity.reserve(user_activity.size() + pin_count);
  size_t annotated = 0;
  // A clock net that toggles every phase is the clock itself and keeps the
  // SDC waveform; gated clock nets are annotated.
  uint64_t clock_toggles = 2 * static_cast<uint64_t>(cycles) * words_ * 64;
  for (size_t s = 1; s < signal_drvr_.size(); s++) {
    const Pin *drvr = signal_drvr_[s];
    if (drvr
        && !(clk_network->isClock(drvr) && toggles_[s] == clock_toggles)) {
      PwrActivity activity(toggles_[s] / samples / period,
                           ones_[s] / (2.0 * samples),
                           PwrActivityOrigin::simulation);
      // Keep set_power_activity, read_saif and read_vcd annotations.
      annotated += user_activity.try_emplace(drvr, activity).second;
      for (const Pin *load : signal_loads_[s])
        annotated += user_activity.try_emplace(load, activity).second;
    }
  }
  power_->propagationInvalid();
  if (verbose_)
    report_->report("Simulation annotated {} pins.", annotated);
}

void
Power::simulateActivities(const Scene *scene,
                          int warmup,
                          int cycles,
                          int lanes,
                          uint64_t seed,
                          bool command)
{
  clearActivitySimResults();
  activity_sim_valid_ = true;
  activity_sim_mode_ = scene->mode();

  // Clocks with source pins; virtual and generated clocks are not simulated.
  const Sdc *sdc = scene->mode()->sdc();
  std::vector<const Clock *> clocks;
  float period = 0.0;
  for (const Clock *clk : sdc->clocks()) {
    if (!clk->isGenerated() && !clk->isVirtual() && !clk->leafPins().empty()
        && clk->period() > 0.0) {
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
    return;
  }
  for (const Clock *clk : clocks) {
    if (clk->period() != period) {
      // Clocks with different periods are not simulated yet. The default
      // (automatic) run leaves activities to the formulas instead.
      if (!command)
        return;
      report_->warn(2412, "clock {} period differs from the fastest clock; "
                    "simulation runs all clocks at the fastest period.",
                    clk->name());
    }
  }
  PwrActivity input_activity = input_activity_;
  if (input_activity.origin() == PwrActivityOrigin::unknown)
    input_activity.set(0.1 / period, 0.5, PwrActivityOrigin::input);
  // 64, 128, 256, 512 or 1024 lanes.
  int words = 1;
  while (words * 64 < lanes && words < 16)
    words *= 2;
  ActivitySim sim(this, scene, words, seed, command, this);
  sim.build(input_activity, period, clocks);
  sim.run(warmup, cycles);
  sim.annotate(cycles, period);
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
