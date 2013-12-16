// Copyright 2010-2013 Google
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#if defined(__GNUC__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#define HAVE_MMAP
#endif

#include <fstream>   // NOLINT
#include <iostream>  // NOLINT
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "flatzinc/flatzinc.h"
#include "flatzinc/parser.h"
#include "flatzinc/flatzinc.tab.hh"
#include "util/string_array.h"

extern int orfz_parse(void* input);
extern int orfz_lex_init(void** scanner);
extern int orfz_lex_destroy(void* scanner);
extern int orfz_get_lineno(void* scanner);
extern void orfz_set_extra(void* user_defined, void* yyscanner);
extern void yyerror(void* parm, const char* str);

DECLARE_bool(use_sat);
DECLARE_bool(logging);

namespace operations_research {
// ----- Misc -----
bool HasDomainAnnotation(AstNode* const annotations) {
  if (annotations != nullptr) {
    return annotations->hasAtom("domain");
  }
  return false;
}

bool HasDefineAnnotation(AstNode* const annotations) {
  if (annotations != nullptr) {
    if (annotations->isArray()) {
      AstArray* const ann_array = annotations->getArray();
      if (ann_array->a[0]->isCall("defines_var")) {
        return true;
      }
    }
  }
  return false;
}

// ----- Parser State -----

ParserState::~ParserState() {
  STLDeleteElements(&int_variables_);
  STLDeleteElements(&bool_variables_);
  STLDeleteElements(&set_variables_);
  STLDeleteElements(&constraints_);
  for (int i = 0; i < int_domain_constraints_.size(); ++i) {
    delete int_domain_constraints_[i].first;
    delete int_domain_constraints_[i].second;
  }
  STLDeleteElements(&int_args_);
  STLDeleteElements(&bool_args_);
}

int ParserState::FillBuffer(char* lexBuf, unsigned int lexBufSize) {
  if (pos >= length) return 0;
  int num = std::min(length - pos, lexBufSize);
  memcpy(lexBuf, buf + pos, num);
  pos += num;
  return num;
}

void ParserState::output(std::string x, AstNode* n) {
  output_.push_back(std::pair<std::string, AstNode*>(x, n));
}

// Strict weak ordering for output items
class OutputOrder {
 public:
  // Return if \a x is less than \a y, based on first component
  bool operator()(const std::pair<std::string, AstNode*>& x,
                  const std::pair<std::string, AstNode*>& y) {
    return x.first < y.first;
  }
};

AstArray* ParserState::Output(void) {
  OutputOrder oo;
  std::sort(output_.begin(), output_.end(), oo);
  AstArray* const a = new AstArray();
  for (unsigned int i = 0; i < output_.size(); i++) {
    a->a.push_back(new AstString(output_[i].first + " = "));
    if (output_[i].second->isArray()) {
      AstArray* const oa = output_[i].second->getArray();
      for (unsigned int j = 0; j < oa->a.size(); j++) {
        a->a.push_back(oa->a[j]);
        oa->a[j] = nullptr;
      }
      delete output_[i].second;
    } else {
      a->a.push_back(output_[i].second);
    }
    a->a.push_back(new AstString(";\n"));
  }
  return a;
}

AstNode* ParserState::FindTarget(AstNode* const annotations) const {
  if (annotations != nullptr) {
    if (annotations->isArray()) {
      AstArray* const ann_array = annotations->getArray();
      if (!ann_array->a.empty() && ann_array->a[0]->isCall("defines_var")) {
        AstCall* const call = ann_array->a[0]->getCall();
        AstNode* const args = call->args;
        return Copy(args);
      }
    }
  }
  return nullptr;
}

void ParserState::CollectRequired(AstArray* const args,
                                  const NodeSet& candidates,
                                  NodeSet* const require) const {
  for (int i = 0; i < args->a.size(); ++i) {
    AstNode* const node = args->a[i];
    if (node->isArray()) {
      CollectRequired(node->getArray(), candidates, require);
    } else {
      AstNode* const copy = Copy(node);
      if (copy != nullptr && ContainsKey(candidates, copy)) {
        require->insert(copy);
      }
    }
  }
}

void ParserState::ComputeViableTarget(CtSpec* const spec,
                                      NodeSet* const candidates) const {
  const std::string& id = spec->Id();
  if (id == "bool2int" || id == "int_plus" || id == "int_minus" ||
      (id == "array_var_int_element" && !IsBound(spec->Arg(2))) ||
      id == "array_int_element" || id == "int_abs" ||
      (id == "int_lin_eq" && !HasDomainAnnotation(spec->annotations()) &&
       !spec->Ignored()) ||
      id == "int_max" || id == "int_min" || id == "int_mod" || id == "int_eq") {
    // Defines an int var.
    AstNode* const define = FindTarget(spec->annotations());
    if (define != nullptr) {
      //      CHECK(IsIntroduced(define));
      candidates->insert(define);
      VLOG(3) << id << " -> insert " << define->DebugString();
    }
  } else if (id == "int_times") {
    if (FLAGS_use_sat && IsBoolean(spec->Arg(1)) && IsBoolean(spec->Arg(2))) {
      VLOG(3) << "Not marking " << spec->DebugString();
    } else {
      AstNode* const define = FindTarget(spec->annotations());
      if (define != nullptr) {
        candidates->insert(define);
        VLOG(3) << id << " -> insert " << define->DebugString();
      }
    }
  } else if ((id == "array_bool_and" && !FLAGS_use_sat) ||
             (id == "array_bool_or" && !FLAGS_use_sat) ||
             id == "array_bool_element" ||
             id == "int_lin_eq_reif" || id == "int_lin_ne_reif" ||
             id == "int_lin_ge_reif" || id == "int_lin_le_reif" ||
             id == "int_lin_gt_reif" || id == "int_lin_lt_reif" ||
             id == "int_eq_reif" || id == "int_ne_reif" ||
             id == "int_le_reif" || id == "int_ge_reif" ||
             id == "int_lt_reif" || id == "int_gt_reif" ||
             (id == "bool_eq_reif" && !FLAGS_use_sat) ||
             (id == "bool_ne_reif" && !FLAGS_use_sat) ||
             (id == "bool_le_reif" && !FLAGS_use_sat) ||
             (id == "bool_ge_reif" && !FLAGS_use_sat) || id == "bool_not") {
    // Defines a bool var.
    AstNode* const bool_define = FindTarget(spec->annotations());
    if (bool_define != nullptr) {
      CHECK(IsIntroduced(bool_define));
      candidates->insert(bool_define);
      VLOG(3) << id << " -> insert " << bool_define->DebugString();
    }
  } else if (id == "bool2bool") {
    candidates->insert(Copy(spec->Arg(1)));
    VLOG(3) << id << " -> insert " << spec->Arg(1)->DebugString();
  }
}

void ParserState::ComputeDependencies(
    const NodeSet& candidates, CtSpec* const spec) const {
  AstNode* const define =
      spec->DefinedArg() == nullptr ? FindTarget(spec->annotations())
                                 : spec->DefinedArg();
  if (ContainsKey(candidates, define)) {
    spec->SetDefinedArg(define);
  }
  NodeSet* const requires = spec->mutable_require_map();
  CollectRequired(spec->Args(), candidates, requires);
  if (define != nullptr) {
    requires->erase(define);
  }
}

void ParserState::MarkAllVariables(AstNode* const node,
                                   NodeSet* const computed) {
  if (node->isIntVar()) {
    computed->insert(Copy(node));
    VLOG(2) << "  - " << node->DebugString();
  }
  if (node->isArray()) {
    AstArray* const array = node->getArray();
    for (int i = 0; i < array->a.size(); ++i) {
      if (array->a[i]->isIntVar()) {
        computed->insert(Copy(array->a[i]));
        VLOG(2) << "  - " << array->a[i]->DebugString();
      }
    }
  }
}

void ParserState::MarkComputedVariables(CtSpec* const spec,
                                        NodeSet* const computed) {
  const std::string& id = spec->Id();
  if (id == "global_cardinality") {
    VLOG(2) << "  - marking " << spec->DebugString();
    MarkAllVariables(spec->Arg(2), computed);
  }
  if (id == "maximum_int" && spec->Arg(0)->isIntVar() &&
      spec->DefinedArg() == nullptr) {
    computed->insert(Copy(spec->Arg(0)));
  }
  if (id == "minimum_int" && spec->Arg(0)->isIntVar() &&
      spec->DefinedArg() == nullptr) {
    computed->insert(Copy(spec->Arg(0)));
  }

  if (id == "int_lin_eq" && spec->DefinedArg() == nullptr) {
    AstArray* const array_coefficients = spec->Arg(0)->getArray();
    const int size = array_coefficients->a.size();
    bool todo = true;
    if (size == 0) {
      return;
    }
    if (array_coefficients->a[0]->getInt() == -1) {
      for (int i = 1; i < size; ++i) {
        if (array_coefficients->a[i]->getInt() < 0) {
          todo = false;
          break;
        }
      }
      // Can mark the first one, this is a hidden sum.
      if (todo) {
        AstArray* const array_variables = spec->Arg(1)->getArray();
        computed->insert(Copy(array_variables->a[0]));
        VLOG(2) << "  - marking " << spec->DebugString() << ": "
                << array_variables->a[0]->DebugString();
        return;
      }
    }
    todo = true;
    if (array_coefficients->a[0]->getInt() == 1) {
      for (int i = 1; i < size; ++i) {
        if (array_coefficients->a[i]->getInt() > 0) {
          todo = false;
          break;
        }
      }
      if (todo) {
        // Can mark the first one, this is a hidden sum.
        AstArray* const array_variables = spec->Arg(1)->getArray();
        computed->insert(Copy(array_variables->a[0]));
        VLOG(2) << "  - marking " << spec->DebugString() << ": "
                << array_variables->a[0]->DebugString();
        return;
      }
    }
    todo = true;
    if (array_coefficients->a[size - 1]->getInt() == 1) {
      for (int i = 0; i < size - 1; ++i) {
        if (array_coefficients->a[i]->getInt() > 0) {
          todo = false;
          break;
        }
      }
      if (todo) {
        // Can mark the last one, this is a hidden sum.
        AstArray* const array_variables = spec->Arg(1)->getArray();
        computed->insert(Copy(array_variables->a[size - 1]));
        VLOG(2) << "  - marking " << spec->DebugString() << ": "
                << array_variables->a[size - 1]->DebugString();
        return;
      }
    }
  }
}

void ParserState::Sanitize(CtSpec* const spec) {
  if (spec->Id() == "int_lin_eq" && HasDomainAnnotation(spec->annotations())) {
    AstArray* const array_coefficients = spec->Arg(0)->getArray();
    if (array_coefficients->a.size() > 2) {
      VLOG(2) << "  - presolve: remove defines part on " << spec->DebugString();
      spec->RemoveDefines();
    } else {
      VLOG(2) << "  - presolve: remove domain part on " << spec->DebugString();
      spec->RemoveDomain();
    }
  }
}

void ParserState::CollectIgnored(NodeSet* const ignored) {
  if (!FLAGS_use_sat) {
    return;
  }
  for (int i = 0; i < constraints_.size(); ++i) {
    CtSpec* const spec = constraints_[i];
    const std::string& id = spec->Id();
    if (id == "array_bool_and" || id == "array_bool_or" ||
        id == "bool_eq_reif" || id == "bool_ne_reif" || id == "bool_le_reif" ||
        id == "bool_ge_reif") {
      AstNode* const bool_define = FindTarget(spec->annotations());
      if (bool_define != nullptr) {
        VLOG(2) << "Remove target from " << spec->DebugString();
        ignored->insert(bool_define);
        spec->RemoveDefines();
      }
    }
  }
}

void ParserState::ReuseIgnored(NodeSet* const ignored) {
  if (ignored->empty()) {
    return;
  }
  for (int i = 0; i < constraints_.size(); ++i) {
    CtSpec* const spec = constraints_[i];
    const std::string& id = spec->Id();
    AstNode* const bool_define = FindTarget(spec->annotations());
    AstNode* const last_arg = Copy(spec->LastArg());
    if (bool_define == nullptr && last_arg != nullptr &&
        ContainsKey(*ignored, last_arg) &&(id == "int_lin_eq_reif" ||
                                           id == "int_lin_ne_reif" ||
                                           id == "int_lin_ge_reif" ||
                                           id == "int_lin_le_reif" ||
                                           id == "int_lin_gt_reif" ||
                                           id == "int_lin_lt_reif" ||
                                           id == "int_eq_reif" ||
                                           id == "int_ne_reif" ||
                                           id == "int_le_reif" ||
                                           id == "int_ge_reif" ||
                                           id == "int_lt_reif" ||
                                           id == "int_gt_reif")) {
      AstCall* const call =
          new AstCall("defines_var", new AstBoolVar(last_arg->getBoolVar()));
      spec->AddAnnotation(call);
      VLOG(2) << "Reusing " << last_arg->DebugString() << " in "
              << spec->DebugString();
      ignored->erase(last_arg);
      if (ignored->empty()) {
        return;
      }
    }
  }
}

void ParserState::Presolve() {
  // Sanity.
  VLOG(2) << "Sanitize";
  for (int i = 0; i < constraints_.size(); ++i) {
    Sanitize(constraints_[i]);
  }

  // Hack, loop optimization on int_max and int_min.
  VLOG(2) << "Loop optimization";
  BuildStatistics();
  if (ContainsKey(constraints_per_id_, "int_max")) {
    Regroup("maximum_int", constraints_per_id_["int_max"]);
  }
  if (ContainsKey(constraints_per_id_, "int_min")) {
    Regroup("minimum_int", constraints_per_id_["int_min"]);
  }

  // Find orphans (is_defined && not a viable target).
  VLOG(2) << "Search for orphans";
  for (int i = 0; i < constraints_.size(); ++i) {
    AstNode* const target = FindTarget(constraints_[i]->annotations());
    if (target != nullptr) {
      VLOG(2) << "  - presolve:  mark " << target->DebugString()
              << " as defined";
      targets_.insert(target);
    }
  }
  for (int i = 0; i < int_variables_.size(); ++i) {
    IntVarSpec* const spec = int_variables_[i];
    AstNode* const var = IntCopy(i);
    if (spec->introduced && !ContainsKey(targets_, var)) {
      orphans_.insert(var);
      VLOG(2) << "  - presolve:  mark " << var->DebugString() << " as orphan";
    }
  }
  // Collect booleans that will not be used as defined and try to reuse them.
  NodeSet ignored;
  CollectIgnored(&ignored);
  ReuseIgnored(&ignored);

  // Collect aliases.
  VLOG(2) << "Collect aliases";
  for (int i = 0; i < int_variables_.size(); ++i) {
    IntVarSpec* const spec = int_variables_[i];
    if (spec->alias) {
      VLOG(2) << "  - presolve: xi(" << i << ") is aliased to xi(" << spec->i
              << ")";
      int_aliases_[i] = spec->i;
    }
  }

  // Discover new aliases
  VLOG(2) << "Discover new aliases";
  for (int i = 0; i < constraints_.size(); ++i) {
    CtSpec* const spec = constraints_[i];
    if (spec->Nullified()) {
      continue;
    }
    DiscoverAliases(spec);
  }

  // Merge domains of aliases, update alias map to point to root.
  VLOG(2) << "Merge domain, update aliases";
  for (int i = 0; i < int_variables_.size(); ++i) {
    IntVarSpec* const spec = int_variables_[i];
    //    AstNode* const var = IntCopy(i); // CHECKME
    // TODO(user) : loop on hash_table.
    if (ContainsKey(int_aliases_, i)) {
      int index = i;
      while (int_variables_[index]->alias) {
        index = int_variables_[index]->i;
      }
      int_aliases_[i] = index;
      if (spec->HasDomain()) {
        MergeIntDomain(spec, int_variables_[index]);
      }
    }
  }

  // Remove all aliases from constraints.
  VLOG(2) << "Remove duplicate aliases";
  for (int i = 0; i < constraints_.size(); ++i) {
    CtSpec* const spec = constraints_[i];
    if (!spec->Nullified()) {
      ReplaceAliases(spec);
    }
  }

  // Presolve (propagate bounds, simplify constraints...).
  VLOG(2) << "Main presolve loop.";
  bool repeat = true;
  while (repeat) {
    repeat = false;
    for (int i = 0; i < constraints_.size(); ++i) {
      CtSpec* const spec = constraints_[i];
      if (spec->Nullified()) {
        continue;
      }
      if (PresolveOneConstraint(spec)) {
        repeat = true;
      }
    }
  }

  // Add aliasing constraints.
  for (int i = 0; i < bool_variables_.size(); ++i) {
    BoolVarSpec* const spec = bool_variables_[i];
    if (spec->alias) {
      AstArray* args = new AstArray(2);
      args->a[0] = new AstBoolVar(spec->i);
      args->a[1] = new AstBoolVar(i);
      CtSpec* const alias_ct =
          new CtSpec(constraints_.size(), "bool2bool", args, nullptr);
      alias_ct->SetDefinedArg(BoolCopy(i));
      constraints_.push_back(alias_ct);
    }
  }

  FZLOG << "Model statistics" << std::endl;
  BuildStatistics();
  for (ConstIter<hash_map<std::string, std::vector<int>>> it(constraints_per_id_);
       !it.at_end(); ++it) {
    FZLOG << "  - " << it->first << ": " << it->second.size() << std::endl;
  }
  switch (model_->ProblemType()) {
    case FlatZincModel::SAT:
      FZLOG << "  - Satisfaction problem" << std::endl;
      break;
    case FlatZincModel::MIN:
      FZLOG << "  - Minimization problem" << std::endl;
      break;
    case FlatZincModel::MAX:
      FZLOG << "  - Maximization problem" << std::endl;
      break;
    default:
      FZLOG << "  - Unkown problem" << std::endl;
      break;
  }
}

void ParserState::BuildStatistics() {
  // Setup mapping structures (constraints per id, and constraints per
  // variable).
  constraints_per_id_.clear();
  for (int i = 0; i < int_variables_.size(); ++i) {
    constraints_per_int_variables_[i].clear();
  }
  for (int i = 0; i < bool_variables_.size(); ++i) {
    constraints_per_bool_variables_[i].clear();
  }

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    const int index = spec->Index();
    if (!spec->Nullified()) {
      constraints_per_id_[spec->Id()].push_back(index);
      const int num_args = spec->NumArgs();
      for (int i = 0; i < num_args; ++i) {
        AstNode* const arg = spec->Arg(i);
        if (arg->isIntVar()) {
          constraints_per_int_variables_[arg->getIntVar()].push_back(index);
        } else if (arg->isBoolVar()) {
          constraints_per_bool_variables_[arg->getBoolVar()].push_back(index);
        } else if (arg->isArray()) {
          const std::vector<AstNode*>& array = arg->getArray()->a;
          for (int j = 0; j < array.size(); ++j) {
            if (array[j]->isIntVar()) {
              constraints_per_int_variables_[array[j]->getIntVar()]
                  .push_back(index);
            } else if (array[j]->isBoolVar()) {
              constraints_per_bool_variables_[array[j]->getBoolVar()]
                  .push_back(index);
            }
          }
        }
      }
    }
  }
  // Export occurrences.
  for (int i = 0; i < int_variables_.size(); ++i) {
    model_->SetIntegerOccurrences(i, constraints_per_int_variables_[i].size());
  }
  for (int i = 0; i < bool_variables_.size(); ++i) {
    model_->SetBooleanOccurrences(i, constraints_per_bool_variables_[i].size());
  }
}

