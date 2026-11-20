/* SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

// The consumer takes a claim once on the compositor's display thread.
// A task, thread, surface or time mismatch cannot leave a claim armed.
class NativeScanoutClaims {
    struct Claim {
        uint64_t task=0, thread=0, at=0, command=0;
        uint32_t surface=0, format=0, width=0, height=0;
        NativeGpuScanoutFence fence{};
    };
    Claim claims_[32]{};
    Claim &slot(uint64_t thread) {return claims_[thread&31];}
public:
    void reset() {for(auto &claim:claims_)claim={};}
    void clear(uint64_t task,uint64_t thread)
    {
        auto &claim=slot(thread);
        if(claim.task==task && claim.thread==thread)claim={};
    }
    void put(uint64_t task,uint64_t thread,uint64_t at,uint64_t command,
             uint32_t surface,uint32_t format,uint32_t width,uint32_t height,NativeGpuScanoutFence fence)
    {
        slot(thread)={task,thread,at,command,surface,format,width,height,fence};
    }
    bool take(uint64_t task,uint64_t thread,uint64_t now,uint32_t surface,
              uint32_t format,uint32_t width,uint32_t height,NativeGpuScanoutFence &out)
    {
        const Claim claim=slot(thread);
        if(claim.task!=task || claim.thread!=thread || !claim.command)return false;
        slot(thread)={};
        if(claim.surface!=surface || claim.format!=format || claim.width!=width ||
           claim.height!=height || now<claim.at || now-claim.at>2000)return false;
        out=claim.fence;
        return true;
    }
};
