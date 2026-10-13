# STA API

The STA is built in C++ with heavy use of STL (Standard Template
Libraries). It also uses the zlib library to read compressed Liberty,
Verilog, SDF, SPF, and SPEF files.

The sub-directories of the STA code are:

| Directory | Description |
| --- | --- |
| `doc` | Documentation files. |
| `util` | Basic utilities. |
| `liberty` | Liberty timing library classes and file reader. |
| `network` | Network and library API used by all STA code. |
| `verilog` | Verilog netlist reader that implements the network API. |
| `graph` | Timing graph built from network and library cell timing arcs. |
| `sdc` | SDC timing constraint classes. |
| `sdf` | SDF reader, writer and annotator. |
| `dcalc` | Delay calculator API and implementations. |
| `search` | Search engine used to annotate the graph with arrival, required times and find timing check slacks. |
| `parasitics` | Parasitics API, Spef and Spf readers. |
| `app` | Interface between Tcl and STA (built with SWIG). Main program definition. |
| `tcl` | User interface code. SDC argument parsing. |

Each sub-directory builds a library that is linked to build the STA
executable or linked into another application.

The file [Coding guidelines](CodingGuidelines.md) defines naming conventions used in
the code.

Major components of the STA such as the network, timing graph, sdc,
and search are implemented as separate classes. The `Sta` class
contains an instance of each of these components.

The `Sta` class defines the bulk of the externally visible API used by
the Tcl interface, and coordinates operations that involve multiple
components. For example, when a false path command is entered into
the Tcl command interpreter, the `Sta` passes the declaration on to the
`Sdc` component and tells the `Search` component to invalidate all arrival
and required times.

Applications should call functions defined by the `Sta` class rather
than functions defined by the components. Calling functions defined
by the components will get you in trouble unless you understand them
in detail. For example, telling the delay calculator to recompute the
delays leaves the arrival times that depend on them wrong. Always
remember that the `Sta` coordinates the components.

In general, objects passed as arguments to `Sta` functions that are
constructors become "owned" by the STA and should not be deleted by
the caller. For example, a set of pins passed into
`Sta::makeExceptionFrom` are used in the resulting object (rather than
copied into another set). On the other hand, strings passed as
arguments are copied by the `Sta` functions before they are retained in
STA data structures.

In many cases the major components contain pointers to other
components. The `StaState` class is a simple container for these
components that makes initialization of pointers to the components
easier.

An STA with modified behavior can be built by defining classes derived
from the component classes and overloading some of the member
functions (which may have to be modified to be virtual). Components
are created by `Sta::makeComponents()`. The `Sta::makeComponents()`
function in turn calls each of the `Sta::make<Component>` component
constructors. These constructors can be overloaded by redefining them
in a class derived from `Sta`. Because the components refer to each
other, `Sta::updateComponentsState()` must be called to notify the
components if any of them are changed after creation.

## Units

Units for values in `Sta` and liberty data structures are always the
following:

| Quantity | Unit |
| --- | --- |
| time | seconds |
| length | meters |
| capacitance | farads |
| resistance | ohms |
| voltage | volts |
| current | amperes |
| power | watts |

All file readers and the user interface are responsible for converting
any user input or output to these units.

## Utilities

The STA uses the C++ standard library containers (`std::vector`,
`std::map`, `std::set`, and related types). Helper functions for
lookups and iteration live in `ContainerHelpers.hh` (`sta::findKey`,
`sta::findKeyValue`, and range helpers). Prefer range-`for` over
hand-written iterators.

All printing done by the STA core is done using the `Report` class API.
The report class supports output redirection to a file and logging to
a file. The Tcl interpreter prints to "channels" that are
encapsulated by functions in the `ReportTcl` class. Printing inside
the STA is directed to the Tcl channels so that it appears with the
Tcl interpreter output.

## Network

The network API is the key to making the STA a timing engine that can
be bolted onto another application. This API allows the STA to
efficiently communicate with external network data structures without
the overhead of making and maintaining a copy of it.

The network API encapsulates both library and netlist accessors.
Libraries are composed of cells that have ports that define connections
to the cell. Netlists are built out of cell instances, pins and nets.

The `ConcreteLibrary` and `ConcreteNetwork` classes are used by the STA
netlist readers (notably Verilog). These class definitions are to
support a stand alone STA that does not depend on external netlist
data structures.

External network data structures are interfaced to the STA by casting
pointers to network objects across the interface. The external objects
do not have to be derived from STA network base classes. The network
API functions are typically very thin functions that cast the STA
network types to the external class types and call the corresponding
external network database accessor.

Bus ports are expanded into ports for each bit in the bus, and
iterators are provided for the expanded and unexpanded set of cell
ports.

