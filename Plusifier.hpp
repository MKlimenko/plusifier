#pragma once

#include <array>
#include <memory>
#include <tuple>

namespace plusifier {
    template <typename ... F>
    class FunctionWrapper final {
        static_assert(sizeof...(F) != 0, "FunctionWrapper should be not empty");
        std::tuple<F...> var;
        constexpr static inline std::size_t pack_size = sizeof...(F);

        template<typename T>
        struct ReturnType;

        template<typename R, typename ...Args>
        struct ReturnType<R(*)(Args...)> {
            using type = R;
        };

        template <typename ... Args>
        constexpr static auto NoOverloadFound() {
            return VerificationResult{ pack_size, false };
        }

        struct VerificationResult {
            std::size_t function_number = 0;
            bool is_invokable = false;

            constexpr operator std::size_t() const {
                return function_number;
            }

            constexpr operator bool() const {
                return is_invokable;
            }
        };

        template <std::size_t function_number, typename ... Args>
        constexpr static auto VerifyOverload() {
            using function_pointer_signature = std::remove_reference_t<decltype(std::get<function_number>(var))>;

            constexpr bool is_invokable = std::is_invocable_v<function_pointer_signature, Args...>;

            if constexpr (is_invokable)
                return VerificationResult{ function_number, true };

            if constexpr (function_number + 1 < pack_size)
                return VerifyOverload<function_number + 1, Args...>();
            else
                return NoOverloadFound<Args...>();
        }

        template <std::size_t function_number, typename T, typename ... Args>
        constexpr static auto VerifyOverloadByReturnType() {
            using function_pointer_signature = std::remove_reference_t<decltype(std::get<function_number>(var))>;

            constexpr bool is_invokable = std::is_invocable_v<function_pointer_signature, Args...>;

            if constexpr (is_invokable && std::is_same_v<T, typename ReturnType<function_pointer_signature>::type>)
                return VerificationResult{ function_number, true };

            if constexpr (function_number + 1 < pack_size)
                return VerifyOverloadByReturnType<function_number + 1, T, Args...>();
            else
                return NoOverloadFound<Args...>();
        }

    public:
        FunctionWrapper(F ... functions) : var(functions...) {}

        template <typename ... Args>
        auto operator()(Args ... args) const {
            constexpr auto verification_result = VerifyOverload<0, Args...>();
            if constexpr (!verification_result)
                static_assert(NoOverloadFound<F...>(), "No suitable overload is found");

            return std::get<verification_result>(var)(args...);
        }

        template <typename T, typename ... Args>
        auto OverloadByReturnType(Args ... args) const {
            constexpr auto verification_result = VerifyOverloadByReturnType<0, T, Args...>();
            if constexpr (!verification_result)
                static_assert(NoOverloadFound<F...>(), "No suitable overload is found");

            return std::get<verification_result>(var)(args...);
        }
    };

    template <typename T, auto D>
    class PointerWrapper {
        std::unique_ptr<T, decltype(D)> pointer;
    public:
        template <typename F, typename ... Args>
        PointerWrapper(F alloc_fn, Args ... args) : pointer(alloc_fn(args...), D) {}

        template <typename ... F, typename ... Args>
        PointerWrapper(FunctionWrapper<F...> alloc_fn, Args ... args) : pointer(alloc_fn.template OverloadByReturnType<T*>(args...), D) {}
    };
}
