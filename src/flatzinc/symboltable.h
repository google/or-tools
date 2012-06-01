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

#ifndef OR_TOOLS_FLATZINC_SYMBOLTABLE_H_
#define OR_TOOLS_FLATZINC_SYMBOLTABLE_H_

#include <map>
#include <vector>

namespace operations_research {

  /// Symbol table mapping identifiers (strings) to values
  template<class Val>
  class SymbolTable {
  private:
    std::map<std::string,Val> m;
  public:
    /// Insert \a val with \a key
    void put(const std::string& key, const Val& val);
    /// Return whether \a key exists, and set \a val if it does exist
    bool get(const std::string& key, Val& val) const;
  };

  template<class Val>
  void
  SymbolTable<Val>::put(const std::string& key, const Val& val) {
    m[key] = val;
  }

  template<class Val>
  bool
  SymbolTable<Val>::get(const std::string& key, Val& val) const {
    typename std::map<std::string,Val>::const_iterator i =
      m.find(key);
    if (i == m.end())
      return false;
    val = i->second;
    return true;
  }

}
#endif
