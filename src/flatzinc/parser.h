/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
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

#include "flatzinc/ast.h"
#include "flatzinc/flatzinc.h"
#include "flatzinc/parser.tab.h"
#include "flatzinc/spec.h"

namespace operations_research {
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

/// Strict weak ordering for output items
class OutputOrder {
 public:
  /// Return if \a x is less than \a y, based on first component
  bool operator ()(const std::pair<std::string,AST::Node*>& x,
                   const std::pair<std::string,AST::Node*>& y) {
    return x.first < y.first;
  }
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
  SymbolTable<AST::SetLit> set_map_;
  SymbolTable<std::vector<AST::SetLit> > set_value_array_map_;

  std::vector<IntVarSpec*> int_variables_;
  std::vector<BoolVarSpec*> bool_variables_;
  std::vector<SetVarSpec*> set_variables_;

  std::vector<std::pair<int, AST::SetLit*> > int_domain_constraints_;
  std::vector<std::pair<int, AST::SetLit*> > bool_domain_constraints_;
  std::vector<std::pair<int, AST::SetLit*> > set_domain_constraints_;
  std::vector<CtSpec*> constraints_;

  bool hadError;

  int FillBuffer(char* lexBuf, unsigned int lexBufSize);
  void output(std::string x, AST::Node* n);
  AST::Array* Output(void);
  void CreateModel();
  AST::Node* ArrayElement(string id, unsigned int offset);
  AST::Node* VarRefArg(string id, bool annotation);
  void AddIntVarDomainConstraint(int var_id, AST::SetLit* const dom);
  void AddBoolVarDomainConstraint(int var_id, AST::SetLit* const dom);
  void AddSetVarDomainConstraint(int var_id, AST::SetLit* const dom);
  void AddConstraint(const std::string& id,
                     AST::Array* const args,
                     AST::Node* const annotations);
  void InitModel();
  void FillOutput(operations_research::FlatZincModel& m);
  bool Propagate(CtSpec* const spec);
  bool IsReifTrue(CtSpec* const spec) const;
  bool IsBound(AST::Node* const node) const;
  int GetBound(AST::Node* const node) const;
  bool AllDifferent(AST::Node* const node) const;

  FlatZincModel* model() const {
    return model_;
  }

 private:
  int FindEndIntegerVariable(int index);
  int FindTarget(AST::Node* const annotations) const;
  void CollectRequired(AST::Array* const args,
                       const hash_set<int>& candidates,
                       hash_set<int>* const require) const;
  void ComputeDependencies(const hash_set<int>& candidates,
                           CtSpec* const spec) const;
  void ComputeViableTarget(CtSpec* const spec,
                           hash_set<int>* const candidates) const;

  operations_research::FlatZincModel* model_;
  std::vector<std::pair<std::string,AST::Node*> > output_;
  hash_set<int> orphans_;
  hash_set<int> all_different_constraints_;
  std::vector<std::vector<int> > all_differents_;
};

AST::Node* ArrayOutput(AST::Call* ann);
}  // namespace operations_research

#endif

// STATISTICS: flatzinc-any
