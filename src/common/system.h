#pragma once

// DIAG_PUSH
// DIAG_IGNORE("-Wconversion")
// DIAG_IGNORE("-Wsign-conversion")
//
// <source code>
//
// DIAG_POP


// Clang also defines __GNUC__, so __clang__ has to be checked first
#if defined(__clang__)
    #define DIAG_CLANG 1
#elif defined(__GNUC__)
    #define DIAG_GCC 1
#endif

#define DIAG_DO_PRAGMA(x) _Pragma(#x)

#if defined(DIAG_CLANG)
    #define DIAG_PUSH \
        DIAG_DO_PRAGMA(clang diagnostic push)
    #define DIAG_POP \
        DIAG_DO_PRAGMA(clang diagnostic pop)

#elif defined(DIAG_GCC)
    #define DIAG_PUSH \
        DIAG_DO_PRAGMA(GCC diagnostic push)
    #define DIAG_POP \
        DIAG_DO_PRAGMA(GCC diagnostic pop)

#else
    #define DIAG_PUSH
    #define DIAG_POP
#endif

#if defined(DIAG_CLANG)
    #define DIAG_IGNORE(w) \
        DIAG_DO_PRAGMA(clang diagnostic ignored w)

#elif defined(DIAG_GCC)
    #define DIAG_IGNORE(w) \
        DIAG_DO_PRAGMA(GCC diagnostic ignored w)

#else
    #define DIAG_IGNORE(w)
#endif
