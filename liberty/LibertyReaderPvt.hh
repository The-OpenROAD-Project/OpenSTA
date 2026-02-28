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

#include <functional>
#include <memory>
#include <array>
#include <vector>
#include <unordered_map>

#include "StringSeq.hh"
#include "MinMax.hh"
#include "NetworkClass.hh"
#include "Transition.hh"
#include "TimingArc.hh"
#include "InternalPower.hh"
#include "LeakagePower.hh"
#include "Liberty.hh"
#include "Sequential.hh"
#include "TableModel.hh"
#include "LibertyParser.hh"
#include "LibertyReader.hh"
#include "LibertyBuilder.hh"

namespace sta {

class LibertyBuilder;
class LibertyReader;
class PortNameBitIterator;
class TimingArcBuilder;
class OutputWaveform;

using LibraryGroupVisitor = void (LibertyReader::*)(const LibertyGroup *group,
                                                    LibertyGroup *parent_group);
using LibraryGroupVisitorMap = std::unordered_map<std::string, LibraryGroupVisitor>;
using StdStringSeq = std::vector<std::string>;
using LibertyPortGroupMap = std::map<const LibertyGroup*, LibertyPortSeq,
                                     LibertyGroupLineLess>;
using OutputWaveformSeq = std::vector<OutputWaveform>;

class LibertyReader : public LibertyGroupVisitor
{
public:
  LibertyReader(const char *filename,
                bool infer_latches,
                Network *network);
  virtual LibertyLibrary *readLibertyFile(const char *filename);
  LibertyLibrary *library() { return library_; }
  const LibertyLibrary *library() const { return library_; }

  virtual void beginLibrary(const LibertyGroup *group,
                            LibertyGroup *library_group);
  virtual void endLibrary(const LibertyGroup *group,
                          LibertyGroup *null_group);
  virtual void visitAttr(const LibertySimpleAttr *attr);
  virtual void visitAttr(const LibertyComplexAttr *attr);
  virtual void visitVariable(LibertyVariable *var);
  // Extension points for custom attributes (e.g. LibertyExt).
  virtual void visitAttr1(const LibertySimpleAttr *) {}
  virtual void visitAttr2(const LibertySimpleAttr *) {}

  void endCell(const LibertyGroup *group,
               LibertyGroup *library_group);
  void endScaledCell(const LibertyGroup *group,
                     LibertyGroup *library_group);
  void checkScaledCell(LibertyCell *scaled_cell,
                       LibertyCell *owner,
                       const LibertyGroup *scaled_cell_group,
                       const char *op_cond_name);

  void setPortCapDefault(LibertyPort *port);
  void checkLatchEnableSense(FuncExpr *enable_func,
                             int line);
  FloatTable makeFloatTable(const LibertyComplexAttr *attr,
                            const LibertyGroup *table_group,
                            size_t rows,
                            size_t cols,
                            float scale);

  LibertyPort *findPort(LibertyCell *cell,
                        const char *port_name);
  StdStringSeq findAttributStrings(const LibertyGroup *group,
                                   const char *name_attr);

protected:
  virtual void begin(const LibertyGroup *group,
                     LibertyGroup *library_group);
  virtual void end(const LibertyGroup *group,
                   LibertyGroup *library_group);

