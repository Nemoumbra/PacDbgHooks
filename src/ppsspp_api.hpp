#pragma once

#include "ppsspp_api/pac.hpp"
#include "ppsspp_api/mips.hpp"
#include "ppsspp_api/Log.h"
#include "bindings.hpp"

struct EmuApi {
private:
    EmuFuncs funcs;

public:
    void LoadFuncs(EmuFuncs &from_PPSSPP) { funcs = from_PPSSPP; }

    u8 ReadU8(u32 address) { return funcs.ReadOrException_U8(address); }
    u16 ReadU16(u32 address) { return funcs.ReadOrException_U16(address); }
    u32 ReadU32(u32 address) { return funcs.ReadOrException_U32(address); }
    u64 ReadU64(u32 address) { return funcs.ReadOrException_U64(address); }

    u8 ReadUncheckedU8(u32 address) { return funcs.ReadUnchecked_U8(address); }
    u16 ReadUncheckedU16(u32 address) { return funcs.ReadUnchecked_U16(address); }
    u32 ReadUncheckedU32(u32 address) { return funcs.ReadUnchecked_U32(address); }
    u64 ReadUncheckedU64(u32 address) { return funcs.ReadUnchecked_U64(address); }
    float ReadUncheckedFloat(u32 address) { return funcs.ReadUnchecked_Float(address); }

    void WriteU8(u8 value, u32 address) { funcs.WriteOrException_U8(value, address); }
    void WriteU16(u16 value, u32 address) { funcs.WriteOrException_U16(value, address); }
    void WriteU32(u32 value, u32 address) { funcs.WriteOrException_U32(value, address); }
    void WriteU64(u64 value, u32 address) { funcs.WriteOrException_U64(value, address); }

    void WriteUncheckedU8(u8 value, u32 address) { funcs.WriteUnchecked_U8(value, address); }
    void WriteUncheckedU16(u16 value, u32 address) { funcs.WriteUnchecked_U16(value, address); }
    void WriteUncheckedU32(u32 value, u32 address) { funcs.WriteUnchecked_U32(value, address); }
    void WriteUncheckedU64(u64 value, u32 address) { funcs.WriteUnchecked_U64(value, address); }
    void WriteUncheckedFloat(float value, u32 address) { funcs.WriteUnchecked_Float(value, address); }

    u8 *GetPtr(u32 address) { return funcs.GetPointerWriteOrException(address); }
    const u8 *GetConstPtr(u32 address) { return funcs.GetPointerOrException(address); }
    u8 *GetUncheckedPtr(u32 address) { return funcs.GetPointerWriteUnchecked(address); }
    const u8 *GetUncheckedConstPtr(u32 address) { return funcs.GetPointerUnchecked(address); }

    const u8 *GetPtrOrNull(u32 address) { return funcs.GetPointerOrNull(address); }

    u8 *GetRangePtr(u32 address, u32 size) { return funcs.GetPointerWriteRangeOrException(address, size); }
    const u8 *GetConstRangePtr(u32 address, u32 size) { return funcs.GetPointerRangeOrException(address, size); }

    const char *GetCharPtr(u32 address) { return funcs.GetCharPointer(address); }
    const char *GetUncheckedCharPtr(u32 address) { return funcs.GetCharPointerUnchecked(address); }

    bool IsRamAddr(u32 address) { return funcs.IsRAMAddress(address); }
    bool IsSramAddr(u32 address) { return funcs.IsScratchpadAddress(address); }
    bool IsVramAddr(u32 address) { return funcs.IsVRAMAddress(address); }
    bool IsDepthTexVramAddr(u32 address) { return funcs.IsDepthTexVRAMAddress(address); }
    bool IsMMIOAccessAddr(u32 address) { return funcs.IsMMIOAccess(address); }
    bool IsKernelAddr(u32 address) { return funcs.IsKernelAddress(address); }
    bool IsKernelNonvolatileAddr(u32 address) { return funcs.IsKernelAndNotVolatileAddress(address); }

    bool IsValidAddr(u32 address) { return funcs.IsValidAddress(address); }
    bool IsValid2AlignedAddr(u32 address) { return funcs.IsValid2AlignedAddress(address); }
    bool IsValid4AlignedAddr(u32 address) { return funcs.IsValid4AlignedAddress(address); }

    bool IsValidRange(u32 address, u32 size) { return funcs.IsValidRange(address, size); }
    bool IsValid4AlignedRange(u32 address, u32 size) { return funcs.IsValid4AlignedRange(address, size); }

    void MemcpyUnchecked(void *to, u32 from, u32 size) { return funcs.MemcpyUnchecked(to, from, size); }
    void MemcpyUnchecked(u32 to, void *from, u32 size) { return funcs.MemcpyUnchecked(to, from, size); }
    void MemcpyUnchecked(u32 to, u32 from, u32 size) { return funcs.MemcpyUnchecked(to, from, size); }

    void EnterStepping(MIPSState *mips, BreakReason reason, u32 relatedAddress) {
        funcs.EnterStepping(mips, reason, relatedAddress);
    }
    CoreState GetCoreState() { return funcs.GetCoreState(); }
    bool IsStepping() { return funcs.Core_IsStepping(); }

    void EmuLog(Log type, LogLevel level, const char *msg) { funcs.EmuLog(type, level, msg); }
    void OutputDebugStringUTF8(const char *msg) { funcs.OutputDebugStringUTF8(msg); }
};