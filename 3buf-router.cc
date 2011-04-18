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

/// Three-buffer router
class TBRouter: public cSimpleModule {
private:
  /// number of central queues
  static const int qn=3;
  /// number of dimensions of the torus
  static const int dim=3;
  /// central queues size
  unsigned int freeSpace[qn];
  /// free space in neighbors' queues (e.g., nfs[4][1] = free space of B queue in neighbor node along z+)
  int nfs[2*dim][qn];
  /// router node coordinates
  vector<int> coor;
  /// torus dimensions sizes
  vector<int> kCoor;
  /// parent node address
  int addr;
  /// vector queue  0=A, 1=B, 2=C
  vector<OrdPack> coda[qn];
  /// Time priority (decrements every time a new message is inserted into a queue)
  long tp;
  /// map of not ACKed packages
  map<long, OrdPack> waiting[qn];
  /// map of not sent (N)ACKs with index of output channel
  map<cPacket*, int> nacks;
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
  /// rearrange packets
  bool rearrPacks();
  /// forword packets
  bool forwPacks();
  /// forword packets (variant, preserves time order)
  bool forwPacks2();
  /// send/rearrange packets
  void movePacks();
  /// Select the right queue for a packet
  int sqPack(Pack* p);
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
  TBRouter();
  virtual ~TBRouter();
};

// register module class with `\opp`
Define_Module(TBRouter);

TBRouter::TBRouter(){
}

TBRouter::~TBRouter(){
  delete tom;
}

void TBRouter::initialize(){
  freeSpace[0] = par("AQueueSize");
  freeSpace[1] = par("BQueueSize");
  freeSpace[2] = par("CQueueSize");
  for(int n=0; n<2*dim; ++n)
    for(int i=0; i<qn; ++i)
      nfs[n][i]=freeSpace[i];

  addr = getParentModule()->par("addr");
  kCoor.assign(3,0);
  kCoor[0] = getParentModule()->getParentModule()->par("kX");
  kCoor[1] = getParentModule()->getParentModule()->par("kY");
  kCoor[2] = getParentModule()->getParentModule()->par("kZ");
  coor = addr2coor(addr);
  tp = 0;
  numRcvd = 0;
  tom = new TO("Timeout");

  WATCH(numRcvd);
  WATCH(coor[0]);
  WATCH(coor[1]);
  WATCH(coor[2]);
  WATCH(freeSpace[0]);
  WATCH(freeSpace[1]);
  WATCH(freeSpace[2]);
  WATCH_MAP(waiting[0]);
  WATCH_VECTOR(coda[0]);
  WATCH_MAP(waiting[1]);
  WATCH_VECTOR(coda[1]);
  WATCH_MAP(waiting[2]);
  WATCH_VECTOR(coda[2]);
  WATCH_MAP(nacks);
}

void TBRouter::finish(){
  //recordScalar("#received", numRcvd);
}

void TBRouter::updateDisplay()
{
  char buf[10];
  sprintf(buf, "%ld", numRcvd);
  getParentModule()->getDisplayString().setTagArg("t",0,buf);
}

bool TBRouter::Rtest(vector<int> dir){
  vector<int>::iterator it;
  bool res=false;
  for(it=dir.begin(); it!=dir.end(); ++it){
    int d=*it/2;
    if (*it%2==0) // X+
      res = res || (coor[d]!=kCoor[d]-1);
    else // X-
      res = res || (coor[d]==0);
  }
  return res;
}

bool TBRouter::Ltest(vector<int> dir){
  vector<int>::iterator it;
  bool res=false;
  for(it=dir.begin(); it!=dir.end(); ++it){
    int d=*it/2;
    if (*it%2==0) // X+
      res = res || (coor[d]==kCoor[d]-1);
    else // X-
      res = res || (coor[d]!=0);
  }
  return res;
}