  // Library gruops.
  void makeLibrary(const LibertyGroup *libary_group);
  void readLibraryAttributes(const LibertyGroup *library_group);
  void readLibraryUnits(const LibertyGroup *library_group);
  void readDelayModel(const LibertyGroup *library_group);
  void readBusStyle(const LibertyGroup *library_group);
  void readDefaultWireLoad(const LibertyGroup *library_group);
  void readDefaultWireLoadMode(const LibertyGroup *library_group);
  void readTechnology(const LibertyGroup *library_group);
  void readDefaultWireLoadSelection(const LibertyGroup *library_group);
  void readUnit(const char *unit_attr_name,
                const char *unit_suffix,
                float &scale_var,
                Unit *unit,
                const LibertyGroup *library_group);
  void readBusTypes(LibertyCell *cell,
                    const LibertyGroup *type_group);
  void readTableTemplates(const LibertyGroup *library_group);
  void readTableTemplates(const LibertyGroup *library_group,
                          const char *group_name,
                          TableTemplateType type);
  void readThresholds(const LibertyGroup *library_group);
  TableAxisPtr makeTableTemplateAxis(const LibertyGroup *template_group,
                                     int axis_index);
  void readVoltateMaps(const LibertyGroup *library_group);
  void readWireloads(const LibertyGroup *library_group);
  void readWireloadSelection(const LibertyGroup *library_group);
  void readOperatingConds(const LibertyGroup *library_group);
  void readScaleFactors(const LibertyGroup *library_group);
  void readScaleFactors(const LibertyGroup *scale_group,
                        ScaleFactors *scale_factors);
  void readOcvDerateFactors(LibertyCell *cell,
                            const LibertyGroup *library_group);
  void readDefaultOcvDerateGroup(const LibertyGroup *library_group);
  void readNormalizedDriverWaveform(const LibertyGroup *library_group);
  void readSlewDegradations(const LibertyGroup *library_group);
  void readLibAttrFloat(const LibertyGroup *library_group,
                        const char *attr_name,
                        void (LibertyLibrary::*set_func)(float value),
                        float scale);
  void readLibAttrFloat(const LibertyGroup *library_group,
                        const char *attr_name,
                        void (LibertyLibrary::*set_func)(const RiseFall *rf,
                                                         float value),
                        const RiseFall *rf,
                        float scale);
  void readLibAttrFloatWarnZero(const LibertyGroup *library_group,
                                const char *attr_name,
                                void (LibertyLibrary::*set_func)(float value),
                                float scale);

  // Cell groups.
  void readCell(LibertyCell *cell,
                const LibertyGroup *cell_group);
  void readScaledCell(const LibertyGroup *scaled_cell_group);
  LibertyPortGroupMap makeCellPorts(LibertyCell *cell,
                                    const LibertyGroup *cell_group);
  void makePinPort(LibertyCell *cell,
                   const LibertyGroup *pin_group,
                   LibertyPortGroupMap &port_group_map);
  void makeBusPort(LibertyCell *cell,
                   const LibertyGroup *bus_group,
                   LibertyPortGroupMap &port_group_map);
  void makeBusPinPorts(LibertyCell *cell,
                       const LibertyGroup *bus_group,
                       LibertyPortGroupMap &port_group_map);
  void makeBundlePort(LibertyCell *cell,
                      const LibertyGroup *bundle_group,
                      LibertyPortGroupMap &port_group_map);
  void makeBundlePinPorts(LibertyCell *cell,
                          const LibertyGroup *bundle_group,
                          LibertyPortGroupMap &port_group_map);
  void makePgPinPort(LibertyCell *cell,
                     const LibertyGroup *pg_pin_group);
  LibertyPort *makePort(LibertyCell *cell,
                        const char *port_name);
  LibertyPort *makeBusPort(LibertyCell *cell,
                           const char *bus_name,
                           int from_index,
                           int to_index,
                           BusDcl *bus_dcl);

