#include "Plusifier.hpp"

#include <array>
#include <cstdint>

namespace {
    constexpr int square_s8(const std::int8_t* val, int sz) {
        return 1;
    }
    constexpr int square_s32(const std::int32_t* val, int sz) {
        return 4;
    }
    constexpr int square_fp32(const float* val) {
        return 8;
    }

    std::int8_t* malloc_s8(int sz) {
        return reinterpret_cast<std::int8_t*>(0);
    }
    std::int32_t* malloc_s32(int sz) {
        return reinterpret_cast<std::int32_t*>(0);
    }

    void free_univ(void* val) {}
}

void test() {
    const std::array<std::int8_t, 1> arr_ch{};
    const std::array<std::int32_t, 1> arr_int{};
    const std::array<float, 1> arr_fp32{};

    constexpr auto square = plusifier::FunctionWrapper(square_s8, square_s32, square_fp32);
    constexpr auto dst_ch = square(arr_ch.data(), 0);
    constexpr auto dst_int = square(arr_int.data(), 0);
    constexpr auto dst_fp32 = square(arr_fp32.data());

    static_assert(dst_ch == square_s8(arr_ch.data(), 1));
    static_assert(dst_int == square_s32(arr_int.data(), 1));
    static_assert(dst_fp32 == square_fp32(arr_fp32.data()));
}
