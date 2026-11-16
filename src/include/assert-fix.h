//
//  file: %assert-fix.h
//  summary: "An alternative to what you get from #include <assert.h>"
//  project: "Rebol 3 Interpreter and Run-time (Revolt branch)"
//  homepage: https://github.com/metaeducation/ren-c/
//
//=////////////////////////////////////////////////////////////////////////=//
//
// Copyright 2012-2023 Revolt Open Source Contributors
//
// See README.md and CREDITS.md for more information.
//
// Licensed under the Lesser GPL, Version 3.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// https://www.gnu.org/licenses/lgpl-3.0.html
//
//=////////////////////////////////////////////////////////////////////////=//
//
// For some reason, Windows implementation of "_wassert" corrupts the stack
// by calling abort(), to where you only see at most 3 C stack frames above
// the assert in the VSCode debugger.  That's unusable.
//
// Also, in the Chromium debugger for WebAssembly an assert() causes a
// termination with no ability to inspect the stack.  It's nice to be able
// have a place to set a breakpoint, as well as to potentially continue.
//
//=//// NOTES /////////////////////////////////////////////////////////////=//
//
// * assert() is enabled by default; disable with `#define NDEBUG`
//
//   http://stackoverflow.com/a/17241278
//
// * NO STRINGS IN assert().  The C idiom for "always fail, with a message"
//   is `assert(!"message")`.  If you forget the `!`, or put it inside the
//   quotes, the assert silently always passes...since a string literal is a
//   non-null pointer.  So there are three macros:
//
//       assert(expr)        // refuses anything convertible to `const char*`
//       assert_msg("text")  // always fails; argument *must* be a C string
//       assert_raw(expr)    // no checking of the argument's type
//
//   The refusal is a compile-time check done by Needful's known_not(), so it
//   only has teeth in C++ builds with the Needful enhancements.  In C builds
//   the macros still exist, and assert_msg() can't be gotten wrong.
//
//   The checks expand to Needful macros *at the point of use*, so %needful.h
//   has to be included before the first assert() or assert_msg() in a file.
//   It does not have to be included before this header.
//
//   One consequence: assert(p) for a `char*` or `const char*` variable `p`
//   is refused too, as it can't be told from a string.  Say what you mean
//   with assert(p != nullptr).
//
// * Needful's own internal assertions go through NEEDFUL_ASSERT(), which is
//   pointed at assert_raw() here (unless you defined it yourself).  They need
//   the raw form: some are in generic code where the asserted thing may
//   legitimately be a `const char*`, and some appear in %needful.h before
//   known_not() has been defined.
//

#if !defined(NDEBUG) && !defined(USE_STANDARD_ASSERT_H)

    #include <stdio.h>

  // PROGRAMMATIC C BREAKPOINT
  //
  // This header brings in the ability to trigger a programmatic breakpoint
  // in C code, by calling `debug_break();`  It is not supported by HaikuOS R1,
  // so instead kick into an infinite loop which can be broken and stepped out
  // of in the debugger.
  //
  #if defined(__HAIKU__) || defined(__EMSCRIPTEN__)
      static inline void debug_break() {
          int x = 0;
        #if !defined(NDEBUG)
          printf("debug_break() called\n");
          fflush(stdout);
        #endif
          while (1) { ++x; }
          x = 0; // set next statement in debugger to here
          (void)(x);
      }
  #else
      #include "debugbreak.h"
  #endif

    #include <assert.h>  // include so it will think it has been included
    #undef assert  // (this way its include guard prevents it defining again)

    static inline void Assertion_Failure(
        const char* file,
        int line,
        const char* expr
    ){
        printf("Assertion failure: %s\n", expr);
        printf("Line %d, File: %s\n", line, file);
        fflush(stdout);  // if stdout is a pipe or file, text is lost w/o this
        debug_break();  // calling debug_break() allows us to step afterward
    }

    #define assert_raw(expr) \
        ((expr) ? (void)0 : Assertion_Failure(__FILE__, __LINE__, #expr))

    #define assert(expr) \
        (needful_rigid_known_not(const char*, (expr)) \
            ? (void)0 \
            : Assertion_Failure(__FILE__, __LINE__, #expr))

    #define assert_msg(msg) \
        Assertion_Failure( \
            __FILE__, __LINE__, needful_rigid_known(const char*, (msg)) \
        )
#else
    #include <assert.h>

  // There is a bug in older GCC where the assert macro expands arguments
  // unnecessarily.  Since Rebol tries to build on fairly old systems, this
  // patch corrects the issue:
  //
  // https://sourceware.org/bugzilla/show_bug.cgi?id=18604
  //
  #if !defined(NDEBUG) && defined(__GLIBC__) && defined(__GLIBC_MINOR__) \
      && (__GLIBC__ < 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ < 23))

      #undef assert
    #if !defined(__GNUC__) || defined (__STRICT_ANSI__)
      #define assert(expr) \
          ((expr) \
              ? __ASSERT_VOID_CAST (0) \
              : __assert_panic (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION))
    #else
      #define assert(expr) \
          ({ \
            if (expr) \
              ; /* empty */ \
            else \
              __assert_panic (#expr, __FILE__, __LINE__, __ASSERT_FUNCTION); \
          })
      #endif
  #endif

  // The standard assert() can't be wrapped to check its argument's type (a
  // macro can't be redefined in terms of its old self).  So on this path
  // assert() takes anything, as it always has.  assert_msg() still insists
  // on a C string, and supplies the `!` so that nobody has to remember it.
  //
    #define assert_raw(expr)  assert(expr)

    #define assert_msg(msg) \
        assert_raw(! needful_rigid_known(const char*, (msg)))
#endif

#if !defined(NEEDFUL_ASSERT)  // Needful's internals need the raw form
    #define NEEDFUL_ASSERT(expr)  assert_raw(expr)
#endif
