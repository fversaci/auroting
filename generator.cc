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
  /// Generate and send a packet
  void genPack();
  /// Timeout message for sending next message
  TO* tom;
  /// Packet countdown
  int count;
  /// Delta time between packets
  simtime_t delta;
protected:
  virtual void initialize();
  virtual void handleMessage(cMessage *msg);
  int nsize;
  int addr;
public:
  virtual ~Generator();
};

// register module class with `\opp`
Define_Module(Generator)
;

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
  tom = new TO("Send a new packet timeout");
  scheduleAt(simTime(),tom);
  WATCH(count);
}

void Generator::genPack(){
  int dest = intrand(nsize);
  string roba = "To " + IntToStr(dest);
  Pack *msg = new Pack(roba.c_str());
  msg->setDst(dest);
  msg->setSrc(addr);
  msg->setQueue(-1);
  send(msg, "inject$o");
}

void Generator::handleMessage(cMessage *msg){
  // if NAK, add another message to count
  if (dynamic_cast<Nak*>(msg) != NULL){
    ++count;
    delete msg;
  }
  // if timeout, send a message
  else if (dynamic_cast<TO*>(msg) != NULL){
    genPack();
    --count;
  }
  // cancel old timeout
  if (tom->isScheduled())
    cancelEvent(tom);
  // schedule next packet
  if ((count)>0)
    scheduleAt(simTime()+delta,tom);
  // cout << simTime() << endl;
}
