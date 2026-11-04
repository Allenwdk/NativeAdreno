# Adreno740

Native XNU Adreno driver. Owns GMU startup, the CP ring, BOs, native 4 KiB SMMU contexts, fences and cache maintenance.

Current state on sheng (Adreno 740): GPU commands and the scanout copy run on hardware. The same driver carries an Adreno 732 (SM7675, `gen71100` firmware) profile for uke; its SoC-specific values are selected from the GPU node's `qcom,chipid` and `reg-names`, the current iPadOS 27 integration awaits device qualification. The profile enables CP preemption with a 2196 KiB record, 1.5 MiB GMEM and the board GMU counter offset. IFPC remains disabled by the startup policy.

Two priority rings preserve queue FIFO and per-ring page tables. Context switching uses the executed-table snapshot, GMEM save/restore and a preemption postamble. The host scheduling check covers packet sequencing, priority changes and fence-history rollover; board policy selects the mode.

Build and interface details: [documentation](https://unixnu.com/drivers/adreno740). Component sources and dependencies: [catalog](https://unixnu.com/reference/components).

## Frame diagnostics

`sheng-gpu-trace=1` enables the kernel GPU ring: submission, retirement, display arrival, display fence and latch, build identity, and the render server's render, encode and completion thread identities, 65536 records. Repeated pending display queries are coalesced. See [GPU frame diagnostics](https://unixnu.com/porting/gpu-diagnostics) for fields and decoding.

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
