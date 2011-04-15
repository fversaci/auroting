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
  /// insert p in the right coda
  void routePack(Pack* p);
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
  /// send packets
  void sendPacks();
  /// flush (N)ACKS and Packs
  inline void flushAll();
  /// return minimal paths to destination
  vector<int> minimal(Pack* p);
  /// convert from int address to (x,y,z) coordinates
  vector<int> addr2coor(int a);
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
  for (int i=0; i<2*dim; ++i){
    freeSpace[2*i] = par("EscQueueSize");
    freeSpace[2*i+1] = par("AdapQueueSize");
  }
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
  for (int i=0; i<qn; ++i){
    createWatch("freeSpace",freeSpace[i]);
    createStdMapWatcher("waiting",waiting[i]);
    createStdVectorWatcher("coda",coda[i]);
  }
  WATCH_MAP(nacks);
  tom = new TO("Timeout");
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


vector<int> BRouter::minimal(Pack* p){
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
  if (coda[q].size() < freeSpace[q])
    return false;
  return true;
}

bool BRouter::bubble(int q){
  // true if q has two free slots
  if (coda[q].size() < freeSpace[q]-1)
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
  // q has already been tested to have free slots
  sendACK(p);
  p->setQueue(q);
  take(p);
  coda[q].push_back(OrdPack(p,--tp));
  push_heap(coda[q].begin(), coda[q].end());
  --freeSpace[q];
}

void BRouter::routePack(Pack* p){
  // try and enqueue in an adaptive queue along some (random) minimal path.
  // Note: bubble paper prefers to continue along the same direction
  vector<int> dirs=minimal(p);
  int n=dirs.size();
  int ran=intrand(n);
  for(int d=0; d<n; ++d){
    int q=1+2*dirs[(ran+d)%n];
    if (!full(q)){ // if one free adaptive queue is found insert the packet 
      enqueue(p,q);
      return;
    }
  }
  // if all adaptive queues are full, try the DOR escape one
  int q=2*dirs[0];
  /*
   * if full drop the packet, if just injected (i.e., it comes from a
   * queue different from q) also requires a bubble
   */
  if (full(q) || (!bubble(q) && p->getQueue()!=q) ) {
    sendNAK(p);
    delete p;
    getParentModule()->bubble("Packet dropped!");
    return;
  }
  // otherwise insert it
  enqueue(p,q);
}

void BRouter::rcvPack(Pack* p){
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
    ++numRcvd;
    ev << (p->getTreeId()) <<  ": Consumed at " << addr << " sent from " << p->getSrc() << endl;
    getParentModule()->bubble("Arrived!");
    return;
  }
  // otherwise insert it into the right queue
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

