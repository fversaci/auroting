/*
 * router.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>
#include <vector>
#include <map>
#include "pack_m.h"
using namespace std;

class Router: public cSimpleModule {
private:
	/// number of central queues
	static const int qs=3;
	/// number of dimensions of the torus
	static const int dim=3;
	int queueSize;
	/// router node coordinates
	vector<int> coor;
	/// torus dimensions sizes
	vector<int> kCoor;
	/// parent node address
	int addr;
	/// vector queue  0=A, 1=B, 2=C
	cQueue coda[qs];
	/// map of not ACKed packages
	map<long, Pack*> mrep;
	/// RoundRobin queue selector \f$ \in \f$ {0=A, 1=B, 2=C}
	int RRq;
	/// put the packet in the right queue
	inline void rcvPack(Pack* p);
	/// enqueue p in coda[q] (drop packet if full(q) or send ACK if insert works)
	void enqueue(Pack* p, int q);
	/// test if given queue is full
	inline bool full(int q);
	/// send NAK to a packet
	inline void sendNAK(Pack *mess);
	/// send ACK to a packet
	inline void sendACK(Pack *mess);
	/// route a packet
	inline void routePack(Pack* p);
	/// Send some packet from a queue (if available)
	inline void sendPack();
	/// increment the round-robin queue index
	inline int incRRq();
	/// return minimal paths to destination
	vector<int> minimal(Pack* p);
	/// convert from int address to (x,y,z) coordinates
	vector<int> addr2coor(int a);
	/// test the existence of a "bigger" (Right order) neighbor along the given directions
	bool Rtest(vector<int> dir);
	/// test the existence of a "smaller" (Left order) neighbor along the given directions
	bool Ltest(vector<int> dir);
	/// handle ACK messages
    void handleACK(Ack * ap);
	/// handle NAK messages
    void handleNAK(Nak * ap);
	/// Packets received (both routed and consumed)
	long numRcvd;
protected:
	virtual void handleMessage(cMessage *msg);
	virtual void initialize();
	virtual void finish();
	virtual void updateDisplay();
public:
	Router();
	virtual ~Router();
};

// register module class with `\opp`
Define_Module(Router);

Router::Router(){
}

Router::~Router(){
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
	queueSize = par("queueSize");
	addr = getParentModule()->par("addr");
	kCoor.assign(3,0);
	kCoor[0] = getParentModule()->getParentModule()->par("kX");
	kCoor[1] = getParentModule()->getParentModule()->par("kY");
	kCoor[2] = getParentModule()->getParentModule()->par("kZ");
	coor = addr2coor(addr);
	RRq = 0;
	coda[0].setName("Queue A");
	coda[1].setName("Queue B");
	coda[2].setName("Queue C");
    numRcvd = 0;
    WATCH(numRcvd);
}

void Router::finish(){
	recordScalar("#received", numRcvd);
}

void Router::updateDisplay()
{
    char buf[10];
    sprintf(buf, "%ld", numRcvd);
    getParentModule()->getDisplayString().setTagArg("t",0,buf);
}

void Router::routePack(Pack* pac){
	// at destination, consume it
	int dst = pac->getDst();
	if (dst == addr) {
		// arrived at destination: consume
		Pack *toer=mrep[pac->getTreeId()];
		mrep.erase(pac->getTreeId());
		drop(toer);
		delete toer;
		send(pac, "consume");
		ev << "Reached node " << addr << endl;
		getParentModule()->bubble("Arrived!");
		return;
	}
	// otherwise forward
	vector<int> dirs=minimal(pac);
	int d=dirs[intrand(dirs.size())];
	send(pac, "gate$o", d);
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
	ackpack->setTID(mess->getTreeId());
	send(ackpack, "gate$o", ind);
}

void Router::sendNAK(Pack *mess){
	cGate *orig=mess->getArrivalGate();
	// if just injected do not send NAK
	if (orig->getId()==gate("inject")->getId()){
		return;
	}
	// otherwise send NAK back
	int ind=orig->getIndex();
	Nak *nakpack=new Nak();
	nakpack->setTID(mess->getTreeId());
	send(nakpack, "gate$o", ind);
}

void Router::enqueue(Pack* p, int q){
	// if queue full or trasmission error, drop the packet
	bool crp=p->hasBitError();
	if (full(q) || crp)
	{
		sendNAK(p);
		delete p;
		if (!crp)
			getParentModule()->bubble("Packet dropped!");
		else
			getParentModule()->bubble("Packet corrupted!");
		return;
	}
	// otherwise insert it or consume it
	sendACK(p);
	p->setQueue(q);
	coda[q].insert(p);
}

void Router::rcvPack(Pack* p){
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
		// change queue until a non empty one is found
		do {
			if (!coda[incRRq()].empty())
				somedata=true;
		}
		while (!somedata && RRq!=startRRq);
		// if no free queues, then exit
		if (!somedata)
			break;
		// if found a suitable queue send a packet from it and restart looking
		Pack* pac=(Pack *) coda[RRq].pop();
		take(pac);
		mrep[pac->getTreeId()]=pac;
		routePack(pac->dup());
	}
}

void Router::handleACK(Ack *ap)
{
    ev << "ACK received at " << ((int) getParentModule()->par("addr")) << endl;
    long tid = ap->getTID();
    Pack *per = mrep[tid];
    mrep.erase(tid);
    drop(per);
    delete per; // delete committed packet
    delete ap; // delete ACK
}

void Router::handleNAK(Nak *ap)
{
    ev << "NAK received at " << ((int) getParentModule()->par("addr")) << endl;
    long tid = ap->getTID();
    Pack *per = mrep[tid];
    routePack(per->dup());
    delete ap; // delete ACK
}


void Router::handleMessage(cMessage *msg) {
	++numRcvd;
	if (ev.isGUI())
		updateDisplay();
	// ACK received
	if (dynamic_cast<Ack*>(msg) != NULL){
		handleACK((Ack *)msg);
		sendPack();
		return;
	}
	// NAK received
	if (dynamic_cast<Nak*>(msg) != NULL){
		handleNAK((Nak *)msg);
		sendPack();
		return;
	}
	// arrived a new packet
	Pack *p = check_and_cast<Pack *>(msg);
	rcvPack(p);
	sendPack();
}


