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
  /// Packets count-down
  int count;
  /// Delta time between packets generation
  simtime_t deltaG;
  /// Delta time between packets transmission
  simtime_t deltaS;
  /// Reschedule timeout
  void resTO();
  /// Packet length
  int pl;
  /// queue of packets to be sent
  cQueue togo;
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
  addr = getParentModule()->par("addr");
  nsize = getParentModule()->getVectorSize();
  count = par("count");
  deltaG = par("deltaG");
  deltaS = par("deltaS");
  pl = par("packLen");
  tom = new TO("Send a new packet timeout");
  tog = new TO2("Generate a new packet timeout");
  scheduleAt(simTime(),tog);
  scheduleAt(simTime(),tom);
  WATCH(count);
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
	if (count>0)
		scheduleAt(simTime()+deltaG,tog);
}

void Generator::sendPack(){
	if (!togo.empty()){
		Pack *p=(Pack*) togo.front();
		send(p->dup(), "inject$o");
	}
}

void Generator::resTO(){
  // cancel old timeout
  if (tom->isScheduled())
    cancelEvent(tom);
	// schedule next packet transmission
	if (count>0 || !togo.empty())
		scheduleAt(simTime()+deltaS,tom);
}

void Generator::handleMessage(cMessage *msg){
  // if NAK, add another message to count
  if (dynamic_cast<Nak*>(msg) != NULL){
    ++count;
    delete msg;
    resTO();
    return;
  }
  // if ACK, delete stored message
  if (dynamic_cast<Ack*>(msg) != NULL){
	  Pack *p = (Pack *) togo.pop();
	  delete p;
	  delete msg;
	  resTO();
	  return;
  }
  // if timeout 2, generate a message
  if (dynamic_cast<TO2*>(msg) != NULL){
    genPack();
    --count;
    return;
  }
  // if timeout, send a message
  if (dynamic_cast<TO*>(msg) != NULL){
    sendPack();
  }
}
