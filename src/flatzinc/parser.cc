// Copyright 2010-2012 Google
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

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "flatzinc/flatzinc.h"
#include "flatzinc/parser.h"
#include "flatzinc/parser.tab.h"

using namespace std;
extern int yyparse(void*);
extern int yylex(YYSTYPE*, void* scanner);
extern int yylex_init (void** scanner);
extern int yylex_destroy (void* scanner);
extern int yyget_lineno (void* scanner);
extern void yyset_extra (void* user_defined ,void* yyscanner );
extern void yyerror(void* parm, const char *str);

DECLARE_bool(use_minisat);

namespace operations_research {
// ----- Misc -----
bool HasDomainAnnotation(AstNode* const annotations) {
  if (annotations != NULL) {
    return annotations->hasAtom("domain");
  }
  return false;
}

bool HasDefineAnnotation(AstNode* const annotations) {
  if (annotations != NULL) {
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
  if (pos >= length)
    return 0;
  int num = std::min(length - pos, lexBufSize);
  memcpy(lexBuf, buf + pos, num);
  pos += num;
  return num;
}

void ParserState::output(std::string x, AstNode* n) {
  output_.push_back(std::pair<std::string,AstNode*>(x,n));
}

/// Strict weak ordering for output items
class OutputOrder {
 public:
  /// Return if \a x is less than \a y, based on first component
  bool operator ()(const std::pair<std::string, AstNode*>& x,
                   const std::pair<std::string, AstNode*>& y) {
    return x.first < y.first;
  }
};

AstArray* ParserState::Output(void) {
  OutputOrder oo;
  std::sort(output_.begin(), output_.end(), oo);
  AstArray* const a = new AstArray();
  for (unsigned int i = 0; i < output_.size(); i++) {
    a->a.push_back(new AstString(output_[i].first+" = "));
    if (output_[i].second->isArray()) {
      AstArray* const oa = output_[i].second->getArray();
      for (unsigned int j = 0; j < oa->a.size(); j++) {
        a->a.push_back(oa->a[j]);
        oa->a[j] = NULL;
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
  if (annotations != NULL) {
    if (annotations->isArray()) {
      AstArray* const ann_array = annotations->getArray();
      if (ann_array->a[0]->isCall("defines_var")) {
        AstCall* const call = ann_array->a[0]->getCall();
        AstNode* const args = call->args;
        return Copy(args);
      }
    }
  }
  return NULL;
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
      if (copy != NULL && ContainsKey(candidates, copy)) {
        require->insert(copy);
      }
    }
  }
}

void ParserState::ComputeViableTarget(CtSpec* const spec,
                                      NodeSet* const candidates) const {
  const string& id = spec->Id();
  if (id == "bool2int" ||
      id == "int_plus" ||
      id == "int_minus" ||
      id == "int_times" ||
      (id == "array_var_int_element" && !IsBound(spec->Arg(2))) ||
      id == "array_int_element" ||
      id == "int_abs" ||
      (id == "int_lin_eq" && !HasDomainAnnotation(spec->annotations())) ||
      id == "int_max" ||
      id == "int_min" ||
      id == "int_eq") {
    // Defines an int var.
    AstNode* const define = FindTarget(spec->annotations());
    if (define != NULL) {
      //      CHECK(IsIntroduced(define));
      candidates->insert(define);
      VLOG(2) << id << " -> insert " << define->DebugString();
    }
  } else if ((id == "array_bool_and" && !FLAGS_use_minisat) ||
             (id == "array_bool_or" && !FLAGS_use_minisat) ||
             id == "array_bool_element" ||
             id == "int_lin_eq_reif" ||
             id == "int_eq_reif" ||
             id == "int_ne_reif" ||
             id == "int_le_reif" ||
             id == "int_ge_reif" ||
             (id == "bool_eq_reif" && !FLAGS_use_minisat) ||
             (id == "bool_ne_reif" && !FLAGS_use_minisat) ||
             (id == "bool_le_reif" && !FLAGS_use_minisat) ||
             (id == "bool_ge_reif" && !FLAGS_use_minisat) ||
             id == "bool_not") {
    // Defines a bool var.
    AstNode* const bool_define = FindTarget(spec->annotations());
    if (bool_define != NULL) {
      CHECK(IsIntroduced(bool_define));
      candidates->insert(bool_define);
      VLOG(2) << id << " -> insert " << bool_define->DebugString();
    }
  } else if (id == "int2int" || id == "bool2bool") {
    candidates->insert(Copy(spec->Arg(1)));
    VLOG(2) << id << " -> insert " << spec->Arg(1)->DebugString();
  }
}

void ParserState::ComputeDependencies(const NodeSet& candidates,
                                      CtSpec* const spec) const {
  AstNode* const define = spec->DefinedArg() == NULL ?
      FindTarget(spec->annotations()) :
      spec->DefinedArg();
  if (ContainsKey(candidates, define)) {
    spec->SetDefinedArg(define);
  }
  NodeSet* const requires = spec->mutable_require_map();
  CollectRequired(spec->Args(), candidates, requires);
  if (define != NULL) {
    requires->erase(define);
  }
}

void ParserState::MarkAllVariables(AstNode* const node,
                                   NodeSet* const computed) {
  if (node->isIntVar()) {
    computed->insert(Copy(node));
    VLOG(1) << "  - " << node->DebugString();
  }
  if (node->isArray()) {
    AstArray* const array = node->getArray();
    for (int i = 0; i < array->a.size(); ++i) {
      if (array->a[i]->isIntVar()) {
        computed->insert(Copy(array->a[i]));
        VLOG(1) << "  - " << array->a[i]->DebugString();
      }
    }
  }
}

void ParserState::MarkComputedVariables(CtSpec* const spec,
                                        NodeSet* const computed) {
  const string& id = spec->Id();
  if (id == "global_cardinality") {
    VLOG(1) << "Marking " << spec->DebugString();
    MarkAllVariables(spec->Arg(2), computed);
  }
}

void ParserState::Sanitize(CtSpec* const spec) {
  if (spec->Id() == "int_lin_eq" && HasDomainAnnotation(spec->annotations())) {
    AstArray* const array_coefficients = spec->Arg(0)->getArray();
    if (array_coefficients->a.size() > 2) {
      VLOG(1) << "  - presolve: remove defines part on " << spec->DebugString();
      spec->RemoveDefines();
    } else {
      VLOG(1) << "  - presolve: remove domain part on " << spec->DebugString();
      spec->RemoveDomain();
    }
  }
}

void ParserState::Presolve() {
  // Sanity.
  for (int i = 0; i < constraints_.size(); ++i) {
    Sanitize(constraints_[i]);
  }
  // Find orphans (is_defined && not a viable target).
  for (int i = 0; i < constraints_.size(); ++i) {
    AstNode* const target = FindTarget(constraints_[i]->annotations());
    if (target != NULL) {
      VLOG(1) << "  - presolve:  mark " << target->DebugString()
              << " as defined";
      targets_.insert(target);
    }
  }
  for (int i = 0; i < int_variables_.size(); ++i) {
    IntVarSpec* const spec = int_variables_[i];
    AstNode* const var = IntCopy(i);
    if (spec->introduced && !ContainsKey(targets_, var)) {
      orphans_.insert(var);
      VLOG(1) << "  - presolve:  mark " << var->DebugString() << " as orphan";
    }
  }

  // Presolve (propagate bounds).
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
  for (int i = 0; i < int_variables_.size(); ++i) {
    IntVarSpec* const spec = int_variables_[i];
    if (spec->alias) {
      AstArray* args = new AstArray(2);
      args->a[0] = new AstIntVar(spec->i);
      args->a[1] = new AstIntVar(i);
      CtSpec* const alias_ct = new CtSpec(constraints_.size(),
                                          "int2int",
                                          args,
                                          NULL);
      alias_ct->SetDefinedArg(IntCopy(i));
      constraints_.push_back(alias_ct);
    }
  }

  for (int i = 0; i < bool_variables_.size(); ++i) {
    BoolVarSpec* const spec = bool_variables_[i];
    if (spec->alias) {
      AstArray* args = new AstArray(2);
      args->a[0] = new AstBoolVar(spec->i);
      args->a[1] = new AstBoolVar(i);
      CtSpec* const alias_ct = new CtSpec(constraints_.size(),
                                          "bool2bool",
                                          args,
                                          NULL);
      alias_ct->SetDefinedArg(BoolCopy(i));
      constraints_.push_back(alias_ct);
    }
  }

  // Setup mapping structures (constraints per id, and constraints per
  // variable).
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
              constraints_per_int_variables_[array[j]->getIntVar()].push_back(
                  index);
            } else if (array[j]->isBoolVar()) {
              constraints_per_bool_variables_[array[j]->getBoolVar()].push_back(
                  index);
            }
          }
        }
      }
    }
  }

  VLOG(1) << "Model statistics";
  for (ConstIter<hash_map<string, std::vector<int> > > it(constraints_per_id_);
       !it.at_end();
       ++it) {
    VLOG(1) << "  - " << it->first << ": " << it->second.size();
  }

  if (ContainsKey(constraints_per_id_, "array_bool_or")) {
    const std::vector<int>& ors = constraints_per_id_["array_bool_or"];
    for (int i = 0; i < ors.size(); ++i) {
      Strongify(ors[i]);
    }
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
    if (spec->DefinedArg() != NULL || !spec->require_map().empty()) {
      VLOG(2) << spec->DebugString();
    }
  }

  VLOG(1) << "Sort constraints";
  std::vector<CtSpec*> defines_only;
  std::vector<CtSpec*> no_defines;
  std::vector<CtSpec*> defines_and_require;
  int nullified = 0;

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    if(spec->Nullified()) {
      nullified++;
    } else if (spec->DefinedArg() != NULL && spec->require_map().empty()) {
      defines_only.push_back(spec);
    } else if (spec->DefinedArg() == NULL) {
      no_defines.push_back(spec);
    } else {
      defines_and_require.push_back(spec);
    }
  }

  VLOG(1) << "  - defines only          : " << defines_only.size();
  VLOG(1) << "  - no defines            : " << no_defines.size();
  VLOG(1) << "  - defines and require   : " << defines_and_require.size();
  VLOG(1) << "  - nullified constraints : " << nullified;

  const int size = constraints_.size();
  constraints_.clear();
  constraints_.resize(size - nullified);
  int index = 0;
  NodeSet defined;
  for (int i = 0; i < defines_only.size(); ++i) {
    if (!defines_only[i]->Nullified()) {
      constraints_[index++] = defines_only[i];
      defined.insert(Copy(defines_only[i]->DefinedArg()));
      VLOG(2) << "defined.insert "
              << defines_only[i]->DefinedArg()->DebugString();
    }
  }

  // Topological sorting.
  ConstraintSet to_insert;
  for (int i = 0; i < defines_and_require.size(); ++i) {
    if (!defines_and_require[i]->Nullified()) {
      to_insert.insert(defines_and_require[i]);
      VLOG(2) << " to_insert " << defines_and_require[i]->DebugString();
    }
  }

  NodeSet forced;
  while (!to_insert.empty()) {
    std::vector<CtSpec*> inserted;
    for (ConstIter<ConstraintSet> it(to_insert); !it.at_end(); ++it) {
      CtSpec* const spec = *it;
      VLOG(2) << "check " << spec->DebugString();
      if (ContainsKey(forced, spec->DefinedArg())) {
        VLOG(2) << "  - cleaning defines";
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
        if (spec->DefinedArg() != NULL) {
          defined.insert(Copy(spec->DefinedArg()));
          VLOG(2) << "inserted.push_back " << spec->DebugString();
          VLOG(2) << "defined.insert " << spec->DefinedArg()->DebugString();
        }
      }
    }
    if (inserted.empty()) {
      // Recovery mode. We have a dependency!.  Let's find the one
      // with the smallest number of unsatisfied dependencies.
      CtSpec* to_correct = NULL;
      int best_unsatisfied = kint32max;
      for (ConstIter<ConstraintSet> it(to_insert); !it.at_end(); ++it) {
        CtSpec* const spec = *it;
        VLOG(2) << "evaluate " << spec->DebugString();

        int unsatisfied = 0;
        const NodeSet& required = spec->require_map();
        for (ConstIter<NodeSet> def(required); !def.at_end(); ++def) {
          AstNode* const dep = *def;
          unsatisfied += !ContainsKey(defined, dep);
          if (IsAlias(dep)) {
            VLOG(2) << "  - " << dep->DebugString()
                    << "is an alias, disqualified";
            unsatisfied = kint32max;
            break;
          }
        }
        CHECK_GT(unsatisfied, 0);
        VLOG(2) << "  - unsatisfied = " << unsatisfied;
        if (unsatisfied < best_unsatisfied) {
          best_unsatisfied = unsatisfied;
          to_correct = spec;
        }
      }
      VLOG(2) << "Lifting " << to_correct->DebugString()
              << " with " << best_unsatisfied << " unsatisfied dependencies";
      const NodeSet& required = to_correct->require_map();
      for (ConstIter<NodeSet> def(required); !def.at_end(); ++def) {
        AstNode* const dep = Copy(*def);
        if (!ContainsKey(defined, dep)) {
          candidates->erase(dep);
          defined.insert(dep);
          forced.insert(dep);
          VLOG(2) << "removing " << dep->DebugString()
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
    if(!no_defines[i]->Nullified()) {
      constraints_[index++] = no_defines[i];
    }
  }

  CHECK_EQ(index, size - nullified);

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    CtSpec* const spec = constraints_[i];
    VLOG(2) << i << " -> " << spec->DebugString();
    MarkComputedVariables(spec, computed_variables);
  }
}

void ParserState::BuildModel(const NodeSet& candidates,
                             const NodeSet& computed_variables) {
  VLOG(1) << "Creating variables";

  int array_index = 0;
  for (unsigned int i = 0; i < int_variables_.size(); i++) {
    VLOG(1) << "xi(" << i << ") -> " << int_variables_[i]->DebugString();
    if (!hadError) {
      const std::string& raw_name = int_variables_[i]->Name();
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
      AstNode* const var = IntCopy(i);
      if (!ContainsKey(candidates, var)) {
        const bool active =
            !IsIntroduced(var) && !ContainsKey(computed_variables, var);
        model_->NewIntVar(name, int_variables_[i], active);
      } else {
        model_->SkipIntVar();
        VLOG(1) << "  - skipped";
        if (!IsAlias(var) &&
            !int_variables_[i]->assigned &&
            int_variables_[i]->HasDomain() &&
            int_variables_[i]->Domain() != NULL) {
          AddIntVarDomainConstraint(i, int_variables_[i]->Domain()->Copy());
        }
      }
    }
  }

  array_index = 0;
  for (unsigned int i = 0; i < bool_variables_.size(); i++) {
    AstNode* const var = BoolCopy(i);
    VLOG(1) << var->DebugString() << " -> "
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
      } else {
        model_->SkipBoolVar();
        VLOG(1) << "  - skipped";
      }
    }
  }

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

  VLOG(1) << "Creating constraints";

  for (unsigned int i = 0; i < constraints_.size(); i++) {
    if (!hadError) {
      CtSpec* const spec = constraints_[i];
      VLOG(1) << "Constraint " << constraints_[i]->DebugString();
      model_->PostConstraint(constraints_[i]);
    }
  }

  VLOG(1) << "Adding domain constraints";

  for (unsigned int i = int_domain_constraints_.size(); i--;) {
    if (!hadError) {
      AstNode* const var_node = int_domain_constraints_[i].first;
      IntVar* const var = model_->GetIntExpr(var_node)->Var();
      AstSetLit* const dom = int_domain_constraints_[i].second;
      if (dom->interval && (dom->imin > var->Min() || dom->imax < var->Max())) {
        VLOG(1) << "Reduce integer variable " << var->DebugString()
                << " to " << dom->DebugString();
        var->SetRange(dom->imin, dom->imax);
      } else if (!dom->interval) {
        VLOG(1) << "Reduce integer variable " << var->DebugString()
                << " to " << dom->DebugString();
        var->SetValues(dom->s);
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

AstNode* ParserState::ArrayElement(string id, unsigned int offset) {
  if (offset > 0) {
    vector<int64> tmp;
    if (int_var_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AstIntVar(tmp[offset-1]);
    if (bool_var_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AstBoolVar(tmp[offset-1]);
    if (set_var_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AstSetVar(tmp[offset-1]);

    if (int_value_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AstIntLit(tmp[offset-1]);
    if (bool_value_array_map_.get(id, tmp) && offset<=tmp.size())
      return new AstBoolLit(tmp[offset-1]);
    vector<AstSetLit> tmpS;
    if (set_value_array_map_.get(id, tmpS) && offset<=tmpS.size())
      return new AstSetLit(tmpS[offset-1]);
  }

  LOG(ERROR) << "Error: array access to " << id << " invalid"
             << " in line no. " << yyget_lineno(yyscanner);
  hadError = true;
  return new AstIntVar(0); // keep things consistent
}

AstNode* ParserState::VarRefArg(string id, bool annotation) {
  int64 tmp;
  if (int_var_map_.get(id, tmp))
    return new AstIntVar(tmp);
  if (bool_var_map_.get(id, tmp))
    return new AstBoolVar(tmp);
  if (set_var_map_.get(id, tmp))
    return new AstSetVar(tmp);
  if (annotation)
    return new AstAtom(id);
  LOG(ERROR) << "Error: undefined variable " << id
             << " in line no. " << yyget_lineno(yyscanner);
  hadError = true;
  return new AstIntVar(0); // keep things consistent
}

void ParserState::AddIntVarDomainConstraint(int var_id,
                                            AstSetLit* const dom) {
  if (dom != NULL) {
    VLOG(1) << "  - adding int var domain constraint (" << var_id
            << ") : " << dom->DebugString();
    int_domain_constraints_.push_back(
        std::make_pair(new AstIntVar(var_id), dom));
  }
}

void ParserState::AddBoolVarDomainConstraint(int var_id,
                                             AstSetLit* const dom) {
  if (dom != NULL) {
    VLOG(1) << "  - adding bool var domain constraint (" << var_id
            << ") : " << dom->DebugString();
    int_domain_constraints_.push_back(
        std::make_pair(new AstBoolVar(var_id), dom));
  }
}

void ParserState::AddSetVarDomainConstraint(int var_id,
                                            AstSetLit* const dom) {
  if (dom != NULL) {
    VLOG(1) << "  - adding set var domain constraint (" << var_id
            << ") : " << dom->DebugString();
    set_domain_constraints_.push_back(std::make_pair(var_id, dom));
  }
}

int ParserState::FindEndIntegerVariable(int index) const {
  while (int_variables_[index]->alias) {
    index = int_variables_[index]->i;
  }
  return index;
}

bool ParserState::IsBound(AstNode* const node) const {
  return node->isInt() ||
      (node->isIntVar() &&
       int_variables_[FindEndIntegerVariable(node->getIntVar())]->IsBound()) ||
      node->isBool() ||
      (node->isBoolVar() && bool_variables_[node->getBoolVar()]->IsBound());
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
}

int ParserState::GetBound(AstNode* const node) const {
  if (node->isInt()) {
    return node->getInt();
  }
  if (node->isIntVar()) {
    return int_variables_[node->getIntVar()]->GetBound();
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

bool ParserState::PresolveOneConstraint(CtSpec* const spec) {
  const string& id = spec->Id();
  const int index = spec->Index();
  if (id == "int_le") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec =
          int_variables_[FindEndIntegerVariable(spec->Arg(0)->getIntVar())];
      const int bound = GetBound(spec->Arg(1));
      VLOG(1) << "  - presolve:  merge " << var_spec->DebugString()
              << " with kint32min.." << bound;
      const bool ok = var_spec->MergeBounds(kint32min, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec =
          int_variables_[FindEndIntegerVariable(spec->Arg(1)->getIntVar())];
      const int bound = GetBound(spec->Arg(0));
      VLOG(1) << "  - presolve:  merge " << var_spec->DebugString() << " with "
              << bound << "..kint32max";
      const bool ok = var_spec->MergeBounds(bound, kint32max);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    }
  }
  if (id == "int_eq") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec =
          int_variables_[FindEndIntegerVariable(spec->Arg(0)->getIntVar())];
      const int bound = GetBound(spec->Arg(1));
      VLOG(1) << "  - presolve:  assign " << var_spec->DebugString()
              << " to " << bound;
      const bool ok = var_spec->MergeBounds(bound, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec =
          int_variables_[FindEndIntegerVariable(spec->Arg(1)->getIntVar())];
      const int bound = GetBound(spec->Arg(0));
      VLOG(1) << "  - presolve:  assign " <<  var_spec->DebugString()
              << " to " << bound;
      const bool ok = var_spec->MergeBounds(bound, bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (spec->Arg(0)->isIntVar() &&
               spec->Arg(1)->isIntVar() &&
               spec->annotations() == NULL &&
               !ContainsKey(stored_constraints_, spec) &&
               (ContainsKey(orphans_, Copy(spec->Arg(0))) ||
                ContainsKey(orphans_, Copy(spec->Arg(1))))) {
      stored_constraints_.insert(spec);
      AstNode* const var0 = Copy(spec->Arg(0));
      AstNode* const var1 = Copy(spec->Arg(1));
      if (ContainsKey(orphans_, var0)) {
        IntVarSpec* const spec0 =
            int_variables_[FindEndIntegerVariable(var0->getIntVar())];
        AstCall* const call =
            new AstCall("defines_var", new AstIntVar(var0->getIntVar()));
        spec->AddAnnotation(call);
        VLOG(1) << "  - presolve:  aliasing " << var0->DebugString()
                << " to " << var1->DebugString();
        orphans_.erase(var0);
        return true;
      } else if (ContainsKey(orphans_, var1)) {
        IntVarSpec* const spec1 =
            int_variables_[FindEndIntegerVariable(var1->getIntVar())];
        AstCall* const call =
            new AstCall("defines_var", new AstIntVar(var1->getIntVar()));
        spec->AddAnnotation(call);
        VLOG(1) << "  - presolve:  aliasing " << var1->DebugString()
                << " to " << var0->DebugString();
        orphans_.erase(var1);
        return true;
      }
    } else if (spec->Arg(0)->isIntVar() &&
               spec->Arg(1)->isIntVar() &&
               spec->annotations() == NULL &&
               !ContainsKey(stored_constraints_, spec) &&
               (!ContainsKey(targets_, Copy(spec->Arg(0))) ||
                !ContainsKey(targets_, Copy(spec->Arg(1))))) {
      stored_constraints_.insert(spec);
      AstNode* const var0 = Copy(spec->Arg(0));
      AstNode* const var1 = Copy(spec->Arg(1));
      IntVarSpec* const spec0 = int_variables_[var0->getIntVar()];
      IntVarSpec* const spec1 = int_variables_[var1->getIntVar()];
      if (!ContainsKey(targets_, var0) && !spec0->alias) {
        AstCall* const call =
            new AstCall("defines_var", new AstIntVar(var0->getIntVar()));
        spec->AddAnnotation(call);
        VLOG(1) << "  - presolve:  force aliasing " << var0->DebugString()
                << " to " << var1->DebugString();
        targets_.insert(var0);
        return true;
      } else if (!ContainsKey(targets_, var1) && !spec1->alias) {
        AstCall* const call =
            new AstCall("defines_var", new AstIntVar(var1->getIntVar()));
        spec->AddAnnotation(call);
        VLOG(1) << "  - presolve:  force aliasing " << var1->DebugString()
                << " to " << var0->DebugString();
        targets_.insert(var1);
        return true;
      }
    }
  }
  if (id == "int_ne") {
    if (spec->Arg(0)->isIntVar() && IsBound(spec->Arg(1))) {
      IntVarSpec* const var_spec =
          int_variables_[FindEndIntegerVariable(spec->Arg(0)->getIntVar())];
      const int bound = GetBound(spec->Arg(1));
      VLOG(1) << "  - presolve:  remove value " << bound << " from "
              << var_spec->DebugString();
      const bool ok = var_spec->RemoveValue(bound);
      if (ok) {
        spec->Nullify();
      }
      return ok;
    } else if (IsBound(spec->Arg(0)) && spec->Arg(1)->isIntVar()) {
      IntVarSpec* const var_spec =
          int_variables_[FindEndIntegerVariable(spec->Arg(1)->getIntVar())];
      const int bound = GetBound(spec->Arg(0));
      VLOG(1) << "  - presolve:  remove value " << bound << " from "
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
      IntVarSpec* const var_spec =
          int_variables_[FindEndIntegerVariable(spec->Arg(0)->getIntVar())];
      AstSetLit* const domain = spec->Arg(1)->getSet();
      VLOG(1) << "  - presolve:  merge " << var_spec->DebugString() << " with "
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
  if (id == "array_bool_and" &&
      IsBound(spec->Arg(1)) &&
      GetBound(spec->Arg(1)) == 1) {
    VLOG(1) << "  - presolve:  forcing array_bool_and to 1 on "
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
  if (id == "array_bool_or" &&
      IsBound(spec->Arg(1)) &&
      GetBound(spec->Arg(1)) == 0) {
    VLOG(1) << "  - presolve:  forcing array_bool_or to 0 on "
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
  if (id.find("_reif") != string::npos &&
      IsBound(spec->LastArg()) &&
      GetBound(spec->LastArg()) == 1) {
    VLOG(1) << "  - presolve:  unreify " << spec->DebugString();
    spec->Unreify();
    return true;
  }
  if (id.find("_reif") != string::npos &&
      IsBound(spec->LastArg()) &&
      GetBound(spec->LastArg()) == 0) {
    VLOG(1) << "  - presolve:  unreify and inverse " << spec->DebugString();
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
    VLOG(1) << "  - presolve:  store all diff info " << spec->DebugString();
    std::sort(variables.begin(), variables.end());
    stored_constraints_.insert(spec);
    all_differents_.push_back(variables);
    return true;
  }
  if (id == "array_var_int_element" &&
      IsBound(spec->Arg(2)) &&
      IsAllDifferent(spec->Arg(1))) {
    VLOG(1) << "  - presolve:  reinforce " << spec->DebugString()
            << " to array_var_int_position";
    spec->SetId("array_var_int_position");
    const int bound = GetBound(spec->Arg(2));
    spec->ReplaceArg(2, new AstIntLit(bound));
    return true;
  }
  if (id == "int_abs" &&
      !ContainsKey(stored_constraints_, spec) &&
      spec->Arg(0)->isIntVar() &&
      spec->Arg(1)->isIntVar()) {
    abs_map_[spec->Arg(1)->getIntVar()] = spec->Arg(0)->getIntVar();
    stored_constraints_.insert(spec);
    return true;
  }
  if (id == "int_eq_reif") {
    if (spec->Arg(0)->isIntVar() &&
        ContainsKey(abs_map_, spec->Arg(0)->getIntVar()) &&
        spec->Arg(1)->isInt() &&
        spec->Arg(1)->getInt() == 0) {
      VLOG(1) << "  - presolve:  remove abs() in " << spec->DebugString();
      dynamic_cast<AstIntVar*>(spec->Arg(0))->i =
          abs_map_[spec->Arg(0)->getIntVar()];
    }
  }
  if (id == "int_ne_reif") {
    if (spec->Arg(0)->isIntVar() &&
        ContainsKey(abs_map_, spec->Arg(0)->getIntVar()) &&
        spec->Arg(1)->isInt() &&
        spec->Arg(1)->getInt() == 0) {
      VLOG(1) << "  - presolve:  remove abs() in " << spec->DebugString();
      dynamic_cast<AstIntVar*>(spec->Arg(0))->i =
          abs_map_[spec->Arg(0)->getIntVar()];
    }
  }
  if (id == "int_ne") {
    if (spec->Arg(0)->isIntVar() &&
        ContainsKey(abs_map_, spec->Arg(0)->getIntVar()) &&
        spec->Arg(1)->isInt() &&
        spec->Arg(1)->getInt() == 0) {
      VLOG(1) << "  - presolve:  remove abs() in " << spec->DebugString();
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
    if (!one_positive) {
      VLOG(1) << "  - presolve:  transform all negative int_lin_le into "
              << "int_lin_ge in " << spec->DebugString();

      for (int i = 0; i < size; ++i) {
        array_coefficients->a[i]->setInt(-array_coefficients->a[i]->getInt());
      }
      spec->Arg(2)->setInt(-spec->Arg(2)->getInt());
      spec->SetId("int_lin_ge");
      return true;
    }
  }
  if (id == "int_lin_le_reif") {
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
    if (!one_positive) {
      VLOG(1) << "  - presolve:  transform all negative int_lin_le_reif into "
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
    if (!one_positive) {
      VLOG(1) << "  - presolve:  transform all negative int_lin_lt into "
              << "int_lin_gt in " << spec->DebugString();

      for (int i = 0; i < size; ++i) {
        array_coefficients->a[i]->setInt(-array_coefficients->a[i]->getInt());
      }
      spec->Arg(2)->setInt(-spec->Arg(2)->getInt());
      spec->SetId("int_lin_gt");
      return true;
    }
  }
  if (id == "int_lin_lt_reif") {
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
    if (!one_positive) {
      VLOG(1) << "  - presolve:  transform all negative int_lin_lt_reif into "
              << "int_lin_gt_reif in " << spec->DebugString();
      for (int i = 0; i < size; ++i) {
        array_coefficients->a[i]->setInt(-array_coefficients->a[i]->getInt());
      }
      spec->Arg(2)->setInt(-spec->Arg(2)->getInt());
      spec->SetId("int_lin_gt_reif");
      return true;
    }
  }
  if (id == "int_lin_eq") {
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
    if (!one_positive && !HasDefineAnnotation(spec->annotations())) {
      VLOG(1) << "  - presolve:  transform all negative int_lin_eq into "
              << "int_lin_eq in " << spec->DebugString();

      for (int i = 0; i < size; ++i) {
        array_coefficients->a[i]->setInt(-array_coefficients->a[i]->getInt());
      }
      spec->Arg(2)->setInt(-spec->Arg(2)->getInt());
      return true;
    }
  }
  if (id == "int_lin_eq_reif") {
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
    if (!one_positive) {
      VLOG(1) << "  - presolve:  transform all negative int_lin_eq_reif into "
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
      VLOG(1) << "  - presolve:  simplify bool_eq_reif in "
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
      VLOG(1) << "  - presolve:  simplify bool_ne_reif in "
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
  return false;
}

void ParserState::Strongify(int constraint_index) {

}

void ParserState::AddConstraint(const std::string& id,
                                AstArray* const args,
                                AstNode* const annotations) {
  constraints_.push_back(
      new CtSpec(constraints_.size(), id, args, annotations));
}

void ParserState::InitModel() {
  if (!hadError) {
    model_->Init(int_variables_.size(),
                 bool_variables_.size(),
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

void ParserState::FillOutput(operations_research::FlatZincModel& m) {
  m.InitOutput(Output());
}

void FlatZincModel::Parse(const std::string& filename) {
  filename_ = filename;
  filename_.resize(filename_.size() - 4);
  size_t found = filename_.find_last_of("/\\");
  if (found != string::npos) {
    filename_ = filename_.substr(found + 1);
  }
#ifdef HAVE_MMAP
  int fd;
  char* data;
  struct stat sbuf;
  fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1) {
    LOG(ERROR) << "Cannot open file " << filename;
    return NULL;
  }
  if (stat(filename.c_str(), &sbuf) == -1) {
    LOG(ERROR) << "Cannot stat file " << filename;
    return NULL;
  }
  data = (char*)mmap((caddr_t)0, sbuf.st_size, PROT_READ, MAP_SHARED, fd,0);
  if (data == (caddr_t)(-1)) {
    LOG(ERROR) << "Cannot mmap file " << filename;
    return NULL;
  }

  ParserState pp(data, sbuf.st_size, this);
#else
  std::ifstream file;
  file.open(filename.c_str());
  if (!file.is_open()) {
    LOG(FATAL) << "Cannot open file " << filename;
  }
  std::string s = string(istreambuf_iterator<char>(file),
                         istreambuf_iterator<char>());
  ParserState pp(s, this);
#endif
  yylex_init(&pp.yyscanner);
  yyset_extra(&pp, pp.yyscanner);
  // yydebug = 1;
  yyparse(&pp);
  pp.FillOutput(*this);

  if (pp.yyscanner)
    yylex_destroy(pp.yyscanner);
  parsed_ok_ = !pp.hadError;
}

void FlatZincModel::Parse(std::istream& is) {
  filename_ = "stdin";
  std::string s = string(istreambuf_iterator<char>(is),
                         istreambuf_iterator<char>());

  ParserState pp(s, this);
  yylex_init(&pp.yyscanner);
  yyset_extra(&pp, pp.yyscanner);
  // yydebug = 1;
  yyparse(&pp);
  pp.FillOutput(*this);

  if (pp.yyscanner)
    yylex_destroy(pp.yyscanner);
  parsed_ok_ = !pp.hadError;
}

AstNode* ArrayOutput(AstCall* ann) {
  AstArray* a = NULL;

  if (ann->args->isArray()) {
    a = ann->args->getArray();
  } else {
    a = new AstArray(ann->args);
  }

  std::string out;

  out = StringPrintf("array%dd(", a->a.size());;
  for (unsigned int i = 0; i < a->a.size(); i++) {
    AstSetLit* s = a->a[i]->getSet();
    if (s->empty()) {
      out += "{}, ";
    } else if (s->interval) {
      out += StringPrintf("%d..%d, ", s->imin, s->imax);
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
    a->a[0] = NULL;
    delete a;
  }
  return new AstString(out);
}
}  // namespace operations_research
