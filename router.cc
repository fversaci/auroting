/*
 * router.cc
 *
 *  Created on: Dec 13, 2010
 *      Author: cesco
 */

#include <omnetpp.h>
#include <vector>
#include <map>
#include <queue>
#include "pack_m.h"
#include "ordpack.h"
using namespace std;

class Router: public cSimpleModule {
private:
	/// number of central queues
	static const int qs=3;
	/// number of dimensions of the torus
	static const int dim=3;
	/// central queues size
	unsigned int freeSpace[qs];
	/// router node coordinates
	vector<int> coor;
	/// torus dimensions sizes
	vector<int> kCoor;
	/// parent node address
	int addr;
	/// vector queue  0=A, 1=B, 2=C
	priority_queue<OrdPack> coda[qs];
	/// Time priority (decrements every time a new message is inserted into a queue)
	long tp;
	/// map of not ACKed packages
	map<long, OrdPack> waiting[qs];
	/// map of not sent (N)ACKs with index of output channel
	map<cPacket*, int> nacks;
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
	/// send (N)ACKS waiting in nacks
	void flushNACKs();
	/// send packs waiting in queues
	void flushPacks();
	/// route a packet
	inline bool routePack(Pack* p);
	/// flush (N)ACKS and Packs
	inline void flushAll();
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
	/// Timeout message
	TO* tom;
	/// Schedule timeout
	void schedTO();
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
	delete tom;
}

void Router::initialize(){
	for (int i=0; i<qs; ++i)
		freeSpace[i] = par("queueSize");
	addr = getParentModule()->par("addr");
	kCoor.assign(3,0);
	kCoor[0] = getParentModule()->getParentModule()->par("kX");
	kCoor[1] = getParentModule()->getParentModule()->par("kY");
	kCoor[2] = getParentModule()->getParentModule()->par("kZ");
	coor = addr2coor(addr);
	RRq = 0;
	tp = 0;
    numRcvd = 0;
    WATCH(numRcvd);
	//for (int i=0; i<qs; ++i)
		//WATCH_MAP(waiting[i]);
    WATCH_MAP(nacks);
    tom = new TO("Timeout");
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

void Router::schedTO(){
	// cancel old timeout
	if (tom->isScheduled())
	    cancelEvent(tom);
	// compute when the first busy channel will become free
	simtime_t min=simTime();
	bool first=true;
	for (int i=0; i<2*dim; ++i){
		simtime_t fin = gate("gate$o",i)->getTransmissionChannel()->getTransmissionFinishTime();
		if (fin > simTime()){
			if (first){
				min=fin;
				first=false;
			}
			if (fin<min)
				min=fin;
		}
	}
	// reschedule
	if (min > simTime())
		scheduleAt(min, tom);
}

void Router::flushNACKs(){
	// send (N)ACKs if possible
	vector<cPacket*> sent;
	for(map<cPacket*, int>::iterator it=nacks.begin(); it!=nacks.end(); ++it){
		if (!gate("gate$o",it->second)->getTransmissionChannel()->isBusy()){
			send(it->first->dup(), "gate$o", it->second);
			sent.push_back(it->first);
		}
	}
	// remove the sent ones from the queue
	for(vector<cPacket*>::iterator it=sent.begin(); it!=sent.end(); ++it){
		nacks.erase(nacks.find(*it));
		drop(*it);
		delete *it;
	}
}

bool Router::routePack(Pack* pac){
	// forward the packet, starting from a random admissible channel
	vector<int> dirs=minimal(pac);
	int n=dirs.size();
	int ran=intrand(n);
	for(int d=0; d<n; ++d){
		int des=dirs[(ran+d)%n];
		if (!gate("gate$o",des)->getTransmissionChannel()->isBusy()){
			send(pac, "gate$o", des);
			return true;
		}
	}
	delete pac;
	return false;
}

bool Router::full(int q){
	// A, B, C queues have finite capacity
	if (coda[q].size() < freeSpace[q])
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
	ackpack->setQueue(mess->getQueue());
	take(ackpack);
	nacks[ackpack]=ind;
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
	nakpack->setQueue(mess->getQueue());
	take(nakpack);
	nacks[nakpack]=ind;
}

void Router::enqueue(Pack* p, int q){
	// if queue full, drop the packet
	if (full(q))
	{
		sendNAK(p);
		delete p;
		getParentModule()->bubble("Packet dropped!");
		return;
	}
	// otherwise insert it
	sendACK(p);
	p->setQueue(q);
	take(p);
	coda[q].push(OrdPack(p,--tp));
	--freeSpace[q];
}

void Router::rcvPack(Pack* p){
	// if packet corrupted, drop it and send NAK
	bool crp=p->hasBitError();
	if (crp)
	{
		sendNAK(p);
		delete p;
		getParentModule()->bubble("Packet corrupted!");
		return;
	}
	// if packet at destination, consume it
	int dst = p->getDst();
	if (dst == addr) {
		// arrived at destination: consume
		sendACK(p);
		send(p, "consume");
		ev << "Reached " << addr << " from " << p->getSrc() << endl;
		getParentModule()->bubble("Arrived!");
		return;
	}
	// otherwise try and insert it into the right queue
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

void Router::flushPacks(){
	int q=RRq;
	vector<int> neq; // non-empty queues
	// save non empty queues
	do {
		if (!coda[q=(q+1)%qs].empty())
			neq.push_back(q);
	}
	while (q!=RRq); // exits when q=RRq
	// process non empty queues
	if (!neq.empty()){
		RRq=*(neq.rbegin()); // set RRq to last non empty queue
		for (vector<int>::iterator it=neq.begin(); it!=neq.end(); ++it){
			OrdPack op=coda[*it].top();
			Pack* pac=(Pack *) op.p;
			// try and route packet. If successful, add it to waiting[q]
			if (routePack(pac->dup())){
				coda[*it].pop();
				waiting[*it][pac->getTreeId()] = op;
			}
		}
	}
}

void Router::flushAll(){
	flushNACKs();
	flushPacks();
	schedTO();
}

void Router::handleACK(Ack *ap)
{
    ev << "ACK received at " << ((int) getParentModule()->par("addr")) << endl;
    long tid = ap->getTID();
    int q=ap->getQueue();
    Pack *per = (Pack *) waiting[q][tid].p;
    waiting[q].erase(tid);
    ++freeSpace[q];
    drop(per);
    delete per; // delete committed packet
    delete ap; // delete ACK
}

void Router::handleNAK(Nak *ap)
{
    ev << "NAK received at " << ((int) getParentModule()->par("addr")) << endl;
    long tid = ap->getTID();
    int q=ap->getQueue();
    OrdPack op = waiting[q][tid];
    waiting[q].erase(tid);
    coda[q].push(op);
    delete ap; // delete NAK
}

void Router::handleMessage(cMessage *msg) {
	++numRcvd;
	if (ev.isGUI())
		updateDisplay();
	// ACK received
	if (dynamic_cast<Ack*>(msg) != NULL){
		handleACK((Ack *)msg);
	}
	// NAK received
	else if (dynamic_cast<Nak*>(msg) != NULL){
		handleNAK((Nak *)msg);
	}
	// Timeout received
	else if (dynamic_cast<TO*>(msg) != NULL){
	}
	// arrived a new packet
	else {
		Pack *p = check_and_cast<Pack *>(msg);
		rcvPack(p);
	}
	flushAll();
}
