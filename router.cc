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
	cQueue coda;
	simtime_t timeout;  // timeout
	cMessage *timeoutEvent;  // holds pointer to the timeout self-message
	cMessage *ack;
	int queueSize;
	int ackind;
	inline bool full();
	inline void sendACK(cMessage *mess);
	inline void routePac(Pack *pac);
	inline void sendPack();
protected:
	virtual void handleMessage(cMessage *msg);
	virtual void initialize();
public:
	Router();
	virtual ~Router();
};

// register module class with `\opp`
Define_Module(Router);

Router::Router(){
}

Router::~Router(){
	cancelAndDelete(timeoutEvent);
	delete ack;
}

void Router::routePac(Pack *pac){
	send(pac, "gate$o", intrand(6));
}

void Router::initialize(){
	timeout = .001 * (simtime_t) par("timeout"); // in milliseconds
	timeoutEvent = new cMessage("TIMEOUT");
	ackind=par("ackind");
	ack = new cMessage("ACK");
	ack->setKind(ackind);
	queueSize = par("queueSize");
}

bool Router::full(){
	if (coda.length() <= queueSize)
		return false;
	else
		return true;
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

void Router::sendPack(){
	routePac((Pack *) coda.front()->dup());
	scheduleAt(simTime() + timeout, timeoutEvent);
}

void Router::handleMessage(cMessage *msg) {
	if (msg == timeoutEvent) // timeout expired, re-send packet and restart timer
		sendPack();
	else if (msg->getKind() == ackind){
		// ack received
		ev << "ACK received at " << ((int) getParentModule()->par("addr")) << endl;
		cancelEvent(timeoutEvent);
		delete coda.pop(); // delete committed packet
		delete msg; // delete ACK
		if (!coda.empty())
			sendPack();
	} else {
		// arrived a new packet
		Pack *p = check_and_cast<Pack *>(msg);
		int addr = getParentModule()->par("addr");
		int dst = p->getDst();

		if (dst == addr) {
			// arrived at destination: consume
			sendACK(msg);
			send(p, "consume");
			ev << "Reached node " << addr << endl;
		}
		else if (full()) // discard packet
		{
			// message lost
			delete msg;
			ev << "Message lost" << endl;
		} else {
			// forward message
			sendACK(msg);
			coda.insert(p);
			if (!timeoutEvent->isScheduled())
				sendPack();
		}
	}
}
