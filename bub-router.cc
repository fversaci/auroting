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
  /// see if it's ok to deroute a packet (with OutFlank or CQR)
  void setRouting(Pack *p);
  /// distance of a path through a midpoint
  int distance3(int asource, int amid, int adest);
  /// distance between two points (minimal)
  int distance2(int asource, int adest);
  /// set minimal directions for the packet routing
  void setmindirs(Pack* p);
  /// set routing directions for packets along orthant ort
  void setCQRdirs(Pack* p, int ort);
  /// use outflank derouting?
  bool outflank;
  /// is minimal orthant free enough to send something?
  bool minfree(Pack* p);
  /// use CQR routing
  bool cqr;
  /// choose directions used by CQR
  int chooseCQRdirs(Pack* p, double* pr);
  /// choose OF midpoint (ofdist set to distance through the midpoint)
  int chooseOFmid(Pack* p, double* pr);
  /** computes priority of a path as function distance lenghtening (proposed/minimal)
   *  and free slots
   **/
  double prior(double fs, double disfac);
  /// edge length of outflank cube
  int ofEdge;
  /// test if given queue is full
  inline bool full(int q);
  /// variance of free space if chosen a direction to route into
  double fsvariance(vector<int> dirsout);
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
  /// return minimal paths from asource to adest
  vector<int> minimal(int asource, int adest);
  /// returns the admissible directions
  vector<int> routedirs(Pack* p);
  /// compute ISet
  vector<int> ISet(Pack* p);
  /// compute MSet
  vector<int> MSet(Pack* p);
  /// flip a direction
  int flip(int dir);
  /// is direction dir (\in {0,1}) for dimension d among the ones returned by "minimal"?
  bool isinminimal(Pack* p, int d, int dir);
  /// convert from int address to (x,y,z) coordinates
  vector<int> addr2coor(int a);
  /// convert from  (x,y,z) coordinates to int address
  int coor2addr(vector<int> c);
  /// convert from orthant index to (\pm,\pm,\pm) coordinates
  vector<int> ind2orth(int a);
  /// convert from (\pm,\pm,\pm) coordinates to orthant index
  int orth2ind(vector<int> c);
  /// add intermediate destinations
  inline bool setOFmidpoint(Pack* p, int ort);
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
  /// compute a possible midpoint for outflanking
  int OFMid(Pack* p, vector<int> nout, vector<int> nin, vector<int> nhalf);
  /// computes max free slots along a direction
  int maxfreeslots();
  /// jolly parameter
  double jolly;
  /// adaptive queues size
  int aqs;
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
  ofEdge = par("ofEdge");
  aqs = par("AdapQueueSize");
  for (int i=0; i<2*dim; ++i){
    freeSpace[2*i] = par("EscQueueSize");
    freeSpace[2*i+1] = aqs;
  }
  ioqsize = par("InOutQueueSize");
  jolly = par("jolly");
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
  // choose routing algorithm
  int alg=par("algorithm");
  switch (alg) {
  case 0:
    outflank=false;
    cqr=false;
    break;
  case 1:
    outflank=true;
    cqr=false;
    break;
  case 2:
    cqr=true;
    outflank=false;
    break;
  case 3:
    cqr=true;
    outflank=true;
    break;
  default:
    throw cRuntimeError("Unknown routing algorithm: %d", alg);
    break;
  }
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

vector<int> BRouter::routedirs(Pack* p){
  vector<int> r;
  int adest=p->getDst();
  vector<int> dest=addr2coor(adest);
  vector<int> source=addr2coor(addr);
  for (int i=0; i<dim; ++i){
    int d=(kCoor[i]+dest[i]-source[i])%kCoor[i]; // (N % k is not well defined if N<0 in C)
    if (d==0)
      continue;
    r.push_back(2*i+p->getCqrdir(i));
  }
  return r;
}

