/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2007
 *  Modified:
 *     Laurent Perron <lperron@google.com>
 *
 *  Last modified:
 *     $Date: 2010-07-02 19:18:43 +1000 (Fri, 02 Jul 2010) $ by $Author: tack $
 *     $Revision: 11149 $
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#ifndef OR_TOOLS_FLATZINC_SPEC__H_
#define OR_TOOLS_FLATZINC_SPEC__H_

#include "base/concise_iterator.h"
#include "base/hash.h"
#include "base/map-util.h"
#include "flatzinc/ast.h"
#include <vector>
#include <iostream>

namespace operations_research {

/// %Alias for a variable specification
class Alias {
 public:
  const int v;
  Alias(int v0) : v(v0) {}
};

/// Optional value
template<class Val> struct Option {
 private:
  bool defined_;
  Val value_;
 public:
  bool defined() const { return defined_; }
  const Val& value(void) const { return value_; }

  static Option<Val> none(void) {
    Option<Val> o;
    o.defined_ = false;
    new (&o.value_) Val();
    return o;
  }

  static Option<Val> some(const Val& v) {
    Option<Val> o;
    o.defined_ = true;
    o.value_ = v;
    return o;
  }
};

/// Base class for variable specifications
class VarSpec {
 public:
  /// Whether the variable was introduced in the mzn2fzn translation
  const bool introduced;
  /// Destructor
  virtual ~VarSpec(void) {}
  /// Variable index
  int i;
  /// Whether the variable aliases another variable
  const bool alias;
  /// Whether the variable is assigned
  bool assigned;
  // name
  string name;
  /// Constructor
  VarSpec(const string& name0, bool introduced0, bool alias0, bool assigned0)
      : name(name0),
        introduced(introduced0),
        alias(alias0),
        assigned(assigned0) {}
  // Debug string.
  virtual string DebugString() const {
    return "VarSpec";
  }
  void SetName(const string& n) {
    name = n;
  }
  const string& Name() const {
    return name;
  }
};

/// Specification for integer variables
class IntVarSpec : public VarSpec {
 public:
  IntVarSpec(const string& name,
             const Option<AST::SetLit*>& d,
             bool introduced,
             bool own_domain)
      : VarSpec(name, introduced, false, false),
        own_domain_(own_domain) {
    i = -1;
    domain_ = d;
  }

  IntVarSpec(const string& name, int i0, bool introduced)
      : VarSpec(name, introduced, false, true),
        own_domain_(false) {
    i = i0;
  }

  IntVarSpec(const string& name, const Alias& eq, bool introduced)
      : VarSpec(name, introduced, true, false),
        own_domain_(false) {
    i = eq.v;
  }

  virtual ~IntVarSpec(void) {
    if (!alias && !assigned && domain_.defined() && own_domain_)
      delete domain_.value();
  }

  bool MergeBounds(int64 nmin, int64 nmax) {
    CHECK(!alias);
    if (assigned) {
      return false;
    }
    if (!domain_.defined()) {
      domain_ =  Option<AST::SetLit*>::some(new AST::SetLit(nmin, nmax));
      own_domain_ = true;
      return true;
    }
    if (!own_domain_) {
      return false;  // IMPROVE ME.
    }
    AST::SetLit* const domain = domain_.value();
    if (domain->interval) {
      domain->min = std::max(domain->min, nmin);
      domain->max = std::min(domain->max, nmax);
      return true;
    }
    return false;
  }

  bool RemoveValue(int64 val) {
    CHECK(!alias);
    if (assigned) {
      return val != i;
    }
    if (!domain_.defined()) {
      return false;
    }
    if (!own_domain_) {
      return false;  // IMPROVE ME.
    }
    AST::SetLit* const domain = domain_.value();
    if (domain->interval) {
      if (domain->min == val) {
        domain->min++;
        return true;
      } else if (domain->max == val) {
        domain->max--;
        return true;
      } else if (val < domain->min || val > domain->max) {
        return true;
      }
    }
    return false;
  }

  bool MergeDomain(const std::vector<int64>& values) {
    CHECK(!alias);
    if (assigned) {
      return false;
    }
    if (!domain_.defined()) {
      domain_ =  Option<AST::SetLit*>::some(new AST::SetLit(values));
      own_domain_ = true;
      return true;
    }
    if (!own_domain_) {
      return false;  // IMPROVE ME.
    }
    AST::SetLit* const domain = domain_.value();
    if (domain->interval) {
      const int64 old_min = domain->min;
      const int64 old_max = domain->max;
      for (int i = 0; i < values.size(); ++i) {
        const int64 v = values[i];
        if (v >= old_min && v <= old_max) {
          domain->s.push_back(v);
        }
      }
      domain->interval = false;
      return true;
    }
    return false;
  }

  bool IsBound() const {
    return assigned || (domain_.defined() &&
                        domain_.value()->min == domain_.value()->max);
  }

