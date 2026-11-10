# Adreno740

Native XNU Adreno driver. Owns GMU startup, the CP ring, BOs, native 4 KiB SMMU contexts, fences and cache maintenance.

Current state on sheng (Adreno 740): GPU commands and the scanout copy run on hardware. The same driver carries an Adreno 732 (SM7675, `gen71100` firmware) profile for uke; its SoC-specific values are selected from the GPU node's `qcom,chipid` and `reg-names`, the current iPadOS 27 integration awaits device qualification. The profile enables CP preemption with a 2196 KiB record, 1.5 MiB GMEM and the board GMU counter offset. IFPC remains disabled by the startup policy.

Two priority rings preserve queue FIFO and per-ring page tables. Context switching uses the executed-table snapshot, GMEM save/restore and a preemption postamble. The host scheduling check covers packet sequencing, priority changes and fence-history rollover; board policy selects the mode.

Build and interface details: [documentation](https://unixnu.com/drivers/adreno740). Component sources and dependencies: [catalog](https://unixnu.com/reference/components).

## Fence waits

A waiter sleeps until the completion pass finds its fence written on the ring that issued it; its only timeout is the caller's deadline. The board's GPU interrupt handler asks for the pass through `native_adreno_gpu_interrupt(status)`; the pass retires finished jobs, looks at the switch between the two rings again and wakes the waiters. The hang check runs one pass per period. A wait that pauses under the driver's lock drives the ring switch itself. `gpu-fence-wake=0` keeps the board's timed pause for the waiters. Counters: `native_adreno_fence_wake`.

## CPU cache maintenance

The GPU maps its own memory non-cacheable, so it sees what DRAM holds. User BOs are write-combined for the CPU. The GPU context's eleven kernel buffers (SQE, rings, memptrs, preemption records, 8548 KiB) are declared synchronized (`NativeAdrenoMemory::declareSynchronized`): their users write them through a non-cacheable view or clean what they stored, so a map, an unmap and a submission leave them alone. The first 8 submissions and every 1024th compare the head of each buffer as a CPU cache and DRAM hold it; a dirty line ends that trust and the buffers are cleaned at every submission again. A submission, a completion and the write-combine re-assertion visit only the mappings they have work for. `gpu-maint-all=1` visits every mapping and maintains the synchronized buffers like any other memory, for a comparison. Counters: `native_adreno_maint_kind` (maintenance at submissions by kind of mapping), `native_adreno_maint_walk` (mappings visited), `native_adreno_sync_audit`, `native_adreno_sync_skips`, `native_adreno_submit_us` (a submission's time by phase). The submission log reads the first IB's words only with `diag=1`.

## Memory accounting

`native_adreno_bo_owners` and `native_adreno_bo_sizes` count the system BOs by allocating process and by size. `native_adreno_import_memory` and `native_adreno_import_owners` count the imported memory the driver keeps wired, in all and by importing process. `native_adreno_view_owners` counts the BO views each process maps; the VM bills a view to the mapping task by its size.

## Pool of DRAM outside the kernel's

A board whose kernel manages only a window of its DRAM registers the free runs outside that window with `native_adreno_pool_add(physical, bytes)`: whole pages, entirely outside `[gPhysBase, gPhysBase+gPhysSize)`, from any thread once IOKit runs, before or after the driver starts. A write-combined system BO then takes its pages from the pool instead of wiring the window's memory: one run that fits best, else the longest runs first, at most sixteen runs up to 1 MiB and one a 64 KiB above. Whatever the pool does not serve takes the descriptor path: no range, a BO that is not write-combined, a request above the client's physical address limit, not enough free pages, or free pages too scattered.

The kernel has no `vm_page` and no cacheable mapping for these pages. Each range has one write-combined kernel mapping; every page is zero-filled through it before a BO gets it. A pool BO has no descriptor, is never cleaned or audited, and its client view maps its physical runs write-combined. The BO and each client view hold the pages; an import of a range that maps pool pages is refused. Pages a client had a view of do not return to the pool, because a copy of that mapping made in the VM cannot be seen or revoked; `gpu-pool=2` returns them after dropping their cache lines. `gpu-pool=0` turns the pool off.

Counters: `native_adreno_pool_state` (ranges, KiB total, free and lowest free, grants alive and their KiB, served, freed, retired and their KiB, the boot argument, refused registrations), `native_adreno_pool_fallbacks` (by reason), `native_adreno_pool_zero` (zero-fill and cache-pass time and KiB).

## Frame diagnostics

`gpu-trace=1` enables the kernel GPU ring: submission, retirement, display arrival, display fence and latch, build identity, and the render server's render, encode and completion thread identities, 65536 records. Repeated pending display queries are coalesced. See [GPU frame diagnostics](https://unixnu.com/porting/gpu-diagnostics) for fields and decoding.

`native_adreno_stall_ring` holds, with each stall snapshot, the ring of the waited fence, the ring the CP runs, a switch in flight and the unfinished jobs of each ring.

`NativeGpuPerfCounters.h` defines the opt-in 19-counter A7XX BR plan used by sheng diagnostics. Capture records selector readback, unstable 64-bit reads, reset masks and preemption epochs. The host check verifies the register plan against pinned Mesa XML.


