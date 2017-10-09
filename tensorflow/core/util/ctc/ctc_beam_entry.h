/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef TENSORFLOW_CORE_UTIL_CTC_CTC_BEAM_ENTRY_H_
#define TENSORFLOW_CORE_UTIL_CTC_CTC_BEAM_ENTRY_H_

#include <algorithm>
#include <memory>
#include <vector>

#include "third_party/eigen3/Eigen/Core"
#include "tensorflow/core/lib/gtl/flatmap.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/util/ctc/ctc_loss_util.h"

namespace tensorflow {
namespace ctc {

// The ctc_beam_search namespace holds several classes meant to be accessed only
// in case of extending the CTCBeamSearch decoder to allow custom scoring
// functions.
//
// BeamEntry is exposed through template arguments BeamScorer and BeamComparer
// of CTCBeamSearch (ctc_beam_search.h).
namespace ctc_beam_search {

struct EmptyBeamState {};

struct BeamProbability {
  BeamProbability() : total(kLogZero), blank(kLogZero), label(kLogZero) {}
  void Reset() {
    total = kLogZero;
    blank = kLogZero;
    label = kLogZero;
  }
  float total;
  float blank;
  float label;
};

template <class CTCBeamState = EmptyBeamState>
struct BeamEntry {
  // Default constructor does not create a vector of children.
  BeamEntry() : parent(nullptr), label(-1) {}
  // Constructor giving parent, label, and number of children does
  // create a vector of children.  The object pointed to by p
  // cannot be copied and should not be moved, otherwise parent will
  // become invalid.
  BeamEntry(BeamEntry* p, int l) : parent(p), label(l) {}
  inline bool Active() const { return newp.total != kLogZero; }
  // Return the child at the given index, or construct a new one in-place if
  // none was found.
  BeamEntry& GetChild(int ind) {
    auto entry = children.emplace(ind, nullptr);
    auto& child_entry = entry.first->second;
    // If this is a new child, populate the uniqe_ptr.
    if (entry.second) {
      child_entry.reset(new BeamEntry(this, ind));
    }
    return *(child_entry.get());
  }
  std::vector<int> LabelSeq(bool merge_repeated) const {
    std::vector<int> labels;
    int prev_label = -1;
    const BeamEntry* c = this;
    while (c->parent != nullptr) {  // Checking c->parent to skip root leaf.
      if (!merge_repeated || c->label != prev_label) {
        labels.push_back(c->label);
      }
      prev_label = c->label;
      c = c->parent;
    }
    std::reverse(labels.begin(), labels.end());
    return labels;
  }

  BeamEntry<CTCBeamState>* parent;
  int label;
  gtl::FlatMap<int, std::unique_ptr<BeamEntry<CTCBeamState>>> children;
  BeamProbability oldp;
  BeamProbability newp;
  CTCBeamState state;

 private:
  TF_DISALLOW_COPY_AND_ASSIGN(BeamEntry);
};

// BeamComparer is the default beam comparer provided in CTCBeamSearch.
template <class CTCBeamState = EmptyBeamState>
class BeamComparer {
 public:
  virtual ~BeamComparer() {}
  virtual bool inline operator()(const BeamEntry<CTCBeamState>* a,
                                 const BeamEntry<CTCBeamState>* b) const {
    return a->newp.total > b->newp.total;
  }
};

}  // namespace ctc_beam_search

}  // namespace ctc
}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_UTIL_CTC_CTC_BEAM_ENTRY_H_
