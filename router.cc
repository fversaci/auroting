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
	Pack *bufpac;
	cMessage *ack;
	bool full;
	inline void sendACK(cMessage *mess);
	inline void routePac(Pack *pac);
protected:
	virtual void handleMessage(cMessage *msg);
	virtual void initialize();
public:
	Router();
	virtual ~Router();
};

// register module class with `\opp`
Define_Module(Router)
;

Router::Router(){
	bufpac = NULL;
	ack = NULL;
}

Router::~Router(){
	cancelAndDelete(timeoutEvent);
	if (bufpac != NULL)
		delete bufpac;
	delete ack;
}

void Router::routePac(Pack *pac){
	send(pac, "gate$o", intrand(6));
}

void Router::initialize(){
	timeout = .001 * (simtime_t) par("timeout"); // in milliseconds
	timeoutEvent = new cMessage("TIMEOUT");
	ack = new cMessage("ACK");
	ack->setKind(par("ackind"));
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
		routePac(bufpac->dup());
		scheduleAt(simTime() + timeout, timeoutEvent);
	} else if (msg->getKind() == 33){
		// ack received
		ev << "ACK received at " << ((int) getParentModule()->par("addr")) << endl;
		cancelEvent(timeoutEvent);
		delete bufpac;
		bufpac = NULL;
		full = false;
		delete msg;
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
		else if (full) // discard packet
		{
			// message lost
			delete msg;
			ev << "Message lost" << endl;
		} else {
			// forward message
			sendACK(msg);
			bufpac=p->dup();
			full = true;
			routePac(p);
			scheduleAt(simTime() + timeout, timeoutEvent);
		}
	}
}