vector<int> TBRouter::minimal(Pack* p){
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

vector<int> TBRouter::addr2coor(int a){
  vector<int> r(dim,0);
  for(int i=0; i<dim; ++i){
    r[i] = a % kCoor[i];
    a /= kCoor[i];
  }
  if (a != 0)
    ev << "Error in node address: out of range" << endl;
  return r;
}

void TBRouter::schedTO(){
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

void TBRouter::flushNACKs(){
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

bool TBRouter::routePack(Pack* pac){
  // forward the packet, along the free admissible channel the seems
  // to have more free space
  int q=pac->getQueue();
  vector<int> mirs=minimal(pac);
  int des=-1; int cap=-1;
  for(int i=0; i<(int) mirs.size(); ++i){
    int dir=mirs[i];
    int nc=nfs[dir][q]; // estimated capacity along dir
    if (!gate("gate$o", dir)->getTransmissionChannel()->isBusy() && nc>cap){
      des=dir;
      cap=nc;
    }
  }
  if(des>=0){
      send(pac, "gate$o", des);
      ev << (pac->getTreeId()) <<  ": From " << addr << endl;
      return true;
  }
  delete pac;
  return false;
}

bool TBRouter::full(int q){
  // A, B, C queues have finite capacity
  if (coda[q].size() < freeSpace[q])
    return false;
  return true;
}

void TBRouter::sendACK(Pack *mess){
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
  for (int i=0; i<qn; ++i)
    ackpack->setFs(i,freeSpace[i]);
  take(ackpack);
  nacks[ackpack]=ind;
}

void TBRouter::sendNAK(Pack *mess){
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
  for (int i=0; i<qn; ++i)
    nakpack->setFs(i,freeSpace[i]);
  take(nakpack);
  nacks[nakpack]=ind;
}

void TBRouter::enqueue(Pack* p, int q){
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
  coda[q].push_back(OrdPack(p,--tp));
  push_heap(coda[q].begin(), coda[q].end());
  --freeSpace[q];
}

void TBRouter::rcvPack(Pack* p){
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
  // otherwise insert it into the same queue it originates from (or 0 if just injected)
  int q=p->getQueue();
  if (q<0)
    q=0;
  enqueue(p, q);
}

int TBRouter::sqPack(Pack* p){
  int q=p->getQueue();
  // compute minimal possible directions
  vector<int> dirs=minimal(p);
  if (q==0){ // from queue A
    if (Rtest(dirs)) // bigger minimal neighbors
      return 0;
    else
      return 1;
  }
  if (q==1){ // from queue B
    if (Ltest(dirs)) // smaller minimal neighbors
      return 1;
    else
      return 2;
  }
  // from queue C
  return 2; // smaller neighbors?
}

bool TBRouter::rearrPacks(){
  bool workdone=false;
  for (int q=2; q>=0; --q){
    // process non empty queues
    if (!coda[q].empty()){
      OrdPack op=coda[q].front();
      Pack* pac=(Pack *) op.p;
      // if not in the right queue try and move it
      int d=sqPack(pac);
      if (d!=q && !full(d)){
	// pop from q and push into d
	workdone = true;
	OrdPack op=coda[q].front();
	pop_heap(coda[q].begin(),coda[q].end());
	coda[q].pop_back();
	++freeSpace[q];
	((Pack*) op.p)->setQueue(d);
	coda[d].push_back(op);
	push_heap(coda[d].begin(),coda[d].end());
	--freeSpace[d];
      }
    }
  }
  return workdone;
}

bool TBRouter::forwPacks(){
  bool workdone=false;
  for (int q=2; q>=0; --q){
    vector<OrdPack> newq;
    sort_heap(coda[q].begin(),coda[q].end());
    for(vector<OrdPack>::iterator it=coda[q].begin(); it!=coda[q].end(); ++it) {
      OrdPack op=*it;
      Pack* pac=(Pack *) op.p;
      int d=sqPack(pac);
      // if in the right queue try and route packet. If successful, add it to waiting[q]
      if (d==q && routePack(pac->dup())){
	workdone=true;
	waiting[q][pac->getTreeId()] = op;
      }
      // else keep it in the same queue
      else
	newq.push_back(op);
    }
    // heapify and restore the queue
    make_heap(newq.begin(), newq.end());
    coda[q]=newq;
  }
  return workdone;
}

bool TBRouter::forwPacks2(){
  bool workdone=false;
  for (int q=2; q>=0; --q){
    // process non empty queues
    if (!coda[q].empty()){
      OrdPack op=coda[q].front();
      Pack* pac=(Pack *) op.p;
      int d=sqPack(pac);
      // if in the right queue try and route packet. If successful, add it to waiting[q]
      if (d==q && routePack(pac->dup())){
	workdone=true;
	pop_heap(coda[q].begin(),coda[q].end());
	coda[q].pop_back();
	waiting[q][pac->getTreeId()] = op;
      }
    }
  }
  return workdone;
}

void TBRouter::movePacks(){
  while (rearrPacks() || forwPacks());
}


void TBRouter::flushAll(){
  flushNACKs();
  movePacks();
  schedTO();
}

void TBRouter::handleACK(Ack *ap)
{
  // update free space statistics
  int n=ap->getArrivalGate()->getIndex();
  for(int i=0; i<qn; ++i)
    nfs[n][i]=ap->getFs(i);
  // handle the ACK
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

void TBRouter::handleNAK(Nak *ap)
{
  // update free space statistics
  int n=ap->getArrivalGate()->getIndex();
  for(int i=0; i<qn; ++i)
    nfs[n][i]=ap->getFs(i);
  // handle the NAK
  ev << (ap->getTID()) <<  ": NAK at " << addr << endl;
  long tid = ap->getTID();
  int q=ap->getQueue();
  OrdPack op = waiting[q][tid];
  waiting[q].erase(tid);
  coda[q].push_back(op);
  push_heap(coda[q].begin(),coda[q].end());
  delete ap; // delete NAK
}

void TBRouter::handleMessage(cMessage *msg) {
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
