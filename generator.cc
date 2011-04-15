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
  /// Send a packet (generate one if needed)
  void sendPack();
  /// Timeout message for sending next message
  TO* tom;
  /// Packet to send
  Pack* p;
  /// Packets count-down
  int count;
  /// Delta time between packets
  simtime_t delta;
  /// Reschedule timeout
  void resTO();
  /// Packet length
  int pl;
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
  delta = par("delta");
  pl = par("packLen");
  tom = new TO("Send a new packet timeout");
  p = NULL;
  scheduleAt(simTime(),tom);
  WATCH(count);
}


void Generator::sendPack(){
  if (p == NULL){
    int dest = intrand(nsize);
    string roba = "To " + IntToStr(dest);
    p = new Pack(roba.c_str());
    p->setByteLength(pl);
    p->setDst(dest);
    p->setSrc(addr);
    p->setQueue(-1);
    p->setHops(0);
  }
  send(p->dup(), "inject$o");
}


void Generator::resTO(){
  // cancel old timeout
  if (tom->isScheduled())
    cancelEvent(tom);
  // schedule next packet
  if ((count)>0)
    scheduleAt(simTime()+delta,tom);
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
    delete p;
    p = NULL;
    delete msg;
    resTO();
    return;
  }
  // if timeout, send a message
  if (dynamic_cast<TO*>(msg) != NULL){
    sendPack();
    --count;
  }
}
