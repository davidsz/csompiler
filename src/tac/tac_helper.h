#pragma once

namespace tac {

template <typename Fn>
static void ForEachValue(const Instruction &instr, Fn &&fn)
{
    std::visit([&](const auto &i) {
        using T = std::decay_t<decltype(i)>;
        if constexpr (std::is_same_v<T, Return>) {
            if (i.val)
                fn(*i.val);
        } else if constexpr (std::is_same_v<T, Unary>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Binary>) {
            fn(i.src1);
            fn(i.src2);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Copy>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, GetAddress>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Load>) {
            fn(i.src_ptr);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Store>) {
            fn(i.src);
            fn(i.dst_ptr);
        } else if constexpr (std::is_same_v<T, Jump>) {
        } else if constexpr (std::is_same_v<T, JumpIfZero>) {
            fn(i.condition);
        } else if constexpr (std::is_same_v<T, JumpIfNotZero>) {
            fn(i.condition);
        } else if constexpr (std::is_same_v<T, Label>) {
        } else if constexpr (std::is_same_v<T, FunctionCall>) {
            for (const auto &arg : i.args)
                fn(arg);
            if (i.dst)
                fn(*i.dst);
        } else if constexpr (std::is_same_v<T, SignExtend>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, Truncate>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, ZeroExtend>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, DoubleToInt>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, DoubleToUInt>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, IntToDouble>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, UIntToDouble>) {
            fn(i.src);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, AddPtr>) {
            fn(i.ptr);
            fn(i.index);
            fn(i.dst);
        } else if constexpr (std::is_same_v<T, CopyToOffset>) {
            fn(i.src);
        } else if constexpr (std::is_same_v<T, CopyFromOffset>) {
            fn(i.dst);
        }
    }, instr);
}

} // namespace tac
