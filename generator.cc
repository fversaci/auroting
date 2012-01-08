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
	/// choose packet destinations
	vector<int> chooseDsts();
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
	/// Packets number per burst
	int pn;
	/// Communication pattern
	int commPatt;
	/// queue of packets to be sent
	cQueue togo;
	/// waiting for (N)ACKS?
	bool wacks;
	/// convert from int address to (x,y,z) coordinates
	vector<int> addr2coor(int a);
	/// convert from  (x,y,z) coordinates to int address
	int coor2addr(vector<int> c);
	/// this node coordinates
	vector<int> coor;
	/// torus dimensions sizes
	vector<int> kCoor;
	/// number of dimensions of the torus
	static const int dim=3;
	/// log_2(nsize), for FFT pattern
	int l2;
	/// current butterfly dimension
	int bdim;
	/// random distribution for packet generation given some average value
	inline simtime_t randist(simtime_t av);
	/// generator random distribution -- 0=deterministic, 1=exponential, 2=nomrmal(av,av/10)
	int rantype;
	/// destination in random permutation
	int pdest;
	/// init permutation
	int init_permutation(unsigned int seed);
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
	double L = 8.0 * par("packLen").doubleValue() * par("packNum").doubleValue();
	double x = getParentModule()->getParentModule()->par("kX").doubleValue();
	double y = getParentModule()->getParentModule()->par("kY").doubleValue();
	double z = getParentModule()->getParentModule()->par("kZ").doubleValue();
	double max=x>y?x:y;
	max=max>z?max:z;
    // surely saturate for delta <= (max/8)*(PackLen/datarate)
	SimTime delta=max*.125*L/B;

	kCoor.assign(3,0);
	kCoor[0] = getParentModule()->getParentModule()->par("kX");
	kCoor[1] = getParentModule()->getParentModule()->par("kY");
	kCoor[2] = getParentModule()->getParentModule()->par("kZ");
	addr = getParentModule()->par("addr");
	coor = addr2coor(addr);
	nsize = getParentModule()->getVectorSize();

	bdim=0; l2=0;
	if ((nsize & (nsize - 1))==0) // if power of 2 compute logarithm
		for (int ns=nsize; ns!=1; ns/=2, ++l2) ; // l2=log_2(nsize)

	count = par("count");
	rantype = par("rantype");
	togo.setName("ToGo queue");
	pl = par("packLen");
	pn = par("packNum");
	commPatt = par("commPatt");
	deltaG = delta / par("deltaG");
	deltaS = delta / (pn*((double) par("deltaS")));
	tom = new TO("Send a new packet timeout");
	tog = new TO2("Generate a new packet timeout");
	scheduleAt(simTime(),tog);
	scheduleAt(simTime(),tom);
	WATCH(count);
	wacks = false;
	unsigned int seed=1;
	pdest = init_permutation(seed);
}

int Generator::init_permutation(unsigned int seed){
	unsigned int sdp;
	sdp=seed;
	int n=nsize;
	vector<int> dests(n,0);
	for(int i=0; i<n; ++i)
		dests[i]=i;
	for(int i=n-1; i>0; --i){
		int ri=rand_r(&sdp)%i;
		int old=dests[i];
		dests[i]=dests[ri];
		dests[ri]=old;
	}
	return dests[addr];
}


vector<int> Generator::addr2coor(int a){
	vector<int> r(dim,0);
	for(int i=0; i<dim; ++i){
		r[i] = a % kCoor[i];
		a /= kCoor[i];
	}
	if (a != 0)
		ev << "Error in node address: out of range" << endl;
	return r;
}