Network instances are calls of cells in the design hierarchy. Both
hierarchical and leaf instances are in the network. Hierarchical
instances have children instances at the next lower hierarchy level.
Leaf instances have liberty cells with timing model data. At the top
of the hierarchy is a top level instance that has instances for the
top level netlist. If a cell has multiple instances the entire
sub-tree of hierarchy is repeated in the network. This "unfolded"
network representation allows optimization to specialize instances of
a hierarchical block. A "folded" network representation that has only
one sub-tree for each hierarchical block means that all copies must
have identical sub-trees, preventing optimizations that specialize the
contents.

Pins are a connection between an instance and a net corresponding to a
port. For bus ports each bit in the bus has a corresponding pin
(library iterators can be used to find the pins that correspond to all
of the bits in a bus). Ports on the top level instance also have pins
in the network that are the top level inputs and outputs.

Nets connect together a group of pins. Both hierarchical and leaf
pins are on a net. Nets can connect pins on multiple levels of
hierarchy.

The network objects inside the STA are always pointers to instances of
undefined class objects. The implementation and definition of the
network objects themselves is never visible inside the STA. The
network API is implemented as an adapter that performs all operations
on all network objects. There is one network adapter instance used by
all STA code. For example, to find the cell of an instance:

```cpp
Cell *cell = network->cell(instance);
```

The network adapter returns iterators for looping over groups of
network objects. For example, the following code iterates over the
children of the top level instance.

```cpp
Instance *top_instance = network->topInstance();
InstanceChildIterator *child_iter = network->childIterator(top_instance);
while (child_iter->hasNext()) {
  Instance *child = child_iter->next();
  ...
}
delete child_iter;
```

