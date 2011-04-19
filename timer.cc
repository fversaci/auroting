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

#include "timer.h"
#include <iostream>
using namespace std;

Define_Module(Timer);

void Timer::finish() {
  recordScalar("#endTime", simTime());
  recordScalar("#totalHops", hops);
  recordScalar("#totalPacks", rcvdPacks);

  cDatarateChannel* chan00 = (cDatarateChannel*) getParentModule()->getSubmodule("node",0)->gate("gate$o",0)->getChannel();
  double B = chan00->getDatarate();
  double lat = chan00->getDelay().dbl();
  double L = 8.0 * getParentModule()->getSubmodule("node",0)->getSubmodule("generator")->par("packLen").doubleValue();
  double count=getParentModule()->getSubmodule("node",0)->getSubmodule("generator")->par("count").doubleValue();
  double T=lat + L/B; // latency of both packet and ACK
  double x = getParentModule()->par("kX").doubleValue();
  double y = getParentModule()->par("kY").doubleValue();
  double z = getParentModule()->par("kZ").doubleValue();
  double max=x>y?z:y;
  max=max>z?max:z;
  double lb1 = count*T*max/8.0; // bisection bandwidth lower bound
  double lb2 = T*mh; // time for longest path
  recordScalar("#LB1", lb1);
  recordScalar("#LB2", lb2);
  // cout << T << endl;
  // cout << lb2 << " < " << simTime() << endl;
}

void Timer::handleMessage(cMessage *msg) {
  if (dynamic_cast<NoM*>(msg) != NULL){
    NoM* m=(NoM*) msg;
    addRP(m->getCow());
    int h=m->getHops()-1;
    addHops(h);
    if (h>mh)
    	mh=h;
    delete msg;
  }
}
