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

// This file illustratates how to customize the liberty file reader to
// read attributes that are not used by the STA.  In this example:
//  * code is called at the beginning of a library definition
//  * a string attribute named "thingy" is parsed

#include <stdio.h>
#include "Machine.hh"
#include "StringUtil.hh"
#include "LibertyReader.hh"
#include "LibertyReaderPvt.hh"
#include "LibertyBuilder.hh"
#include "Network.hh"
#include "ConcreteNetwork.hh"
#include "Sta.hh"

// Import symbols from sta package (this example is in the global package).
using sta::Report;
using sta::Debug;
using sta::Network;
using sta::LibertyReader;
using sta::LibertyAttr;
using sta::LibertyGroup;
using sta::TimingGroup;
using sta::LibertyCell;
using sta::LibertyPort;
using sta::LibertyLibrary;
using sta::TimingArcSet;
using sta::LibertyBuilder;
using sta::TimingRole;
using sta::TimingArcAttrs;
using sta::Sta;
using sta::stringCopy;

// LibertyCell with Bigco thingy variable.
class BigcoCell : public LibertyCell
{
public:
  BigcoCell(LibertyLibrary *library, const char *name, const char *filename);
  void setThingy(const char *thingy);

protected:
  const char *thingy_;
};

BigcoCell::BigcoCell(LibertyLibrary *library, const char *name, 
		     const char *filename) :
  LibertyCell(library, name, filename),
  thingy_(0)
{
}

void 
BigcoCell::setThingy(const char *thingy)
{
  thingy_ = thingy;
}

////////////////////////////////////////////////////////////////

class BigcoTimingGroup : public TimingGroup
{
public:
  BigcoTimingGroup(int line);
  const char *frob() const { return frob_; }
  void setFrob(const char *frob);

protected:
  const char *frob_;
};

BigcoTimingGroup::BigcoTimingGroup(int line) :
  TimingGroup(line),
  frob_(0)
{
}

void 
BigcoTimingGroup::setFrob(const char *frob)
{
  frob_ = frob;
}

////////////////////////////////////////////////////////////////

class BigcoTimingArcSet : public TimingArcSet
{
public:
  BigcoTimingArcSet(LibertyCell *cell, LibertyPort *from, LibertyPort *to, 
		    LibertyPort *related_out, TimingRole *role, 
		    TimingArcAttrs *attrs);

protected:
  const char *frob_;
};

BigcoTimingArcSet::BigcoTimingArcSet(LibertyCell *cell, LibertyPort *from, 
				     LibertyPort *to, 
				     LibertyPort *related_out, TimingRole *role, 
				     TimingArcAttrs *attrs) :
  TimingArcSet(cell, from, to, related_out, role, attrs)
{
  const char *frob = static_cast<BigcoTimingGroup*>(attrs)->frob();
  if (frob)
    frob_ = stringCopy(frob);
}

////////////////////////////////////////////////////////////////

// Make Bigco objects instead of Liberty objects.
class BigcoLibertyBuilder : public LibertyBuilder
{
public:
  virtual LibertyCell *makeCell(LibertyLibrary *library, const char *name,
				const char *filename);

protected:
  virtual TimingArcSet *makeTimingArcSet(LibertyCell *cell, LibertyPort *from, 
					 LibertyPort *to, 
					 LibertyPort *related_out,
					 TimingRole *role,
					 TimingArcAttrs *attrs);
};

LibertyCell *
BigcoLibertyBuilder::makeCell(LibertyLibrary *library, const char *name,
			      const char *filename)
{
  LibertyCell *cell = new BigcoCell(library, name, filename);
  library->addCell(cell);
  return cell;
}

TimingArcSet *
BigcoLibertyBuilder::makeTimingArcSet(LibertyCell *cell, LibertyPort *from, 
				      LibertyPort *to, 
				      LibertyPort *related_out,
				      TimingRole *role,
				      TimingArcAttrs *attrs)
{
  return new BigcoTimingArcSet(cell, from, to, related_out, role, attrs);
}

////////////////////////////////////////////////////////////////

// Liberty reader to parse Bigco attributes.
class BigcoLibertyReader : public LibertyReader
{
public:
  BigcoLibertyReader(LibertyBuilder *builder);

protected:
  virtual void visitAttr1(LibertyAttr *attr);
  virtual void visitAttr2(LibertyAttr *attr);
  virtual void beginLibrary(LibertyGroup *group);
  virtual TimingGroup *makeTimingGroup(int line);
  virtual void beginCell(LibertyGroup *group);
};

BigcoLibertyReader::BigcoLibertyReader(LibertyBuilder *builder) :
  LibertyReader(builder)
{
  // Define a visitor for the "thingy" attribute.
  // Note that the function descriptor passed to defineAttrVisitor 
  // must be defined by the LibertyVisitor class, so a number of 
  // extra visitor functions are pre-defined for extensions.
  defineAttrVisitor("thingy", &LibertyReader::visitAttr1);
  defineAttrVisitor("frob", &LibertyReader::visitAttr2);
}

bool
libertyCellRequired(const char *)
{
  // Predicate for cell names.
  return true;
}

// Prune cells from liberty file based on libertyCellRequired predicate.
void 
BigcoLibertyReader::beginCell(LibertyGroup *group)
{
  const char *name = group->firstName();
  if (name
      && libertyCellRequired(name))
    LibertyReader::beginCell(group);
}

TimingGroup *
BigcoLibertyReader::makeTimingGroup(int line)
{
  return new BigcoTimingGroup(line);
}

// Called at the beginning of a library group.
void 
BigcoLibertyReader::beginLibrary(LibertyGroup *group)
{
  LibertyReader::beginLibrary(group);
  // Do Bigco stuff here.
  printf("Bigco was here.\n");
}

void 
BigcoLibertyReader::visitAttr1(LibertyAttr *attr)
{
  const char *thingy = getAttrString(attr);
  if (thingy) {
    printf("Bigco thingy attribute value is %s.\n", thingy);
    if (cell_)
      static_cast<BigcoCell*>(cell_)->setThingy(thingy);
  }
}

void 
BigcoLibertyReader::visitAttr2(LibertyAttr *attr)
{
  const char *frob = getAttrString(attr);
  if (frob) {
    if (timing_)
      static_cast<BigcoTimingGroup*>(timing_)->setFrob(frob);
  }
}

////////////////////////////////////////////////////////////////

// Define a BigcoSta class derived from the Sta class to install the
// new liberty reader/visitor in BigcoSta::makeLibertyReader.
class BigcoSta : public Sta
{
public:
  BigcoSta();

protected:
  virtual LibertyLibrary *readLibertyFile(const char *filename, 
					  bool infer_latches,
					  Network *network);
};

BigcoSta::BigcoSta() :
  Sta()
{
}

// Replace Sta liberty file reader with Bigco's very own.
LibertyLibrary *
Sta::readLibertyFile(const char *filename,
		bool infer_latches,
		     Network *network)
{
  BigcoLibertyBuilder builder;
  BigcoLibertyReader reader(&builder);
  return reader.readLibertyFile(filename, infer_latches, network);
}
