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
  if (dynamic_cast<Pack*>(msg) != NULL){
    Pack* p=(Pack *) msg;
    // if to be reinjected send it to generator
    if (p->getReinjectable()){
    	send(p,"reinj");
    	return;
    }
    // notify the counter
    NoM* nm=new NoM();
    nm->setCow(1);
    nm->setHops(p->getHops());
    nm->setBirthtime(p->getBirthtime());
    nm->setDer(p->getDerouted()? 1 : 0);
    nm->setCqr(p->getCQRed()? 1 : 0);
    nm->setOfl(p->getOFlanked()? 1 : 0);
    nm->setOidn(p->getOidn()? 1 : 0);
    nm->setWidn(p->getWidn()? 1 : 0);
    sendDirect(nm, getParentModule()->getParentModule()->getSubmodule("timer"), "addCow");
  }
  // discard the message
  delete msg;
}
