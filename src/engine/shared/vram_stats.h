#ifndef ENGINE_SHARED_VRAM_STATS_H
#define ENGINE_SHARED_VRAM_STATS_H

#include <atomic>
#include <cstdint>

extern std::atomic<uint64_t> g_VramFallbackBytes;
extern std::atomic<uint64_t> g_VramFallbackCount;
extern std::atomic<uint64_t> g_VramFallbackBytesByUsage[4];

#endif
