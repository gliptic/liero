//#include "asm.h"

#include <stddef.h>
#include "tl/platform.h"

#if TL_WINDOWS

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOMINMAX
#include <windows.h>

#define MCPROT_RW       PAGE_READWRITE
#define MCPROT_RX       PAGE_EXECUTE_READ
#define MCPROT_RWX      PAGE_EXECUTE_READWRITE

void* mcode_alloc(size_t sz, int prot) {
	void* p = VirtualAlloc(NULL, sz,
  		MEM_RESERVE|MEM_COMMIT|MEM_TOP_DOWN, MCPROT_RWX);
	return p;
}

void* mem_commit_exec(void* p, size_t s) {
	void* code = VirtualAlloc(p, s,
		MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	return (char*)code + s;
}

void* mem_commit_data(void* p, size_t s) {
	void* data = VirtualAlloc(p, s,
		MEM_COMMIT, PAGE_READWRITE);
	return (char*)data + s;
}

void* mem_reserve() {
	SIZE_T space = 0x100000000ull;
	SIZE_T range = 100 * (1ull << 30);
	void* guard = VirtualAlloc(NULL, range,
		MEM_RESERVE, PAGE_NOACCESS);
	printf("%p, %d\n", guard, GetLastError());

	char* ptr = (char*)guard + range / 2;

	return ptr;

}

#elif TL_LINUX

#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define MCPROT_RW       (PROT_READ|PROT_WRITE)
#define MCPROT_RX       (PROT_READ|PROT_EXEC)
#define MCPROT_RWX      (PROT_READ|PROT_WRITE|PROT_EXEC)

void* mcode_alloc(size_t sz, int prot) {
	void* p = mmap(NULL, sz, MCPROT_RWX, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	return p;
}

#else

void* mcode_alloc(size_t sz, int prot) {
	void* p = malloc(sz);
	return p;
}

#endif
