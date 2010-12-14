/*
 * processor.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>

class Processor: public cSimpleModule {
protected:
	virtual void handleMessage(cMessage *msg);
	virtual void initialize();
};

// register module class with `\opp`
Define_Module(Processor)
;

void Processor::handleMessage(cMessage *msg) {
	delete msg; // just discard everything we receive
}

void Processor::initialize() {
	cMessage *msg = new cMessage("Messaggio di prova");
	send(msg, "inject");
}
