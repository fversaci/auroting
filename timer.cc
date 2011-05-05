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

void Timer::initialize(){
	cDatarateChannel* chan00 = (cDatarateChannel*) getParentModule()->getSubmodule("node",0)->gate("gate$o",0)->getChannel();
	B = chan00->getDatarate(); // b/s
	lat = chan00->getDelay().dbl();
	L = 8.0 * getParentModule()->getSubmodule("node",0)->getSubmodule("generator")->par("packLen").doubleValue();
	count = getParentModule()->getSubmodule("node",0)->getSubmodule("generator")->par("count").doubleValue();
	T = L/B; // no latency
	x = getParentModule()->par("kX").doubleValue();
	y = getParentModule()->par("kY").doubleValue();
	z = getParentModule()->par("kZ").doubleValue();
	max=x>y?x:y;
	max=max>z?max:z;
	lb = count*T*max/8.0; // bisection bandwidth time lower bound
	lifetimes.setName("Lifetimes");
	lifetimes_hist.setNumCells(100);
	double hr=par("histrange");
	lifetimes_hist.setRange(0,hr*lb);
}

void Timer::finish() {
  recordScalar("#endTime", simTime());
  recordScalar("#totalHops", hops);
  recordScalar("#totalPacks", rcvdPacks);

  recordScalar("#LB", lb);
  recordScalar("#ratio", lb/simTime().dbl());
  lifetimes_hist.recordAs("Life Times");
}

void Timer::handleMessage(cMessage *msg) {
  if (dynamic_cast<NoM*>(msg) != NULL){
    NoM* m=(NoM*) msg;
    addRP(m->getCow());
    int h=m->getHops()-1;
    addHops(h);
    lifetimes.record(simTime()-m->getBirthtime());
    lifetimes_hist.collect(simTime()-m->getBirthtime());
    delete msg;
  }
}
