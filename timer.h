//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef __AUROUTING_TIMER_H_
#define __AUROUTING_TIMER_H_

#include <omnetpp.h>
#include "pack_m.h"

/// Simple object to record simulation end time
class Timer: public cSimpleModule {
 private:
  /// Number of total received packets
  int rcvdPacks;
  /// Number of total hops
  int hops;
  /// maximum number of hops
  int mh;
  /// Add a number to received packets
  void addRP(int n) { rcvdPacks+=n; }
  /// Add a number to hops
  void addHops(int n) { hops+=n; }
 public:
  /// Constructor
  Timer() : rcvdPacks(0), mh(0) {}
 protected:
  virtual void finish();
  virtual void handleMessage(cMessage *msg);
};

#endif