void ParserState::SortConstraints(NodeSet* const candidates,
                                  NodeSet* const computed_variables) {
  // Discover expressions, topological sort of constraints.

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    ComputeViableTarget(spec, candidates);
  }

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    ComputeDependencies(*candidates, spec);
    if (spec->DefinedArg() != nullptr || !spec->require_map().empty()) {
      VLOG(3) << spec->DebugString();
    }
  }

  FZLOG << "Sort constraints" << std::endl;
  std::vector<CtSpec*> defines_only;
  std::vector<CtSpec*> no_defines;
  std::vector<CtSpec*> defines_and_require;
  int nullified = 0;

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    if (spec->Nullified()) {
      nullified++;
    } else if (spec->DefinedArg() != nullptr && spec->require_map().empty()) {
      defines_only.push_back(spec);
    } else if (spec->DefinedArg() == nullptr) {
      no_defines.push_back(spec);
    } else {
      defines_and_require.push_back(spec);
    }
  }

  FZLOG << "  - defines only          : " << defines_only.size() << std::endl;
  FZLOG << "  - no defines            : " << no_defines.size() << std::endl;
  FZLOG << "  - defines and require   : " << defines_and_require.size()
        << std::endl;
  FZLOG << "  - nullified constraints : " << nullified << std::endl;

  const int size = constraints_.size();
  constraints_.clear();
  constraints_.resize(size - nullified);
  int index = 0;
  NodeSet defined;
  for (int i = 0; i < defines_only.size(); ++i) {
    if (!defines_only[i]->Nullified()) {
      constraints_[index++] = defines_only[i];
      defined.insert(Copy(defines_only[i]->DefinedArg()));
      VLOG(3) << "defined.insert "
              << defines_only[i]->DefinedArg()->DebugString();
    }
  }

  // Topological sorting.
  ConstraintSet to_insert;
  for (int i = 0; i < defines_and_require.size(); ++i) {
    if (!defines_and_require[i]->Nullified()) {
      to_insert.insert(defines_and_require[i]);
      VLOG(3) << " to_insert " << defines_and_require[i]->DebugString();
    }
  }

  NodeSet forced;
  while (!to_insert.empty()) {
    std::vector<CtSpec*> inserted;
    for (ConstIter<ConstraintSet> it(to_insert); !it.at_end(); ++it) {
      CtSpec* const spec = *it;
      VLOG(3) << "check " << spec->DebugString();
      if (ContainsKey(forced, spec->DefinedArg())) {
        VLOG(3) << "  - cleaning defines";
        spec->RemoveDefines();
      }
      bool ok = true;
      for (ConstIter<NodeSet> def(spec->require_map()); !def.at_end(); ++def) {
        if (!ContainsKey(defined, *def)) {
          ok = false;
          break;
        }
      }
      if (ok) {
        inserted.push_back(spec);
        constraints_[index++] = spec;
        if (spec->DefinedArg() != nullptr) {
          defined.insert(Copy(spec->DefinedArg()));
          VLOG(3) << "inserted.push_back " << spec->DebugString();
          VLOG(3) << "defined.insert " << spec->DefinedArg()->DebugString();
        }
      }
    }
    if (inserted.empty()) {
      // Recovery mode. We have a cycle!.  Let's find the one
      // with the smallest number of unsatisfied dependencies.
      CtSpec* to_correct = nullptr;
      int best_unsatisfied = kint32max;
      for (ConstIter<ConstraintSet> it(to_insert); !it.at_end(); ++it) {
        CtSpec* const spec = *it;
        VLOG(3) << "evaluate " << spec->DebugString();

        int unsatisfied = 0;
        const NodeSet& required = spec->require_map();
        for (ConstIter<NodeSet> def(required); !def.at_end(); ++def) {
          AstNode* const dep = *def;
          unsatisfied += !ContainsKey(defined, dep);
          if (IsAlias(dep)) {
            VLOG(3) << "  - " << dep->DebugString()
                    << "is an alias, disqualified";
            unsatisfied = kint32max;
            break;
          }
        }
        CHECK_GT(unsatisfied, 0);
        VLOG(3) << "  - unsatisfied = " << unsatisfied;
        if (unsatisfied < best_unsatisfied) {
          best_unsatisfied = unsatisfied;
          to_correct = spec;
        }
      }
      VLOG(3) << "Lifting " << to_correct->DebugString() << " with "
              << best_unsatisfied << " unsatisfied dependencies";
      const NodeSet& required = to_correct->require_map();
      for (ConstIter<NodeSet> def(required); !def.at_end(); ++def) {
        AstNode* const dep = Copy(*def);
        if (!ContainsKey(defined, dep)) {
          candidates->erase(dep);
          defined.insert(dep);
          forced.insert(dep);
          VLOG(3) << "removing " << dep->DebugString()
                  << " from set of candidates and forcing as defined";
        }
      }
    } else {
      for (int i = 0; i < inserted.size(); ++i) {
        to_insert.erase(inserted[i]);
      }
    }
  }

  // Push the rest.
  for (int i = 0; i < no_defines.size(); ++i) {
    if (!no_defines[i]->Nullified()) {
      constraints_[index++] = no_defines[i];
    }
  }

  CHECK_EQ(index, size - nullified);

  VLOG(2) << "Detecting computed variables";
  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    VLOG(3) << i << " -> " << spec->DebugString();
    MarkComputedVariables(spec, computed_variables);
  }
}

