#ifndef CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP
#define CORE_LIBS_PRX_LIBKERNEL_DIRECTMEMORY_DIRECTMEMORY_HPP

#include <cstdint>
#include <cstddef>
#include <mutex>

#include "prx/libkernel/KernelErrors.hpp"

#ifdef RTY_PS4_MEMORY
// Base PS4: 5248 MiB in total, of which 448 MiB is flexible memory.
static constexpr size_t DIRECT_MEMORY_SIZE = (5248ULL - 448ULL) * 1024 * 1024;
#else
static constexpr size_t DIRECT_MEMORY_SIZE = 13824ULL * 1024 * 1024;
#endif
static constexpr size_t PS5_PAGE_SIZE = 0x4000;

int DirectMemoryAlloc(int64_t searchStart, int64_t searchEnd, size_t len, size_t alignment, int memoryType, int64_t* physOut);
void DirectMemoryFree(int64_t start, size_t len);
void CreateDirectMemoryBacking(int64_t start, size_t len, int memoryType);
bool QueryDirectMapping(std::uintptr_t address, std::uintptr_t* start, std::uintptr_t* end, std::uint64_t* offset, int* memoryType);
void ForgetDirectMemory(int64_t start, size_t len);
void UnmapDirectMemory(int64_t start, size_t len);
bool DirectMemoryFind(int64_t offset, bool findNext, int64_t* start, int64_t* end, int* memoryType);
size_t DirectMemoryFreeRun(uint64_t offset, uint64_t limit);
int DoMapDirect(void** addr, size_t len, int prot, int flags, int64_t physStart, size_t alignment);
// PERF-GT7-090: one part of a run of consecutive fixed direct mappings (sceKernelBatchMap2).
struct DirectMapPart {
    size_t length;
    int64_t phys;
};
// Maps the fixed range [addr, addr + len) as consecutive parts, each to its own physical range,
// as one DoMapDirect per part would, but with one registration, one protection record and one
// report to the renderer for the whole run. Returns 1, having mapped nothing, when the run cannot
// be taken as a whole (its parts then go one by one); 0 when mapped.
// coalesce: physically contiguous parts become one native view (else one view per part).
int DoMapDirectRun(void* addr, size_t len, int prot, int flags, const DirectMapPart* parts, size_t count, bool coalesce);
// Whether the converted game is GT7.exe (the rule the renderer uses, shadGT game_title.h). For the
// few defaults measured per game; Process.cpp (in its extern "C" block).
extern "C" bool RunningGt7();
// PERF-GT7-092 (RYGT_DEFER_UNMAP=0 off): DoMunmap whose native views go later, on a worker thread.
int DoMunmapDeferred(void* addr, size_t len);
bool DeferUnmapEnabled();
// Finishes the pending unmaps over the range now; call with the GuestAllocations::Mutation held.
void FlushPendingUnmaps(const void* addr, size_t len);
bool PendingUnmapContains(std::uintptr_t address);
void NoteBatchMapActivity();
int DoMapAnon(void** addr, size_t len, int prot, int flags);
int DoMprotect(const void* addr, size_t len, int prot);
int DoMunmap(void* addr, size_t len);
int DoReserveVirtual(void** addr, size_t len, int flags, size_t alignment);
bool GuestProtection(uintptr_t addr, int* prot);

// PERF-GTS-265: tells the renderer once that a run of adjacent fixed direct maps replaces
// [addr, addr + len), and holds the mapping transition while the run is mapped; DoMapDirect on
// this thread does not tell it again for each piece inside the range.
class FixedReplacementNotice {
public:
    FixedReplacementNotice(void* addr, size_t len, std::size_t entries);
    ~FixedReplacementNotice();
    FixedReplacementNotice(const FixedReplacementNotice&) = delete;
    FixedReplacementNotice& operator=(const FixedReplacementNotice&) = delete;

private:
    std::lock_guard<std::recursive_mutex> transition;
    void* start;
    size_t bytes;
};

// PERF-GTS-268: whether one registered guest allocation holds all of [addr, addr + len), so a
// fixed direct map of it is remapped in place (only with RYGT_BATCH_MAP_MERGE=1).
bool FixedRangeInOneAllocation(const void* addr, size_t len);

#endif