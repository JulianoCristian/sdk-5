// Copyright (c) 2015, the Fletch project authors. Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE.md file.

#ifndef SRC_VM_IMMUTABLE_HEAP_H_
#define SRC_VM_IMMUTABLE_HEAP_H_

#include "src/shared/globals.h"
#include "src/vm/heap.h"

namespace fletch {

class ImmutableHeap {
 public:
  class Part {
   public:
    Part(Part* next, uword budget)
        : heap_(NULL, budget),
          budget_(budget),
          used_original_(0),
          next_(next) {
    }

    Heap* heap() { return &heap_; }

    uword budget() { return budget_; }
    void set_budget(int new_budget) { budget_ = new_budget; }

    uword used() { return used_original_; }

    void ResetUsed() {
      used_original_ = heap_.space()->Used();
    }

    uword NewlyAllocated() { return heap_.space()->Used() - used_original_; }

    Part* next() { return next_; }
    void set_next(Part* next) { next_ = next; }

   private:
    Heap heap_;
    uword budget_;
    uword used_original_;
    Part* next_;
  };

  ImmutableHeap();
  ~ImmutableHeap();

  // Will return a [Heap] which will have an allocation budget which is
  // `known_live_memory / number_of_hw_threads_`. This is an approximation of a
  // 2x growth strategy.
  //
  // TODO(kustermann): instead of `number_of_hw_threads_` we could make this
  // better by keeping track of the current number of used scheduler threads.
  Part* AcquirePart();

  // Will return `true` if the caller should trigger an immutable GC.
  //
  // It is assumed that this function is only called on allocation failures.
  bool ReleasePart(Part* part);

  // Merges all parts which have been acquired and subsequently released into
  // the accumulated immutable heap.
  //
  // This function assumes that there are no parts outstanding.
  void MergeParts();

  // This method can only be called if
  //   * all acquired parts were released again
  //   * all cached parts were merged via [MergeParts]
  void IterateProgramPointers(PointerVisitor* visitor);

  // This method can only be called if
  //   * all acquired parts were released again
  //   * all cached parts were merged via [MergeParts]
  Heap* heap() {
    ASSERT(outstanding_parts_ == 0 && unmerged_parts_ == NULL);
    return &heap_;
  }

  // This method can only be called if
  //   * all acquired parts were released again
  //   * all cached parts were merged via [MergeParts]
  void UpdateLimitAfterImmutableGC(uword mutable_size_at_last_gc);

  // The number of used bytes at the moment. Note that this is an over
  // approximation.
  uword EstimatedUsed();

  // The total size of the immutable heap at the moment. Note that this is an
  // over approximation.
  uword EstimatedSize();

 private:
  bool HasUnmergedParts() { return unmerged_parts_ != NULL; }
  void AddUnmergedPart(Part* part);
  Part* RemoveUnmergedPart();

  int number_of_hw_threads_;

  Mutex* heap_mutex_;
  Heap heap_;
  int outstanding_parts_;
  Part* unmerged_parts_;

  // The limit of bytes we give out before a immutable GC should happen.
  uword immutable_allocation_limit_;

  // The amount of memory consumed by unmerged parts.
  uword unmerged_allocated_;

  // The allocated memory and budget of all outstanding parts.
  //
  // Adding these two number gives an overapproximation of used memory by
  // oustanding parts.
  uword outstanding_parts_allocated_;
  uword outstanding_parts_budget_;
};

}  // namespace fletch


#endif  // SRC_VM_IMMUTABLE_HEAP_H_