void ParserState::BuildModel(const NodeSet& candidates,
                             const NodeSet& computed_variables) {
  FZLOG << "Creating variables" << std::endl;
  int created = 0;
  int skipped = 0;

  int array_index = 0;
  for (unsigned int i = 0; i < int_variables_.size(); i++) {
    VLOG(2) << "xi(" << i << ") -> " << int_variables_[i]->DebugString();
    if (!hadError) {
      const std::string& name = int_variables_[i]->Name();
      AstNode* const var = IntCopy(i);
      if (!ContainsKey(candidates, var) && !ContainsKey(int_aliases_, i)) {
        const bool active =
            !IsIntroduced(var) && !ContainsKey(computed_variables, var);
        model_->NewIntVar(name, int_variables_[i], active);
        created++;
      } else {
        model_->SkipIntVar();
        VLOG(2) << "  - skipped";
        if (int_variables_[i]->HasDomain()) {
          AddIntVarDomainConstraint(i, int_variables_[i]->Domain()->Copy());
        }
        skipped++;
      }
    }
  }
  FZLOG << "  - " << created << " integer variables created" << std::endl;
  FZLOG << "  - " << skipped << " integer variables skipped" << std::endl;

  created = 0;
  skipped = 0;

  array_index = 0;
  for (unsigned int i = 0; i < bool_variables_.size(); i++) {
    AstNode* const var = BoolCopy(i);
    VLOG(2) << var->DebugString() << " -> "
            << bool_variables_[i]->DebugString();
    if (!hadError) {
      const std::string& raw_name = bool_variables_[i]->Name();
      std::string name;
      if (raw_name[0] == '[') {
        name = StringPrintf("%s[%d]", raw_name.c_str() + 1, ++array_index);
      } else {
        if (array_index == 0) {
          name = raw_name;
        } else {
          name = StringPrintf("%s[%d]", raw_name.c_str(), array_index + 1);
          array_index = 0;
        }
      }
      if (!ContainsKey(candidates, var)) {
        model_->NewBoolVar(name, bool_variables_[i]);
        created++;
      } else {
        model_->SkipBoolVar();
        VLOG(2) << "  - skipped";
        skipped++;
      }
    }
  }
  FZLOG << "  - " << created << " boolean variables created" << std::endl;
  FZLOG << "  - " << skipped << " boolean variables skipped" << std::endl;

  array_index = 0;
  for (unsigned int i = 0; i < set_variables_.size(); i++) {
    if (!hadError) {
      const std::string& raw_name = set_variables_[i]->Name();
      std::string name;
      if (raw_name[0] == '[') {
        name = StringPrintf("%s[%d]", raw_name.c_str() + 1, ++array_index);
      } else {
        if (array_index == 0) {
          name = raw_name;
        } else {
          name = StringPrintf("%s[%d]", raw_name.c_str(), array_index + 1);
          array_index = 0;
        }
      }
      model_->NewSetVar(name, set_variables_[i]);
    }
  }

  FZLOG << "Creating constraints" << std::endl;
  for (unsigned int i = 0; i < constraints_.size(); i++) {
    if (!hadError) {
      CtSpec* const spec = constraints_[i];
      VLOG(2) << "Constraint " << spec->DebugString();
      model_->PostConstraint(spec);
    }
  }
  FZLOG << "  - " << constraints_.size() << " constraints created" << std::endl;

  FZLOG << "Populating aliases" << std::endl;
  int aliases = 0;
  for (int i = 0; i < int_variables_.size(); ++i) {
    if (ContainsKey(int_aliases_, i)) {
      IntExpr* const expr = model_->GetIntExpr(IntCopy(int_aliases_[i]));
      CHECK(expr != nullptr);
      model_->CheckIntegerVariableIsNull(IntCopy(i));
      model_->SetIntegerExpression(IntCopy(i), expr);
      VLOG(2) << "  - setting x(" << i << ") to be " << expr->DebugString();
      aliases++;
    }
  }
  FZLOG << "  - " << aliases << " aliases filled" << std::endl;

  FZLOG << "Adding domain constraints" << std::endl;

  for (unsigned int i = int_domain_constraints_.size(); i--;) {
    if (!hadError) {
      AstNode* const var_node = int_domain_constraints_[i].first;
      IntExpr* const expr = model_->GetIntExpr(var_node);
      if (expr->IsVar()) {
        IntVar* const var = expr->Var();
        AstSetLit* const dom = int_domain_constraints_[i].second;
        if (dom->interval &&
            (dom->imin > var->Min() || dom->imax < var->Max())) {
          VLOG(2) << "Reduce integer variable " << var->DebugString() << " to "
                  << dom->DebugString();
          var->SetRange(dom->imin, dom->imax);
        } else if (!dom->interval) {
          VLOG(2) << "Reduce integer variable " << var->DebugString() << " to "
                  << dom->DebugString();
          var->SetValues(dom->s);
        }
      } else {
        AstSetLit* const dom = int_domain_constraints_[i].second;
        Solver* const s = expr->solver();
        if (dom->interval &&
            (dom->imin > expr->Min() || dom->imax < expr->Max())) {
          VLOG(2) << "Reduce integer expression " << expr->DebugString()
                  << " to " << dom->DebugString();
          s->AddConstraint(s->MakeBetweenCt(expr->Var(), dom->imin, dom->imax));
        } else if (!dom->interval) {
          VLOG(2) << "Reduce integer expression " << expr->DebugString()
                  << " to " << dom->DebugString();
          s->AddConstraint(s->MakeMemberCt(expr->Var(), dom->s));
        }
      }
    }
  }
}

