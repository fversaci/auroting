/*
 * router.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>
#include <vector>
#include "pack_m.h"
using namespace std;

class Router: public cSimpleModule {
private:
	static const int qs=3; ///< number of central queues
	static const int dim=3; ///< number of dimensions of the torus
	simtime_t timeout;  ///< timeout
	int queueSize;
	/// router node coordinates
	vector<int> coor;
	/// torus dimensions sizes
	vector<int> kCoor;
	/// parent node address
	int addr;
	cQueue coda[qs]; ///< vector queue  0=A, 1=B, 2=C
	Timeout* tv[qs]; ///< vector of timeouts for the queues
	int RRq; ///< RoundRobin queue selector \f$ \in \f$ {0=A, 1=B, 2=C}
	inline void RcvPack(Pack* p); ///< put the packet in the right queue
	/// enqueue p in coda[q] (drop packet if full(q) or send ACK if insert works)
	void enqueue(Pack* p, int q);
	inline bool full(int q);
	inline void sendACK(Pack *mess);
	inline void routePack(int q);
	inline void sendPack();
	inline int incRRq();
	/// return minimal paths to destination
	vector<int> minimal(Pack* p);
	/// convert from int address to (x,y,z) coordinates
	vector<int> addr2coor(int a);
	/// test the existence of a "bigger" (Right order) neighbor along the given directions
	bool Rtest(vector<int> dir);
	/// test the existence of a "smaller" (Left order) neighbor along the given directions
	bool Ltest(vector<int> dir);
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

bool Router::Rtest(vector<int> dir){
	vector<int>::iterator it;
	bool res=false;
	for(it=dir.begin(); it!=dir.end(); ++it){
		int d=*it/2;
		if (*it%2==0) // X+
			res |= (coor[d]!=kCoor[d]-1);
		else // X-
			res |= (coor[d]==0);
	}
	return res;
}

bool Router::Ltest(vector<int> dir){
	vector<int>::iterator it;
	bool res=false;
	for(it=dir.begin(); it!=dir.end(); ++it){
		int d=*it/2;
		if (*it%2==0) // X+
			res |= (coor[d]==kCoor[d]-1);
		else // X-
			res |= (coor[d]!=0);
	}
	return res;
}

vector<int> Router::minimal(Pack* p){
	vector<int> r;
	// packet destination coordinates
	vector<int> dest=addr2coor(p->getDst());
	for (int i=0; i<dim; ++i){
		int d=(kCoor[i]+dest[i]-coor[i])%kCoor[i]; // (N % k in not well defined if N<0 in C)
		if (d==0)
			continue;
		if (d <= kCoor[i]/2)
			r.push_back(2*i); // Coor[i]+
		else
			r.push_back(2*i+1); // Coor[i]-
	}
	return r;
}

int Router::incRRq(){
	RRq = (RRq + 1) % qs;
	return RRq;
}

vector<int> Router::addr2coor(int a){
	vector<int> r(dim,0);
	for(int i=0; i<dim; ++i){
		r[i] = a % kCoor[i];
		a /= kCoor[i];
	}
	if (a != 0)
		ev << "Error in node address: out of range" << endl;
	return r;
}

void Router::initialize(){
	timeout = .001 * (simtime_t) par("timeout"); // in milliseconds
	queueSize = par("queueSize");
	addr = getParentModule()->par("addr");
	kCoor.assign(3,0);
	kCoor[0] = getParentModule()->getParentModule()->par("kX");
	kCoor[1] = getParentModule()->getParentModule()->par("kY");
	kCoor[2] = getParentModule()->getParentModule()->par("kZ");
	coor = addr2coor(addr);
	RRq = 0;
	for (int c=0; c<qs; ++c){
		tv[c]=new Timeout();
	}
}


void Router::routePack(int q){
	Pack* pac=(Pack *) coda[q].front()->dup();
	// at destination, consume it
	int dst = pac->getDst();
	if (dst == addr) {
		// arrived at destination: consume
		send(pac, "consume");
		ev << "Reached node " << addr << endl;
		delete coda[q].pop();
		return;
	}
	// otherwise forward
	vector<int> dirs=minimal(pac);
	int d=dirs[intrand(dirs.size())];
	send(pac, "gate$o", d);
	scheduleAt(simTime() + timeout, tv[pac->getQueue()]);
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
		ev << "Message lost at queue " << q << endl;
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

	// compute minimal possible directions
	vector<int> dirs=minimal(p);
	if (q==0){ // from queue A
		if (Rtest(dirs)) // bigger neighbors?
			enqueue(p, 0);
		else
			enqueue(p, 1);
		return;
	}
	if (q==1){ // from queue B
		if (Ltest(dirs))
			enqueue(p, 1); // smaller neighbors?
		else
			enqueue(p, 2);
		return;
	}
	// from queue C
	enqueue(p, 2);
}

void Router::sendPack(){
	while(true){
		int startRRq=RRq;
		bool somedata=false; //is there some data to send?
		// change queue until a non empty non blocked one is found
		do {
			if (!coda[incRRq()].empty() && !(tv[RRq]->isScheduled()))
				somedata=true;
		}
		while (!somedata && RRq!=startRRq);
		// if no free queue then exit
		if (!somedata)
			break;
		// if found a suitable queue send a packet from it a restart looking
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


