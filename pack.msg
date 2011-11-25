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

//
// TODO generated message class

// Generic packet
packet Pack {
    int src; // source
    int dst; // destination
    int hops = 0; // hop count
    int route[5]; // intermediate destinations (0-th is the final one)
    int currDst = 0; // dst=route[currDst]
    bool derouted = false; // set to true if gets derouted by outflank
    bool reinjectable = false; // true if just reached an intermediate destination
    simtime_t birthtime; // birth time
    int queue = -1; // 0 = A, 1 = B, 2 = C, -1 = injection queue
    string displayString = "b=10,10,oval,blue";
    ByteLength = 1000;
    int cqrdir[3] = 0; // which direction in dimension i? (0=+, 1=-) -- used by CQR 
    bool OFlanked = false;
    bool CQRed = false;
}

// ACK packet
packet Ack {
    long TID; // Packet TreeId
    int queue; // 0 = A, 1 = B, 2 = C, -1 = injection queue
    string displayString = "b=10,10,rect,green";
    int fs[3]; // free space in central queues
    ByteLength = 10;    
}

// NAK packet
packet Nak {
    int TID; // Packet TreeId
    int queue; // 0 = A, 1 = B, 2 = C, -1 = injection queue
    string displayString = "b=10,10,rect,red";
    int fs[3]; // free space in central queues
    ByteLength = 10;    
}

// Timeout message
message TO {
}

// Timeout message 2
message TO2 {
}

// Notify message
message NoM {
    int cow; // counter increment
    int hops; // hops increment 
    int der; // derouted increment
    int cqr; // CQRed packets
    int ofl; // OFed packets
    simtime_t birthtime; // birth time
}