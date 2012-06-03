/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *
 *  Copyright:
 *     Guido Tack, 2007
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
  const bool assigned;
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

  virtual string DebugString() const {
    return StringPrintf(
        "IntVarSpec(id = %d, domain = %s)",
        i,
        domain_.value()->DebugString().c_str());
  }

  AST::SetLit* Domain() const {
    return domain_.value();
  }

 private:
  Option<AST::SetLit*> domain_;
  const bool own_domain_;
};

/// Specification for Boolean variables
class BoolVarSpec : public VarSpec {
 public:
  BoolVarSpec(const string& name,
              const Option<AST::SetLit*>& d,
              bool introduced,
              bool own_domain)
      : VarSpec(name, introduced, false, false), own_domain_(own_domain) {
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

  ~BoolVarSpec(void) {
    if (!alias && !assigned && domain_.defined() && own_domain_)
      delete domain_.value();
  }

  virtual string DebugString() const {
    return StringPrintf(
        "BoolVarSpec(id = %d, domain = %s)",
        i,
        domain_.value()->DebugString().c_str());
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
  Option<AST::SetLit*> upperBound;
  SetVarSpec(const string& name, bool introduced)
      : VarSpec(name, introduced, false, false) {
    upperBound = Option<AST::SetLit*>::none();
  }
  SetVarSpec(const string& name, const Option<AST::SetLit*>& v, bool introduced)
      : VarSpec(name, introduced, false, false) {
    upperBound = v;
  }
  SetVarSpec(const string& name, AST::SetLit* v, bool introduced)
      : VarSpec(name, introduced, false, true) {
    upperBound = Option<AST::SetLit*>::some(v);
  }
  SetVarSpec(const string& name, const Alias& eq, bool introduced)
      : VarSpec(name, introduced, true, false) {
    i = eq.v;
  }
  ~SetVarSpec(void) {
    if (!alias && upperBound.defined())
      delete upperBound.value();
  }
};

class CtSpec {
 public:
  CtSpec(const int index,
         const std::string& id,
         AST::Array* const args,
         AST::Node* const annotations)
      : index_(index),
        id_(id),
        args_(args),
        annotations_(annotations) {}

  ~CtSpec() {
    delete args_;
    delete annotations_;
  }

  const std::string& Id() const {
    return id_;
  }

  int Index() const {
    return index_;
  }

  AST::Node* Arg(int index) const {
    return args_->a[index];
  }

  int NumArgs() const {
    return args_->a.size();
  }

  AST::Node* annotations() const {
    return annotations_;
  }

  string DebugString() const {
    return StringPrintf("CtSpec(no = %d, id = %s, args = %s, annotations=%s)",
                        index_,
                        id_.c_str(),
                        args_->DebugString().c_str(),
                        (annotations_ != NULL ?
                         annotations_->DebugString().c_str() :
                         "Nil"));
  }

 private:
  const int index_;
  const std::string id_;
  AST::Array* const args_;
  AST::Node* const annotations_;
};
}  // namespace operations_research
#endif