void ParserState::AnalyseAndCreateModel() {
  model_->InitSolver();
  Presolve();
  NodeSet candidates;
  NodeSet computed_variables;
  SortConstraints(&candidates, &computed_variables);
  BuildModel(candidates, computed_variables);
}

AstNode* ParserState::ArrayElement(std::string id, unsigned int offset) {
  if (offset > 0) {
    std::vector<int64> tmp;
    if (Get(int_var_array_map_, id, tmp) && offset <= tmp.size())
      return new AstIntVar(tmp[offset - 1]);
    if (Get(bool_var_array_map_, id, tmp) && offset <= tmp.size())
      return new AstBoolVar(tmp[offset - 1]);
    if (Get(set_var_array_map_, id, tmp) && offset <= tmp.size())
      return new AstSetVar(tmp[offset - 1]);

    if (Get(int_value_array_map_, id, tmp) && offset <= tmp.size())
      return new AstIntLit(tmp[offset - 1]);
    if (Get(bool_value_array_map_, id, tmp) && offset <= tmp.size())
      return new AstBoolLit(tmp[offset - 1]);
    std::vector<AstSetLit> tmpS;
    if (Get(set_value_array_map_, id, tmpS) && offset <= tmpS.size())
      return new AstSetLit(tmpS[offset - 1]);
  }

  LOG(ERROR) << "Error: array access to " << id << " invalid"
             << " in line no. " << orfz_get_lineno(yyscanner);
  hadError = true;
  return new AstIntVar(0);  // keep things consistent
}

AstNode* ParserState::VarRefArg(std::string id, bool annotation) {
  int64 tmp;
  if (Get(int_var_map_, id, tmp)) return new AstIntVar(tmp);
  if (Get(bool_var_map_, id, tmp)) return new AstBoolVar(tmp);
  if (Get(set_var_map_, id, tmp)) return new AstSetVar(tmp);
  if (annotation) return new AstAtom(id);
  LOG(ERROR) << "Error: undefined variable " << id << " in line no. "
             << orfz_get_lineno(yyscanner);
  hadError = true;
  return new AstIntVar(0);  // keep things consistent
}

void ParserState::AddIntVarDomainConstraint(int var_id, AstSetLit* const dom) {
  if (dom != nullptr) {
    VLOG(2) << "  - adding int var domain constraint (" << var_id
            << ") : " << dom->DebugString();
    int_domain_constraints_.push_back(
        std::make_pair(new AstIntVar(var_id), dom));
  }
}

void ParserState::AddBoolVarDomainConstraint(int var_id, AstSetLit* const dom) {
  if (dom != nullptr) {
    VLOG(2) << "  - adding bool var domain constraint (" << var_id
            << ") : " << dom->DebugString();
    int_domain_constraints_.push_back(
        std::make_pair(new AstBoolVar(var_id), dom));
  }
}

void ParserState::AddSetVarDomainConstraint(int var_id, AstSetLit* const dom) {
  if (dom != nullptr) {
    VLOG(2) << "  - adding set var domain constraint (" << var_id
            << ") : " << dom->DebugString();
    set_domain_constraints_.push_back(std::make_pair(var_id, dom));
  }
}

IntVarSpec* ParserState::IntSpec(AstNode* const node) const {
  int index = node->getIntVar();
  while (int_variables_[index]->alias) {
    index = int_variables_[index]->i;
  }
  return int_variables_[index];
}

bool ParserState::IsBound(AstNode* const node) const {
  return node->isInt() || (node->isIntVar() && IntSpec(node)->IsBound()) ||
         node->isBool() ||
         (node->isBoolVar() && bool_variables_[node->getBoolVar()]->IsBound());
}

bool ParserState::IsPositive(AstNode* const node) const {
  return (node->isInt() && node->getInt() >= 0) ||
      (node->isIntVar() && IntSpec(node)->IsPositive()) ||
         node->isBool() || node->isBoolVar();
}

bool ParserState::IsIntroduced(AstNode* const node) const {
  return (node->isIntVar() && int_variables_[node->getIntVar()]->introduced) ||
         (node->isBoolVar() && bool_variables_[node->getBoolVar()]->introduced);
}

bool ParserState::IsAlias(AstNode* const node) const {
  if (node->isIntVar()) {
    return int_variables_[node->getIntVar()]->alias;
  } else if (node->isBoolVar()) {
    return bool_variables_[node->getBoolVar()]->alias;
  }
  return false;
}