  void readPortAttributes(LibertyCell *cell,
                          const LibertyPortSeq &ports,
                          const LibertyGroup *port_group);
  void readPortAttrString(const char *attr_name,
                          void (LibertyPort::*set_func)(const char *value),
                          const LibertyPortSeq &ports,
                          const LibertyGroup *group);
  void readPortAttrFloat(const char *attr_name,
                         void (LibertyPort::*set_func)(float value),
                         const LibertyPortSeq &ports,
                         const LibertyGroup *group,
                         float scale);
  void readPortAttrBool(const char *attr_name,
                        void (LibertyPort::*set_func)(bool value),
                        const LibertyPortSeq &ports,
                        const LibertyGroup *group);
  void readDriverWaveform(const LibertyPortSeq &ports,
                         const LibertyGroup *port_group);
  void readPortAttrFloatMinMax(const char *attr_name,
                               void (LibertyPort::*set_func)(float value,
                                                             const MinMax *min_max),
                               const LibertyPortSeq &ports,
                               const LibertyGroup *group,
                               const MinMax *min_max,
                               float scale);
  void readPulseClock(const LibertyPortSeq &ports,
                      const LibertyGroup *port_group);
  void readSignalType(LibertyCell *cell,
                      const LibertyPortSeq &ports,
                      const LibertyGroup *port_group);
  void readMinPulseWidth(LibertyCell *cell,
                         const LibertyPortSeq &ports,
                         const LibertyGroup *port_group);
  void readModeDefs(LibertyCell *cell,
                    const LibertyGroup *cell_group);
  void makeTimingArcs(LibertyCell *cell,
                      const LibertyPortSeq &ports,
                      const LibertyGroup *port_group);
  bool isGateTimingType(TimingType timing_type);
  TableModel *readGateTableModel(const LibertyGroup *timing_group,
                                 const char *table_group_name,
                                 const RiseFall *rf,
                                 TableTemplateType template_type,
                                 float scale,
                                 ScaleFactorType scale_factor_type);
  TableModelsEarlyLate
  readEarlyLateTableModels(const LibertyGroup *timing_group,
                           const char *table_group_name,
                           const RiseFall *rf,
                           TableTemplateType template_type,
                           float scale,
                           ScaleFactorType scale_factor_type);
  TableModel *readCheckTableModel(const LibertyGroup *timing_group,
                                  const char *table_group_name,
                                  const RiseFall *rf,
                                  TableTemplateType template_type,
                                  float scale,
                                  ScaleFactorType scale_factor_type);
  ReceiverModelPtr readReceiverCapacitance(const LibertyGroup *timing_group,
                                           const RiseFall *rf);
  void readReceiverCapacitance(const LibertyGroup *timing_group,
                               const char *cap_group_name,
                               int index,
                               const RiseFall *rf,
                               ReceiverModelPtr &receiver_model);
  OutputWaveforms *readOutputWaveforms(const LibertyGroup *timing_group,
                                       const RiseFall *rf);
  OutputWaveforms *makeOutputWaveforms(const LibertyGroup *current_group,
                                       OutputWaveformSeq &output_currents,
                                       const RiseFall *rf);

  TableModel *readTableModel(const LibertyGroup *table_group,
                             const RiseFall *rf,
                             TableTemplateType template_type,
                             float scale,
                             ScaleFactorType scale_factor_type);
  TablePtr readTableModel(const LibertyGroup *table_group,
                          const TableTemplate *tbl_template,
                          float scale);
  void makeTimingModels(LibertyCell *cell,
                        const LibertyGroup *timing_group,
                        TimingArcAttrsPtr timing_attrs);
  void makeLinearModels(LibertyCell *cell,
                        const LibertyGroup *timing_group,
                        TimingArcAttrsPtr timing_attrs);
  void makeTableModels(LibertyCell *cell,
                       const LibertyGroup *timing_group,
                       TimingArcAttrsPtr timing_attrs);

  TableAxisPtr makeTableAxis(const LibertyGroup *table_group,
                             const char *index_attr_name,
                             TableAxisPtr template_axis);
  void readGroupAttrFloat(const char *attr_name,
                          const LibertyGroup *group,
                          const std::function<void(float)> &set_func,
                          float scale = 1.0F);
  void readTimingArcAttrs(LibertyCell *cell,
                          const LibertyGroup *timing_group,
                          TimingArcAttrsPtr timing_attrs);
  void readTimingSense(const LibertyGroup *timing_group,
                       TimingArcAttrsPtr timing_attrs);
  void readTimingType(const LibertyGroup *timing_group,
                      TimingArcAttrsPtr timing_attrs);
  void readTimingWhen(const LibertyCell *cell,
                      const LibertyGroup *timing_group,
                      TimingArcAttrsPtr timing_attrs);
  void readTimingMode(const LibertyGroup *timing_group,
                      TimingArcAttrsPtr timing_attrs);
  void makePortFuncs(LibertyCell *cell,
                     const LibertyPortSeq &ports,
                     const LibertyGroup *port_group);

