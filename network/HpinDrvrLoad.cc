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

#include <stdio.h>
#include "Machine.hh"
#include "Network.hh"
#include "HpinDrvrLoad.hh"

namespace sta {

static void
visitPinsAboveNet2(const Pin *hpin,
		   Net *above_net,
		   NetSet &visited,
		   HpinDrvrLoads &above_drvrs,
		   HpinDrvrLoads &above_loads,
		   PinSet *hpin_path,
		   const Network *network);
static void
visitPinsBelowNet2(const Pin *hpin,
		   Net *above_net,
		   Net *below_net,
		   NetSet &visited,
		   HpinDrvrLoads &below_drvrs,
		   HpinDrvrLoads &below_loads,
		   PinSet *hpin_path,
		   const Network *network);
static void
visitHpinDrvrLoads(HpinDrvrLoads drvrs,
		   HpinDrvrLoads loads,
		   HpinDrvrLoadVisitor *visitor);

void
visitHpinDrvrLoads(const Pin *pin,
		   const Network *network,
		   HpinDrvrLoadVisitor *visitor)
{
  NetSet visited;
  HpinDrvrLoads above_drvrs;
  HpinDrvrLoads above_loads;
  PinSet hpin_path;
  Net *above_net = network->net(pin);
  if (above_net) {
    visitPinsAboveNet2(pin, above_net, visited, 
		       above_drvrs, above_loads,
		       &hpin_path, network);
  }

  // Search down from hpin terminal.
  HpinDrvrLoads below_drvrs;
  HpinDrvrLoads below_loads;
  Term *term = network->term(pin);
  if (term) {
    Net *below_net = network->net(term);
    if (below_net)
      visitPinsBelowNet2(pin, above_net, below_net, visited,
			 below_drvrs, below_loads,
			 &hpin_path, network);
  }
  if (network->isHierarchical(pin)) {
    visitHpinDrvrLoads(above_drvrs, below_loads, visitor);
    visitHpinDrvrLoads(below_drvrs, above_loads, visitor);
  }
  else {
    if (network->isDriver(pin)) {
      HpinDrvrLoad drvr(const_cast<Pin*>(pin), nullptr, &hpin_path, nullptr);
      HpinDrvrLoads drvrs;
      drvrs.insert(&drvr);
      visitHpinDrvrLoads(drvrs, below_loads, visitor);
      visitHpinDrvrLoads(drvrs, above_loads, visitor);
    }
    // Bidirects are drivers and loads.
    if (network->isLoad(pin)) {
      HpinDrvrLoad load(nullptr, const_cast<Pin*>(pin), nullptr, &hpin_path);
      HpinDrvrLoads loads;
      loads.insert(&load);
      visitHpinDrvrLoads(below_drvrs, loads, visitor);
      visitHpinDrvrLoads(above_drvrs, loads, visitor);
    }
  }
  above_drvrs.deleteContents();
  above_loads.deleteContents();
  below_drvrs.deleteContents();
  below_loads.deleteContents();
}

static void
visitPinsAboveNet2(const Pin *hpin,
		   Net *above_net,
		   NetSet &visited,
		   HpinDrvrLoads &above_drvrs,
		   HpinDrvrLoads &above_loads,
		   PinSet *hpin_path,
		   const Network *network)
{
  visited.insert(above_net);
  // Visit above net pins.
  NetPinIterator *pin_iter = network->pinIterator(above_net);
  while (pin_iter->hasNext()) {
    Pin *above_pin = pin_iter->next();
    if (above_pin != hpin) {
      if (network->isDriver(above_pin)) {
	HpinDrvrLoad *drvr = new HpinDrvrLoad(above_pin, nullptr,
					      hpin_path, nullptr);
	above_drvrs.insert(drvr);
      }
      if (network->isLoad(above_pin)) {
	HpinDrvrLoad *load = new HpinDrvrLoad(nullptr, above_pin,
					      nullptr, hpin_path);
	above_loads.insert(load);
      }
      Term *above_term = network->term(above_pin);
      if (above_term) {
	Net *above_net1 = network->net(above_term);
	if (above_net1 && !visited.hasKey(above_net1)) {
	  hpin_path->insert(above_pin);
	  visitPinsAboveNet2(above_pin, above_net1, visited,
			     above_drvrs, above_loads,
			     hpin_path, network);
	  hpin_path->erase(above_pin);
	}
      }
    }
  }
  delete pin_iter;

  // Search up from net terminals.
  NetTermIterator *term_iter = network->termIterator(above_net);
  while (term_iter->hasNext()) {
    Term *term = term_iter->next();
    Pin *above_pin = network->pin(term);
    if (above_pin 
	&& above_pin != hpin) {
      Net *above_net1 = network->net(above_pin);
      if (above_net1 && !visited.hasKey(above_net1)) {
	hpin_path->insert(above_pin);
	visitPinsAboveNet2(above_pin, above_net1, visited,
			   above_drvrs, above_loads,
			   hpin_path, network);
	hpin_path->erase(above_pin);
      }

      if (network->isDriver(above_pin)) {
	HpinDrvrLoad *drvr = new HpinDrvrLoad(above_pin, nullptr,
					      hpin_path, nullptr);
	above_drvrs.insert(drvr);
      }
      if (network->isLoad(above_pin)) {
	HpinDrvrLoad *load = new HpinDrvrLoad(nullptr, above_pin,
					      nullptr, hpin_path);
	above_loads.insert(load);
      }
    }
  }
  delete term_iter;
}

static void
visitPinsBelowNet2(const Pin *hpin,
		   Net *above_net,
		   Net *below_net,
		   NetSet &visited,
		   HpinDrvrLoads &below_drvrs,
		   HpinDrvrLoads &below_loads,
		   PinSet *hpin_path,
		   const Network *network)
{
  visited.insert(below_net);
  // Visit below net pins.
  NetPinIterator *pin_iter = network->pinIterator(below_net);
  while (pin_iter->hasNext()) {
    Pin *below_pin = pin_iter->next();
    if (below_pin != hpin) {
      if (above_net && !visited.hasKey(above_net))
	visitPinsAboveNet2(below_pin, above_net,
			   visited, below_drvrs, below_loads,
			   hpin_path, network);
      if (network->isDriver(below_pin)) {
	HpinDrvrLoad *drvr = new HpinDrvrLoad(below_pin, nullptr,
					      hpin_path, nullptr);
	below_drvrs.insert(drvr);
      }
      if (network->isLoad(below_pin)) {
	HpinDrvrLoad *load = new HpinDrvrLoad(nullptr, below_pin,
					      nullptr, hpin_path);
	below_loads.insert(load);
      }
      if (network->isHierarchical(below_pin)) {
	Term *term = network->term(below_pin);
	if (term) {
	  Net *below_net1 = network->net(term);
	  if (below_net1 && !visited.hasKey(below_net1)) {
	    hpin_path->insert(below_pin);
	    visitPinsBelowNet2(below_pin, below_net, below_net1, visited,
			       below_drvrs, below_loads,
			       hpin_path, network);
	    hpin_path->erase(below_pin);
	  }
	}
      }
    }
  }
  delete pin_iter;

  // Search up from net terminals.
  NetTermIterator *term_iter = network->termIterator(below_net);
  while (term_iter->hasNext()) {
    Term *term = term_iter->next();
    Pin *above_pin = network->pin(term);
    if (above_pin
	&& above_pin != hpin) {
      Net *above_net1 = network->net(above_pin);
      if (above_net1 && !visited.hasKey(above_net1)) {
	hpin_path->insert(above_pin);
	visitPinsAboveNet2(above_pin, above_net1, 
			   visited, below_drvrs, below_loads,
			   hpin_path, network);
	hpin_path->erase(above_pin);
      }
    }
  }
  delete term_iter;
}

static void
visitHpinDrvrLoads(HpinDrvrLoads drvrs,
		   HpinDrvrLoads loads,
		   HpinDrvrLoadVisitor *visitor)
{
  HpinDrvrLoads::Iterator drvr_iter(drvrs);
  while (drvr_iter.hasNext()) {
    HpinDrvrLoad *drvr = drvr_iter.next();
    HpinDrvrLoads::Iterator load_iter(loads);
    while (load_iter.hasNext()) {
      HpinDrvrLoad *load = load_iter.next();
      HpinDrvrLoad clone(drvr->drvr(),
			 load->load(),
			 drvr->hpinsFromDrvr(),
			 load->hpinsToLoad());
      visitor->visit(&clone);
    }
  }
}

////////////////////////////////////////////////////////////////

HpinDrvrLoad::HpinDrvrLoad(Pin *drvr,
			   Pin *load,
			   PinSet *hpins_from_drvr,
			   PinSet *hpins_to_load) :
  drvr_(drvr),
  load_(load),
  hpins_from_drvr_(hpins_from_drvr ? new PinSet(*hpins_from_drvr) : nullptr),
  hpins_to_load_(hpins_to_load ? new PinSet(*hpins_to_load) : nullptr)
{
}

HpinDrvrLoad::HpinDrvrLoad(Pin *drvr,
			   Pin *load) :
  drvr_(drvr),
  load_(load)
{
}

HpinDrvrLoad::~HpinDrvrLoad()
{
  delete hpins_from_drvr_;
  delete hpins_to_load_;
}

void 
HpinDrvrLoad::report(const Network *network)
{
  printf("%s -> %s: ", 
	 drvr_ ? network->pathName(drvr_) : "-",
	 load_ ? network->pathName(load_) : "-");
  PinSet::Iterator pin_iter(hpins_from_drvr_);
  while (pin_iter.hasNext()) {
    Pin *pin = pin_iter.next();
    printf("%s ", network->pathName(pin)); 
  }
  printf("* "); 
  PinSet::Iterator pin_iter2(hpins_to_load_);
  while (pin_iter2.hasNext()) {
    Pin *pin = pin_iter2.next();
    printf("%s ", network->pathName(pin)); 
 }
  printf("\n");
}

void 
HpinDrvrLoad::setDrvr(Pin *drvr)
{
  drvr_ = drvr;
}

bool
HpinDrvrLoadLess::operator()(const HpinDrvrLoad *drvr_load1,
			     const HpinDrvrLoad *drvr_load2) const
{
  Pin *load1 = drvr_load1->load();
  Pin *load2 = drvr_load2->load();
  if (load1 == load2) {
    Pin *drvr1 = drvr_load1->drvr();
    Pin *drvr2 = drvr_load2->drvr();
    return drvr1 < drvr2;
  }
  else
    return load1 < load2;
}

} // namespace
