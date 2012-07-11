//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

#ifndef __AUROUTING_CLOCK_GEN_H_
#define __AUROUTING_CLOCK_GEN_H_

#include <omnetpp.h>
#include "pack_m.h"

/**
 * TODO - Generated class
 */
class Clock_gen : public cSimpleModule
{
private:
    /// Timeout message for generating new message
    TO2* tog;
    /// Packets generation count-down
    int count;
    /// Delta time between packets generation
    simtime_t deltaG;
    /// generator random distribution -- 0=deterministic, 1=exponential, 2=nomrmal(av,av/10)
    int rantype;
    /// send out message generation requests
    void sendout();
    /// network size
    int nsize;
    /// random distribution for packet generation given some average value
    inline simtime_t randist(simtime_t av);
protected:
    virtual void initialize();
    virtual void handleMessage(cMessage *msg);
public:
    virtual ~Clock_gen();
};

#endif
