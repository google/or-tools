/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *  Modified:
 *     Laurent Perron <lperron@google.com>
 *
 *  Copyright:
 *     Guido Tack, 2007
 *
 *  Last modified:
 *     $Date: 2011-01-18 20:06:16 +1100 (Tue, 18 Jan 2011) $ by $Author: tack $
 *     $Revision: 11538 $
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

#ifndef __FLATZINC_PARSER_H_
#define __FLATZINC_PARSER_H_


// This is a workaround for a bug in flex that only shows up
// with the Microsoft C++ compiler
#if defined(_MSC_VER)
#define YY_NO_UNISTD_H
#ifdef __cplusplus
extern "C" int isatty(int);
#endif
#endif

// The Microsoft C++ compiler marks certain functions as deprecated,
// so let's take the alternative definitions
#if defined(_MSC_VER)
#define strdup _strdup
#define fileno _fileno
#endif

#include <cstring>
#include <string>
#include <vector>
#include <iostream>
#include <algorithm>
#include <map>

#include "base/concise_iterator.h"
#include "base/hash.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/map-util.h"
#include "base/stringprintf.h"

using std::string;

namespace operations_research {
// ---------- Abstract syntax tree ----------
class AstCall;
class AstArray;
class AstAtom;
class AstSetLit;

/// %Exception signaling type error
class AstTypeError {
 private:
  std::string _what;
 public:
  AstTypeError() : _what("") {}
  AstTypeError(std::string what) : _what(what) {}
  std::string what(void) const { return _what; }
};

/**
 * \brief A node in a %FlatZinc abstract syntax tree
 */
class AstNode {
 public:
  /// Destructor
  virtual ~AstNode(void);

  /// Append \a n to an array node
  void append(AstNode* n);

  /// Test if node has atom with \a id
  bool hasAtom(const std::string& id);
  /// Test if node is int, if yes set \a i to the value
  bool isInt(int& i);
  /// Test if node is function call with \a id
  bool isCall(const std::string& id);
  /// Return function call
  AstCall* getCall(void);
  /// Test if node is function call or array containing function call \a id
  bool hasCall(const std::string& id);
  /// Return function call \a id
  AstCall* getCall(const std::string& id);
  /// Cast this node to an array node
  AstArray* getArray(void);
  /// Cast this node to an Atom node
  AstAtom* getAtom(void);
  /// Cast this node to an integer variable node
  int getIntVar(void);
  /// Cast this node to a Boolean variable node
  int getBoolVar(void);
  /// Cast this node to a set variable node
  int getSetVar(void);

  /// Cast this node to an integer node
  int64 getInt(void);
  /// Cast thos node to an integer node and assign value.
  void setInt(int64);
  /// Cast this node to a Boolean node
  bool getBool(void);
  /// Cast this node to a Float node
  double getFloat(void);
  /// Cast this node to a set literal node
  AstSetLit *getSet(void);

  /// Cast this node to a string node
  std::string getString(void);

  /// Test if node is an integer variable node
  bool isIntVar(void);
  /// Test if node is a Boolean variable node
  bool isBoolVar(void);
  /// Test if node is a set variable node
  bool isSetVar(void);
  /// Test if node is an integer node
  bool isInt(void);
  /// Test if node is a Boolean node
  bool isBool(void);
  /// Test if node is a string node
  bool isString(void);
  /// Test if node is an array node
  bool isArray(void);
  /// Test if node is a set literal node
  bool isSet(void);
  /// Test if node is an atom node
  bool isAtom(void);

