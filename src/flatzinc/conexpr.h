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

#ifndef OR_TOOLS_FLATZINC_CONEXPR_H_
#define OR_TOOLS_FLATZINC_CONEXPR_H_

#include <string>
#include "flatzinc/ast.h"

namespace operations_research {

  /// Abstract representation of a constraint
  class ConExpr {
  public:
    /// Identifier for the constraint
    std::string id;
    /// Constraint arguments
    AST::Array* args;
    /// Constructor
    ConExpr(const std::string& id0, AST::Array* args0);
    /// Return argument \a i
    AST::Node* operator[](int i) const;
    /// Destructor
    ~ConExpr(void);
  };

  inline
  ConExpr::ConExpr(const std::string& id0, AST::Array* args0)
    : id(id0), args(args0) {}

  inline AST::Node*
  ConExpr::operator[](int i) const { return args->a[i]; }

  inline
  ConExpr::~ConExpr(void) {
    delete args;
  }

}

#endif

// STATISTICS: flatzinc-any
