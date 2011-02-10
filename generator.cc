/*
 * generator.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>
#include "pack_m.h"

using namespace std;

class Generator: public cSimpleModule {
protected:
	virtual void initialize();
	int nsize;
};

// register module class with `\opp`
Define_Module(Generator)
;

string IntToStr(int n) {
	ostringstream result;
	result << n;
	return result.str();
}

void Generator::initialize() {
	int addr = getParentModule()->par("addr");
	nsize = getParentModule()->getVectorSize();
	int dest = intrand(nsize);
	string roba = "To " + IntToStr(dest);
	Pack *msg = new Pack(roba.c_str());
	msg->setDst(dest);
	msg->setSrc(addr);
	msg->setQueue(-1);
	send(msg, "inject");
}
