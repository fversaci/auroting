/*
 * router.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>
#include "pack_m.h"

class Router: public cSimpleModule {
private:
	// cQueue coda;
	simtime_t timeout;  // timeout
	cMessage *timeoutEvent;  // holds pointer to the timeout self-message
	cMessage *bufmsg;
	cMessage *ack;
	bool full;
	void sendACK(cMessage *mess);
protected:
	virtual void handleMessage(cMessage *msg);
	virtual void initialize();
};

// register module class with `\opp`
Define_Module(Router)
;

void Router::initialize(){
	timeout = 0.25;
	timeoutEvent = new cMessage("timeoutEvent");
	ack = new cMessage("ACK");
	ack->setKind(33);
	full = false;
}

void Router::sendACK(cMessage *mess){
	// send ACK
	cGate *orig=mess->getArrivalGate();
	if (orig != gate("inject")){
		int ind=orig->getIndex();
		if (ind%2==0)
			++ind;
		else
			--ind;
		send(ack->dup(), "gate$o", ind);
	}
}

void Router::handleMessage(cMessage *msg) {
	if (msg == timeoutEvent) {
		// timeout expired, re-send packet and restart timer
		send(bufmsg->dup(), "gate$o", intrand(6));
		scheduleAt(simTime() + timeout, timeoutEvent);
	} else if (msg->getKind() == 33){
		// ack received
		ev << "ACK received at " << ((int) getParentModule()->par("addr")) << endl;
		cancelEvent(timeoutEvent);
		delete bufmsg;
		full = false;
	} else {
		// arrived packet
		Pack *p = check_and_cast<Pack *>(msg);
		int addr = getParentModule()->par("addr");
		int dst = p->getDst();

		if (dst == addr) {
			// arrived at destination: consume
			sendACK(msg);
			send(p, "consume");
			ev << "Reached node " << addr << endl;
		}
		else if (full || uniform(0,1) < 0.0) // discard packet?
		{
			// message lost
			delete msg;
			ev << "Message lost" << endl;
		} else {
			// forward message
			sendACK(msg);
			bufmsg=msg->dup();
			full = true;
			send(msg->dup(), "gate$o", intrand(6));
			scheduleAt(simTime() + timeout, timeoutEvent);
		}
	}
}


