#ifndef ORDPACK_H
#define ORDPACK_H

//#include "pack_m.h"
using namespace std;

/// Packet with a time priority (bigger=older)
class OrdPack{
public:
	/// Packet
	void* p;
	/// Time priority (bigger=older)
	long ord;
	/// Comparison based on priority
	friend bool operator<(const OrdPack& o1, const OrdPack& o2);
	/// Costructors
	OrdPack(void* pp, long oo) : p(pp), ord(oo) {};
	OrdPack() {};
};

#endif
