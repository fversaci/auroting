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

package _3buf;

// Bubble Router
simple BRouter
{
    parameters:
        int EscQueueSize; // size of escape (bubble) queues
        int AdapQueueSize; // size of adaptive queues
        int InOutQueueSize; // size of queues to and from the CPU
        int algorithm = default(0); // 0=plain bubble, 1=outflank, 2=CQR, 3=OF&CQR
        int ofEdge = default(2); // Edge of outflank cube
        double jolly = default(1.0); // jolly parameter for simulations 
        int jollyint = default(4); // integer jolly parameter for simulations 
    @display("i=block/routing");
    gates:
        inout inject;
        output consume;
        // gates: 0=x+ 1=x- 2=y+ 3=y- 4=z+ 5=z-
        inout gate[6];
}
