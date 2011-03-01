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

import ned.IdealChannel;


//
// TODO auto-generated type
//
module Node
{
    parameters:
        @display("b=15,15,oval,yellow");
        int addr = default(0);
    gates:
        inout gate[6];
    submodules:
        generator: Generator {
            @display("p=60,56");
        }
        router: Router {
            @display("p=119,154");
        }
        sink: Sink {
            @display("p=179,56");
        }
    connections:
        router.consume --> IdealChannel --> sink.consume;
        generator.inject <--> IdealChannel <--> router.inject;
        for i=0..5 {
            router.gate[i] <--> IdealChannel <--> gate[i];
        }
}