An adapter to a network database is built by defining a class derived
from the base class `Network`, or `NetworkEdit` if it supports incremental
editing operations. `network/ConcreteNetwork.cc` is an example of
a network adapter that supports hierarchy. An example of a network adapter
for a flat DEF based netlist is in OpenROAD:
[dbNetwork.hh](https://github.com/The-OpenROAD-Project/OpenROAD/blob/master/src/dbSta/include/db_sta/dbNetwork.hh),
[dbNetwork.cc](https://github.com/The-OpenROAD-Project/OpenROAD/blob/master/src/dbSta/src/dbNetwork.cc).

A network adaptor to interface to an external network database must
define the virtual functions of the `Network` class (about 45
functions). The external network objects do not have to use any STA
network objects as base classes or even be C++ objects. These network
adapter functions should cast the network object pointers to the
underlying network object.

Network adapters built on the `Network` class must define the following
functions to find corresponding liberty objects.

```cpp
virtual LibertyLibrary *libertyLibrary(Library *library) const;
virtual LibertyLibrary *makeLibertyLibrary(const char *name,
                                           LibraryAnalysisPt *ap);
virtual LibertyCell *libertyCell(Cell *cell) const;
virtual LibertyPort *libertyPort(Port *port) const;
```

The `NetworkLiberty` class provides implementations of the first two
functions for derived network classes.

If the network adapter implements the `NetworkEdit` API the following
Tcl commands are supported:

```tcl
make_cell
replace_cell
delete_cell
make_net
delete_net
connect_pins
disconnect_pins
```

Each of these commands call corresponding functions in `network/NetworkEdit.i`
that notify the `Sta` before and/or after the network operation is
performed.

## Liberty

The liberty timing library reader builds classes that are derived from
the concrete library classes. In addition to the library, cell and
port classes, there are classes to represent timing arcs, timing
models, wireload models, operating conditions, and scale factors for
derating timing data.

Timing arcs are grouped into sets of arcs between a pair of cell
ports. For example, a buffer has two timing arcs between the input
and output; one for a rising output and another for a falling output.
The timing arcs are:

```
A r -> Z r
A f -> Z f
```

Since a buffer is non-inverting, the timing arc set is positive-unate.
Similarly, an inverter has two negative-unate timing arcs.

```
A f -> Z r
A r -> Z f
```

On the other hand, a multiplexor, has a non-unate path from the select
input to the output because a rise or fall change on the input can
cause the output to either rise or fall. There are four timing arcs
in this arc set:

```
S f -> Z r
S f -> Z f
S r -> Z r
S r -> Z f
```

The liberty file reader can be customized to read attributes that are
not used by the STA.

## Graph

The timing graph is the central data structure used by the delay
calculation and search algorithms. It is annotated with timing arc
delay values and slews (from SDF or a delay calculator). A forward
search annotates the graph with arrival times, and a backward search
annotates required times.

The graph is composed of vertices and edges. Each pin in the design
has a vertex. Bidirect pins have two vertices, one for its use as an
input and another for its use as an output.

The `Network` adapter supplies functions to find and set the index
(unsigned) of a graph vertex corresponding to a pin.

```cpp
Network::vertexIndex(const Pin *pin) const;
Network::setVertexIndex(Pin *pin, VertexIndex index);
```

An STL map can be used for the lookup, but it is rather memory hungry
compared to storing the value in the pin structure.

A pointer to the vertex used for a bidirectional pin driver is kept in
a map owned by the `Graph` class.

Edges in the graph connect vertices. The pins connected together by a
net have wire edges between the pin vertices. Timing arc sets in the
leaf instance timing models have corresponding edges in the graph
between pins on the instance.

## SDC

There is no support for updating SDC when network edits delete
the instance, pin, or net objects referred to by the SDC.

## Delay Calculation

The graph is annotated with arc delay values and slews (also known as
transition times) by the graph delay calculator or the SDF reader.
The `GraphDelayCalc` class seeds slews from SDC constraints and uses a
breadth first search to visit each gate output pin. The `GraphDelayCalc`
then calls a timing arc delay calculator for each timing arc and
annotates the graph arc delays and vertex slews.

The delay calculator is architected to support multiple delay
calculation results. Each result has an associated delay calculation
analysis point (class `DcalcAnalysisPt`) that specifies the operating
conditions and parasitics used to find the delays.

The `ArcDelayCalc` class defines the API used by the `GraphDelayCalc` to
calculate the gate delay, driver slew, load delays and load slews
driven by a timing arc. The following delay calculation algorithms
are defined in the `dcalc` directory.

### `UnitDelayCalc`

All gate delays are 1. Wire delays are zero.

### `LumpedCapArcDelayCalc`

Liberty table models using lumped capacitive load (RSPF pi model
total capacitance). Wire delays are zero.

### `DmpCeffElmoreDelayCalc`

RSPF (Driver Pi model with elmore interconnect delays) delay
calculator. Liberty table models using effective capacitive model as
described in "Performance Computation for Precharacterized CMOS Gates
with RC Loads", Florentin Dartu, Noel Menezes and Lawrence Pileggi,
IEEE Transactions on Computer-Aided Design of Integrated Circuits and
Systems, Vol 15, No 5, May 1996. Wire delays are computed by applying
the driver waveform to the RSPF dependent source and solving the RC
network.

### `DmpCeffTwoPoleDelayCalc`

Driver Pi model with two pole interconnect delays and effective
capacitance as in `DmpCeffElmoreDelayCalc`.

Other delay calculators can be interfaced by defining a class based on
`ArcDelayCalc` and using the `registerDelayCalc` function to register it
for the `set_delay_calculator` Tcl command. The `Sta::setArcDelayCalc`
function can be used to set the delay calculator at run time.

## Search

A breadth first forward search is used to find arrival times at graph
vertices. Vertices are annotated with instances of the `Event` class to
record signal arrival and required times. As each vertex is visited
in the forward search its required time is found using If the vertex
is constrained by setup or hold timing checks, min/max path delay
exceptions or gated timing checks its required time is found from the
SDC. The slack is the difference between the vertex required time and
arrival time. If the vertex is constrained it is scheduled for a
breadth first backward search to propagate required times to the fanin
vertices. Separate events (and hence arrival and required times) are
used for each clock edge and exception set that cause a vertex to
change.

Arrival, required and slack calculations are incremental using a level
based "lazy evaluation" algorithm. The first time arrival/required
times are found for a vertex the arrival/required times are propagated
to/from the vertex's logic level. After that no search is required
for any vertex with a lower/higher logic level when the
arrival/required time is requested.

Clock arrival times are found before data arrival times by
`Search::findClkArrivals()`. Clock arrival times include insertion delay
(source latency).

When an incremental netlist change is made (for instance, changing the
drive strength of a gate with `swap_cell`), the STA incrementally updates
delay calculation, arrival times, required times and slacks. Because
gate delay is only weakly dependent on slew, the effect of the change
will diminish in gates downstream of the change. The STA uses a
tolerance on the gate delays to determine when to stop propagating the
change. The tolerance is set using the
`Sta::setIncrementalDelayTolerance` function.

```cpp
void Sta::setIncrementalDelayTolerance(float tol);
```

The tolerance is a percentage (0.0:1.0) change in delay that causes
downstream delays to be recomputed during incremental delay
calculation. The default value is 0.0 for maximum accuracy and
slowest incremental speed. The delay calculation will not recompute
delays for downstream gates when the change in the gate delay is less
than the tolerance. Required times must be recomputed backward from
any gate delay changes, so increasing the tolerance can significantly
reduce incremental timing run time.

## Tcl Interface

The interface from Tcl to C++ is written in a SWIG
([www.swig.org](https://www.swig.org)) interface description
(`app/StaApp.i`). SWIG generates the interface code from the
description file.

All commands are written in Tcl. SDC argument parsing and checking is
done with Tcl procedures that call a SWIG interface function.

The Tcl `sta` namespace is used to segregate internal STA functions
from the global Tcl namespace. All user visible STA and SDC commands
are exported to the global Tcl namespace.

A lot of the internal STA state can be accessed from Tcl to make
debugging easier. Some debugging commands are not intended for casual
users and live in the `sta` namespace. Others, such as `report_arrival`,
`report_required`, `report_slack`, and `report_edges`, are also exported
to the global namespace. Examples:

```tcl
report_arrival
report_required
report_slack
report_edges
report_slews
sta::report_level pin
sta::report_constant pin|instance
sta::report_network

sta::network_pin_count
sta::network_net_count
sta::network_leaf_instance_count
sta::network_leaf_pin_count
```

Additionally, many of the STA network and graph objects themselves are
exposed to Tcl using SWIG. These Tcl objects have methods for
inspecting them. Examples of how to use these methods can be found in
the `tcl/Graph.tcl` and `tcl/Network.tcl` files.

## Architecture alternatives for using the STA Engine

There are a number of alternatives for using the STA engine with
an application.

### STA with Tcl application

The simplest example is an application written in Tcl. The application
calls STA commands and primitives defined in the SWIG C++/Tcl
interface. A stand-alone STA executable is built and a Tcl file that
defines the application is included as part of the STA by modifying
`CMakeLists.txt` to add the Tcl file to `STA_TCL_FILES` (encoded into
`StaTclInitVar.cc`).

The user calls STA commands to read design files (liberty, verilog,
SDF, parasitics) to define and link the design. The user defines SDC
commands or sources an SDC file. The user calls the application's Tcl
commands.

A simple gate sizer is an example of an application that can be built
this way because it has very little computation in the sizer itself.
STA Tcl commands can be used to find the worst path and upsize gates
or insert buffers.

### STA with C++ application

The application is built by adding C++ files to the `app` directory and
modifying `CMakeLists.txt` to include them in the executable. Interface
commands between C++ and Tcl are put in a SWIG `.i` file in the `app`
directory and modifying `app/StaApp.i` to include them. Tcl commands are
added to the STA by modifying `CMakeLists.txt` to add the application's
Tcl files to `STA_TCL_FILES`.

The user calls STA commands to read design files (liberty, verilog,
SDF, parasitics) to define and link the design. The user defines SDC
commands or sources an SDC file. The user calls the application's Tcl
commands.

### C++ application without native Network data structures linking STA libraries

The application builds `main()` and links STA libraries. On startup it
calls STA initialization functions like `staMain()` defined in
`app/StaMain.cc`.

The application must link and instantiate a Tcl interpreter to read
SDC commands like `staMain()`. The application can choose to expose the Tcl
interpreter to the user or not. The STA depends on the following data
that can be read by calling Tcl commands or `Sta` class member functions.

Liberty files that define the leaf cells used in the design.
Read using the `read_liberty` command or by calling `Sta::readLibertyFile()`.

Verilog files that define the netlist. Read using the `read_verilog`
command or by calling `readVerilogFile()` (see `verilog/Verilog.i`
`read_verilog`).

Link the design using the `link_design` command or calling `Sta::linkDesign()`.

SDC commands to define timing constraints.
Defined using SDC commands in the Tcl interpreter, or sourced
from a file using `Tcl_Eval(sta::tclInterp())`.

Parasitics used by delay calculation.
Read using the `read_spef` command, `Sta::readSpef()`, or
using the `Sta::Parasitics` class API.

The application calls network editing functions such as
`Sta::deleteInstance()` to edit the network.

### C++ application with native Network data structures linking STA libraries

The application defines a Network adapter (described above) so that
the STA can use the native network data structures without duplicating
them in the STA. The application defines a class built on class `Sta`
that defines the `makeNetwork()` member function to build an instance of
the network adapter.

The application builds `main()` and links STA libraries. On startup it
calls STA initialization functions like `staMain()` defined in
`app/StaMain.cc`. The application reads the netlist and builds network
data structures that the STA accesses through the Network adapter.

The application must link and instantiate a Tcl interpreter to read
SDC commands like `staMain()`. The application can choose to expose the Tcl
interpreter to the user or not. The STA depends on the following data
that can be read by calling Tcl commands or `Sta` class member functions.

Liberty files that define the leaf cells used in the design.
Read using the `read_liberty` command or by calling `Sta::readLibertyFile`.

SDC commands to define timing constraints.
Defined using SDC commands in the Tcl interpreter, or sourced
from a file using `sta::sourceTclFile`.

Parasitics used by delay calculation.
Read using the `read_spef` command, `Sta::readSpef()`, or
using the `Sta::Parasitics` class API.

The application calls network editing before/after functions such as
`Sta::deleteInstanceBefore()` to notify the `Sta` of network edits.

A placement tool is likely to use this pattern to integrate the STA
because the DEF file includes netlist connectivity.
