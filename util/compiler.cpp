// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "util/compiler.h"
#ifdef _MSC_VER
# include <intrin.h>
#endif

namespace util {

unsigned ilog2(uint64_t n) {
#ifdef __GNUC__
  return n == 0 ? 0 : (64 - __builtin_clzll(n));
#elif defined(_MSC_VER)
# ifdef _M_X64
  return 64 - (unsigned)__lzcnt64(n);
# else
# error Unknown compiler
# endif
#else
# error Unknown compiler
#endif
}

}