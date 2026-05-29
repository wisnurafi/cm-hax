// utils/xorstr.h
// Compile-time XOR string encryption. Strings are encrypted at compile time
// and decrypted on the stack at runtime. The decrypted buffer is automatically
// zeroed when it goes out of scope to minimize the window where plaintext
// exists in memory.
//
// Usage:
//   auto s = XorStr("PlayerRoot");
//   const char* decrypted = s.c_str();  // valid until 's' goes out of scope
//
#pragma once
#include <string.h>
#include <intrin.h>

namespace detail {

constexpr char XOR_KEY = 0x5A;
constexpr char xor_char(char c, int i) { return c ^ (XOR_KEY + (i % 13)); }

template<int N>
struct XorString {
    char encrypted[N];
    
    // Encrypt at compile time
    constexpr XorString(const char(&str)[N]) : encrypted{} {
        for (int i = 0; i < N; i++) {
            encrypted[i] = xor_char(str[i], i);
        }
    }

    // Decrypt at runtime into a stack buffer, return wrapper that auto-zeros
    struct Decrypted {
        char buf[N];
        
        Decrypted(const char* enc) {
            for (int i = 0; i < N; i++) {
                buf[i] = xor_char(enc[i], i);
            }
        }
        
        ~Decrypted() {
            SecureZeroMemory(buf, N);
        }
        
        const char* c_str() const { return buf; }
        operator const char*() const { return buf; }
    };

    Decrypted decrypt() const {
        return Decrypted(encrypted);
    }
};

} // namespace detail

// Macro for easy usage. Creates a constexpr encrypted string and decrypts it.
// The decrypted value lives on the stack and is valid for the current scope.
// 
// Example:
//   auto name = ENC("PlayerRoot");
//   fn_class_from_name(image, ENC("Namespace"), ENC("ClassName"));
//
// Note: Each ENC() call creates a temporary. For function arguments, the
// temporaries live until the end of the full expression (;), so passing
// multiple ENC() to one function call is safe.
#define ENC(str) ([]() { \
    constexpr static auto encrypted = ::detail::XorString<sizeof(str)>(str); \
    return encrypted.decrypt(); \
}())
