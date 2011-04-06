/*
 * sink.cc
 *
 *  Created on: Dec 15, 2010
 *      Author: cesco
 */

#include <omnetpp.h>

// Sink for packet consumption
class Sink: public cSimpleModule {
protected:
	virtual void handleMessage(cMessage *msg);
};

// register module class with `\opp`
Define_Module(Sink)
;

void Sink::handleMessage(cMessage *msg) {
	delete msg; // just discard everything we receive
}
