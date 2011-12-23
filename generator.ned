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

// Packet generator
simple Generator
{
    parameters:
        int count = default(10);
        // delta to generate a new packet (in saturation units)
        double deltaG = default(1);
        // delta to send a new packet (in saturation units)
        double deltaS = default(1);
        // Packet length
        int packLen @unit(B) = default(2 KiB);        
        // number of packet to be produced per burst
        int packNum = default(10);
        // communication pattern -- 0=uniform, 1,2=nodes at distance <= 1,2, 5=fixed permutation, 10=butterfly, 20=transposition, 30=tornado, 40=bitcomplement
        int commPatt = default(0);
        // generator random distribution -- 0=deterministic, 1=exponential, 2=nomrmal(av,av/10)
        int rantype= default(0);
        @display("i=block/source");
    gates:
        inout inject;
        input reinj; // reinject gate
}