  void makeSequentials(LibertyCell *cell,
                       const LibertyGroup *cell_group);
  void makeSequentials(LibertyCell *cell,
                       const LibertyGroup *cell_group,
                       bool is_register,
                       const char *seq_group_name,
                       const char *clk_attr_name,
                       const char *data_attr_name);
  FuncExpr *makeSeqFunc(LibertyCell *cell,
                        const LibertyGroup *seq_group,
                        const char *attr_name,
                        int size);
  void makeSeqPorts(LibertyCell *cell,
                    const LibertyGroup *seq_group,
                    // Return values.
                    LibertyPort *&out_port,
                    LibertyPort *&out_port_inv,
                    size_t &size);
  void seqPortNames(const LibertyGroup *group,
                    const char *&out_name,
                    const char *&out_inv_name,
                    bool &has_size,
                    size_t &size);
  TimingModel *makeScalarCheckModel(LibertyCell *cell,
                                    float value,
                                    ScaleFactorType scale_factor_type,
                                    const RiseFall *rf);
  void readPortDir(const LibertyPortSeq &ports,
                   const LibertyGroup *port_group);
  void readCapacitance(const LibertyPortSeq &ports,
                       const LibertyGroup *port_group);
  void makeTimingArcs(LibertyCell *cell,
                      const std::string &from_port_name,
                      LibertyPort *to_port,
                      LibertyPort *related_out_port,
                      bool one_to_one,
                      TimingArcAttrsPtr timing_attrs,
                      int timing_line);
  void makeTimingArcs(LibertyCell *cell,
                      LibertyPort *to_port,
                      LibertyPort *related_out_port,
                      TimingArcAttrsPtr timing_attrs,
                      int timing_line);

  void readInternalPowerGroups(LibertyCell *cell,
                               const LibertyPortSeq &ports,
                               const LibertyGroup *port_group);
  void readLeagageGrouops(LibertyCell *cell,
                          const LibertyGroup *port_group);

  void readCellAttributes(LibertyCell *cell,
                          const LibertyGroup *cell_group);
  void readScaleFactors(LibertyCell *cell,
                        const LibertyGroup *cell_group);
  void readCellAttrString(const char *attr_name,
                          void (LibertyCell::*set_func)(const char *value),
                          LibertyCell *cell,
                          const LibertyGroup *group);
  void readCellAttrFloat(const char *attr_name,
                         void (LibertyCell::*set_func)(float value),
                         LibertyCell *cell,
                         const LibertyGroup *group,
                         float scale);
  void readCellAttrBool(const char *attr_name,
                         void (LibertyCell::*set_func)(bool value),
                        LibertyCell *cell,
                        const LibertyGroup *group);
  void readLevelShifterType(LibertyCell *cell,
                            const LibertyGroup *cell_group);
  void readSwitchCellType(LibertyCell *cell,
                          const LibertyGroup *cell_group);
  void readCellOcvDerateGroup(LibertyCell *cell,
                              const LibertyGroup *cell_group);
  void readStatetable(LibertyCell *cell,
                      const LibertyGroup *cell_group);
  void readTestCell(LibertyCell *cell,
                    const LibertyGroup *cell_group);

  FuncExpr *readFuncExpr(LibertyCell *cell,
                         const LibertyGroup *group,
                         const char *attr_name);
  LibertyPort *findLibertyPort(LibertyCell *cell,
                               const LibertyGroup *group,
                               const char *port_name_attr);
  LibertyPortSeq findLibertyPorts(LibertyCell *cell,
                                  const LibertyGroup *group,
                                  const char *port_name_attr);

  float energyScale();
  void defineVisitors();

  void defineGroupVisitor(const char *type,
                          LibraryGroupVisitor begin_visitor,
                          LibraryGroupVisitor end_visitor);

  float defaultCap(LibertyPort *port);
  void visitPorts(std::function<void (LibertyPort *port)> func);
  StateInputValues parseStateInputValues(StdStringSeq &inputs,
                                         const LibertySimpleAttr *attr);
  StateInternalValues parseStateInternalValues(StdStringSeq &states,
                                               const LibertySimpleAttr *attr);

