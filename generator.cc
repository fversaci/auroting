/*
 * generator.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>
#include "pack_m.h"

using namespace std;

/// Packet generator
class Generator: public cSimpleModule {
private:
	/// Send a packet (if available)
	void sendPack();
	/// Generate a packet
	void genPack();
	/// Timeout message for sending next message
	TO* tom;
	/// Timeout message for generating new message
	TO2* tog;
	/// Packets generation count-down
	int count;
	/// Delta time between packets generation
	simtime_t deltaG;
	/// Delta time between packets transmission
	simtime_t deltaS;
	/// Packet length
	int pl;
	/// queue of packets to be sent
	cQueue togo;
	/// waiting for (N)ACKS?
	bool wacks;
protected:
	virtual void initialize();
	virtual void handleMessage(cMessage *msg);
	int nsize;
	int addr;
public:
	virtual ~Generator();
};

// register module class with `\opp`
Define_Module(Generator);


Generator::~Generator(){
	delete tom;
	delete tog;
}


string IntToStr(int n) {
	ostringstream result;
	result << n;
	return result.str();
}

void Generator::initialize() {
	cDatarateChannel* chan00 = (cDatarateChannel*) getParentModule()->gate("gate$o",0)->getChannel();
	double B = chan00->getDatarate(); // b/s
	double L = 8.0 * par("packLen").doubleValue();
	double x = getParentModule()->getParentModule()->par("kX").doubleValue();
	double y = getParentModule()->getParentModule()->par("kY").doubleValue();
	double z = getParentModule()->getParentModule()->par("kZ").doubleValue();
	double max=x>y?x:y;
	max=max>z?max:z;
    // surely saturate for delta <= (max/8)*(PackLen/datarate)
	SimTime delta=max*.125*L/B;

	addr = getParentModule()->par("addr");
	nsize = getParentModule()->getVectorSize();
	count = par("count");
	deltaG = delta / par("deltaG");
	deltaS = delta / par("deltaS");
	pl = par("packLen");
	tom = new TO("Send a new packet timeout");
	tog = new TO2("Generate a new packet timeout");
	scheduleAt(simTime(),tog);
	scheduleAt(simTime(),tom);
	WATCH(count);
	wacks = false;
}

void Generator::genPack(){
	int dest = intrand(nsize);
	string roba = "To " + IntToStr(dest);
	Pack *p = new Pack(roba.c_str());
	p->setByteLength(pl);
	p->setDst(dest);
	p->setSrc(addr);
	p->setQueue(-1);
	p->setHops(0);
	p->setBirthtime(simTime());
	togo.insert(p);
	// schedule next packet generation
	if (--count>0)
		scheduleAt(simTime()+exponential(deltaG),tog);
}

void Generator::sendPack(){
	if (!togo.empty() && !wacks){
		Pack *p=(Pack*) togo.front();
		send(p->dup(), "inject$o");
		wacks = true;
	}
	// schedule next packet transmission
	if (count>0 || !togo.empty()) // stop rescheduling when count==0 and togo is empty
		scheduleAt(simTime()+deltaS,tom);
}

void Generator::handleMessage(cMessage *msg){
	// if NAK, add another message to count
	if (dynamic_cast<Nak*>(msg) != NULL){
		wacks = false;
		delete msg;
		return;
	}
	// if ACK, delete stored message
	if (dynamic_cast<Ack*>(msg) != NULL){
		wacks = false;
		Pack *p = (Pack *) togo.pop();
		delete p;
		delete msg;
		return;
	}
	// if timeout 2, generate a message
	if (dynamic_cast<TO2*>(msg) != NULL){
		genPack();
		return;
	}
	// if timeout, send a message
	if (dynamic_cast<TO*>(msg) != NULL){
		sendPack();
	}
}