bool ParserState::IsBoolean(AstNode* const node) const {
  return (
      node->isBool() || node->isBoolVar() ||
      (node->isInt() && (node->getInt() == 0 || node->getInt() == 1)) ||
      (node->isIntVar() && ContainsKey(bool2int_vars_, node->getIntVar())) ||
      (node->isIntVar() && IntSpec(node)->IsBoolean()));
}

int64 ParserState::GetBound(AstNode* const node) const {
  if (node->isInt()) {
    return node->getInt();
  }
  if (node->isIntVar()) {
    return IntSpec(node)->GetBound();
  }
  if (node->isBool()) {
    return node->getBool();
  }
  if (node->isBoolVar()) {
    return bool_variables_[node->getBoolVar()]->GetBound();
  }
  return 0;
}

bool ParserState::IsAllDifferent(AstNode* const node) const {
  AstArray* const array_variables = node->getArray();
  const int size = array_variables->a.size();
  std::vector<int> variables(size);
  for (int i = 0; i < size; ++i) {
    if (array_variables->a[i]->isIntVar()) {
      const int var = array_variables->a[i]->getIntVar();
      variables[i] = var;
    } else {
      return false;
    }
  }
  std::sort(variables.begin(), variables.end());

  // Naive.
  for (int i = 0; i < all_differents_.size(); ++i) {
    const std::vector<int>& v = all_differents_[i];
    if (v.size() != size) {
      continue;
    }
    bool ok = true;
    for (int i = 0; i < size; ++i) {
      if (v[i] != variables[i]) {
        ok = false;
        continue;
      }
    }
    if (ok) {
      return true;
    }
  }
  return false;
}

bool ParserState::MergeIntDomain(IntVarSpec* const source,
                                 IntVarSpec* const dest) {
  VLOG(2) << "  - merge " << dest->DebugString() << " with "
          << source->DebugString();
  if (source->assigned) {
    return dest->MergeBounds(source->i, source->i);
  } else if (source->HasDomain()) {
    AstSetLit* const domain = source->Domain();
    if (domain->interval) {
      return dest->MergeBounds(domain->imin, domain->imax);
    } else {
      return dest->MergeDomain(domain->s);
    }
  }
  return true;
}

bool ParserState::DiscoverAliases(CtSpec* const spec) {
  const std::string& id = spec->Id();
  if (id == "int_eq") {
    if (spec->Arg(0)->isIntVar() && spec->Arg(1)->isIntVar() &&
        !ContainsKey(stored_constraints_, spec)) {
      AstNode* const var0 = Copy(spec->Arg(0));
      AstNode* const var1 = Copy(spec->Arg(1));
      IntVarSpec* const spec0 = IntSpec(var0);
      IntVarSpec* const spec1 = IntSpec(var1);
      MergeIntDomain(spec0, spec1);
      MergeIntDomain(spec1, spec0);
      if (spec->annotations() == nullptr &&
          (ContainsKey(orphans_, Copy(spec->Arg(0))) ||
           ContainsKey(orphans_, Copy(spec->Arg(1))))) {
        if (ContainsKey(orphans_, var0)) {
          AstCall* const call =
              new AstCall("defines_var", new AstIntVar(var0->getIntVar()));
          spec->AddAnnotation(call);
          VLOG(2) << "  - presolve:  aliasing " << var0->DebugString() << " to "
                  << var1->DebugString();
          orphans_.erase(var0);
          targets_.insert(var0);
          return true;
        } else if (ContainsKey(orphans_, var1)) {
          AstCall* const call =
              new AstCall("defines_var", new AstIntVar(var1->getIntVar()));
          spec->AddAnnotation(call);
          VLOG(2) << "  - presolve:  aliasing " << var1->DebugString() << " to "
                  << var0->DebugString();
          orphans_.erase(var1);
          targets_.insert(var1);
          return true;
        }
      } else if (spec->annotations() == nullptr &&
                 (!ContainsKey(targets_, Copy(spec->Arg(0))) ||
                  !ContainsKey(targets_, Copy(spec->Arg(1))))) {
        if (!ContainsKey(targets_, var0)) {
          AstCall* const call =
              new AstCall("defines_var", new AstIntVar(var0->getIntVar()));
          spec->AddAnnotation(call);
          VLOG(2) << "  - presolve:  force aliasing " << var0->DebugString()
                  << " to " << var1->DebugString();
          targets_.insert(var0);
          return true;
        } else if (!ContainsKey(targets_, var1)) {
          AstCall* const call =
              new AstCall("defines_var", new AstIntVar(var1->getIntVar()));
          spec->AddAnnotation(call);
          VLOG(2) << "  - presolve:  force aliasing " << var1->DebugString()
                  << " to " << var0->DebugString();
          targets_.insert(var1);
          return true;
        }
      }
    }
  }
  return false;
}