vector<int> BRouter::minimal(int asource, int adest){
  vector<int> r;
  // get coordinates
  vector<int> dest=addr2coor(adest);
  vector<int> source=addr2coor(asource);
  for (int i=0; i<dim; ++i){
    int d=(kCoor[i]+dest[i]-source[i])%kCoor[i]; // (N % k is not well defined if N<0 in C)
    if (d==0)
      continue;
    if (d <= kCoor[i]/2)
      r.push_back(2*i); // Coor[i]+
    else
      r.push_back(2*i+1); // Coor[i]-
  }
  return r;
}

bool BRouter::isinminimal(Pack* p, int d, int dir){
  vector<int> mindirs=minimal(addr,p->getDst());
  int mydir=2*d+dir;
  bool r=false;
  for (int i=0; i<(int) mindirs.size(); ++i)
    if (mindirs[i]==mydir)
      r=true;
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

vector<int> BRouter::ind2orth(int a){
  vector<int> r(dim,0);
  for(int i=0; i<dim; ++i){
    r[i] = a % 2;
    a /= 2;
  }
  if (a != 0)
    ev << "Error in node address: out of range" << endl;
  return r;
}

int BRouter::orth2ind(vector<int> c){
  return c[0]+c[1]*2+c[2]*4;

  // algorithm for arbitrary dimension number
  //	int r=0;
  //	int i=dim-1;
  //	for(; i>0; --i){
  //		r += c[i];
  //		r *= 2;
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

int BRouter::OFMid(Pack* p, vector<int> nout, vector<int> nin, vector<int> nhalf){
  // compute intermediate destination
  vector<int> mid=coor; // midpoint
  vector<int> endp = addr2coor(p->getDst()); // final destination
  // use non-minimal outgoing directions
  for (int i=0; i<nout.size(); ++i){
    int d=nout[i]/2;
    int sign = ((nout[i]%2)==0?1:-1);
    mid[d] = (coor[d] + ofEdge * sign + kCoor[d]) % kCoor[d];
  }
  // use non-minimal incoming directions
  for (int i=0; i<nin.size(); ++i){
    int d=nin[i]/2;
    int sign = ((nin[i]%2)==0?1:-1);
    mid[d] = (endp[d] + ofEdge * sign + kCoor[d]) % kCoor[d];
  }
  // compute mid-coordinates
  for (int i=0; i<nhalf.size(); ++i){
    int d=nhalf[i]/2;
    int y=endp[d];
    int x=coor[d];
    if (x>y){ // we want y>=x
      int z=y;
      y=x;
      x=z;
    }
    int k=kCoor[d];
    int m=(x+y)/2; // x <= m <= y
    if (y-x>k/2)
      m=((x+y)%k)/2;
    mid[d] = m;
  }
  return coor2addr(mid);
}

bool BRouter::setOFmidpoint(Pack* p, int mid){
  if(mid<0)
    throw cRuntimeError("Negative midpoint for OutFlank");
  p->setRoute(0, p->getDst());
  p->setRoute(1, mid);
  p->setCurrDst(1);
  p->setDst(mid);
  p->setDerouted(true);
  p->setOFlanked(true);
  setmindirs(p);
  return true;
}

bool BRouter::minfree(Pack* p){
  // if minimal orthant is not congested exit
  int adapfree=0;
  vector<int> mindirs=minimal(addr, p->getDst());
  for(int i=0; i<(int) mindirs.size(); ++i)
    adapfree+=freeSpace[2*mindirs[i]+1];
  // int dorfree=freeSpace[2*mindirs[0]];
  return (adapfree>=1);
}

int BRouter::maxfreeslots()
{
  // compute the maximal free slots available for a routing
  int maxfs = 0;
  for(int d = 0;d < dim;++d){
    int pos = freeSpace[4 * d + 1]; // d+ adaptive
    int neg = freeSpace[4 * d + 3]; // d- adaptive
    if(pos > maxfs)
      maxfs = pos;
    if(neg > maxfs)
      maxfs = neg;
  }
  return maxfs;
}

void BRouter::setRouting(Pack* p){
  // if destination is too close return false?
  // ............

  // as default go minimal
  setmindirs(p);

  // if already derouted exit
  if (p->getDerouted()){
    return;
  }

  // compute free slots for minimal routing
  vector<int> mindirs = minimal(addr, p->getDst());
  int fsofmin = 0; // free slots of minimal routing
  for(int i = 0; i < (int)(mindirs.size()); ++i){
    fsofmin += freeSpace[1 + 2 * mindirs[i]];
  }
  double fsav =((double) fsofmin)/((double) mindirs.size());
  double MINpr=prior(fsav, 1.0);

  // if minimal orthant is not congested exit (go minimal)
  //if (minfree(p))
  //	return;

  double OFpr=0.0; // priority using OF
  double CQRpr=0.0; // priority using CQR

  // choose best OF midpoint
  int mid=-1;
  if(outflank)
    mid=chooseOFmid(p, &OFpr);

  // choose best orthant for CQR
  int ort=-1;
  if(cqr)
    ort=chooseCQRdirs(p, &CQRpr);

  // if alternate routes worse than minimal, then go minimal
  if (CQRpr<=MINpr && OFpr<=MINpr)
    return;

  // otherwise choose the best between OF and CQR paths
  if(CQRpr>OFpr)
    setCQRdirs(p,ort);
  else
    setOFmidpoint(p,mid);
}

vector<int> BRouter::ISet(Pack* p){
  vector<int> r;
  vector<int> endp = addr2coor(p->getDst()); // final destination
  for (int d=0; d<dim; ++d){
    if (coor[d]==endp[d])
      r.push_back(2*d);
  }
  return r;
}

vector<int> BRouter::MSet(Pack* p){
  vector<int> r;
  vector<int> endp = addr2coor(p->getDst()); // final destination
  for (int d=0; d<dim; ++d){
    if (coor[d]!=endp[d]){
      int pdis=(kCoor[d]+endp[d]-coor[d])%kCoor[d];
      if (pdis<=kCoor[d]/2)
	r.push_back(2*d);
      else
	r.push_back(1+2*d);
    }
  }
  return r;
}


int BRouter::flip(int dir){
  int d=dir/2;
  int v=dir%2;
  v=(v+1)%2;
  return 2*d+v;
}

int BRouter::chooseOFmid(Pack* p, double* ofpr){
  int mindis=distance2(addr, p->getDst());
  // choose the admissible midpoint with maximum priority
  double mp=0.0;
  int md=-1; // midpoint
  vector<int> none;
  vector<int> iset=ISet(p);
  vector<int> mset=MSet(p);
  vector<int> points;
  // consider nonmin-nonmin directions
  for(int i=0; i<iset.size(); ++i){
    vector<int> nout(1,-1);
    nout[0]=iset[i]; // d+
    points.push_back(OFMid(p,nout,none,none));
    nout[0]=flip(nout[0]); // d-
    points.push_back(OFMid(p,nout,none,none));
  }
  // consider min-nonmin directions
  if(mset.size()==2) // same plane
    for(int i=0; i<mset.size(); ++i){
      vector<int> nout(1,-1);
      vector<int> nin(1,-1);
      nout[0]=flip(mset[0]);
      nin[0]=mset[1];
      points.push_back(OFMid(p,nout,nin,none));
      nout[0]=flip(mset[1]);
      nin[0]=mset[0];
      points.push_back(OFMid(p,nout,nin,none));
    }
  if(mset.size()==3){ // different plane
    vector<int> nout(1,-1);
    vector<int> nin(1,-1);
    vector<int> nhalf(1,-1);
    // p1
    nout[0]=flip(mset[0]);
    nin[0]=mset[1];
    nhalf[0]=mset[2];
    points.push_back(OFMid(p,nout,nin,nhalf));
    // p1=2
    nout[0]=flip(mset[0]);
    nin[0]=mset[2];
    nhalf[0]=mset[1];
    points.push_back(OFMid(p,nout,nin,nhalf));
    // p3
    nout[0]=flip(mset[1]);
    nin[0]=mset[0];
    nhalf[0]=mset[2];
    points.push_back(OFMid(p,nout,nin,nhalf));
    // p4
    nout[0]=flip(mset[1]);
    nin[0]=mset[2];
    nhalf[0]=mset[0];
    points.push_back(OFMid(p,nout,nin,nhalf));
    // p5
    nout[0]=flip(mset[2]);
    nin[0]=mset[0];
    nhalf[0]=mset[1];
    points.push_back(OFMid(p,nout,nin,nhalf));
    // p6
    nout[0]=flip(mset[2]);
    nin[0]=mset[1];
    nhalf[0]=mset[0];
    points.push_back(OFMid(p,nout,nin,nhalf));
  }
  for (int j=0; j<(int) points.size(); ++j){
    // compute candidate midpoint
    int candmid=points[j];
    // at least a non-minimal dir in the first path
    vector<int> middirs=minimal(addr, candmid);
    bool an=false; // at least one non-minimal direction?
    int mfs=0; // free slots on an adaptive queue
    int dis=distance3(addr, candmid, p->getDst());  // distance through the candidate midpoint
    for(int i=0; i<(int) middirs.size(); ++i){
      int lfs=freeSpace[2*middirs[i]+1];
      mfs+=lfs;
      int d=middirs[i]/2;
      int dirbit=middirs[i]%2;
      if (!isinminimal(p,d,dirbit))
	an=true;
    }
    if (!an)
      continue;
    // at least a non-minimal dir in the second path
    middirs=minimal(candmid, p->getDst());
    an=false; // at least one non-minimal direction?
    for(int i=0; i<(int) middirs.size(); ++i){
      int d=middirs[i]/2;
      int dir=middirs[i]%2;
      if (!isinminimal(p,d,dir))
	an=true;
    }
    if (!an)
      continue;

    // mid is admissible, look at its priority
    double disfac=((double) dis)/((double) mindis);
    double fsav =((double) mfs)/((double) middirs.size());
    double lp=prior(fsav,disfac);
    if(lp>mp){
      mp=lp;
      md=candmid;
    }
  }
  *ofpr=mp; // save its priority
  // use the computed midpoint
  return md;
}

double BRouter::fsvariance(vector<int> dirsout){
  // compute variance
  int fssum=0;
  for (int dir=0; dir<2*dim; ++dir)
    fssum+=freeSpace[1+2*dir];
  --fssum;
  double fsmean=((double) fssum)/(2.0*dim);
  double fssqsum=0.0;
  vector<double> fsd(2*dim,0.0);
  for (int dir=0; dir<2*dim; ++dir){
    fsd[dir]=freeSpace[1+2*dir];
  }
  double den=0.0;
  for (int i=0; i<(int) fsd.size(); ++i)
    den+=fsd[i];
  for (int i=0; i<(int) fsd.size(); ++i){
    fsd[i]-=fsd[i]/den;
    fssqsum+=fsd[i]*fsd[i];
  }
  double fsvar=((double)fssqsum)/(2.0*dim)-fsmean*fsmean;
  return fsvar;
}

double BRouter::prior(double fs, double disfac){
  if (fs==0.0)
    return 0.0;
  int maxfs=aqs-maxfreeslots();
  double fsfac=((double) maxfs)/((double) aqs-fs+1.0);
  // return fsfac/disfac;
  return fsfac+jolly/disfac;
}

int BRouter::distance3(int asource, int amid, int adest){
  return distance2(asource, amid)+distance2(amid, adest);
}

int BRouter::distance2(int asource, int adest){
  vector<int> dest=addr2coor(adest);
  vector<int> source=addr2coor(asource);
  int dis=0;
  for (int i=0; i<dim; ++i){
    int d=(kCoor[i]+dest[i]-source[i])%kCoor[i]; // (N % k is not well defined if N<0 in C)
    if (d>kCoor[i]/2)
      d=kCoor[i]-d;
    dis+=d;
  }
  return dis;
}

int BRouter::chooseCQRdirs(Pack* p, double* pr){
  // get coordinates
  vector<int> dest=addr2coor(p->getDst());
  vector<int> source=addr2coor(addr);
  // free slots for each direction
  vector<int> fsl(2*dim, 0);
  for(int i=0; i<2*dim; ++i){
    fsl[i]=freeSpace[2*i+1];
  }
  // distance along each direction
  vector<int> dists(2*dim, 0);
  for (int i=0; i<dim; ++i){
    int d=(kCoor[i]+dest[i]-source[i])%kCoor[i]; // (N % k is not well defined if N<0 in C)
    if (d==0)
      continue;
    dists[2*i]=d;
    dists[2*i+1]=kCoor[i]-d;
  }
  // distance and free slots for each orthant
  vector<int> distorth(1<<dim, 0);
  vector<int> fslorth(1<<dim, 0);
  for (int i=0; i<(1<<dim); ++i){
    vector<int> o=ind2orth(i);
    for(int d=0; d<dim; ++d){
      distorth[i]+=dists[2*d+o[d]];
      if(dists[2*d+o[d]]!=0){ // consider only directions that will be used
	fslorth[i]+=fsl[2*d+o[d]];
      }
    }
  }
  // select orthant for routing (the one with maximum priority)
  int ort=-1;
  int mindis=distance2(addr, p->getDst());
  double pp=0.0;
  for (int i=0; i<(1<<dim); ++i){
    double df=((double) distorth[i])/((double) mindis);
    double fsav =((double) fslorth[i])*0.333333333333333333333;
    double lp=prior(fsav, df);
    if (lp>pp){
      ort=i;
      pp=lp;
    }
  }
  *pr=pp; // save priority
  return ort;
}


void BRouter::routePack(Pack* p){
  // if just injected consider derouting the packet (if OutFlank or CQR)
  if (p->getQueue()==-1)
    setRouting(p);
  // try and enqueue in the most free adaptive queue
  // Note: bubble paper prefers to continue along the same direction
  vector<int> dirs=routedirs(p);
  int fs=0;
  int q=-1;
  for(int d=0; d<(int) dirs.size(); ++d){
    int lq=1+2*dirs[d];
    int lfs=freeSpace[lq];
    if (lfs>fs){
      fs=lfs;
      q=lq;
    }
  }
  if (q!=-1){
    enqueue(p,q);
    return;
  }
  // if all adaptive queues are full, try the DOR escape one
  q=2*dirs[0];
  // if full drop the packet, if just injected (i.e., it comes from a
  // queue different from q) also requires a bubble
  if (full(q) || (!bubble(q) && p->getQueue()!=q) ) {
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
  setmindirs(p);
  routePack(p);
}

void BRouter::setmindirs(Pack* p){
  vector<int> mindirs=minimal(addr, p->getDst());
  for (int i=0; i<(int) mindirs.size(); ++i){
    int d=mindirs[i]/2;
    int dir=mindirs[i]%2;
    p->setCqrdir(d,dir);
  }
}

void BRouter::setCQRdirs(Pack* p, int ort){
  if(ort<0)
    throw cRuntimeError("Negative orthant for CQR");
  // save directions for CQR routing in the packet
  vector<int> o=ind2orth(ort);
  bool nonmin=false;
  for (int i=0; i<dim; ++i){
    p->setCqrdir(i,o[i]);
    if (!isinminimal(p,i,o[i]))
      nonmin=true;
  }
  if (nonmin){
    p->setDerouted(true);
    p->setCQRed(true);
  }
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

