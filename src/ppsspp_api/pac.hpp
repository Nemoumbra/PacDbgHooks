#pragma once

#include <optional>
#include <string_view>

#include "CommonTypes.h"

inline constexpr u32 MAKE_PAC_SIGNATURE(u8 class_id, u16 instr_id) {
    return (0x25 << 24) | (class_id << 16) | (instr_id & 0xFF) << 8 | (instr_id >> 8);
}

struct PacPosition {
    u32 pac_start;
    u32 offset;
    std::optional<std::string_view> file_name;
    u32 file_size;
    u32 instruction_signature;
    const char *instruction_name;

    u32 get_pc() const { return pac_start + offset; }
};

struct PacExecBlock {
    u32 callstack_size;
    PacPosition callstack[15];
    u32 status;

    const PacPosition &get_callstack_item(u32 level) const {
        // _assert_msg_(level < 15, "Invalid PAC stack item requested: %d", level);
        return callstack[level];
    }
    PacPosition &get_callstack_item(u32 level) {
        // _assert_msg_(level < 15, "Invalid PAC stack item requested: %d", level);
        return callstack[level];
    }

    const PacPosition &get_top() const { return get_callstack_item(callstack_size - 1); }
    PacPosition &get_top() { return get_callstack_item(callstack_size - 1); }
};

struct PacState {
    u32 ctx_id;
    u32 ctx_addr;

    u32 blk;
    PacExecBlock blocks[3];

    const PacExecBlock &get_block(u32 index) const {
        // _assert_msg_(index < 3, "Invalid PAC block requested: %d", index);
        return blocks[index];
    }
    PacExecBlock &get_block(u32 index) {
        // _assert_msg_(index < 3, "Invalid PAC block requested: %d", index);
        return blocks[index];
    }

    const PacExecBlock &cur_block() const { return get_block(blk); }
    PacExecBlock &cur_block() { return get_block(blk); }

    const PacPosition &top() const { return cur_block().get_top(); }
    PacPosition &top() { return cur_block().get_top(); }
};