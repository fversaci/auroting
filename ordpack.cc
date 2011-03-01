#include "ordpack.h"

bool operator<(const OrdPack& o1, const OrdPack& o2){
	return (o1.ord < o2.ord);
}

ostream& operator<<(ostream& os, const OrdPack& o){
	return os << o.p;
}
