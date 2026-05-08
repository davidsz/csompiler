#pragma once

namespace assembly {

static inline std::list<Instruction>::iterator removeIfNeeded(
    std::list<Instruction> &instructions,
    std::list<Instruction>::iterator it,
    const Operand &a,
    const Operand &b,
    bool changed)
{
    if (!changed)
        return std::next(it);
    const Reg *ra = std::get_if<Reg>(&a);
    const Reg *rb = std::get_if<Reg>(&b);
    if (ra && rb && ra->reg == rb->reg && ra->bytes == rb->bytes)
        return instructions.erase(it);
    return std::next(it);
}

template <typename ReplaceFn>
void replaceOperandsInFunction(
    std::list<CFGBlock> &blocks,
    ReplaceFn &&replaceFn)
{
    for (auto &block : blocks) {
        for (auto it = block.instructions.begin(); it != block.instructions.end();) {
            it = std::visit([&](auto &obj) {
                bool changed = false;
                using T = std::decay_t<decltype(obj)>;
                if constexpr (std::is_same_v<T, Mov>) {
                    changed |= replaceFn(obj.src, obj.type);
                    changed |= replaceFn(obj.dst, obj.type);
                    return removeIfNeeded(block.instructions, it, obj.src, obj.dst, changed);
                } else if constexpr (std::is_same_v<T, Movsx>) {
                    changed |= replaceFn(obj.src, obj.src_type);
                    changed |= replaceFn(obj.dst, obj.dst_type);
                    return removeIfNeeded(block.instructions, it, obj.src, obj.dst, changed);
                } else if constexpr (std::is_same_v<T, MovZeroExtend>) {
                    changed |= replaceFn(obj.src, obj.src_type);
                    changed |= replaceFn(obj.dst, obj.dst_type);
                    return removeIfNeeded(block.instructions, it, obj.src, obj.dst, changed);
                } else if constexpr (std::is_same_v<T, Lea>) {
                    // The loaded address is always 8-bytes
                    replaceFn(obj.src, Quadword);
                    replaceFn(obj.dst, Quadword);
                } else if constexpr (std::is_same_v<T, Cvttsd2si>) {
                    replaceFn(obj.src, obj.type);
                    replaceFn(obj.dst, obj.type);
                } else if constexpr (std::is_same_v<T, Cvtsi2sd>) {
                    replaceFn(obj.src, obj.type);
                    replaceFn(obj.dst, obj.type);
                } else if constexpr (std::is_same_v<T, Unary>) {
                    replaceFn(obj.src, obj.type);
                } else if constexpr (std::is_same_v<T, Binary>) {
                    replaceFn(obj.src, obj.type);
                    replaceFn(obj.dst, obj.type);
                } else if constexpr (std::is_same_v<T, Idiv>) {
                    replaceFn(obj.src, obj.type);
                } else if constexpr (std::is_same_v<T, Div>) {
                    replaceFn(obj.src, obj.type);
                } else if constexpr (std::is_same_v<T, Cmp>) {
                    replaceFn(obj.lhs, obj.type);
                    replaceFn(obj.rhs, obj.type);
                } else if constexpr (std::is_same_v<T, SetCC>) {
                    // SetCC always uses the 1-byte version of the registers
                    replaceFn(obj.op, Byte);
                } else if constexpr (std::is_same_v<T, Push>) {
                    // The operand is always an 8-bytes register
                    replaceFn(obj.op, Quadword);
                }
                return std::next(it);
            }, *it);
        }
    }
}

} // namespace assembly
