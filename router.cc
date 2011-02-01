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
	static const int qs=3; ///< queue numbers
	cQueue coda[qs]; ///< vector queue  0=A, 1=B, 2=C
	Timeout* tv[qs]; ///< vector of timeouts for the queues
	simtime_t timeout;  // timeout
	int queueSize;
	int RRq; ///< RoundRobin queue selector \f$ \in \f$ {0=A, 1=B, 2=C}
	inline void RcvPack(Pack* p); ///< put the packet in the right queue
	/// enqueue p in coda[q] (drop packet if full(q) or send ACK in insert works)
	void enqueue(Pack* p, int q);
	inline bool full(int q);
	inline void sendACK(Pack *mess);
	inline void routePack(int q);
	inline void sendPack();
	inline int incRRq();
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
	for (int c=0; c<qs; ++c){
		delete(tv[c]);
	}
}

int Router::incRRq(){
	RRq = (RRq + 1) % qs;
	return RRq;
}

void Router::routePack(int q){
	Pack* pac=(Pack *) coda[q].front()->dup();
	// at destination, consume it
	int addr = getParentModule()->par("addr");
	int dst = pac->getDst();
	if (dst == addr) {
		// arrived at destination: consume
		send(pac, "consume");
		ev << "Reached node " << addr << endl;
		delete coda[q].pop();
		return;
	}
	// otherwise forward
	send(pac, "gate$o", intrand(6));
	scheduleAt(simTime() + timeout, tv[pac->getQueue()]);
}

void Router::initialize(){
	timeout = .001 * (simtime_t) par("timeout"); // in milliseconds
	queueSize = par("queueSize");
	RRq = 0;
	for (int c=0; c<qs; ++c){
		tv[c]=new Timeout();
	}
}

bool Router::full(int q){
	// A,B,C queues have finite capacity
	if (coda[q].length() < queueSize)
		return false;
	return true;
}

void Router::sendACK(Pack *mess){
	cGate *orig=mess->getArrivalGate();
	// if just injected do not send ACK
	if (orig->getId()==gate("inject")->getId()){
		return;
	}
	// otherwise send ACK back
	int ind=orig->getIndex();
	Ack *ackpack=new Ack();
	ackpack->setQueue(mess->getQueue());
	send(ackpack, "gate$o", ind);
}

void Router::enqueue(Pack* p, int q){
	// if queue full drop the packet
	if (full(q))
	{
		delete p;
		ev << "Message lost" << endl;
		return;
	}

	// otherwise insert it or consume it
	sendACK(p);
	p->setQueue(q);
	coda[q].insert(p);
}

void Router::RcvPack(Pack* p){
	int q=p->getQueue();

	// if it has just been injected insert it in coda[0]
	if (q<0){
		enqueue(p,0);
		return;
	}
	// if q=2, keep the same queue
	if (q==2){
		enqueue(p, q);
		return;
	}
	// else increase the queue
	enqueue(p, q+1);
}

void Router::sendPack(){
	int startRRq=RRq;
	bool somedata=false; //is there some data to send?
	// change queue until a non empty non blocked one is found
	do {
		if (!coda[incRRq()].empty() && !(tv[RRq]->isScheduled()))
			somedata=true;
	}
	while (!somedata && RRq!=startRRq);
	// if found a suitable queue send a packet
	if (somedata){
		routePack(RRq);
	}
}

void Router::handleMessage(cMessage *msg) {
	// timeout expired
	if (dynamic_cast<Timeout*>(msg) != NULL){
		sendPack();
		return;
	}
	// ack received
	if (dynamic_cast<Ack*>(msg) != NULL){
		Ack* ap=(Ack*) msg;
		ev << "ACK received at " << ((int) getParentModule()->par("addr")) << endl;
		int q=ap->getQueue();
		cancelEvent(tv[q]); // cancel timeout
		delete coda[q].pop(); // delete committed packet
		delete ap; // delete ACK
		sendPack();
		return;
	}
	// arrived a new packet
	Pack *p = check_and_cast<Pack *>(msg);
	RcvPack(p);
	sendPack();
}