bool ParserState::PresolveOneConstraint(CtSpec* const spec) {
  const std::string& id = spec->Id();
  if (id == "int_le") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(0));
      const int64 bound = GetBound(spec->Arg(1));
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString()
              << " with kint32min.." << bound;
      const bool ok = var_spec->MergeBounds(kint32min, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(1));
      const int64 bound = GetBound(spec->Arg(0));
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString() << " with "
              << bound << "..kint32max";
      const bool ok = var_spec->MergeBounds(bound, kint32max);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    }
  }
  if (id == "int_lt") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(0));
      const int64 bound = GetBound(spec->Arg(1)) - 1;
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString()
              << " with kint32min.." << bound;
      const bool ok = var_spec->MergeBounds(kint32min, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(1));
      const int64 bound = GetBound(spec->Arg(0)) + 1;
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString() << " with "
              << bound << "..kint32max";
      const bool ok = var_spec->MergeBounds(bound, kint32max);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    }
  }
  if (id == "int_ge") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(0));
      const int64 bound = GetBound(spec->Arg(1));
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString()
              << " with " << bound << "..kint32max" << bound;
      const bool ok = var_spec->MergeBounds(bound, kint32max);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(1));
      const int64 bound = GetBound(spec->Arg(0));
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString()
              << " with kint32min.." << bound;
      const bool ok = var_spec->MergeBounds(kint32min, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    }
  }
  if (id == "int_gt") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(0));
      const int64 bound = GetBound(spec->Arg(1)) + 1;
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString()
              << " with " << bound << "..kint32max" << bound;
      const bool ok = var_spec->MergeBounds(bound, kint32max);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(1));
      const int64 bound = GetBound(spec->Arg(0)) - 1;
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString()
              << " with kint32min.." << bound;
      const bool ok = var_spec->MergeBounds(kint32min, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    }
  }
  if (id == "int_eq") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(0));
      const int64 bound = GetBound(spec->Arg(1));
      VLOG(2) << "  - presolve:  assign " << var_spec->DebugString() << " to "
              << bound;
      const bool ok = var_spec->MergeBounds(bound, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(1));
      const int64 bound = GetBound(spec->Arg(0));
      VLOG(2) << "  - presolve:  assign " << var_spec->DebugString() << " to "
              << bound;
      const bool ok = var_spec->MergeBounds(bound, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    }
  }
  if (id == "int_ne") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(0));
      const int64 bound = GetBound(spec->Arg(1));
      VLOG(2) << "  - presolve:  remove value " << bound << " from "
              << var_spec->DebugString();
      const bool ok = var_spec->RemoveValue(bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(1));
      const int64 bound = GetBound(spec->Arg(0));
      VLOG(2) << "  - presolve:  remove value " << bound << " from "
              << var_spec->DebugString();
      const bool ok = var_spec->RemoveValue(bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    }
  }
  if (id == "set_in") {
    if (spec->Arg(0)->isIntVar() && spec->Arg(1)->isSet()) {
      IntVarSpec* const var_spec = IntSpec(spec->Arg(0));
      AstSetLit* const domain = spec->Arg(1)->getSet();
      VLOG(2) << "  - presolve:  merge " << var_spec->DebugString() << " with "
              << domain->DebugString();
      bool ok = false;
      if (domain->interval) {
        ok = var_spec->MergeBounds(domain->imin, domain->imax);
      } else {
        ok = var_spec->MergeDomain(domain->s);
      }
      if (ok) {
        spec->Nullify();
      }
      return ok;
    }
  }
  if (id == "array_bool_and" && IsBound(spec->Arg(1)) &&
      GetBound(spec->Arg(1)) == 1) {
    VLOG(2) << "  - presolve:  forcing array_bool_and to 1 on "
            << spec->DebugString();
    AstArray* const array_variables = spec->Arg(0)->getArray();
    const int size = array_variables->a.size();
    for (int i = 0; i < size; ++i) {
      if (array_variables->a[i]->isBoolVar()) {
        const int boolvar = array_variables->a[i]->getBoolVar();
        bool_variables_[boolvar]->Assign(true);
      }
    }
    spec->Nullify();
    return true;
  }
  if (id == "array_bool_or" && IsBound(spec->Arg(1)) &&
      GetBound(spec->Arg(1)) == 0) {
    VLOG(2) << "  - presolve:  forcing array_bool_or to 0 on "
            << spec->DebugString();
    AstArray* const array_variables = spec->Arg(0)->getArray();
    const int size = array_variables->a.size();
    for (int i = 0; i < size; ++i) {
      if (array_variables->a[i]->isBoolVar()) {
        const int boolvar = array_variables->a[i]->getBoolVar();
        bool_variables_[boolvar]->Assign(false);
      }
    }
    spec->Nullify();
    return true;
  }
  if (id.find("_reif") != std::string::npos && IsBound(spec->LastArg()) &&
      GetBound(spec->LastArg()) == 1) {
    VLOG(2) << "  - presolve:  unreify " << spec->DebugString();
    spec->Unreify();
    return true;
  }
  if (id.find("_reif") != std::string::npos && IsBound(spec->LastArg()) &&
      GetBound(spec->LastArg()) == 0) {
    VLOG(2) << "  - presolve:  unreify and inverse " << spec->DebugString();
    spec->Unreify();
    if (id == "int_eq") {
      spec->SetId("int_ne");
    } else if (id == "int_ne") {
      spec->SetId("int_eq");
    } else if (id == "int_ge") {
      spec->SetId("int_lt");
    } else if (id == "int_gt") {
      spec->SetId("int_le");
    } else if (id == "int_le") {
      spec->SetId("int_gt");
    } else if (id == "int_lt") {
      spec->SetId("int_ge");
    } else if (id == "int_lin_eq") {
      spec->SetId("int_lin_ne");
    } else if (id == "int_lin_ne") {
      spec->SetId("int_lin_eq");
    } else if (id == "int_lin_ge") {
      spec->SetId("int_lin_lt");
    } else if (id == "int_lin_gt") {
      spec->SetId("int_lin_le");
    } else if (id == "int_lin_le") {
      spec->SetId("int_lin_gt");
    } else if (id == "int_lin_lt") {
      spec->SetId("int_lin_ge");
    } else if (id == "bool_eq") {
      spec->SetId("bool_ne");
    } else if (id == "bool_ne") {
      spec->SetId("bool_eq");
    } else if (id == "bool_ge") {
      spec->SetId("bool_lt");
    } else if (id == "bool_gt") {
      spec->SetId("bool_le");
    } else if (id == "bool_le") {
      spec->SetId("bool_gt");
    } else if (id == "bool_lt") {
      spec->SetId("bool_ge");
    }
    return true;
  }
  if (id == "all_different_int" && !ContainsKey(stored_constraints_, spec)) {
    AstArray* const array_variables = spec->Arg(0)->getArray();
    const int size = array_variables->a.size();
    std::vector<int> variables(size);
    for (int i = 0; i < size; ++i) {
      if (array_variables->a[i]->isIntVar()) {
        const int var = array_variables->a[i]->getIntVar();
        variables[i] = var;
      } else {
        return false;
      }
    }
    VLOG(2) << "  - presolve:  store all diff info " << spec->DebugString();
    std::sort(variables.begin(), variables.end());
    stored_constraints_.insert(spec);
    all_differents_.push_back(variables);
    return true;
  }
  if (id == "array_var_int_element" && IsBound(spec->Arg(2)) &&
      IsAllDifferent(spec->Arg(1))) {
    VLOG(2) << "  - presolve:  reinforce " << spec->DebugString()
            << " to array_var_int_position";
    spec->SetId("array_var_int_position");
    const int64 bound = GetBound(spec->Arg(2));
    spec->ReplaceArg(2, new AstIntLit(bound));
    return true;
  }
  if (id == "int_abs" && !ContainsKey(stored_constraints_, spec) &&
      spec->Arg(0)->isIntVar() && spec->Arg(1)->isIntVar()) {
    abs_map_[spec->Arg(1)->getIntVar()] = spec->Arg(0)->getIntVar();
    stored_constraints_.insert(spec);
    return true;
  }
  if (id == "bool2int" && !ContainsKey(stored_constraints_, spec) &&
      spec->Arg(0)->isBoolVar() && spec->Arg(1)->isIntVar()) {
    bool2int_vars_.insert(spec->Arg(1)->getIntVar());
    stored_constraints_.insert(spec);
    return true;
  }
  if (id == "int_eq_reif") {
    if (spec->Arg(0)->isIntVar() &&
        ContainsKey(abs_map_, spec->Arg(0)->getIntVar()) &&
        spec->Arg(1)->isInt() && spec->Arg(1)->getInt() == 0) {
      VLOG(2) << "  - presolve:  remove abs() in " << spec->DebugString();
      dynamic_cast<AstIntVar*>(spec->Arg(0))->i =
          abs_map_[spec->Arg(0)->getIntVar()];
    }
  }
  if (id == "int_ne_reif") {
    if (spec->Arg(0)->isIntVar() &&
        ContainsKey(abs_map_, spec->Arg(0)->getIntVar()) &&
        spec->Arg(1)->isInt() && spec->Arg(1)->getInt() == 0) {
      VLOG(2) << "  - presolve:  remove abs() in " << spec->DebugString();
      dynamic_cast<AstIntVar*>(spec->Arg(0))->i =
          abs_map_[spec->Arg(0)->getIntVar()];
    }
  }
  if (id == "int_ne") {
    if (spec->Arg(0)->isIntVar() &&
        ContainsKey(abs_map_, spec->Arg(0)->getIntVar()) &&
        spec->Arg(1)->isInt() && spec->Arg(1)->getInt() == 0) {
      VLOG(2) << "  - presolve:  remove abs() in " << spec->DebugString();
      dynamic_cast<AstIntVar*>(spec->Arg(0))->i =
          abs_map_[spec->Arg(0)->getIntVar()];
    }
  }
  if (id == "int_lin_le") {
    AstArray* const array_coefficients = spec->Arg(0)->getArray();
    AstNode* const node_rhs = spec->Arg(2);
    const int64 rhs = node_rhs->getInt();
    const int size = array_coefficients->a.size();
    bool one_positive = false;
    for (int i = 0; i < size; ++i) {
      if (array_coefficients->a[i]->getInt() > 0) {
        one_positive = true;
        break;
      }
    }
    if (!one_positive && size > 0) {
      VLOG(2) << "  - presolve:  transform all negative int_lin_le into "
              << "int_lin_ge in " << spec->DebugString();

      for (int i = 0; i < size; ++i) {
        array_coefficients->a[i]->setInt(-array_coefficients->a[i]->getInt());
      }
      spec->Arg(2)->setInt(-spec->Arg(2)->getInt());
      spec->SetId("int_lin_ge");
      return true;
    }
    // Check for all positive to do bound propagation.
    bool all_positive = true;
    AstArray* const array_variables = spec->Arg(1)->getArray();
    for (int i = 0; i < size; ++i) {
      if (array_coefficients->a[i]->getInt() < 0) {
        all_positive = false;
        break;
      }
      IntVarSpec* const spec =
          int_variables_[array_variables->a[i]->getIntVar()];
      if (!spec->HasDomain() || !spec->Domain()->interval ||
          spec->Domain()->imin < 0) {
        all_positive = false;
        break;
      }
    }
    if (all_positive && rhs >= 0) {
      for (int i = 0; i < size; ++i) {
        IntVarSpec* const spec =
            int_variables_[array_variables->a[i]->getIntVar()];
        const int64 vmax = spec->Domain()->imax;
        const int64 vcoeff = array_coefficients->a[i]->getInt();
        if (vcoeff > 0) {
          const int64 bound = rhs / vcoeff;
          if (vmax > bound) {
            VLOG(2) << "  - presolve:  merge " << spec->DebugString()
                    << " with 0.." << bound;
            spec->MergeBounds(0, bound);
          }
        }
      }
    }
  }
  if (id == "int_lin_le_reif") {
    AstArray* const array_coefficients = spec->Arg(0)->getArray();
    AstArray* const array_variables = spec->Arg(1)->getArray();
    const int size = array_coefficients->a.size();
    bool one_positive = false;
    for (int i = 0; i < size; ++i) {
      if (array_coefficients->a[i]->getInt() > 0) {
        one_positive = true;
        break;
      }
    }
    bool all_ones = true;
    for (int i = 0; i < size; ++i) {
      if (array_coefficients->a[i]->getInt() != 1) {
        all_ones = false;
        break;
      }
    }
    bool all_booleans = true;
    for (int i = 0; i < size; ++i) {
      if (!IsBoolean(array_variables->a[i])) {
        all_booleans = false;
        break;
      }
    }
    if (!one_positive && size > 0) {
      VLOG(2) << "  - presolve:  transform all negative int_lin_le_reif into "
              << "int_lin_ge_reif in " << spec->DebugString();
      for (int i = 0; i < size; ++i) {
        array_coefficients->a[i]->setInt(-array_coefficients->a[i]->getInt());
      }
      spec->Arg(2)->setInt(-spec->Arg(2)->getInt());
      spec->SetId("int_lin_ge_reif");
      return true;
    }
  }
  if (id == "int_lin_lt") {
    spec->Arg(2)->setInt(spec->Arg(2)->getInt() - 1);
    spec->SetId("int_lin_le");
    return true;
  }
  if (id == "int_lin_lt_reif") {
    spec->Arg(2)->setInt(spec->Arg(2)->getInt() - 1);
    spec->SetId("int_lin_le_reif");
    return true;
  }
  if (id == "int_lin_gt") {
    spec->Arg(2)->setInt(spec->Arg(2)->getInt() + 1);
    spec->SetId("int_lin_ge");
    return true;
  }
  if (id == "int_lin_gt_reif") {
    spec->Arg(2)->setInt(spec->Arg(2)->getInt() + 1);
    spec->SetId("int_lin_ge_reif");
    return true;
  }
  if (id == "int_lin_eq") {
    AstArray* const array_coefficients = spec->Arg(0)->getArray();
    AstArray* const array_variables = spec->Arg(1)->getArray();
    AstNode* const node_rhs = spec->Arg(2);
    const int64 rhs = node_rhs->getInt();
    const int size = array_coefficients->a.size();
    bool one_positive = false;
    for (int i = 0; i < size; ++i) {
      if (array_coefficients->a[i]->getInt() > 0) {
        one_positive = true;
        break;
      }
    }
    int num_bounds = 0;
    int not_bound_index = -1;
    int64 new_rhs = rhs;
    for (int i = 0; i < size; ++i) {
      if (array_coefficients->a[i]->getInt() == 0 ||
          IsBound(array_variables->a[i])) {
        num_bounds++;
        new_rhs -= GetBound(array_variables->a[i]) *
            array_coefficients->a[i]->getInt();
      } else {
        not_bound_index = i;
      }
    }
    if (num_bounds == size - 1) {
      DCHECK_NE(not_bound_index, -1);
      IntVarSpec* const var_spec = IntSpec(array_variables->a[not_bound_index]);
      const int64 coefficient =
          array_coefficients->a[not_bound_index]->getInt();
      DCHECK_NE(coefficient, 0);
      if (new_rhs % coefficient == 0) {
        const int64 bound = new_rhs / coefficient;
        VLOG(2) << "  - presolve:  assign " << var_spec->DebugString() << " to "
                << bound;
        const bool ok = var_spec->MergeBounds(bound, bound);
        if (ok) {
          spec->Nullify();
          return ok;
        }
      }
    }
    if (!one_positive && !HasDefineAnnotation(spec->annotations()) &&
        size > 0) {
      VLOG(2) << "  - presolve:  transform all negative int_lin_eq into "
              << "int_lin_eq in " << spec->DebugString();

      for (int i = 0; i < size; ++i) {
        array_coefficients->a[i]->setInt(-array_coefficients->a[i]->getInt());
      }
      spec->Arg(2)->setInt(-spec->Arg(2)->getInt());
      return true;
    }
    // Check for all positive to do bound propagation.
    bool all_positive = true;
    for (int i = 0; i < size; ++i) {
      if (array_coefficients->a[i]->getInt() < 0) {
        all_positive = false;
        break;
      }
      if (!IsPositive(array_variables->a[i])) {
        all_positive = false;
        break;
      }
    }
    if (all_positive && rhs >= 0) {
      for (int i = 0; i < size; ++i) {
        AstNode* const node = array_variables->a[i];
        if (node->isIntVar()) {
          IntVarSpec* const spec =
              int_variables_[array_variables->a[i]->getIntVar()];
          if (spec->HasDomain()) {
            const int64 vmax = spec->Domain()->imax;
            const int64 vcoeff = array_coefficients->a[i]->getInt();
            if (vcoeff > 0) {
              const int64 bound = rhs / vcoeff;
              if (vmax > bound) {
                VLOG(2) << "  - presolve:  merge " << spec->DebugString()
                        << " with 0.." << bound;
                spec->MergeBounds(0, bound);
              }
            }
          }
        }
      }
    }
  }
  if (id == "int_lin_eq_reif") {
    AstArray* const array_coefficients = spec->Arg(0)->getArray();
    const int size = array_coefficients->a.size();
    bool one_positive = false;
    for (int i = 0; i < size; ++i) {
      if (array_coefficients->a[i]->getInt() > 0) {
        one_positive = true;
        break;
      }
    }
    if (!one_positive && size > 0) {
      VLOG(2) << "  - presolve:  transform all negative int_lin_eq_reif into "
              << "int_lin_eq_reif in " << spec->DebugString();
      for (int i = 0; i < size; ++i) {
        array_coefficients->a[i]->setInt(-array_coefficients->a[i]->getInt());
      }
      spec->Arg(2)->setInt(-spec->Arg(2)->getInt());
      return true;
    }
  }
  if (id == "bool_eq_reif") {
    if (spec->Arg(0)->isBoolVar() && spec->Arg(1)->isBool()) {
      VLOG(2) << "  - presolve:  simplify bool_eq_reif in "
              << spec->DebugString();
      if (spec->Arg(1)->getBool()) {  // == 1
        spec->RemoveArg(1);
        spec->SetId("bool_eq");
      } else {
        spec->RemoveArg(1);
        spec->SetId("bool_not");
      }
    }
  }
  if (id == "bool_ne_reif") {
    if (spec->Arg(0)->isBoolVar() && spec->Arg(1)->isBool()) {
      VLOG(2) << "  - presolve:  simplify bool_ne_reif in "
              << spec->DebugString();
      if (spec->Arg(1)->getBool()) {  // == 1
        spec->RemoveArg(1);
        spec->SetId("bool_not");
      } else {
        spec->RemoveArg(1);
        spec->SetId("bool_eq");
      }
    }
  }
  if (id == "int_div") {
    if (IsBound(spec->Arg(0)) && IsBound(spec->Arg(1))) {
      const int64 div = GetBound(spec->Arg(0)) / GetBound(spec->Arg(1));
      if (spec->Arg(2)->isIntVar()) {
        IntVarSpec* const var_spec = int_variables_[spec->Arg(2)->getIntVar()];
        VLOG(2) << "  - presolve:  assign " << var_spec->DebugString() << " to "
                << div;
        if (var_spec->MergeBounds(div, div)) {
          spec->Nullify();
          return true;
        }
      }
    }
  }
  if (id == "int_times") {
    if (IsBound(spec->Arg(0)) && IsBound(spec->Arg(1))) {
      const int64 div = GetBound(spec->Arg(0)) * GetBound(spec->Arg(1));
      if (spec->Arg(2)->isIntVar()) {
        IntVarSpec* const var_spec = int_variables_[spec->Arg(2)->getIntVar()];
        VLOG(2) << "  - presolve:  assign " << var_spec->DebugString() << " to "
                << div;
        if (var_spec->MergeBounds(div, div)) {
          spec->Nullify();
          return true;
        }
      }
    }
  }
  if (id == "regular") {
    if (spec->Arg(1)->getInt() == 1) {
      // One state, this is a constant table.
      AstArray* const array_variables = spec->Arg(0)->getArray();
      const int size = array_variables->a.size();
      const int64 num_states = spec->Arg(1)->getInt();
      const int64 num_values = spec->Arg(2)->getInt();
      const AstArray* const array_transitions = spec->Arg(3)->getArray();
      const int64 initial_state = spec->Arg(4)->getInt();
      // Find the value that is stable w.r.t. the transitions.
      int count = 0;
      bool value_defined = false;
      int64 value = 0;
      for (int q = 1; q <= num_states; ++q) {
        for (int s = 1; s <= num_values; ++s) {
          const int64 next = array_transitions->a[count++]->getInt();
          if (next == initial_state && q == initial_state) {
            if (value_defined) {
              // Many value are stables. We abort.
              return false;
            } else {
              value = s;
              value_defined = true;
            }
          }
        }
      }
      if (!value_defined) {
        return false;
      }

      AstSetLit* const final_set = spec->Arg(5)->getSet();
      std::vector<int64> final_states;
      if (final_set->interval) {
        for (int v = final_set->imin; v <= final_set->imax; ++v) {
          final_states.push_back(v);
        }
      } else {
        final_states.insert(
            final_states.end(), final_set->s.begin(), final_set->s.end());
      }
      if (final_states.size() == 1 && final_states.back() == initial_state) {
        VLOG(2) << "  - presolve:  regular, one state and one stable value: "
                << value << " for " << spec->DebugString();
        // All variables should be set to the stable value w.r.t transisions.
        bool all_succeed = true;
        for (int i = 0; i < size; ++i) {
          AstNode* var_node = array_variables->a[i];
          if (var_node->isIntVar()) {
            IntVarSpec* const var_spec = int_variables_[var_node->getIntVar()];
            VLOG(2) << "               assign " << var_spec->DebugString()
                    << " to " << value;
            if (!var_spec->MergeBounds(value, value)) {
              all_succeed = false;
            }
          } else if (!var_node->isInt() || var_node->getInt() != value) {
            all_succeed = false;
          }
        }
        if (all_succeed) {
          spec->Nullify();
        }
      }
    }
  }
  return false;
}

