#include "ppsspp_api/pac.hpp"
#include "ppsspp_api.hpp"

#define PAC_ADDR(PAC_OFFSET) top.pac_start + PAC_OFFSET

// TODO: turn them into methods?
namespace pac {
u32 get_til(const PacState &state, u32 idx) {
    // _assert_msg_(idx < state.locals.icount, "Invalid PAC TIL requested: %d", idx);
    u32 base = state.locals.iptr;
    return g_emu.ReadUncheckedU32(base + idx * 4);
}
u32 get_tig(const PacState &state, u32 idx) {
    // _assert_msg_(idx < state.globals.icount, "Invalid PAC TIG requested: %d", idx);
    u32 base = state.globals.iptr;
    return g_emu.ReadUncheckedU32(base + idx * 4);
}
float get_tfl(const PacState &state, u32 idx) {
    // _assert_msg_(idx < state.locals.fcount, "Invalid PAC TFL requested: %d", idx);
    u32 base = state.locals.fptr;
    return g_emu.ReadUncheckedFloat(base + idx * 4);
}
float get_tfg(const PacState &state, u32 idx) {
    // _assert_msg_(idx < state.globals.fcount, "Invalid PAC TFG requested: %d", idx);
    u32 base = state.globals.fptr;
    return g_emu.ReadUncheckedFloat(base + idx * 4);
}
void set_til(PacState &state, u32 idx, u32 value) {
    // _assert_msg_(idx < state.locals.icount, "Invalid PAC TIL requested: %d", idx);
    u32 base = state.locals.iptr;
    g_emu.WriteUncheckedU32(value, base + idx * 4);
}
void set_tig(PacState &state, u32 idx, u32 value) {
    // _assert_msg_(idx < state.globals.icount, "Invalid PAC TIG requested: %d", idx);
    u32 base = state.globals.iptr;
    g_emu.WriteUncheckedU32(value, base + idx * 4);
}
void set_tfl(PacState &state, u32 idx, float value) {
    // _assert_msg_(idx < state.locals.fcount, "Invalid PAC TFL requested: %d", idx);
    u32 base = state.locals.fptr;
    g_emu.WriteUncheckedFloat(value, base + idx * 4);
}
void set_tfg(PacState &state, u32 idx, float value) {
    // _assert_msg_(idx < state.globals.fcount, "Invalid PAC TFG requested: %d", idx);
    u32 base = state.globals.fptr;
    g_emu.WriteUncheckedFloat(value, base + idx * 4);
}

bool get_flag(const PacState &state, u32 id) {
    // _assert_msg_(id < 8 * state.flags.capacity, "Invalid PAC flag requested: %d", id);
    auto byte_idx = id / 8;
    auto bit_idx = id % 8;

    const u32 byte_ptr = state.flags.ptr + byte_idx;
    u8 flag_byte = g_emu.ReadUncheckedU8(byte_ptr);

    return flag_byte & (1 << bit_idx);
}
void flip_flag(const PacState &state, u32 id) {
    // _assert_msg_(id < 8 * state.flags.capacity, "Invalid PAC flag requested: %d", id);
    auto byte_idx = id / 8;
    auto bit_idx = id % 8;

    const u32 byte_ptr = state.flags.ptr + byte_idx;
    u8 flag_byte = g_emu.ReadUncheckedU8(byte_ptr);

    u8 new_flag_byte = flag_byte ^ (1 << bit_idx);
    g_emu.WriteUncheckedU8(new_flag_byte, byte_ptr);
}
void assign_flag(const PacState &state, u32 id, bool value) {
    // _assert_msg_(id < 8 * state.flags.capacity, "Invalid PAC flag requested: %d", id);
    auto byte_idx = id / 8;
    auto bit_idx = id % 8;

    const u32 byte_ptr = state.flags.ptr + byte_idx;
    u8 flag_byte = g_emu.ReadUncheckedU8(byte_ptr);

    if (value == bool(flag_byte & (1 << bit_idx))) {
        // no need to do anything
        return;
    }
    // Wrong value, we have to flip it.
    u8 new_flag_byte = flag_byte ^ (1 << bit_idx);
    g_emu.WriteUncheckedU8(new_flag_byte, byte_ptr);
}
void enable_flag(const PacState &state, u32 id) { assign_flag(state, id, true); }
void disable_flag(const PacState &state, u32 id) { assign_flag(state, id, false); }

u32 get_array(const PacState &state, u32 idx) {
    // _assert_msg_(idx < state.array.count, "Invalid PAC array item requested: %d", idx);
    u32 base = state.array.ptr;
    return g_emu.ReadUncheckedU32(base + 4 * idx);
}
void set_array(const PacState &state, u32 idx, u32 value) {
    // _assert_msg_(idx < state.array.count, "Invalid PAC array item requested: %d", idx);
    u32 base = state.array.ptr;
    g_emu.WriteUncheckedU32(value, base + 4 * idx);
}
} // namespace pac