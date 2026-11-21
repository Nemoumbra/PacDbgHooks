#pragma once

#include "CommonTypes.h"
#include "ppsspp_config.h"

// Must be set when breaking.
enum class BreakReason {
    None,
    AssertChoice,
    DebugBreak,
    DebugStep,
    DebugStepInto,
    UIFocus,
    AfterFrame,
    MemoryException,
    CpuException,
    BreakInstruction,
    SavestateLoad,
    SavestateSave,
    SavestateRewind,
    SavestateCrash,
    MemoryBreakpoint,
    CpuBreakpoint,
    RegBreakpoint,
    MemoryAccess, // ???
    JitBranchDebug,
    BreakOnBoot,
    RABreak,
    AddBreakpoint,
    FrameAdvance,
    UIPause,
    HLEDebugBreak,
    RunUntilTime,
};

// These work on the MIPSState named mips, which HLE functions get passed (see HLEFunc and FunctionWrappers.h).
#define PARAM(n) mips->r[MIPS_REG_A0 + n]
#define PARAM64(n) (mips->r[MIPS_REG_A0 + n] | ((u64)mips->r[MIPS_REG_A0 + n + 1] << 32))
#define PARAMF(n) mips->f[12 + n]
#define RETURN(n) mips->r[MIPS_REG_V0] = n;
#define RETURN64(n)                                                                                                    \
    {                                                                                                                  \
        u64 RETURN64_tmp = n;                                                                                          \
        mips->r[MIPS_REG_V0] = RETURN64_tmp & 0xFFFFFFFF;                                                              \
        mips->r[MIPS_REG_V1] = RETURN64_tmp >> 32;                                                                     \
    }
#define RETURNF(fl) mips->f[0] = fl

// A fake opcode the CPU cores fetch in place of an instruction with a replacement function (see
// ReplaceTables.h), with the replacement's index in the low bits. It's never in PSP memory. Opcode
// 26, which the Allegrex doesn't have, decodes it as CallRepl.
#define MIPS_CALL_REPLACEMENT_OP 0x6A000000
#define MIPS_REPLACEMENT_INDEX_MASK 0x00FFFFFF
// Another (BranchPair): the IR frontend's way to run a branch with a branch or jump in its delay
// slot, which it leaves to the interpreter. Also never in memory.
#define MIPS_BRANCH_PAIR_OP 0x69000000

enum MIPSGPReg {
    MIPS_REG_ZERO = 0,
    MIPS_REG_COMPILER_SCRATCH = 1,

    MIPS_REG_V0 = 2,
    MIPS_REG_V1 = 3,

    MIPS_REG_A0 = 4,
    MIPS_REG_A1 = 5,
    MIPS_REG_A2 = 6,
    MIPS_REG_A3 = 7,
    MIPS_REG_A4 = 8,
    MIPS_REG_A5 = 9,

    MIPS_REG_T0 = 8, // alternate names for A4/A5
    MIPS_REG_T1 = 9,
    MIPS_REG_T2 = 10,
    MIPS_REG_T3 = 11,
    MIPS_REG_T4 = 12,
    MIPS_REG_T5 = 13,
    MIPS_REG_T6 = 14,
    MIPS_REG_T7 = 15,

    MIPS_REG_S0 = 16,
    MIPS_REG_S1 = 17,
    MIPS_REG_S2 = 18,
    MIPS_REG_S3 = 19,
    MIPS_REG_S4 = 20,
    MIPS_REG_S5 = 21,
    MIPS_REG_S6 = 22,
    MIPS_REG_S7 = 23,
    MIPS_REG_T8 = 24,
    MIPS_REG_T9 = 25,
    MIPS_REG_K0 = 26,
    MIPS_REG_K1 = 27,
    MIPS_REG_GP = 28,
    MIPS_REG_SP = 29,
    MIPS_REG_FP = 30,
    MIPS_REG_RA = 31,

    // Not real regs, just for convenience/jit mapping.
    // NOTE: These are not the same as the offsets the IR has to use!
    MIPS_REG_HI = 32,
    MIPS_REG_LO = 33,
    MIPS_REG_FPCOND = 34,
    MIPS_REG_VFPUCC = 35,

    MIPS_REG_INVALID = -1,
};

enum {
    VFPU_CTRL_SPREFIX,
    VFPU_CTRL_TPREFIX,
    VFPU_CTRL_DPREFIX,
    VFPU_CTRL_CC,
    VFPU_CTRL_INF4,
    VFPU_CTRL_RSV5,
    VFPU_CTRL_RSV6,
    VFPU_CTRL_REV,
    VFPU_CTRL_RCX0,
    VFPU_CTRL_RCX1,
    VFPU_CTRL_RCX2,
    VFPU_CTRL_RCX3,
    VFPU_CTRL_RCX4,
    VFPU_CTRL_RCX5,
    VFPU_CTRL_RCX6,
    VFPU_CTRL_RCX7,

    VFPU_CTRL_MAX,
    // unknown....
};

