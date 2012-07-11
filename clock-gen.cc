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

#include "clock-gen.h"
#include <iostream>

Define_Module(Clock_gen);

void Clock_gen::initialize()
{
    cDatarateChannel* chan00 = (cDatarateChannel*) getParentModule()->getSubmodule("node",0)->gate("gate$o",0)->getChannel();
    double B = chan00->getDatarate(); // b/s
    double L = 8.0 * getParentModule()->par("packLen").doubleValue() * getParentModule()->par("packNum").doubleValue();
    double x = getParentModule()->par("kX").doubleValue();
    double y = getParentModule()->par("kY").doubleValue();
    double z = getParentModule()->par("kZ").doubleValue();
    double max=x>y?x:y;
    max=max>z?max:z;
    // surely saturate (under uniform traffic) for delta <= (max/8)*(PackLen/datarate)
    SimTime delta=max*.125*L/B;

    nsize = getParentModule()->getSubmodule("node",0)->getVectorSize();
    count = getParentModule()->par("count");
    rantype = getParentModule()->par("rantype");
    deltaG = delta / getParentModule()->par("deltaG");
    WATCH(count);
    tog = new TO2("Generate a new packet timeout");
    if ((getParentModule()->par("external"))) // if external clock is needed, then start to do something
        scheduleAt(simTime(),tog);
}

Clock_gen::~Clock_gen(){
    delete tog;
}

simtime_t Clock_gen::randist(simtime_t av){
  if (rantype==0)
    return av;
  if (rantype==1)
    return exponential(av);
  if (rantype==2)
    return normal(av,av*.1);

  // non-recognized random distribution
  throw cRuntimeError("Non recognized generator random distribution: %d", rantype);
}


void Clock_gen::sendout(){
    for (int i=0; i<nsize; ++i){
        cGate* dst = getParentModule()->getSubmodule("node",i)->getSubmodule("generator")->gate("newburst");
        cMessage* msg=new cMessage;
        sendDirect(msg,dst);
    }
    if (--count>0)
      scheduleAt(simTime()+randist(deltaG),tog);
    // ev << "--->" << count << endl;
}

void Clock_gen::handleMessage(cMessage *msg)
{
    if (dynamic_cast<TO2*>(msg) != NULL){
      sendout();
      return;
    }
}
