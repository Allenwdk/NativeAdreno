/* SPDX-License-Identifier: MIT */
#ifndef NATIVE_GPU_PERF_COUNTERS_H
#define NATIVE_GPU_PERF_COUNTERS_H
#include <stdint.h>
/* A7XX BR counters from Mesa a6xx.xml and a7xx_perfcntrs.xml. Slots are reserved
 * in explicit performance captures; selector readback detects another owner. */
struct NativeGpuPerfCounter {uint32_t select,value,low;};
static constexpr NativeGpuPerfCounter NativeGpuPerfPlan[]={
 {0x08dd,0,0x031a}, /* CP_ALWAYS_COUNT, slot 13 */
 {0x0e22,1,0x0382},{0x0e23,2,0x0384},{0x0e24,3,0x0386},
 {0x0e25,4,0x0388},{0x0e26,8,0x038a},{0x0e27,9,0x038c},
 {0xb619,0,0x03a0},{0xb61a,6,0x03a2},{0xb61b,7,0x03a4},
 {0xae91,0,0x03c8},{0xae92,1,0x03ca},{0xae93,4,0x03cc},
 {0xae94,40,0x03ce},{0xae95,51,0x03d0},{0xae96,52,0x03d2},{0xae97,101,0x03d4},
 {0x8e2e,7,0x03f6},{0x8e2f,8,0x03f8},
 {0x8e1b,8,0x0362},{0x8e1c,9,0x0364}, /* CCU GMEM read/write, slots 3 and 4 */
 /* BR SP slots7..16. Slots24..35 belong to BV, not additional BR counters
  * (a7xx_perfcntrs.json); selector readback alone cannot prove activity.
  * v2 (2026-11-05): LM_LOAD_INSTRUCTIONS (countable 27) never left zero on
  * Adreno 732 across 1395 valid windows while LM_STORE did, so slot 13 now
  * counts STALL_CYCLES_VPC (3) instead. The register and the 31-counter
  * layout are unchanged; only that slot's meaning differs from 79EC. */
 {0xae87,5,0x03b4},{0xae88,6,0x03b6},   /* UCHE/RB backpressure */
 {0xae89,9,0x03b8},{0xae8a,20,0x03ba},  /* wave context/wait cycles */
 {0xae8b,24,0x03bc},{0xae8c,25,0x03be}, /* long/short scoreboard sync */
 {0xae8d,3,0x03c0},{0xae8e,28,0x03c2}, /* VPC stall / local memory store */
 {0xae8f,41,0x03c4},{0xae90,2,0x03c6}   /* FS half ALU / EFU working */
};
enum {NativeGpuLegacyPerfCount=19,NativeGpuPerfCount=sizeof(NativeGpuPerfPlan)/sizeof(NativeGpuPerfPlan[0])};
static_assert(NativeGpuPerfCount<=31,"counter validity masks are 32 bits");
struct NativeGpuPerfSample {
 uint64_t values[NativeGpuPerfCount]{};uint32_t mismatch=0,unstable=0,reset=0;bool first=false;
};
struct NativeGpuPerfCounters {
 uint64_t previous[NativeGpuPerfCount]{};bool sampled=false;
 template<unsigned Count=NativeGpuPerfCount,class Write> void configure(Write write) const {
  static_assert(Count<=NativeGpuPerfCount);
  for(unsigned i=0;i<Count;++i)write(NativeGpuPerfPlan[i].select,NativeGpuPerfPlan[i].value);
 }
 template<unsigned Count=NativeGpuPerfCount,class Read> NativeGpuPerfSample capture(Read read) {
  static_assert(Count<=NativeGpuPerfCount);
  NativeGpuPerfSample out{};out.first=!sampled;
  for(unsigned i=0;i<Count;++i) {
   const auto &c=NativeGpuPerfPlan[i];if(read(c.select)!=c.value)out.mismatch|=1u<<i;
   bool stable=false;uint32_t lo=0,hi=0;
   for(unsigned retry=0;retry<3;++retry) {
    hi=read(c.low+1);lo=read(c.low);if(hi==read(c.low+1)){stable=true;break;}
   }
   if(!stable)out.unstable|=1u<<i;
   out.values[i]=(uint64_t)hi<<32|lo;
   if(sampled && out.values[i]<previous[i])out.reset|=1u<<i;
   previous[i]=out.values[i];
  }
  sampled=true;return out;
 }
};
#endif
