/***********************************************************************[Sort.h]
Copyright (c) 2003-2007, Niklas Een, Niklas Sorensson
Copyright (c) 2007-2010, Niklas Sorensson

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

#ifndef Minisat_Sort_h
#define Minisat_Sort_h

#include "mtl/Vec.h"

//==============================================================================
// Some sorting algorithms for vec's


namespace Minisat {

template<class T>
struct LessThan_default {
  bool operator () (T x, T y) { return x < y; }
};


template <class T, class LessThan>
void selectionSort(T* array, int size, LessThan lt) {
  int     i, j, best_i;
  T       tmp;

  for (i = 0; i < size-1; i++){
    best_i = i;
    for (j = i+1; j < size; j++){
      if (lt(array[j], array[best_i]))
        best_i = j;
    }
    tmp = array[i]; array[i] = array[best_i]; array[best_i] = tmp;
  }
}
template <class T> static inline void selectionSort(T* array, int size) {
  selectionSort(array, size, LessThan_default<T>()); }

template <class T, class LessThan> void sort(T* array, int size, LessThan lt) {
  if (size <= 15) {
    selectionSort(array, size, lt);
  } else {
    T           pivot = array[size / 2];
    T           tmp;
    int         i = -1;
    int         j = size;

    for(;;) {
      do i++; while(lt(array[i], pivot));
      do j--; while(lt(pivot, array[j]));

      if (i >= j) break;

      tmp = array[i]; array[i] = array[j]; array[j] = tmp;
    }

    sort(array    , i     , lt);
    sort(&array[i], size-i, lt);
  }
}

template <class T> static inline void sort(T* array, int size) {
  sort(array, size, LessThan_default<T>());
}

//=============================================================================
// For 'vec's:

template <class T, class LessThan> void sort(vec<T>& v, LessThan lt) {
  sort((T*)v, v.size(), lt);
}

template <class T> void sort(vec<T>& v) {
  sort(v, LessThan_default<T>());
}
}  // namespace Minisat

#endif
