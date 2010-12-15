/*
 * router.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>
#include "pack_m.h"

class Router: public cSimpleModule {
protected:
	virtual void handleMessage(cMessage *msg);
};

// register module class with `\opp`
Define_Module(Router)
;

void Router::handleMessage(cMessage *msg) {
	Pack *p = check_and_cast<Pack *>(msg);


	int addr = getParentModule()->par("addr");
	int dst = p->getDst();
	if (dst == addr) {
		send(p, "consume");
		ev << "Raggiunto nodo " << addr << endl;
	} else
		send(p, "gate$o", intrand(6));
}