  void getAttrInt(const LibertySimpleAttr *attr,
                  // Return values.
                  int &value,
                  bool &exists);
  void getAttrFloat2(const LibertyComplexAttr *attr,
                     // Return values.
                     float &value1,
                     float &value2,
                     bool &exists);
  void getAttrFloat(const LibertyComplexAttr *attr,
                    const LibertyAttrValue *attr_value,
                    // Return values.
                    float &value,
                    bool &valid);
  LogicValue getAttrLogicValue(const LibertySimpleAttr *attr);
  void getAttrBool(const LibertySimpleAttr *attr,
                   // Return values.
                   bool &value,
                   bool &exists);
  const EarlyLateAll *getAttrEarlyLate(const LibertySimpleAttr *attr);

  FloatSeq parseStringFloatList(const std::string &float_list,
                                float scale,
                                const LibertySimpleAttr *attr);
  FloatSeq parseStringFloatList(const std::string &float_list,
                                float scale,
                                const LibertyComplexAttr *attr);
  TableAxisPtr makeAxis(int index,
                        const LibertyGroup *group);
  FloatSeq readFloatSeq(const LibertyComplexAttr *attr,
                        float scale);
  void variableValue(const char *var,
                     float &value,
                     bool &exists);
  FuncExpr *parseFunc(const char *func,
                      const char *attr_name,
                      const LibertyCell *cell,
                      int line);
  void libWarn(int id,
               const LibertyGroup *obj,
               const char *fmt,
               ...)
    __attribute__((format (printf, 4, 5)));
  void libWarn(int id,
               const LibertySimpleAttr *obj,
               const char *fmt,
               ...)
    __attribute__((format (printf, 4, 5)));
  void libWarn(int id,
               const LibertyComplexAttr *obj,
               const char *fmt,
               ...)
    __attribute__((format (printf, 4, 5)));
  void libWarn(int id,
               int line,
               const char *fmt,
               ...)
    __attribute__((format (printf, 4, 5)));
  void libError(int id,
                const LibertyGroup *obj,
                const char *fmt, ...)
    __attribute__((format (printf, 4, 5)));
  void libError(int id,
                const LibertySimpleAttr *obj,
                const char *fmt, ...)
    __attribute__((format (printf, 4, 5)));
  void libError(int id,
                const LibertyComplexAttr *obj,
                const char *fmt, ...)
    __attribute__((format (printf, 4, 5)));

  const char *filename_;
  bool infer_latches_;
  Report *report_;
  Debug *debug_;
  Network *network_;
  LibertyBuilder builder_;
  LibertyVariableMap var_map_;
  LibertyLibrary *library_;
  bool first_cell_;
  LibraryGroupVisitorMap group_begin_map_;
  LibraryGroupVisitorMap group_end_map_;

  float time_scale_;
  float cap_scale_;
  float res_scale_;
  float volt_scale_;
  float current_scale_;
  float power_scale_;
  float energy_scale_;
  float distance_scale_;

  static constexpr char escape_ = '\\';

private:
  friend class PortNameBitIterator;
};

// Named port iterator.  Port name can be:
//   Single bit port name - iterates over port.
//   Bus port name - iterates over bus bit ports.
//   Bus range - iterates over bus bit ports.
class PortNameBitIterator : public Iterator<LibertyPort*>
{
public:
  PortNameBitIterator(LibertyCell *cell,
                      const char *port_name,
                      LibertyReader *visitor,
                      int line);
  ~PortNameBitIterator();
  virtual bool hasNext();
  virtual LibertyPort *next();
  unsigned size() const { return size_; }

protected:
  void findRangeBusNameNext();

  void init(const char *port_name);
  LibertyCell *cell_;
  LibertyReader *visitor_;
  int line_;
  LibertyPort *port_;
  LibertyPortMemberIterator *bit_iterator_;
  LibertyPort *range_bus_port_;
  std::string range_bus_name_;
  LibertyPort *range_name_next_;
  int range_from_;
  int range_to_;
  int range_bit_;
  unsigned size_;
};

class OutputWaveform
{
public:
  OutputWaveform(float axis_value1,
                 float axis_value2,
                 Table *currents,
                 float reference_time);
  float slew() const { return slew_; }
  float cap() const { return cap_; }
  Table *releaseCurrents();

  float referenceTime() { return reference_time_; }

private:
  float slew_;
  float cap_;
  std::unique_ptr<Table> currents_;
  float reference_time_;
};

} // namespace
