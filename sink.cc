/*
 * sink.cc
 *
 *  Created on: Dec 15, 2010
 *      Author: cesco
 */

#include <omnetpp.h>
#include "pack_m.h"

// Sink for packet consumption
class Sink: public cSimpleModule {
protected:
  virtual void handleMessage(cMessage *msg);
};

// register module class with `\opp`
Define_Module(Sink);

void Sink::handleMessage(cMessage *msg) {
  delete msg; // discard the message
  // notify the counter
  // INC* mi=new INC();
  // mi->setCow(1);
  // sendDirect(mi, getParentModule()->getParentModule()->getSubmodule("timer"), "addCow");
}