  int GetBound() const {
    CHECK(IsBound());
    if (assigned) {
      return i;
    }
    return domain_.value()->min;
  }

  virtual string DebugString() const {
    if (alias) {
      return StringPrintf(
          "IntVarSpec(name = %s, alias to = %d)",
          name.c_str(),
          i);
    } else if (assigned) {
      return StringPrintf(
          "IntVarSpec(name = %s, assigned to = %d)",
          name.c_str(),
          i);
    } else {
      return StringPrintf(
          "IntVarSpec(name = %s, id = %d, domain = %s%s)",
          name.c_str(),
          i,
          (domain_.defined() ?
           domain_.value()->DebugString().c_str() :
           "no domain"),
          (introduced ? ", introduced" : ""));
    }
  }

  AST::SetLit* Domain() const {
    return domain_.value();
  }

  bool HasDomain() const {
    return domain_.defined();
  }

 private:
  Option<AST::SetLit*> domain_;
  bool own_domain_;
};

/// Specification for Boolean variables
class BoolVarSpec : public VarSpec {
 public:
  BoolVarSpec(const string& name,
              const Option<AST::SetLit*>& d,
              bool introduced,
              bool own_domain)
      : VarSpec(name, introduced, false, false), own_domain_(own_domain) {
    i = -1;
    domain_ = d;
  }

  BoolVarSpec(const string& name, bool b, bool introduced)
      : VarSpec(name, introduced, false, true), own_domain_(false)  {
    i = b;
  }

  BoolVarSpec(const string& name, const Alias& eq, bool introduced)
      : VarSpec(name, introduced, true, false), own_domain_(false) {
    i = eq.v;
  }

  ~BoolVarSpec() {
    if (!alias && !assigned && domain_.defined() && own_domain_)
      delete domain_.value();
  }

  void Assign(bool value) {
    assigned = true;
    i = value;
  }

  bool IsTrue() const {
    return (assigned && i == true);
  }

  bool IsFalse() const {
    return (assigned && i == false);
  }

  bool IsBound() const {
    return IsTrue() || IsFalse();
  }

  int GetBound() const {
    CHECK(IsBound());
    return IsTrue();
  }

  virtual string DebugString() const {
    if (alias) {
      return StringPrintf(
          "BoolVarSpec(name = %s, alias to = %d)",
          name.c_str(),
          i);
    } else if (assigned) {
      return StringPrintf(
          "BoolVarSpec(name = %s, assigned to = %d)",
          name.c_str(),
          i);
    } else {
      return StringPrintf(
          "BoolVarSpec(name = %s, id = %d, domain = %s)",
          name.c_str(),
          i,
          (domain_.defined() ?
           domain_.value()->DebugString().c_str() :
           "no domain"));
    }
  }

 private:
  Option<AST::SetLit*> domain_;
  const bool own_domain_;
};

/// Specification for floating point variables
class FloatVarSpec : public VarSpec {
 public:
  Option<std::vector<double>*> domain;
  FloatVarSpec(const string& name,
               Option<std::vector<double>*>& d,
               bool introduced)
      : VarSpec(name, introduced, false, false) {
    domain = d;
  }

  FloatVarSpec(const string& name, bool b, bool introduced)
      : VarSpec(name, introduced, false, true) {
    i = b;
  }

  FloatVarSpec(const string& name, const Alias& eq, bool introduced)
      : VarSpec(name, introduced, true, false) {
    i = eq.v;
  }

  ~FloatVarSpec(void) {
    if (!alias && !assigned && domain.defined())
      delete domain.value();
  }
};

/// Specification for set variables
class SetVarSpec : public VarSpec {
 public:
  Option<AST::SetLit*> domain_;
  SetVarSpec(const string& name, bool introduced)
      : VarSpec(name, introduced, false, false),
        own_domain_(false) {
    domain_ = Option<AST::SetLit*>::none();
  }
  SetVarSpec(const string& name,
             const Option<AST::SetLit*>& v,
             bool introduced, bool own_domain)
      : VarSpec(name, introduced, false, false),
        own_domain_(own_domain) {
    domain_ = v;

  }
  SetVarSpec(const string& name, AST::SetLit* v, bool introduced)
      : VarSpec(name, introduced, false, true),
        own_domain_(false) {
    domain_ = Option<AST::SetLit*>::some(v);
  }
  SetVarSpec(const string& name, const Alias& eq, bool introduced)
      : VarSpec(name, introduced, true, false),
        own_domain_(false) {
    i = eq.v;
  }
  virtual ~SetVarSpec(void) {
    if (!alias && domain_.defined() && own_domain_)
      delete domain_.value();
  }

  virtual string DebugString() const {
    if (alias) {
      return StringPrintf(
          "SetVarSpec(name = %s, alias to = %d)",
          name.c_str(),
          i);
    } else if (assigned) {
      return StringPrintf(
          "SetVarSpec(name = %s, assigned to = %d)",
          name.c_str(),
          i);
    } else {
      return StringPrintf(
          "SetVarSpec(name = %s, id = %d, domain = %s)",
          name.c_str(),
          i,
          (domain_.defined() ?
           domain_.value()->DebugString().c_str() :
           "no domain"));
    }
  }


