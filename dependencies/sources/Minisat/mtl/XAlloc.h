/*********************************************************************[XAlloc.h]
Copyright (c) 2009-2010, Niklas Sorensson

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*******************************************************************************/


#ifndef Minisat_XAlloc_h
#define Minisat_XAlloc_h

#include <errno.h>
#include <stdlib.h>

namespace Minisat {

//==============================================================================
// Simple layer on top of malloc/realloc to catch out-of-memory
// situtaions and provide some typing:

class OutOfMemoryException{};
static inline void* xrealloc(void *ptr, size_t size) {
  void* mem = realloc(ptr, size);
  if (mem == NULL && errno == ENOMEM){
    throw OutOfMemoryException();
  } else {
    return mem;
  }
}
}  // namespace Minisat
#endif
