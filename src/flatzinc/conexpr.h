#ifndef OR_TOOLS_FLATZINC_CONEXPR_H_
#define OR_TOOLS_FLATZINC_CONEXPR_H_

#include <string>
#include "flatzinc/ast.h"

namespace operations_research {
struct CtSpec {
  CtSpec(const std::string& id0,
         AST::Array* const args0,
         AST::Node* const an0)
      : id(id0),
        args(args0),
        annotations(an0) {}

  ~CtSpec() {
    delete args;
    delete annotations;
  }

  AST::Node* Arg(int index) {
    return args->a[index];
  }

  const std::string id;
  AST::Array* const args;
  AST::Node* const annotations;
};
}  // namespace operations_research
#endif

// STATISTICS: flatzinc-any
