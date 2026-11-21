#include "ppsspp_api/mips.hpp"
#include "ppsspp_api/Log.h"

namespace func_sig {
using u8ptr_u32 = u8 *(*)(u32);
using cu8ptr_u32 = const u8 *(*)(u32);
using u8ptr_u32u32 = u8 *(*)(u32, u32);
using cu8ptr_u32u32 = const u8 *(*)(u32, u32);

using b_u32 = bool (*)(u32);
using b_u32u32 = bool (*)(u32, u32);

using u8_u32 = u8 (*)(u32);
using u16_u32 = u16 (*)(u32);
using u32_u32 = u32 (*)(u32);
using u64_u32 = u64 (*)(u32);
using f32_u32 = float (*)(u32);

using v_u8u32 = void (*)(u8, u32);
using v_u16u32 = void (*)(u16, u32);
using v_u32u32 = void (*)(u32, u32);
using v_u64u32 = void (*)(u64, u32);
using v_f32u32 = void (*)(float, u32);

using BreakFunc = void (*)(MIPSState *, BreakReason, u32);

using EmuLogFunc = void (*)(Log, LogLevel, const char *);
using OutputDebugStrFunc = void (*)(const char *);

using HandleAssertFunc = ATTR_FORMAT_PRINTF(6, 7) bool (*)(
    bool isDebugAssert, const char *function, const char *file, int line, const char *expression,
    MSVC_FORMAT_PRINTF const char *format, ...);

// TODO: HandleAssert

} // namespace func_sig

struct EmuFuncs {
    ///////////////////////////////////////////////////////////////////////////////////////////////////

    // slower psp -> host ptrs
    func_sig::u8ptr_u32 GetPointerWriteOrException;
    func_sig::cu8ptr_u32 GetPointerOrException;

    // faster psp -> host ptrs
    func_sig::u8ptr_u32 GetPointerWriteUnchecked;
    func_sig::cu8ptr_u32 GetPointerUnchecked;

    // psp -> host ptrs to span
    func_sig::u8ptr_u32u32 GetPointerWriteRangeOrException;
    func_sig::cu8ptr_u32u32 GetPointerRangeOrException;

    // ptr tests
    func_sig::b_u32 IsRAMAddress;
    func_sig::b_u32 IsScratchpadAddress;

    // ptr validity
    func_sig::b_u32 IsValidAddress;
    func_sig::b_u32 IsValid2AlignedAddress;
    func_sig::b_u32 IsValid4AlignedAddress;

    // range validity
    func_sig::b_u32u32 IsValidRange;
    func_sig::b_u32u32 IsValid4AlignedRange;

    // slower reads
    func_sig::u8_u32 ReadOrException_U8;
    func_sig::u16_u32 ReadOrException_U16;
    func_sig::u32_u32 ReadOrException_U32;
    func_sig::u64_u32 ReadOrException_U64;

    // slower writes
    func_sig::v_u8u32 WriteOrException_U8;
    func_sig::v_u16u32 WriteOrException_U16;
    func_sig::v_u32u32 WriteOrException_U32;
    func_sig::v_u64u32 WriteOrException_U64;

    // faster reads
    func_sig::u8_u32 ReadUnchecked_U8;
    func_sig::u16_u32 ReadUnchecked_U16;
    func_sig::u32_u32 ReadUnchecked_U32;
    func_sig::u64_u32 ReadUnchecked_U64;
    func_sig::f32_u32 ReadUnchecked_Float;

    // faster writes
    func_sig::v_u8u32 WriteUnchecked_U8;
    func_sig::v_u16u32 WriteUnchecked_U16;
    func_sig::v_u32u32 WriteUnchecked_U32;
    func_sig::v_u64u32 WriteUnchecked_U64;
    func_sig::v_f32u32 WriteUnchecked_Float;

    func_sig::BreakFunc EnterStepping;

    func_sig::EmuLogFunc EmuLog;
    func_sig::OutputDebugStrFunc OutputDebugStringUTF8;

    // TODO: pass HandleAssert

    ///////////////////////////////////////////////////////////////////////////////////////////////////

    inline bool IsVRAMAddress(const u32 address) { return ((address & 0xBF800000) == 0x04000000); }
    inline bool IsDepthTexVRAMAddress(const u32 address) {
        return ((address & 0xBFE00000) == 0x04200000) || ((address & 0xBFE00000) == 0x04600000);
    }

    // Kernel segment 1 (0xA0000000-0xBFFFFFFF): the hardware registers from 0xBC000000, and below them
    // uncached RAM and VRAM, which only the Media Engine's space maps. Kernel code reaches the rest
    // through MMIO::Read/Write.
    inline bool IsMMIOAccess(const u32 address) { return (address & 0xE0000000) == 0xA0000000; }

    // 0x08000000 -> 0x08800000
    inline bool IsKernelAddress(const u32 address) { return ((address & 0x3F800000) == 0x08000000); }

    // 0x08000000 -> 0x08400000
    inline bool IsKernelAndNotVolatileAddress(const u32 address) { return ((address & 0x3FC00000) == 0x08000000); }

    inline const char *GetCharPointerUnchecked(const u32 address) { return (const char *)GetPointerUnchecked(address); }

    // Used for auto-converted char * parameters, which can sometimes legitimately be null -
    // so we don't want to get caught in GetPointer's crash reporting
    // TODO: This should use IsValidNullTerminatedString, but may be expensive since this is used so much - needs
    // evaluation.
    inline const char *GetCharPointer(const u32 address) {
        if (address && IsValidAddress(address)) {
            return GetCharPointerUnchecked(address);
        } else {
            return nullptr;
        }
    }

    // Like GetPointer, but bad values don't result in a memory exception, instead nullptr is returned.
    inline const u8 *GetPointerOrNull(const u32 address) {
        return IsValidAddress(address) ? GetPointerUnchecked(address) : nullptr;
    }

    inline void MemcpyUnchecked(void *to_data, const u32 from_address, const u32 len) {
        memcpy(to_data, GetPointerUnchecked(from_address), len);
    }

    inline void MemcpyUnchecked(const u32 to_address, const void *from_data, const u32 len) {
        memcpy(GetPointerWriteUnchecked(to_address), from_data, len);
    }

    inline void MemcpyUnchecked(const u32 to_address, const u32 from_address, const u32 len) {
        MemcpyUnchecked(GetPointerWriteUnchecked(to_address), from_address, len);
    }
};