// The fields of the VFPU prefixes. Lane i of an S or T prefix takes the lane its swizzle names, or
// with the constant bit, the constant the swizzle and abs bit pick, then the abs and negate. Lane i
// of a D prefix has a saturation (1 = [0:1], 3 = [-1:1]) and a write mask bit, set to keep the lane.
inline int VFPUPrefixSwizzle(u32 prefix, int i) { return (prefix >> (i * 2)) & 3; }
inline bool VFPUPrefixAbs(u32 prefix, int i) { return (prefix >> (8 + i)) & 1; }
inline bool VFPUPrefixIsConstant(u32 prefix, int i) { return (prefix >> (12 + i)) & 1; }
inline bool VFPUPrefixNegate(u32 prefix, int i) { return (prefix >> (16 + i)) & 1; }
inline int VFPUPrefixSat(u32 prefix, int i) { return (prefix >> (i * 2)) & 3; }
inline u8 VFPUPrefixWriteMask(u32 prefix) { return (prefix >> 8) & 0xF; }
inline bool VFPUPrefixWriteMask(u32 prefix, int i) { return (prefix >> (8 + i)) & 1; }

class MIPSState {
public:
    // MUST start with r and be followed by f, v, and t!
    u32 r[32];
    union {
        float f[32];
        u32 fi[32];
        int fs[32];
    };
    union {
        float v[128];
        u32 vi[128];
    };

    // Register-allocated JIT Temps don't get flushed so we don't reserve space for them.
    // However, the IR interpreter needs some temps that can stick around between ops.
    // Can be indexed through r[] using indices 192+, thanks to predictable struct layout.
    // Unfortunately, UBSAN isn't too happy about these.
    u32 t[16]; // 192

    // If vfpuCtrl (prefixes) get mysterious values, check the VFPU regcache code.
    u32 vfpuCtrl[16]; // 208

    float vt[16]; // 224  TODO: VFPU temp

    // ARM64 wants lo/hi to be aligned to 64 bits from the base of this struct.
    u32 padLoHi; // 240

    union {
        struct {
            u32 pc; // 241

            u32 lo; // 242
            u32 hi; // 243

            u32 fcr31;  // 244 fpu control register
            u32 fpcond; // 245 cache the cond flag of fcr31  (& 1 << 23)
        };
        u32 other[6];
    };

    u32 nextPC;
    int downcount; // This really doesn't belong here, it belongs in CoreTiming. But you gotta do what you gotta do,
                   // this needs to be reachable in the JITs without additional pointers.

    bool inDelaySlot;
    int llBit; // ll/sc
    u32 temp;  // can be used to save temporaries during calculations when we need more than R0 and R1
    u32 mxcsrTemp;
    // Temporary used around delay slots and similar.
    u64 saved_flags;

    // Debug stuff
    u32 debugCount; // can be used to count basic blocks before crashes, etc.

    // COP0, for kernel code. Reads of Count (9) come from CoreTiming instead.
    u32 cop0[32];
    // COP0's control registers (cfc0/ctc0), which the firmware's exception code keeps state in.
    u32 cop0Control[32];

    // Temps needed for JitBranch.cpp experiments
    u32 intBranchExit;
    u32 jitBranchExit;

    u32 savedPC;

    alignas(16) u32 vcmpResult[4];

    float sincostemp[2];

    // The operands (s, then t) and result of the JITs' VFPU dot product routine (vfpu_dot()).
    alignas(16) u32 vfpuDotArgs[8];
    u32 vfpuDotResult;
    // The ARM64 JIT's multi-dot routine (IROp::Vec4VfpuMultiDot): the vector registers it saves, and
    // its operands (four term Vec4s and the vector) and result for inf and NaN.
    alignas(16) u32 vfpuMultiDotSave[64];
    alignas(16) float vfpuMultiDotArgs[20];
    alignas(16) float vfpuMultiDotResult[4];

    static const u32 FCR0_VALUE = 0x00003351;

#if PPSSPP_ARCH(X86) || PPSSPP_ARCH(AMD64)
    enum {
        NUM_X86_FPU_TEMPS = 16,
    };
    // FPU TEMP0, etc. are swapped in here if necessary (e.g. on x86.)
    float tempValues[NUM_X86_FPU_TEMPS];
#endif

    u8 VfpuWriteMask() const { return VFPUPrefixWriteMask(vfpuCtrl[VFPU_CTRL_DPREFIX]); }
    bool VfpuWriteMask(int i) const { return VFPUPrefixWriteMask(vfpuCtrl[VFPU_CTRL_DPREFIX], i); }

    bool HasDefaultPrefix() const {
        return vfpuCtrl[VFPU_CTRL_SPREFIX] == 0xe4 && vfpuCtrl[VFPU_CTRL_TPREFIX] == 0xe4 &&
               vfpuCtrl[VFPU_CTRL_DPREFIX] == 0;
    }

    // Owned. Null on the interpreter.
    void *jit = nullptr;
    // What this CPU sees of memory. The main CPU's is Memory::g_mainSpace.
    void *space = nullptr;
};
