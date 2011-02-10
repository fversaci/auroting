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
    int src;
    int dst;
    int queue; // 0 = A, 1 = B, 2 = C, -1 = injection queue
    string displayString = "b=10,10,oval,green";
}

// ACK message
message Ack {
    int queue;
        string displayString = "b=10,10,rect,red";    
}

// Timeout message
message Timeout {
}