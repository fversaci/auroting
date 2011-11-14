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
#include <algorithm>
#include "pack_m.h"
#include "ordpack.h"
using namespace std;



/// Bubble router
class BRouter: public cSimpleModule {
private:
	/// number of dimensions of the torus
	static const int dim=3;
	/// number of queues (an escape and adaptive one for each unidirectional gate)
	static const int qn=4*dim;
	/// queues sizes
	unsigned int freeSpace[qn];
	/// router node coordinates
	vector<int> coor;
	/// torus dimensions sizes
	vector<int> kCoor;
	/// parent node address
	int addr;
	/// size of queues to and from the CPU
	int ioqsize;
	/// consume queue
	cQueue conq;
	/// vector queue  0=x+e, 1=x+a, 2=x-e, 3=x-a, 4=y+e, 5=y+a, 6=y-e, 7=y-a, 8=z+e, 9=z+a, 10=z-e, 11=z-a
	vector<OrdPack> coda[qn];
	/// Time priority (decrements every time a new message is inserted into a queue)
	long tp;
	/// map of not ACKed packages
	map<long, OrdPack> waiting[qn];
	/// map of not sent (N)ACKs with index of output channel
	map<cPacket*, int> nacks;
	/// enqueue the packet
	inline void enqueue(Pack* p, int q);
	/// handle the incoming packet
	inline void rcvPack(Pack* p);
	/// packet has reached (an intermediate?) destination
	inline void acceptPack(Pack* p);
	/// try and consume the packet
	inline void consPack(Pack* p);
	/// insert p in the right coda
	void routePack(Pack* p);
	/// try and deroute packet if just injected
	bool deroute(Pack* p);
	/// use outflank derouting?
	bool outflank;
	/// use CQR routing
	bool cqr;
	/// choose directions used by CQR
	void chooseCQRdirs(Pack* p);
	/// edge length of outflank cube
	int ofEdge;
	/// test if given queue is full
	inline bool full(int q);
	/// test if given queue would leave a bubble after an insertion
	inline bool bubble(int q);
	/// send NAK to a packet
	inline void sendNAK(Pack *mess);
	/// send ACK to a packet
	inline void sendACK(Pack *mess);
	/// send (N)ACKS waiting in nacks
	void flushNACKs();
	/// send a packet from the consumption queue
	void flushCons();
	/// send packets
	void sendPacks();
	/// flush (N)ACKS and Packs
	inline void flushAll();
	/// return minimal paths from source to destination p->getDst()
	vector<int> minimal(int asource, Pack* p);
	/// return outflank paths to destination
	vector<int> ofnonmin(Pack* p);
	/// return outflank paths to destination in (-1, 0, 1, 2) format
	vector<int> ofnonmindirs(Pack* p);
	/// convert from int address to (x,y,z) coordinates
	vector<int> addr2coor(int a);
	/// convert from  (x,y,z) coordinates to int address
	int coor2addr(vector<int> c);
	/// add intermediate destinations
	inline bool addMidpoints(Pack* p, int q);
	/// handle ACK messages
	void handleACK(Ack * ap);
	/// handle NAK messages
	void handleNAK(Nak * ap);
	/// Packets received (consumed at destination)
	long numRcvd;
	/// Timeout message
	TO* tom;
	/// Schedule timeout
	void schedTO();
	/// compute the midpoints for outflanking (it actually computes only one midpoint)
    vector<int> twoMids(Pack * p, int q);
protected:
	virtual void handleMessage(cMessage *msg);
	virtual void initialize();
	virtual void finish();
	virtual void updateDisplay();
public:
	BRouter();
	virtual ~BRouter();
};

// register module class with `\opp`
Define_Module(BRouter);

BRouter::BRouter(){
}

BRouter::~BRouter(){
	delete tom;
}

void BRouter::initialize(){
	outflank = par("outflank");
	ofEdge = par("ofEdge");
	for (int i=0; i<2*dim; ++i){
		freeSpace[2*i] = par("EscQueueSize");
		freeSpace[2*i+1] = par("AdapQueueSize");
	}
	ioqsize = par("InOutQueueSize");
	addr = getParentModule()->par("addr");
	kCoor.assign(3,0);
	kCoor[0] = getParentModule()->getParentModule()->par("kX");
	kCoor[1] = getParentModule()->getParentModule()->par("kY");
	kCoor[2] = getParentModule()->getParentModule()->par("kZ");
	coor = addr2coor(addr);
	tp = 0;
	numRcvd = 0;
	WATCH(numRcvd);
	WATCH(coor[0]);
	WATCH(coor[1]);
	WATCH(coor[2]);
	conq.setName("conq");
	for (int i=0; i<qn; ++i){
		createWatch("freeSpace",freeSpace[i]);
		createStdMapWatcher("waiting",waiting[i]);
		createStdVectorWatcher("coda",coda[i]);
	}
	WATCH_MAP(nacks);
	tom = new TO("Timeout");
	if (cqr && outflank) // CQR and OutFlank cannot both be activated
		throw cRuntimeError("CQR and OutFlank cannot both be activated");

}