void ParserState::RegroupAux(const std::string& ct_id, int start_index,
                             int end_index, int output_var_index,
                             const std::vector<int>& indices) {
  if (indices.size() == 1) {
    CtSpec* const spec = constraints_[start_index];
    VLOG(2) << "  - presolve:  simplify " << spec->DebugString();
    spec->RemoveArg(1);
    spec->SetId("int_eq");
  } else {
    // Check the variables are not used elsewhere.
    VLOG(2) << "  - regroup " << ct_id << "(" << start_index << ".."
            << end_index << "), output = " << output_var_index
            << ", contains = [" << IntVectorToString(indices, ", ") << "]";
    for (int i = start_index; i < end_index; ++i) {
      const int intermediate = constraints_[i]->Arg(2)->getIntVar();
      if (constraints_per_int_variables_[intermediate].size() > 2) {
        VLOG(2) << "    * aborted because of xi(" << intermediate << ")";
        return;
      }
    }
    for (int i = start_index + 1; i <= end_index; ++i) {
      constraints_[i]->Nullify();
    }
    delete constraints_[start_index];
    std::vector<AstNode*> max_args;
    max_args.push_back(new AstIntVar(output_var_index));
    std::vector<AstNode*> max_array;
    for (int i = 0; i < indices.size(); ++i) {
      max_array.push_back(new AstIntVar(indices[i]));
    }
    max_args.push_back(new AstArray(max_array));
    AstArray* const args = new AstArray(max_args);
    constraints_[start_index] = new CtSpec(start_index, ct_id, args, nullptr);
    VLOG(2) << "    + created " << constraints_[start_index]->DebugString();
  }
}

