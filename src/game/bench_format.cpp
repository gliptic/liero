#include <gvl/system/system.hpp>

typedef int32_t soffset_t;

struct VTable {
	uint16_t entries[0];
};

struct TableLayout {
	soffset_t vtable;
	uint8_t   data[0];
};

struct List {
	uint16_t elementSize;
	uint16_t elementCount;

};