Recovery transitions retain a concise kernel-log record with the hung fence, restart result, replay/guilty flags, preemption recovery words and latched SMMU fault. An explicit NUL on the existing plugin marker text also persists bounded refusal summaries before the client exits. Request structures, recovery behavior and scheduling are unchanged; frame/pixel capture stays opt-in.


## Uke iPadOS 27 acceptance (2026-11-03)

The gen7_11_0/0x43030b00 port is part of the user-accepted uke 24A446 GPU baseline `90B6BC701210D9DB`: prior boot, icons/widgets, Settings and Dock UI-restart symptoms are reported resolved. Per-process VM, mode-2 dual-ring preemption, FIFO/priority and recovery remain enabled. This report does not independently quantify frame times, concurrent-client stress or repeated reboot cache reuse.


## Optional native-XNU page preparation (2026-11-04 candidate)

A matching native-XNU call adapter can invoke `native_adreno_prepare_external` before taking IOUserClient's mapping-exclusion lock and pass its private context to the unchanged locked dispatch. Only eligible system BoAlloc pages are prepared; the core rechecks liveness/limits before adoption. Unadopted pages and the faultable allocation diagnostic stack walk are cleaned up through `native_adreno_finish_external` after leaving that lock. An active IPC pin spans prepare/dispatch/cleanup. Default IOKit locking, map/unmap/close and request ABI stay enabled; without the adapter the original path remains. Allocation trace flag16 identifies preflight.

This addresses uke's measured114ms map lock-wait mechanism in a host-checked, built candidate; hardware gains and120Hz are not qualified. Workspace evidence: docs/status/gpu-prepare-outside-iokit-2026-11-04.md and uke-port/fixes/2026-11-04-gpu-prepare-outside-iokit/.


The native preflight adapter is runtime-gated: `native_adreno_preflight_ready` stays0 until GPU publication initializes its classes, lock and context, then becomes1 with release/acquire ordering. Before readiness, the XNU wrapper does not enter its new IPC pin and the driver does not cast unrelated early-boot clients. `native_adreno_preflight_entries` records actual entry; preflight/adoption/lifecycle behavior stays intact. Uke DACD0576367904C3 is built and flashed for hardware verification; the complete cause of the earlier SystemOS timeout and120Hz gains remain unqualified.


The same runtime-gated private preflight context also handles nonzero-timeout WaitFence calls outside IOUserClient's mapping-exclusion lock. Existing fence validation, timeout, retirement and recovery execute unchanged; the locked consumer rechecks request/client/liveness and returns the actual result once. Successful close is published with release ordering and observed by the waiting loop. Zero-timeout polls remain on the original path. The board export can report native_adreno_wait_preflight calls/elapsed/max/non-success (including timeout). This targets the DACD hardware trace of WaitSemaphores holding a reader while CPU mapping waits61ms; the next candidate's hardware benefit remains unqualified.

Both preflight allocations explicitly assign `NativeAdrenoPreparedAlloc{}` after `IONew`. IOKit allocation does not invoke C++ member initializers: zero-filled storage alone leaves `method` at0 instead of BoAlloc2. The wait-version hardware trace confirmed all58 large allocations fell back inside the original lock (flags11), including162ms of mapping exclusion. The corrected host check uses raw zero-filled allocation and rejects that fallback; the old implementation fails this control. The four-minute UI freeze remains unqualified until device verification.

With sheng-diag enabled, the first eight detected hangs save CP registers before restart, plus separately labeled prior stall/IB/switch buffers to the kernel log. diagnosticState reports read failure without poisoning a ring; the existing stallState failure behavior remains. Recovery, submission, timeouts and normal-frame sampling are unchanged. This observation candidate targets the measured post-lock-fix GPU recoveries; it is not a latency fix.

The lightweight execution-counter variant uses `sheng-gpu-trace=2` with `sheng-diag=2`. Events 66–68 carry 21 BR counter values and the window validity fields without inserting per-pass GPU timestamp packets.

The expanded lightweight variant adds ten BR SP counters in slots7..16 through
RBBM_PERFCTR_SP: UCHE/RB backpressure, wave context/wait and long/short sync,
local-memory instructions, FS half ALU and EFU working. Event69 appends them
with the explicit BR scope marker0x4252001f; events66–68 and the original21 values stay unchanged.
The decoder accepts both captures and rejects windows crossing counter plans.
Sampling remains about32ms with the existing reset/preemption/vote validity
checks; no per-pass GPU timestamps are added. This is diagnostic classification,
not a measured performance fix.

The old marker31 used SP24..33, which a7xx_perfcntrs.json assigns to BV.
It is decoded as bv_legacy and excluded from BR attribution. Entirely flat
extra counters during shader activity fail the new activity check. XML address
existence and selector readback alone never establish the pipeline scope.

## Uke upstream synchronization (2026-11-11 candidate)

The Uke integration merges upstream through 9d99240, retaining the A732 board
profile, runtime-gated IOKit preflight and BR trace mode 2. Pool grants participate
in preflight ownership and cleanup through `NativeAdrenoSystemPages::held()`;
the physical output-width admission applies before pool preparation. Allocation
trace flag16 retains its Uke preflight meaning, with flag32 recording a pool grant.
Completion-pass fence wakeups, ring switching, synchronized-buffer maintenance,
mailbox sleep and owner/view counters follow upstream. Build and hardware results
are recorded separately in the workspace's CPU/GPU synchronization report.
