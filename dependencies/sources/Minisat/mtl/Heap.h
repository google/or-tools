/***********************************************************************[Heap.h]
Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson Copyright (c)
2007-2010, Niklas Sorensson

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
******************************************************************************/

#ifndef Minisat_Heap_h
#define Minisat_Heap_h

#include "mtl/Vec.h"

namespace Minisat {

//============================================================================
// A heap implementation with support for decrease/increase key.


template<class Comp> class Heap {
  // The heap is a minimum-heap with respect to this comparator
  Comp     lt;
  // Heap of integers
  vec<int> heap;
  // Each integers position (index) in the Heap
  vec<int> indices;

  // Index "traversal" functions
  static inline int left  (int i) { return i*2+1; }
  static inline int right (int i) { return (i+1)*2; }
  static inline int parent(int i) { return (i-1) >> 1; }


  void percolateUp(int i)
  {
    int x  = heap[i];
    int p  = parent(i);

    while (i != 0 && lt(x, heap[p])){
      heap[i]          = heap[p];
      indices[heap[p]] = i;
      i                = p;
      p                = parent(p);
    }
    heap   [i] = x;
    indices[x] = i;
  }


  void percolateDown(int i)
  {
    int x = heap[i];
    while (left(i) < heap.size()){
      int child = right(i) < heap.size() &&
          lt(heap[right(i)], heap[left(i)]) ? right(i) : left(i);
      if (!lt(heap[child], x)) break;
      heap[i]          = heap[child];
      indices[heap[i]] = i;
      i                = child;
    }
    heap   [i] = x;
    indices[x] = i;
  }


 public:
  Heap(const Comp& c) : lt(c) { }

  int  size      ()          const { return heap.size(); }
  bool empty     ()          const { return heap.size() == 0; }
  bool inHeap    (int n)     const {
    return n < indices.size() && indices[n] >= 0;
  }
  int operator[](int index) const {
    assert(index < heap.size());
    return heap[index];
  }

  void decrease  (int n) { assert(inHeap(n)); percolateUp  (indices[n]); }
  void increase  (int n) { assert(inHeap(n)); percolateDown(indices[n]); }


  // Safe variant of insert/decrease/increase:
  void update(int n) {
    if (!inHeap(n))
      insert(n);
    else {
      percolateUp(indices[n]);
      percolateDown(indices[n]);
    }
  }


  void insert(int n) {
    indices.growTo(n+1, -1);
    assert(!inHeap(n));

    indices[n] = heap.size();
    heap.push(n);
    percolateUp(indices[n]);
  }


  int removeMin() {
    int x            = heap[0];
    heap[0]          = heap.last();
    indices[heap[0]] = 0;
    indices[x]       = -1;
    heap.pop();
    if (heap.size() > 1) percolateDown(0);
    return x;
  }


  // Rebuild the heap from scratch, using the elements in 'ns':
  void build(vec<int>& ns) {
    for (int i = 0; i < heap.size(); i++)
      indices[heap[i]] = -1;
    heap.clear();

    for (int i = 0; i < ns.size(); i++){
      indices[ns[i]] = i;
      heap.push(ns[i]); }

    for (int i = heap.size() / 2 - 1; i >= 0; i--)
      percolateDown(i);
  }

  void clear(bool dealloc = false) {
    for (int i = 0; i < heap.size(); i++)
      indices[heap[i]] = -1;
    heap.clear(dealloc);
  }
};
}  // namespace Minisat

#endif
