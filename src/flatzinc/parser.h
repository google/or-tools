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

#include <cstring>
#include "flatzinc/flatzinc.h"

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

#include <string>
#include <vector>
#include <iostream>
#include <algorithm>

#include "flatzinc/option.h"
#include "flatzinc/varspec.h"
#include "flatzinc/conexpr.h"
#include "flatzinc/ast.h"
#include "flatzinc/parser.tab.h"
#include "flatzinc/symboltable.h"

namespace operations_research {

typedef std::pair<std::string,Option<std::vector<int>* > > intvartype;

class VarSpec;
typedef std::pair<std::string, VarSpec*> varspec;

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
  ParserState(const std::string& b, operations_research::FlatZincModel* fg0)
      : buf(b.c_str()), pos(0), length(b.size()), fg(fg0), hadError(false) {}

  ParserState(char* buf0, int length0,
              operations_research::FlatZincModel* fg0)
      : buf(buf0), pos(0), length(length0), fg(fg0), hadError(false) {}

  void* yyscanner;
  const char* buf;
  unsigned int pos, length;
  operations_research::FlatZincModel* fg;
  std::vector<std::pair<std::string,AST::Node*> > _output;

  SymbolTable<int> intvarTable;
  SymbolTable<int> boolvarTable;
  SymbolTable<int> floatvarTable;
  SymbolTable<int> setvarTable;
  SymbolTable<std::vector<int> > intvararrays;
  SymbolTable<std::vector<int> > boolvararrays;
  SymbolTable<std::vector<int> > floatvararrays;
  SymbolTable<std::vector<int> > setvararrays;
  SymbolTable<std::vector<int> > intvalarrays;
  SymbolTable<std::vector<int> > boolvalarrays;
  SymbolTable<int> intvals;
  SymbolTable<bool> boolvals;
  SymbolTable<AST::SetLit> setvals;
  SymbolTable<std::vector<AST::SetLit> > setvalarrays;

  std::vector<varspec> intvars;
  std::vector<varspec> boolvars;
  std::vector<varspec> setvars;

  std::vector<CtSpec*> domainConstraints;
  std::vector<CtSpec*> constraints;

  bool hadError;

  int fillBuffer(char* lexBuf, unsigned int lexBufSize) {
    if (pos >= length)
      return 0;
    int num = std::min(length - pos, lexBufSize);
    memcpy(lexBuf,buf+pos,num);
    pos += num;
    return num;
  }

  void output(std::string x, AST::Node* n) {
    _output.push_back(std::pair<std::string,AST::Node*>(x,n));
  }

  AST::Array* getOutput(void) {
    OutputOrder oo;
    std::sort(_output.begin(),_output.end(),oo);
    AST::Array* a = new AST::Array();
    for (unsigned int i=0; i<_output.size(); i++) {
      a->a.push_back(new AST::String(_output[i].first+" = "));
      if (_output[i].second->isArray()) {
        AST::Array* oa = _output[i].second->getArray();
        for (unsigned int j=0; j<oa->a.size(); j++) {
          a->a.push_back(oa->a[j]);
          oa->a[j] = NULL;
        }
        delete _output[i].second;
      } else {
        a->a.push_back(_output[i].second);
      }
      a->a.push_back(new AST::String(";\n"));
    }
    return a;
  }

  void AddConstraints() {
    for (unsigned int i=constraints.size(); i--;) {
      if (!hadError) {
        fg->PostConstraint(constraints[i]);
        delete constraints[i];
      }
    }
  }
};
}  // namespace operations_research

#endif

// STATISTICS: flatzinc-any