void BRouter::finish(){
	// recordScalar("#received", numRcvd);
}

void BRouter::updateDisplay()
{
	char buf[10];
	sprintf(buf, "%ld", numRcvd);
	getParentModule()->getDisplayString().setTagArg("t",0,buf);
}


vector<int> BRouter::minimal(int asource, Pack* p){
	int adest = p->getDst();
	vector<int> r;
	// get coordinates
	vector<int> dest=addr2coor(adest);
	vector<int> source=addr2coor(asource);
	for (int i=0; i<dim; ++i){
		int d=(kCoor[i]+dest[i]-source[i])%kCoor[i]; // (N % k is not well defined if N<0 in C)
		if (d==0)
			continue;
		if (!cqr){ // not CQR routing
			if (d <= kCoor[i]/2)
				r.push_back(2*i); // Coor[i]+
			else
				r.push_back(2*i+1); // Coor[i]-
		} else { // CQR routing
			r.push_back(2*i+p->getCqrdir(i));
		}
	}
	return r;
}

vector<int> BRouter::ofnonmindirs(Pack* p){
	vector<int> r;
	// packet destination coordinates
	vector<int> dest=addr2coor(p->getDst());
	for (int i=0; i<dim; ++i){
		if (ofEdge>=(kCoor[i]/2)){ // ignore dimension which are too thin
			r.push_back(0);
			continue;
		}
		int d=(kCoor[i]+dest[i]-coor[i])%kCoor[i]; // (N % k in not well defined if N<0 in C)
		if (d==0){
			r.push_back(2); // Coor[i]+ and Coor[i]-
			continue;
		}
		if (d > kCoor[i]/2)
			r.push_back(+1); // Coor[i]+
		else
			r.push_back(-1); // Coor[i]-
	}
	return r;
}

vector<int> BRouter::ofnonmin(Pack* p){
	vector<int> r;
	// packet destination coordinates
	vector<int> dest=addr2coor(p->getDst());
	for (int i=0; i<dim; ++i){
		if (ofEdge>=(kCoor[i]/2)) // ignore dimension which are too thin
			continue;
		int d=(kCoor[i]+dest[i]-coor[i])%kCoor[i]; // (N % k in not well defined if N<0 in C)
		if (d==0){
			r.push_back(2*i); // Coor[i]+
			r.push_back(2*i+1); // Coor[i]-
			continue;
		}
		if (d > kCoor[i]/2)
			r.push_back(2*i); // Coor[i]+
		else
			r.push_back(2*i+1); // Coor[i]-
	}
	return r;
}

vector<int> BRouter::addr2coor(int a){
	vector<int> r(dim,0);
	for(int i=0; i<dim; ++i){
		r[i] = a % kCoor[i];
		a /= kCoor[i];
	}
	if (a != 0)
		ev << "Error in node address: out of range" << endl;
	return r;
}

int BRouter::coor2addr(vector<int> c){
	return c[0]+c[1]*kCoor[0]+c[2]*kCoor[0]*kCoor[1];

	// algorithm for arbitrary dimension number
	//	int r=0;
	//	int i=dim-1;
	//	for(; i>0; --i){
	//		r += c[i];
	//		r *= kCoor[i-1];
	//	}
	//	r+=c[0];
	//	return r;
}

void BRouter::schedTO(){
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
	// consider also consume channel
	simtime_t fin = gate("consume")->getTransmissionChannel()->getTransmissionFinishTime();
	if (fin > simTime()){
		if (first){
			min=fin;
			first=false;
		}
		if (fin<min)
			min=fin;
	}
	// reschedule
	if (min > simTime())
		scheduleAt(min, tom);
}