  /// Output string representation
  virtual string DebugString() const = 0;
};

/// Boolean literal node
class AstBoolLit : public AstNode {
 public:
  bool b;
  AstBoolLit(bool b0) : b(b0) {}
  virtual string DebugString() const {
    return b ? "b(true)" : "b(false)";
  }
};

/// Integer literal node
class AstIntLit : public AstNode {
 public:
  int64 i;
  AstIntLit(int64 i0) : i(i0) {}
  virtual string DebugString() const {
    return StringPrintf("i(%" GG_LL_FORMAT "d)", i);
  }
};
/// Float literal node
class AstFloatLit : public AstNode {
 public:
  double d;
  AstFloatLit(double d0) : d(d0) {}
  virtual string DebugString() const {
    return StringPrintf("f(%f)", d);
  }
};
/// %Set literal node
class AstSetLit : public AstNode {
 public:
  bool interval;
  int64 imin; int64 imax;
  std::vector<int64> s;
  AstSetLit(void) {}
  AstSetLit(int64 min0, int64 max0) : interval(true), imin(min0), imax(max0) {}
  AstSetLit(const std::vector<int64>& s0) : interval(false), s(s0) {}
  bool empty(void) const {
    return ( (interval && imin > imax) || (!interval && s.size() == 0));
  }
  AstSetLit* Copy() const {
    if (interval) {
      return new AstSetLit(imin, imax);
    } else {
      return new AstSetLit(s);
    }
  }
  virtual string DebugString() const {
    if (interval)
      return StringPrintf("%" GG_LL_FORMAT "d..%" GG_LL_FORMAT "d",
                          imin, imax);
    else {
      string output = "s({";
      for (unsigned int i = 0; i < s.size(); i++) {
        output += StringPrintf("%" GG_LL_FORMAT "d%s",
                               s[i], (i < s.size()-1 ? ", " : "})"));
      }
      return output;
    }
  }
};

/// Variable node base class
class AstVar : public AstNode {
 public:
  int i;
  AstVar(int i0) : i(i0) {}
};
/// Boolean variable node
class AstBoolVar : public AstVar {
 public:
  AstBoolVar(int i0) : AstVar(i0) {}
  virtual string DebugString() const {
    return StringPrintf("xb(%d)", i);
  }
};
/// Integer variable node
class AstIntVar : public AstVar {
 public:
  AstIntVar(int i0) : AstVar(i0) {}
  virtual string DebugString() const {
    return StringPrintf("xi(%d)", i);
  }
};
/// Float variable node
class AstFloatVar : public AstVar {
 public:
  AstFloatVar(int i0) : AstVar(i0) {}
  virtual string DebugString() const {
    return StringPrintf("xf(%d)", i);
  }
};
/// %Set variable node
class AstSetVar : public AstVar {
 public:
  AstSetVar(int i0) : AstVar(i0) {}
  virtual string DebugString() const {
    return StringPrintf("xs(%d)", i);
  }
};

/// %Array node
class AstArray : public AstNode {
 public:
  std::vector<AstNode*> a;
  AstArray(const std::vector<AstNode*>& a0)
      : a(a0) {}
  AstArray(AstNode* n)
  : a(1) { a[0] = n; }
  AstArray(int n=0) : a(n) {}
  virtual string DebugString() const {
    string output = "[";
    for (unsigned int i=0; i<a.size(); i++) {
      output += a[i]->DebugString();
      if (i < a.size() - 1)
        output += ", ";
    }
    output += "]";
    return output;
  }
  ~AstArray(void) {
    for (int i=a.size(); i--;)
      delete a[i];
  }
};

/// %Node representing a function call
class AstCall : public AstNode {
 public:
  const std::string id;
  AstNode* const args;
  AstCall(const std::string& id0, AstNode* args0)
      : id(id0), args(args0) {}
  ~AstCall(void) { delete args; }
  virtual string DebugString() const {
    return StringPrintf("%s(%s)", id.c_str(), args->DebugString().c_str());
  }
  AstArray* getArgs(unsigned int n) {
    AstArray *a = args->getArray();
    if (a->a.size() != n)
      throw AstTypeError("arity mismatch");
    return a;
  }
};

/// %Node representing an array access
class AstArrayAccess : public AstNode {
 public:
  AstNode* a;
  AstNode* idx;
  AstArrayAccess(AstNode* a0, AstNode* idx0)
      : a(a0), idx(idx0) {}
  ~AstArrayAccess(void) { delete a; delete idx; }
  virtual string DebugString() const {
    return StringPrintf("%s[%s]",
                        a->DebugString().c_str(),
                        idx->DebugString().c_str());
  }
};

/// %Node representing an atom
class AstAtom : public AstNode {
 public:
  std::string id;
  AstAtom(const std::string& id0) : id(id0) {}
  virtual string DebugString() const {
    return id;
  }
};

/// %String node
class AstString : public AstNode {
 public:
  std::string s;
  AstString(const std::string& s0) : s(s0) {}
  virtual string DebugString() const {
    return StringPrintf("s(\"%s\")", s.c_str());
  }
};

inline AstNode::~AstNode(void) {}

inline void AstNode::append(AstNode* newNode) {
  AstArray* a = dynamic_cast<AstArray*>(this);
  if (!a)
    throw AstTypeError("array expected");
  a->a.push_back(newNode);
}

inline bool AstNode::hasAtom(const std::string& id) {
  if (AstArray* a = dynamic_cast<AstArray*>(this)) {
    for (int i=a->a.size(); i--;)
      if (AstAtom* at = dynamic_cast<AstAtom*>(a->a[i]))
        if (at->id == id)
          return true;
  } else if (AstAtom* a = dynamic_cast<AstAtom*>(this)) {
    return a->id == id;
  }
  return false;
}

inline bool AstNode::isCall(const std::string& id) {
  if (AstCall* a = dynamic_cast<AstCall*>(this)) {
    if (a->id == id)
      return true;
  }
  return false;
}

inline AstCall* AstNode::getCall(void) {
  if (AstCall* a = dynamic_cast<AstCall*>(this))
    return a;
  throw AstTypeError("call expected");
}

inline bool AstNode::hasCall(const std::string& id) {
  if (AstArray* a = dynamic_cast<AstArray*>(this)) {
    for (int i=a->a.size(); i--;)
      if (AstCall* at = dynamic_cast<AstCall*>(a->a[i]))
        if (at->id == id) {
          return true;
        }
  } else if (AstCall* a = dynamic_cast<AstCall*>(this)) {
    return a->id == id;
  }
  return false;
}

inline bool AstNode::isInt(int& i) {
  if (AstIntLit* il = dynamic_cast<AstIntLit*>(this)) {
    i = il->i;
    return true;
  }
  return false;
}

inline AstCall* AstNode::getCall(const std::string& id) {
  if (AstArray* a = dynamic_cast<AstArray*>(this)) {
    for (int i=a->a.size(); i--;)
      if (AstCall* at = dynamic_cast<AstCall*>(a->a[i]))
        if (at->id == id)
          return at;
  } else if (AstCall* a = dynamic_cast<AstCall*>(this)) {
    if (a->id == id)
      return a;
  }
  throw AstTypeError("call expected");
}

inline AstArray* AstNode::getArray(void) {
  if (AstArray* a = dynamic_cast<AstArray*>(this))
    return a;
  throw AstTypeError("array expected");
}

inline AstAtom* AstNode::getAtom(void) {
  if (AstAtom* a = dynamic_cast<AstAtom*>(this))
    return a;
  throw AstTypeError("atom expected");
}

inline int AstNode::getIntVar(void) {
  if (AstIntVar* a = dynamic_cast<AstIntVar*>(this))
    return a->i;
  throw AstTypeError("integer variable expected");
}
inline int AstNode::getBoolVar(void) {
  if (AstBoolVar* a = dynamic_cast<AstBoolVar*>(this))
    return a->i;
  throw AstTypeError("bool variable expected");
}
inline int AstNode::getSetVar(void) {
  if (AstSetVar* a = dynamic_cast<AstSetVar*>(this))
    return a->i;
  throw AstTypeError("set variable expected");
}
inline int64 AstNode::getInt(void) {
  if (AstIntLit* a = dynamic_cast<AstIntLit*>(this))
    return a->i;
  throw AstTypeError("integer literal expected");
}
inline void AstNode::setInt(int64 v) {
  if (AstIntLit* a = dynamic_cast<AstIntLit*>(this))
    a->i = v;
  else
    throw AstTypeError("integer literal expected");
}
inline bool AstNode::getBool(void) {
  if (AstBoolLit* a = dynamic_cast<AstBoolLit*>(this))
    return a->b;
  throw AstTypeError("bool literal expected");
}
inline double AstNode::getFloat(void) {
  if (AstFloatLit* a = dynamic_cast<AstFloatLit*>(this))
    return a->d;
  throw AstTypeError("float literal expected");
}
inline AstSetLit* AstNode::getSet(void) {
  if (AstSetLit* a = dynamic_cast<AstSetLit*>(this))
    return a;
  throw AstTypeError("set literal expected");
}
inline std::string AstNode::getString(void) {
  if (AstString* a = dynamic_cast<AstString*>(this))
    return a->s;
  throw AstTypeError("string literal expected");
}
inline bool AstNode::isIntVar(void) {
  return (dynamic_cast<AstIntVar*>(this) != NULL);
}
inline bool AstNode::isBoolVar(void) {
  return (dynamic_cast<AstBoolVar*>(this) != NULL);
}
inline bool AstNode::isSetVar(void) {
  return (dynamic_cast<AstSetVar*>(this) != NULL);
}
inline bool AstNode::isInt(void) {
  return (dynamic_cast<AstIntLit*>(this) != NULL);
}
inline bool AstNode::isBool(void) {
  return (dynamic_cast<AstBoolLit*>(this) != NULL);
}
inline bool AstNode::isSet(void) {
  return (dynamic_cast<AstSetLit*>(this) != NULL);
}
inline bool AstNode::isString(void) {
  return (dynamic_cast<AstString*>(this) != NULL);
}
inline bool AstNode::isArray(void) {
  return (dynamic_cast<AstArray*>(this) != NULL);
}
inline bool AstNode::isAtom(void) {
  return (dynamic_cast<AstAtom*>(this) != NULL);
}

inline AstNode* AstExtractSingleton(AstNode* n) {
  if (AstArray* const a = dynamic_cast<AstArray*>(n)) {
    if (a->a.size() == 1) {
      AstNode* const ret = a->a[0];
      a->a[0] = NULL;
      delete a;
      return ret;
    }
  }
  return n;
}

// ---------- Specifications for constraints and variables ----------

class CtSpec;
class FlatZincModel;
typedef hash_set<CtSpec*> ConstraintSet;
typedef hash_set<AstNode*> NodeSet;

/// Symbol table mapping identifiers (strings) to values
template<class Val> class SymbolTable {
 public:
  /// Insert \a val with \a key
  void put(const std::string& key, const Val& val) {
    m[key] = val;
  }

  /// Return whether \a key exists, and set \a val if it does exist
  bool get(const std::string& key, Val& val) const {
    typename std::map<std::string,Val>::const_iterator i =
        m.find(key);
    if (i == m.end())
      return false;
    val = i->second;
    return true;
  }

 private:
  std::map<std::string, Val> m;
};

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
  int64 i;
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
             const Option<AstSetLit*>& d,
             bool introduced,
             bool own_domain)
      : VarSpec(name, introduced, false, false),
        own_domain_(own_domain) {
    i = -1;
    domain_ = d;
  }

  IntVarSpec(const string& name, int64 i0, bool introduced)
      : VarSpec(name, introduced, false, true),
        own_domain_(false),
        domain_(Option<AstSetLit*>::none()) {
    i = i0;
  }

  IntVarSpec(const string& name, const Alias& eq, bool introduced)
      : VarSpec(name, introduced, true, false),
        own_domain_(false),
        domain_(Option<AstSetLit*>::none()) {
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
      domain_ =  Option<AstSetLit*>::some(new AstSetLit(nmin, nmax));
      own_domain_ = true;
      return true;
    }
    if (!own_domain_) {
      AstSetLit* const domain = domain_.value();
      if (domain->interval && domain->imin >= nmin && domain->imax <= nmax) {
        return true;
      }
      return false;  // IMPROVE ME.
    }
    AstSetLit* const domain = domain_.value();
    if (domain->interval) {
      domain->imin = std::max(domain->imin, nmin);
      domain->imax = std::min(domain->imax, nmax);
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
    AstSetLit* const domain = domain_.value();
    if (domain->interval) {
      if (domain->imin == val) {
        domain->imin++;
        return true;
      } else if (domain->imax == val) {
        domain->imax--;
        return true;
      } else if (val < domain->imin || val > domain->imax) {
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
      domain_ =  Option<AstSetLit*>::some(new AstSetLit(values));
      own_domain_ = true;
      return true;
    }
    if (!own_domain_) {
      return false;  // IMPROVE ME.
    }
    AstSetLit* const domain = domain_.value();
    if (domain->interval) {
      const int64 old_min = domain->imin;
      const int64 old_max = domain->imax;
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
                        domain_.value()->imin == domain_.value()->imax);
  }

  int GetBound() const {
    CHECK(IsBound());
    if (assigned) {
      return i;
    }
    return domain_.value()->imin;
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

  AstSetLit* Domain() const {
    return domain_.value();
  }

  bool HasDomain() const {
    return domain_.defined() && domain_.value() != NULL;
  }

 private:
  Option<AstSetLit*> domain_;
  bool own_domain_;
};

/// Specification for Boolean variables
class BoolVarSpec : public VarSpec {
 public:
  BoolVarSpec(const string& name,
              const Option<AstSetLit*>& d,
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
    return (assigned && i == 1);
  }

  bool IsFalse() const {
    return (assigned && i == 0);
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
  Option<AstSetLit*> domain_;
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
  Option<AstSetLit*> domain_;
  SetVarSpec(const string& name, bool introduced)
      : VarSpec(name, introduced, false, false),
        own_domain_(false) {
    domain_ = Option<AstSetLit*>::none();
  }
  SetVarSpec(const string& name,
             const Option<AstSetLit*>& v,
             bool introduced, bool own_domain)
      : VarSpec(name, introduced, false, false),
        own_domain_(own_domain) {
    domain_ = v;

  }
  SetVarSpec(const string& name, AstSetLit* v, bool introduced)
      : VarSpec(name, introduced, false, true),
        own_domain_(false) {
    domain_ = Option<AstSetLit*>::some(v);
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

class CtSpec {
 public:
  CtSpec(int index,
         const std::string& id,
         AstArray* const args,
         AstNode* const annotations)
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

  AstNode* Arg(int index) const {
    return args_->a[index];
  }

  AstNode* LastArg() const {
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

  bool IsDefined(AstNode* const arg) {
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

  AstArray* Args() const {
    return args_;
  }

  void ReplaceArg(int index, AstNode* const node) {
    delete args_->a[index];
    args_->a[index] = node;
  }

  AstNode* annotations() const {
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

  void SetDefinedArg(AstNode* const arg) {
    defined_arg_ = arg;
  }

  AstNode* DefinedArg() const {
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

  void AddAnnotation(AstNode* const node) {
    if (annotations_ == NULL) {
      annotations_ = new AstArray(node);
    } else {
      annotations_->getArray()->a.push_back(node);
    }
  }

  void RemoveDefines() {
    if (annotations_ != NULL) {
      AstArray* const ann_array = annotations_->getArray();
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
      if (AstArray* a = dynamic_cast<AstArray*>(annotations_)) {
        for (int i = a->a.size(); i--;) {
          if (AstAtom* at = dynamic_cast<AstAtom*>(a->a[i])) {
            if (at->id == "domain") {
              at->id = "null_annotation";
            }
          }
        }
      } else if (AstAtom* a = dynamic_cast<AstAtom*>(annotations_)) {
        if (a->id == "domain") {
          a->id = "null_annotation";
        }
      }
    }
  }

 private:
  const int index_;
  std::string id_;
  AstArray* const args_;
  AstNode* annotations_;
  std::vector<int> uses_;
  NodeSet requires_;
  bool nullified_;
  AstNode* defined_arg_;
};

/// %State of the %FlatZinc parser
class ParserState {
 public:
  ParserState(const std::string& b,
              operations_research::FlatZincModel* const model0)
      : buf(b.c_str()),
        pos(0),
        length(b.size()),
        model_(model0),
        hadError(false) {}

  ParserState(char* const buf0,
              int length0,
              operations_research::FlatZincModel* const model0)
      : buf(buf0), pos(0), length(length0), model_(model0), hadError(false) {}

  ~ParserState();

  void* yyscanner;
  const char* buf;
  unsigned int pos, length;

  SymbolTable<int64> int_var_map_;
  SymbolTable<int64> bool_var_map_;
  SymbolTable<int64> float_var_map_;
  SymbolTable<int64> set_var_map_;
  SymbolTable<std::vector<int64> > int_var_array_map_;
  SymbolTable<std::vector<int64> > bool_var_array_map_;
  SymbolTable<std::vector<int64> > float_var_array_map_;
  SymbolTable<std::vector<int64> > set_var_array_map_;
  SymbolTable<std::vector<int64> > int_value_array_map_;
  SymbolTable<std::vector<int64> > bool_value_array_map_;
  SymbolTable<int64> int_map_;
  SymbolTable<bool> bool_map_;
  SymbolTable<AstSetLit> set_map_;
  SymbolTable<std::vector<AstSetLit> > set_value_array_map_;

  std::vector<IntVarSpec*> int_variables_;
  std::vector<BoolVarSpec*> bool_variables_;
  std::vector<SetVarSpec*> set_variables_;

  std::vector<std::pair<AstNode*, AstSetLit*> > int_domain_constraints_;
  std::vector<std::pair<int, AstSetLit*> > set_domain_constraints_;
  std::vector<CtSpec*> constraints_;

  bool hadError;

  int FillBuffer(char* lexBuf, unsigned int lexBufSize);
  void output(std::string x, AstNode* n);
  AstArray* Output(void);
  void AnalyseAndCreateModel();
  AstNode* ArrayElement(string id, unsigned int offset);
  AstNode* VarRefArg(string id, bool annotation);
  void AddIntVarDomainConstraint(int var_id, AstSetLit* const dom);
  void AddBoolVarDomainConstraint(int var_id, AstSetLit* const dom);
  void AddSetVarDomainConstraint(int var_id, AstSetLit* const dom);
  void AddConstraint(const std::string& id,
                     AstArray* const args,
                     AstNode* const annotations);
  void InitModel();
  void FillOutput(operations_research::FlatZincModel& m);
  void Presolve();
  bool IsBound(AstNode* const node) const;
  int64 GetBound(AstNode* const node) const;
  bool IsAllDifferent(AstNode* const node) const;
  bool MergeIntDomain(IntVarSpec* const source, IntVarSpec* const dest);

  FlatZincModel* model() const {
    return model_;
  }

  AstNode* Copy(AstNode* const node) const {
    if (node->isIntVar()) {
      return int_args_[node->getIntVar()];
    } else if (node->isBoolVar()) {
      return bool_args_[node->getBoolVar()];
    }
    return NULL;
  }

  AstNode* IntCopy(int index) const {
    CHECK_NOTNULL(int_args_[index]);
    return int_args_[index];
  }

  AstNode* BoolCopy(int index) const {
    CHECK_NOTNULL(bool_args_[index]);
    return bool_args_[index];
  }

 private:
  void MarkAllVariables(AstNode* const node, NodeSet* const computed);
  void MarkComputedVariables(CtSpec* const spec, NodeSet* const computed);
  void SortConstraints(NodeSet* const candidates,
                       NodeSet* const computed_variables);
  void BuildModel(const NodeSet& candidates,
                  const NodeSet& computed_variables);
  bool PresolveOneConstraint(CtSpec* const spec);
  bool DiscoverAliases(CtSpec* const spec);
  void ReplaceAliases(CtSpec* const spec);
  AstNode* FindTarget(AstNode* const annotations) const;
  void CollectRequired(AstArray* const args,
                       const NodeSet& candidates,
                       NodeSet* const require) const;
  void ComputeDependencies(const NodeSet& candidates, CtSpec* const spec) const;
  void ComputeViableTarget(CtSpec* const spec, NodeSet* const candidates) const;
  void Sanitize(CtSpec* const spec);
  void BuildStatistics();
  void Regroup(const std::string& ct_id, const std::vector<int>& ct_list);
  void RegroupAux(const std::string& ct_id,
                  int start_index,
                  int end_index,
                  int output_var_index,
                  const std::vector<int>& indices);
  void Strongify(int constraint_index);
  bool IsAlias(AstNode* const node) const;
  bool IsIntroduced(AstNode* const node) const;
  IntVarSpec* IntSpec(AstNode* const node) const;

  operations_research::FlatZincModel* model_;
  std::vector<std::pair<std::string,AstNode*> > output_;
  NodeSet orphans_;
  NodeSet targets_;
  ConstraintSet stored_constraints_;
  std::vector<std::vector<int> > all_differents_;
  hash_map<string, std::vector<int> > constraints_per_id_;
  std::vector<std::vector<int> > constraints_per_int_variables_;
  std::vector<std::vector<int> > constraints_per_bool_variables_;
  hash_map<int, int> abs_map_;
  hash_map<int, std::pair<int, int> > differences_;
  std::vector<AstNode*> int_args_;
  std::vector<AstNode*> bool_args_;
  hash_map<int, int> int_aliases_;
  hash_map<int, hash_set<int> > reverse_int_aliases_;
};

AstNode* ArrayOutput(AstCall* ann);
}  // namespace operations_research

#endif