int Generator::coor2addr(vector<int> c){
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


vector<int> Generator::chooseDsts(){
	vector<int> r;
	// uniform traffic
	if (commPatt==0){
		r.push_back(intrand(nsize));
		return r;
	}
	// first neighbors (6 nodes in 3 dim)
	if (commPatt==1){
		for (int d=0; d<dim; ++d){
			vector<int> me=addr2coor(addr);
			me[d]=(me[d]+1+kCoor[d])%kCoor[d];
			r.push_back(coor2addr(me));
			me[d]=(me[d]-2+kCoor[d])%kCoor[d];
			r.push_back(coor2addr(me));
		}
		return r;
	}
	// permutation
	if (commPatt==5){
		r.push_back(pdest);
		return r;
	}
	// nodes at distance <= 2 (24 nodes in 3 dim)
	if (commPatt==2){
		// points along the axes -- num=(4*dim)
		for (int d=0; d<dim; ++d){
			vector<int> me=coor;
			me[d]=(me[d]+1+kCoor[d])%kCoor[d];
			r.push_back(coor2addr(me));
			me[d]=(me[d]+1+kCoor[d])%kCoor[d];
			r.push_back(coor2addr(me));
			me[d]=(me[d]-3+kCoor[d])%kCoor[d];
			r.push_back(coor2addr(me));
			me[d]=(me[d]-1+kCoor[d])%kCoor[d];
			r.push_back(coor2addr(me));
		}
		// points reached by going along two different dimensions d1 and d2 -- num=(4*dim)
		for (int d1=0; d1<dim; ++d1){
			for (int d2=d1+1; d2<dim; ++d2){
				vector<int> me=coor;
				// (1,1)
				me[d1]=(me[d1]+1+kCoor[d1])%kCoor[d1];
				me[d2]=(me[d2]+1+kCoor[d2])%kCoor[d2];
				r.push_back(coor2addr(me));
				// (-1,1)
				me[d1]=(me[d1]-2+kCoor[d1])%kCoor[d1];
				r.push_back(coor2addr(me));
				// (-1,-1)
				me[d2]=(me[d2]-2+kCoor[d2])%kCoor[d2];
				r.push_back(coor2addr(me));
				// (1,-1)
				me[d1]=(me[d1]+2+kCoor[d1])%kCoor[d1];
				r.push_back(coor2addr(me));
			}
		}
		return r;
	}
	// butterfly
	if (commPatt==10){
		if ((nsize & (nsize - 1))!=0) // not power of two (assuming nsize>0)
			throw cRuntimeError("Number of nodes must be a power of two for butterfly pattern");
		// for (int bdim=0; bdim<l2; ++bdim){
		int tog=(1<<bdim); // 2^d
		bdim=(bdim+1)%l2;
		int des=addr;
		des ^= tog;
		r.push_back(des);
		// }
		return r;
	}
	// transposition
	if (commPatt==20){
		int sq=sqrt(nsize);
		if (nsize!=sq*sq) // not a square number
			throw cRuntimeError("Number of nodes must a square number for transposition pattern");
		int x=addr%sq;
		int y=addr/sq;
		int des=y+x*sq;
		r.push_back(des);
		return r;
	}
	// tornado
	if (commPatt==30){
		vector<int> tor(dim,0);
		for(int d=0; d<dim; ++d){
			tor[d]=(coor[d]+(kCoor[d]/2)+(kCoor[d]%2)-1) % kCoor[d];
		}
		r.push_back(coor2addr(tor));
		return r;
	}
	// bitcomplement
	if (commPatt==40){
		if ((nsize & (nsize - 1))!=0) // not power of two (assuming nsize>0)
			throw cRuntimeError("Number of nodes must be a power of two for bitcomplement pattern");
		int bc=(nsize-1)^addr;
		r.push_back(bc);
		return r;

	}
	// non-recognized pattern
	throw cRuntimeError("Non recognized communication pattern: %d", commPatt);
}

simtime_t Generator::randist(simtime_t av){
	if (rantype==0)
		return av;
	if (rantype==1)
		return exponential(av);
	if (rantype==2)
		return normal(av,av*.1);

	// non-recognized random distribution
	throw cRuntimeError("Non recognized generator random distribution: %d", rantype);
}

void Generator::genPack(){
	vector<int> dsts = chooseDsts();
	if (pn % dsts.size()!=0)
		throw cRuntimeError("Choose a number of packets divisible by the desired destinations (%d)", dsts.size());
	int neach=pn/dsts.size();
	for(int i=0; i<(int) dsts.size(); ++i){
		int dest=dsts[i];
		string roba = "To " + IntToStr(dest);
		for (int i=0; i<neach; ++i){
			Pack *p = new Pack(roba.c_str());
			p->setByteLength(pl);
			p->setDst(dest);
			p->setSrc(addr);
			p->setBirthtime(simTime());
			togo.insert(p);
		}
	}
	// schedule next packet generation
	if (--count>0)
		scheduleAt(simTime()+randist(deltaG),tog);
		// scheduleAt(simTime()+exponential(deltaG),tog);
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
		return;
	}
	// packet to reinject
	if (dynamic_cast<Pack*>(msg) != NULL){
		Pack* p=(Pack *) msg;
		p->setReinjectable(false);
		togo.insert(p);
		if (!(tom->isScheduled()))  // reactivate message sending if queue was empty
			sendPack();
		return;
	}
}
