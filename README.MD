# C library plusifier

A header-only helper library to pack the multiple type-dependent C-style functions into single overload deduced at compile-time. No external libraries are required.

- [C library plusifier](#c-library-plusifier)
  - [Motivation](#motivation)
  - [Usage and examples](#usage-and-examples)
    - [Function overloading](#function-overloading)
    - [Pointer automation](#pointer-automation)
  - [Under the hood](#under-the-hood)
    - [Internals of the class](#internals-of-the-class)
    - [`operator()`](#operator)
    - [Function verification](#function-verification)

## Motivation

Many programming languages have the ability to call libraries with the pure C interface. Libraries themselves may be written in various languages, however, it is a de-facto standard for them to have a C interface.

Due to the lack of function overloading in pure C, library maintainers are required to explicitly specify all of the available types for the function. For example, I'd like to list one of my favourite libraries out there, the Intel Integrated Performance Primitives, [IPP](https://software.intel.com/content/www/us/en/develop/tools/oneapi/components/ipp.html):

```cpp
IppStatus   ippsMulC_16s_I(Ipp16s val, Ipp16s* pSrcDst, int len);
IppStatus   ippsMulC_32f_I(Ipp32f val, Ipp32f* pSrcDst, int len);
IppStatus   ippsMulC_64f_I(Ipp64f val, Ipp64f* pSrcDst, int len);
IppStatus   ippsMulC_32fc_I(Ipp32fc val, Ipp32fc* pSrcDst, int len);
IppStatus   ippsMulC_64fc_I(Ipp64fc val, Ipp64fc* pSrcDst, int len);
// ... and so on
```

If you're a C++ developer like myself, you may find this mildly irritating to look up and change the function every single time you decide to change the type. And it works poorly with generic (templated) code as well.

## Usage and examples

Wrapper object is created in the constructor and then the correct overload is selected in the `operator()` call:

```cpp
auto fn = plusifier::FunctionWrapper(/*function overloads*/);

auto dst = fn(/* function arguments... */);
```

Pointer wrapper object is used similarally:

```cpp
auto ptr = plusifier::PointerWrapper<PointerType, DeleterFunction>(allocator_function, /* allocator function arguments... */);
```

Where `allocator_function` may be both the callable (function pointer, lambda, `std::function`) as well as the `plusifier::FunctionWrapper`.

### Function overloading

For a more simplified example, suppose we have three functions with a slightly different signature:

```cpp
int square_s8(const std::int8_t* val, int sz) {
    return 1;
}
int square_s32(const std::int32_t* val, int sz) {
    return 4;
}
int square_fp32(const float* val) {
    return 8;
}
```

With this library, they may be packed into single object:

```cpp
auto square = plusifier::FunctionWrapper(square_s8, square_s32, square_fp32);

auto dst_ch = square(arr_ch.data(), 0);     // <-- calls square_s8
auto dst_int = square(arr_int.data(), 0);   // <-- calls square_s32
auto dst_fp32 = square(arr_fp32.data());    // <-- calls square_fp32
```

It will check if the passed arguments are viable to be used as the arguments for the functions at the compile-time and select the most appropriate overload.

### Pointer automation

RAII is the lifesaver in modern C++. However, it's a bit tedious to mix it with the C-style allocations. One of the approaches would be to use the `std::unique_ptr` with a custom deleter, but it's quite excess, so I decided to expand this library a little bit more.

For example, we might have a specified allocation functions for various types:

```cpp
Ipp8u*      ippsMalloc_8u(int len);
Ipp16u*     ippsMalloc_16u(int len);
Ipp32u*     ippsMalloc_32u(int len);
Ipp8s*      ippsMalloc_8s(int len);
Ipp16s*     ippsMalloc_16s(int len);
Ipp32s*     ippsMalloc_32s(int len);
Ipp64s*     ippsMalloc_64s(int len);
Ipp32f*     ippsMalloc_32f(int len);
Ipp64f*     ippsMalloc_64f(int len);
// and so on...
```

We'll wrap all of them into single `FunctionWrapper` and pass it to the `PointerWrapper`: 

```cpp
auto ippsMalloc = plusifier::FunctionWrapper(ippsMalloc_8u, ippsMalloc_16u, ippsMalloc_32u, /* etc */);

auto ptr = plusifier::PointerWrapper<Ipp8u, ippsFree>(ippsMalloc, size);
```

## Under the hood
### Internals of the class

`FunctionWrapper` is a variadic template class with the types being the function pointers:

```cpp
template <typename ... F>
class FunctionWrapper  final {
        static_assert(sizeof...(F) != 0, "FunctionWrapper should be not empty");
        std::tuple<F...> var;
        constexpr static inline std::size_t pack_size = sizeof...(F);
};
```

First `static_assert` is used to create a legit compile-time error when there are no functions passed. `std::tuple` is a heterogeneous container to store those function pointers, and a `pack_size` is a simple helper constant.

Due to the fact, that there are no references and move semantics in pure C, I've decided to omit the [perfect forwarding](https://eli.thegreenplace.net/2014/perfect-forwarding-and-universal-references-in-c) and pass the parameter pack in the constructor as-is, so the constructor is extremely trivial:

```cpp
FunctionWrapper(F ... functions) : var(functions...) {}
```

Then there is a function call operator (`operator()`), overload search and verification routines and small helper functions and classes.

### `operator()`

Function call operator may be split into two parts: compile-time and run-time. First is used to select the correct overload or to indicate the lack of one, while the runtime calls the selected function.

```cpp
template <typename ... Args>
auto operator()(Args ... args) const {
    // compile-time
    constexpr auto verification_result = VerifyOverload<0, Args...>();
    if constexpr (!verification_result)
        static_assert(NoOverloadFound<F...>(), "No suitable overload is found");

    // run-time
    return std::get<verification_result>(var)(args...);
}
```

Here the `verification_result` variable is an object of a simple helper struct with two fields and conversion operators. In the first place, I wanted to use a structured binding, but the compiler told me I'm not supposed to. This struct contains index of the function inside the tuple and the fact that the correct overload has been found. This flag ended up there due to the recursive nature of the used template metaprogramming approach.

Verification starts at index 0 and iterates up to the end of the tuple. 

### Function verification
Every iteration, I get the function pointer signature from the tuple, as well as the `std::function` signature to ease the following metaprogramming. Then there's an excellent function `std::is_invocable_v` in the standard library, that allows me to check if the function pointer in the tuple may be called with the type pack passed to the `operator()`. If we're good, we would prematurely quit the function, otherwise we'll continue iterating, until the very end of the tuple. 

If there's no suitable overload, a function with a failing `static_assert` is called for a better error diagnostics.