void ParserState::Regroup(const std::string& ct_id, const std::vector<int>& ct_indices) {
  int start_index = -1;
  int end_index = -1;
  std::vector<int> variables;
  int carry_over = -1;
  for (int i = 0; i < ct_indices.size(); ++i) {
    const int constraint_index = ct_indices[i];
    CtSpec* const spec = constraints_[constraint_index];
    if (spec->Nullified()) {
      continue;
    }
    if (spec->Arg(0)->isIntVar() && spec->Arg(1)->isIntVar() &&
        spec->Arg(0)->getIntVar() == spec->Arg(1)->getIntVar() &&
        FindTarget(spec->annotations()) != nullptr &&
        IsIntroduced(FindTarget(spec->annotations()))) {
      if (start_index != -1) {
        RegroupAux(ct_id, start_index, end_index, carry_over, variables);
      }
      start_index = constraint_index;
      variables.clear();
      variables.push_back(spec->Arg(0)->getIntVar());
      carry_over = spec->Arg(2)->getIntVar();
      end_index = constraint_index;
    } else if (spec->Arg(1)->isIntVar() && spec->Arg(1)->getIntVar() ==
               carry_over && FindTarget(spec->annotations()) != nullptr &&
               IsIntroduced(FindTarget(spec->annotations()))) {
      variables.push_back(spec->Arg(0)->getIntVar());
      carry_over = spec->Arg(2)->getIntVar();
      end_index = constraint_index;
    } else if (carry_over != -1) {
      RegroupAux(ct_id, start_index, end_index, carry_over, variables);
      carry_over = -1;
      start_index = -1;
      end_index = -1;
    }
  }
  if (carry_over != -1) {
    RegroupAux(ct_id, start_index, end_index, carry_over, variables);
  }
}

void ParserState::ReplaceAliases(CtSpec* const spec) {
  for (int i = 0; i < spec->NumArgs(); ++i) {
    AstNode* const arg = spec->Arg(i);
    if (arg->isIntVar()) {
      const int var_index = arg->getIntVar();
      if (ContainsKey(int_aliases_, var_index)) {
        dynamic_cast<AstIntVar*>(arg)->i = int_aliases_[var_index];
      }
    } else if (arg->isArray()) {
      AstArray* const array = arg->getArray();
      for (int i = 0; i < array->a.size(); ++i) {
        AstNode* const node = array->a[i];
        if (node->isIntVar() && ContainsKey(int_aliases_, node->getIntVar())) {
          dynamic_cast<AstIntVar*>(node)->i = int_aliases_[node->getIntVar()];
        }
      }
    }
  }
}

void ParserState::AddConstraint(const std::string& id, AstArray* const args,
                                AstNode* const annotations) {
  constraints_.push_back(
      new CtSpec(constraints_.size(), id, args, annotations));
}

void ParserState::InitModel() {
  if (!hadError) {
    model_->Init(int_variables_.size(), bool_variables_.size(),
                 set_variables_.size());
    constraints_per_int_variables_.resize(int_variables_.size());
    constraints_per_bool_variables_.resize(bool_variables_.size());
    int_args_.resize(int_variables_.size());
    for (int i = 0; i < int_args_.size(); ++i) {
      int_args_[i] = new AstIntVar(i);
    }
    bool_args_.resize(bool_variables_.size());
    for (int i = 0; i < bool_args_.size(); ++i) {
      bool_args_[i] = new AstBoolVar(i);
    }
  }
}

void ParserState::InitOutput(operations_research::FlatZincModel* const m) {
  m->InitOutput(Output());
}

bool FlatZincModel::Parse(const std::string& filename) {
  filename_ = filename;
  filename_.resize(filename_.size() - 4);
  size_t found = filename_.find_last_of("/\\");
  if (found != std::string::npos) {
    filename_ = filename_.substr(found + 1);
  }
#ifdef HAVE_MMAP
  int fd;
  char* data;
  struct stat sbuf;
  fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1) {
    LOG(ERROR) << "Cannot open file " << filename;
    return false;
  }
  if (stat(filename.c_str(), &sbuf) == -1) {
    LOG(ERROR) << "Cannot stat file " << filename;
    return false;
  }
  data = reinterpret_cast<char*>(
      mmap((caddr_t) 0, sbuf.st_size, PROT_READ, MAP_SHARED, fd, 0));
  if (data == (caddr_t)(-1)) {
    LOG(ERROR) << "Cannot mmap file " << filename;
    return false;
  }

  ParserState pp(data, sbuf.st_size, this);
#else
  std::ifstream file;
  file.open(filename.c_str());
  if (!file.is_open()) {
    LOG(ERROR) << "Cannot open file " << filename;
    return false;
  }
  std::string s = std::string(std::istreambuf_iterator<char>(file),
                    std::istreambuf_iterator<char>());
  ParserState pp(s, this);
#endif
  orfz_lex_init(&pp.yyscanner);
  orfz_set_extra(&pp, pp.yyscanner);
  // yydebug = 1;
  orfz_parse(&pp);
  pp.InitOutput(this);

  if (pp.yyscanner) orfz_lex_destroy(pp.yyscanner);
  parsed_ok_ = !pp.hadError;
  return parsed_ok_;
}

bool FlatZincModel::Parse(std::istream& is) {  // NOLINT
  filename_ = "stdin";
  std::string s = std::string(std::istreambuf_iterator<char>(is),
                    std::istreambuf_iterator<char>());

  ParserState pp(s, this);
  orfz_lex_init(&pp.yyscanner);
  orfz_set_extra(&pp, pp.yyscanner);
  // yydebug = 1;
  orfz_parse(&pp);
  pp.InitOutput(this);

  if (pp.yyscanner) orfz_lex_destroy(pp.yyscanner);
  parsed_ok_ = !pp.hadError;
  return parsed_ok_;
}

AstNode* ArrayOutput(AstCall* ann) {
  AstArray* a = nullptr;

  if (ann->args->isArray()) {
    a = ann->args->getArray();
  } else {
    a = new AstArray(ann->args);
  }

  std::string out;

  out = StringPrintf("array%lud(", a->a.size());
  for (unsigned int i = 0; i < a->a.size(); i++) {
    AstSetLit* s = a->a[i]->getSet();
    if (s->empty()) {
      out += "{}, ";
    } else if (s->interval) {
      out += StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d, ", s->imin,
                          s->imax);
    } else {
      out += "{";
      for (unsigned int j = 0; j < s->s.size(); j++) {
        out += s->s[j];
        if (j < s->s.size() - 1) {
          out += ",";
        }
      }
      out += "}, ";
    }
  }

  if (!ann->args->isArray()) {
    a->a[0] = nullptr;
    delete a;
  }
  return new AstString(out);
}
}  // namespace operations_research
