# Adreno740

Native XNU Adreno driver. Owns GMU startup, the CP ring, BOs, native 4 KiB SMMU contexts, fences and cache maintenance.

Current state on sheng (Adreno 740): GPU commands and the scanout copy run on hardware. The same driver carries an Adreno 732 (SM7675, `gen71100` firmware) profile for uke; its SoC-specific values are selected from the GPU node's `qcom,chipid` and `reg-names`, but the uke path has not run on a device yet.

Two priority rings preserve queue FIFO and per-ring page tables. Context switching uses the executed-table snapshot, GMEM save/restore and a preemption postamble. The host scheduling check covers packet sequencing, priority changes and fence-history rollover; board policy selects the mode.

Build and interface details: [documentation](https://unixnu.com/drivers/adreno740). Component sources and dependencies: [catalog](https://unixnu.com/reference/components).

## Frame diagnostics

`sheng-gpu-trace=1` enables the kernel GPU ring: submission, retirement, display arrival, display fence and latch, build identity, and the render server's render, encode and completion thread identities, 65536 records. Repeated pending display queries are coalesced. See [GPU frame diagnostics](https://unixnu.com/porting/gpu-diagnostics) for fields and decoding.

`NativeGpuPerfCounters.h` defines the opt-in 19-counter A7XX BR plan used by sheng diagnostics. Capture records selector readback, unstable 64-bit reads, reset masks and preemption epochs. The host check verifies the register plan against pinned Mesa XML.
