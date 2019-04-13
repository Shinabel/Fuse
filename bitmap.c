#include "bitmap.h"

int 
bitmap_get(void* bm, int ii) {
	int byte = ii / 8;
	int bit = ii % 8;
	char* bms = bm;
	int b = ((bms[byte] >> 7 - bit) & 1);
	return b;
}

void 
bitmap_put(void* bm, int ii, int vv) {
	int byte = ii / 8;
	int bit = ii % 8;
	((char*)bm)[byte] |= vv << 7 - bit;
}