 private:
  const bool own_domain_;
};

typedef hash_set<AST::Node*> NodeSet;

class CtSpec {
 public:
  CtSpec(int index,
         const std::string& id,
         AST::Array* const args,
         AST::Node* const annotations)
      : index_(index),
        id_(id),
        args_(args),
        annotations_(annotations),
        nullified_(false),
        defined_arg_(NULL) {
  }

  ~CtSpec() {
    delete args_;
    delete annotations_;
  }

  const std::string& Id() const {
    return id_;
  }

  void SetId(const std::string& id) {
    id_ = id;
  }

  int Index() const {
    return index_;
  }

  AST::Node* Arg(int index) const {
    return args_->a[index];
  }

  AST::Node* LastArg() const {
    return args_->a.back();
  }

  int NumArgs() const {
    return args_->a.size();
  }

  void RemoveArg(int index) {
    delete args_->a[index];
    for (int i = index; i < args_->a.size() - 1; i++) {
      args_->a[i] = args_->a[i + 1];
    }
    args_->a.pop_back();
  }

  bool IsDefined(AST::Node* const arg) {
    if (defined_arg_ != NULL) {
      return ((arg->isIntVar() &&
               defined_arg_->isIntVar() &&
               arg->getIntVar() == defined_arg_->getIntVar()) ||
              (arg->isBoolVar() &&
               defined_arg_->isBoolVar() &&
               arg->getBoolVar() == defined_arg_->getBoolVar()));
    }
    return false;
  }

  AST::Array* Args() const {
    return args_;
  }

  void ReplaceArg(int index, AST::Node* const node) {
    delete args_->a[index];
    args_->a[index] = node;
  }

  AST::Node* annotations() const {
    return annotations_;
  }

  string DebugString() const {
    string output =
        StringPrintf("CtSpec(no = %d, id = %s, args = %s",
                     index_,
                     id_.c_str(),
                     args_->DebugString().c_str());
    if (annotations_ != NULL) {
      output += StringPrintf(", annotations = %s",
                             annotations_->DebugString().c_str());
    }
    if (defined_arg_ != NULL) {
      output += StringPrintf(", target = %s",
                             defined_arg_->DebugString().c_str());
    }
    if (!requires_.empty()) {
      output += ", requires = [";
      for (ConstIter<NodeSet> it(requires_); !it.at_end(); ++it) {
        output.append((*it)->DebugString());
        output.append(" ");
      }
      output += "]";
    }
    output += ")";
    return output;
  }

  void SetDefinedArg(AST::Node* const arg) {
    defined_arg_ = arg;
  }

  AST::Node* const DefinedArg() const {
    return defined_arg_;
  }

  const NodeSet& require_map() const {
    return requires_;
  }

  NodeSet* mutable_require_map() {
    return &requires_;
  }

  void Unreify() {
    id_.resize(id_.size() - 5);
    delete annotations_;
    annotations_ = NULL;
    delete args_->a.back();
    args_->a.pop_back();
  }

  void Nullify() {
    nullified_ = true;
    id_.append("_null");
  }

  bool Nullified() const {
    return nullified_;
  }

  void AddAnnotation(AST::Node* const node) {
    if (annotations_ == NULL) {
      annotations_ = new AST::Array(node);
    } else {
      annotations_->getArray()->a.push_back(node);
    }
  }

  void RemoveDefines() {
    if (annotations_ != NULL) {
      AST::Array* const ann_array = annotations_->getArray();
      if (ann_array->a[0]->isCall("defines_var")) {
        delete ann_array->a[0];
        for (int i = 0; i < ann_array->a.size() - 1; ++i) {
          ann_array->a[i] = ann_array->a[i + 1];
        }
        ann_array->a.pop_back();
      }
    }
    defined_arg_ = NULL;
  }

  void RemoveDomain() {
    if (annotations_ != NULL) {
      if (AST::Array* a = dynamic_cast<AST::Array*>(annotations_)) {
        for (int i = a->a.size(); i--;) {
          if (AST::Atom* at = dynamic_cast<AST::Atom*>(a->a[i])) {
            if (at->id == "domain") {
              at->id = "null_annotation";
            }
          }
        }
      } else if (AST::Atom* a = dynamic_cast<AST::Atom*>(annotations_)) {
        if (a->id == "domain") {
          a->id = "null_annotation";
        }
      }
    }
  }

 private:
  const int index_;
  std::string id_;
  AST::Array* const args_;
  AST::Node* annotations_;
  std::vector<int> uses_;
  NodeSet requires_;
  bool nullified_;
  AST::Node* defined_arg_;
};
}  // namespace operations_research
#endif
