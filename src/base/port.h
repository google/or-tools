#ifndef OR_TOOLS_BASE_PORT_H_
#define OR_TOOLS_BASE_PORT_H_

// Tell the compiler to warn about unused return values for functions declared
// with this macro.  The macro should be used on function declarations
// following the argument list:
//
//   Sprocket* AllocateSprocket() MUST_USE_RESULT;
//
#if defined(SWIG)
#define MUST_USE_RESULT
#elif __GNUC__ > 3 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)
#define MUST_USE_RESULT __attribute__ ((warn_unused_result))
#else
#define MUST_USE_RESULT
#endif

#endif  // OR_TOOLS_BASE_PORT_H_