void BRouter::flushNACKs(){
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

bool BRouter::full(int q){
	// queues have finite capacity
	//OLD: if (coda[q].size() < freeSpace[q])
	if (freeSpace[q]>0)
		return false;
	return true;
}

bool BRouter::bubble(int q){
	// true if q has at least two free slots
	//OLD: if (coda[q].size() < freeSpace[q]-1)
	if (freeSpace[q]>1)
		return true;
	return false;
}

void BRouter::sendACK(Pack *mess){
	mess->setHops(mess->getHops()+1); // increment hop counter
	cGate *orig=mess->getArrivalGate();
	Ack *ackpack=new Ack();
	// if just injected send ACK to generator
	if (orig->getId()==gate("inject$i")->getId()){
		send(ackpack, gate("inject$o"));
		return;
	}
	// otherwise send ACK back
	int ind=orig->getIndex();
	ackpack->setTID(mess->getTreeId());
	ackpack->setQueue(mess->getQueue());
	take(ackpack);
	nacks[ackpack]=ind;
}

void BRouter::sendNAK(Pack *mess){
	cGate *orig=mess->getArrivalGate();
	Nak *nakpack=new Nak();
	// if just injected send NAK to generator
	if (orig->getId()==gate("inject$i")->getId()){
		send(nakpack, gate("inject$o"));
		return;
	}
	// otherwise send NAK back
	int ind=orig->getIndex();
	nakpack->setTID(mess->getTreeId());
	nakpack->setQueue(mess->getQueue());
	take(nakpack);
	nacks[nakpack]=ind;
}

void BRouter::enqueue(Pack* p, int q){
	p->setReinjectable(false); // not reinjectable
	// q has already been tested to have free slots
	sendACK(p);
	p->setQueue(q);
	take(p);
	coda[q].push_back(OrdPack(p,--tp));
	push_heap(coda[q].begin(), coda[q].end());
	--freeSpace[q];
}


vector<int> BRouter::twoMids(Pack* p, int q)
{
	vector<int> rr;
    // compute first intermediate destination
    vector<int> desdir(dim, 0);
    vector<int> dirs = ofnonmindirs(p);
    int dq = (q - 1) / 2;
    int rou = dq / 2; // first routed dimension
    desdir[rou] = (dq % 2) == 0 ? 1 : -1; // positive or negative direction?
    for(int d = 0;d < dim;++d){
        if(d == rou)
            // first routed direction already chosen
            continue;

        desdir[d] = -dirs[d]; // choose minimal direction for other dimensions
        if(dirs[d] == 2){
            // or a random one if both are non-minimal
            desdir[d] = (intrand(2) == 0) ? 1 : -1;
        }
    }
    vector<int> d1(dim, 0); // first midpoint
    vector<int> endp = addr2coor(p->getDst()); // final destination
    for(int d = 0;d < dim;++d){
        if(d == rou){
            d1[d] = (coor[d] + ofEdge * desdir[d] + kCoor[d]) % kCoor[d];
            continue;
        }
        d1[d] = (endp[d] + ofEdge * desdir[d] + kCoor[d]) % kCoor[d];
    }
    vector<int> d2(dim, 0); // second midpoint
    for(int d = 0;d < dim;++d){
        if(d == rou){
            // d2[d] = (coor[d] + ofEdge * desdir[d] + kCoor[d]) % kCoor[d];
            continue;
        }
        d2[d] = (endp[d] + desdir[d] + kCoor[d]) % kCoor[d];
    }

    rr.push_back(coor2addr(d1));
    rr.push_back(coor2addr(d2));
    return rr;
}

bool BRouter::addMidpoints(Pack* p, int q){
	vector<int> mids = twoMids(p,q);
	int ad1 = mids[0];
	// int ad2 = mids[1];
	vector<int> min1=minimal(addr, p);
	vector<int> min2=minimal(ad1, p);
	if (min1==min2) // if intermediate destination is useless return false
		return false;

	// use just one midpoint
	p->setRoute(0, p->getDst());
	p->setRoute(1, ad1);
	p->setCurrDst(1);
	p->setDst(ad1);

	return true;
}


bool BRouter::deroute(Pack* p){
	// if destination is too close return false?
	// ............

	// if not just injected or already derouted return false
	if (p->getArrivalGate()->getId()!=gate("inject$i")->getId() || p->getDerouted()){
		return false;
	}

	// choose some (random) non-minimal, not too thin direction
	vector<int> dirs=ofnonmin(p);
	int n=dirs.size();
	int ran=intrand(n);
	for(int d=0; d<n; ++d){
		int qd=(ran+d)%n; // direction \in {0=x+, ..., 5=z-}
		int q=1+2*dirs[qd];
		if (bubble(q)){ // if two free slots in an *adaptive* queue are found, use the queue
			if (addMidpoints(p,q)==false) // add intermediate destinations
				return false;
			p->setDerouted(true);
			enqueue(p,q); // insert into q
			return true;
		}
	}
	// packet not derouted
	return false;
}

void BRouter::chooseCQRdirs(Pack* p){
	return;
}

void BRouter::routePack(Pack* p){
	// if CRQ and just injected then choose packets directions once for all
	if (cqr && p->getQueue()==-1)
		chooseCQRdirs(p);
	// try and enqueue in an adaptive queue along some (random) minimal path.
	// Note: bubble paper prefers to continue along the same direction
	vector<int> dirs=minimal(addr, p);
	int n=dirs.size();
	int ran=intrand(n);
	for(int d=0; d<n; ++d){
		int q=1+2*dirs[(ran+d)%n];
		if (!full(q)){ // if one free adaptive queue is found, insert the packet
			enqueue(p,q);
			return;
		}
	}
	// if all adaptive queues are full, try the DOR escape one
	int q=2*dirs[0];
	// if full drop the packet, if just injected (i.e., it comes from a
	// queue different from q) also requires a bubble
	if (full(q) || (!bubble(q) && p->getQueue()!=q) ) {
		// before discarding it, try to deroute the packet if just injected from the generator
		if (outflank && deroute(p)){
			getParentModule()->bubble("Derouted!");
			return;
		}
		// if at intermediate destination consume and reinject
		if (p->getReinjectable()){
			consPack(p);
			return;
		}
		// otherwise discard it
		sendNAK(p);
		delete p;
		getParentModule()->bubble("Dropped!");
		return;
	}
	// otherwise insert it
	enqueue(p,q);
}

void BRouter::consPack(Pack* p){
	// reject the packet if consume queue is full
	if (conq.length()>=ioqsize)
	{
		sendNAK(p);
		delete p;
		getParentModule()->bubble("Dropped!");
		return;
	}
	// otherwise consume it
	conq.insert(p);
	sendACK(p);
}

void BRouter::acceptPack(Pack* p){
	int cd=p->getCurrDst();
	// if final destination is reached
	if(cd==0){
		consPack(p);
		return;
	}
	// go to next intermediate destination
	p->setCurrDst(--cd);
	p->setDst(p->getRoute(cd));
	p->setReinjectable(true);
	routePack(p);
}

void BRouter::flushCons(){
	if (!conq.empty() && !gate("consume")->getTransmissionChannel()->isBusy()){
		Pack *p=(Pack*) conq.pop();
		send(p, "consume");
		++numRcvd;
		ev << (p->getTreeId()) <<  ": Consumed at " << addr << " sent from " << p->getSrc() << endl;
		getParentModule()->bubble("Arrived!");
	}
}

void BRouter::rcvPack(Pack* p){
	// if packet corrupted, drop it and send NAK
	bool crp=p->hasBitError();
	if (crp)
	{
		sendNAK(p);
		delete p;
		getParentModule()->bubble("Corrupted!");
		return;
	}
	// if packet at destination, consume it (or route if a midpoint has been reached)
	if (p->getDst() == addr)
		acceptPack(p);
	else // otherwise insert it into the right queue
		routePack(p);
}


void BRouter::sendPacks(){
	for (int q=0; q<qn; ++q){
		// process non empty queues
		if (!coda[q].empty()){
			OrdPack op=coda[q].front();
			Pack* pac=(Pack *) op.p;
			// try and route a packet. If successful, add it to waiting[q]
			int des=q/2;
			if (!gate("gate$o",des)->getTransmissionChannel()->isBusy()){
				send(pac->dup(), "gate$o", des);
				ev << (pac->getTreeId()) <<  ": From " << addr << endl;
				pop_heap(coda[q].begin(),coda[q].end());
				coda[q].pop_back();
				waiting[q][pac->getTreeId()] = op;
			}
		}
	}
}

void BRouter::flushAll(){
	flushNACKs();
	flushCons();
	sendPacks();
	schedTO();
}

void BRouter::handleACK(Ack *ap)
{
	ev << (ap->getTID()) <<  ": ACK at " << addr << endl;
	long tid = ap->getTID();
	int q=ap->getQueue();
	Pack *per = (Pack *) waiting[q][tid].p;
	waiting[q].erase(tid);
	++freeSpace[q];
	drop(per);
	delete per; // delete committed packet
	delete ap; // delete ACK
}

void BRouter::handleNAK(Nak *ap)
{
	ev << (ap->getTID()) <<  ": NAK at " << addr << endl;
	long tid = ap->getTID();
	int q=ap->getQueue();
	OrdPack op = waiting[q][tid];
	waiting[q].erase(tid);
	coda[q].push_back(op);
	push_heap(coda[q].begin(),coda[q].end());
	delete ap; // delete NAK
}

void BRouter::handleMessage(cMessage *msg) {
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

