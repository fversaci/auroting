/*
 * router.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>

class Router: public cSimpleModule {
protected:
	virtual void handleMessage(cMessage *msg);
};

// register module class with `\opp`
Define_Module(Router)
;

void Router::handleMessage(cMessage *msg) {
	//delete msg; // just discard everything we receive
	send(msg, "gate$o", 0);
}

