// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include "constraint_solver/csharp/constraint_solver.swig"

// Include the file we want to wrap a first time.
%{
#include "constraint_solver/routing.h"
%}

// Convert RoutingModel::NodeIndex to (32-bit signed) integers.
%typemap(ctype) operations_research::RoutingModel::NodeIndex "int"
%typemap(imtype) operations_research::RoutingModel::NodeIndex "int"
%typemap(cstype) operations_research::RoutingModel::NodeIndex "int"
%typemap(csin) operations_research::RoutingModel::NodeIndex "$csinput"
%typemap(csout) operations_research::RoutingModel::NodeIndex {
  return $imcall;
}
%typemap(in) operations_research::RoutingModel::NodeIndex {
  $1 = operations_research::RoutingModel::NodeIndex($input);
}
%typemap(out) operations_research::RoutingModel::NodeIndex {
  $result = $1.value();
}
%typemap(csvarin)  operations_research::RoutingModel::NodeIndex
%{
        set { $imcall; }
%}
%typemap(csvarout, excode=SWIGEXCODE)  operations_research::RoutingModel::NodeIndex
%{
  get {
        return $imcall;
  }
%}

// Convert std::vector<RoutingModel::NodeIndex> to/from int arrays.
CS_TYPEMAP_STDVECTOR(operations_research::RoutingModel::NodeIndex, int, int);
CS_TYPEMAP_STDVECTOR_IN1(operations_research::RoutingModel::NodeIndex, int, int);

// Create input mapping for NodeEvaluator2
%module(directors="1") main
%feature("director") NodeEvaluator2;
%{
class NodeEvaluator2 {
 public:
  int64 RunAux(operations_research::RoutingModel::NodeIndex i,
               operations_research::RoutingModel::NodeIndex j) {
    return Run(i.value(), j.value());
  }
  virtual int64 Run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* GetPermanentCallback() {
    return NewPermanentCallback(this, &NodeEvaluator2::RunAux);
  }
  virtual ~NodeEvaluator2() {}
};
%}

class NodeEvaluator2 {
 public:
  virtual ~NodeEvaluator2();
  virtual int64 Run(int i, int j) = 0;
  operations_research::RoutingModel::NodeEvaluator2* GetPermanentCallback();
};

%typemap(cstype) operations_research::RoutingModel::NodeEvaluator2* "NodeEvaluator2";
%typemap(csin) operations_research::RoutingModel::NodeEvaluator2* "$descriptor(ResultCallback2<int64, _RoutingModel_NodeIndex, _RoutingModel_NodeIndex>*).getCPtr($csinput.GetPermanentCallback())";

%ignore operations_research::RoutingModel::AddVectorDimension(
    const int64* values,
    int64 capacity,
    const string& name);

%ignore operations_research::RoutingModel::AddMatrixDimension(
    const int64* const* values,
    int64 capacity,
    const string& name);

%extend operations_research::RoutingModel {
  void AddVectorDimension(const std::vector<int64>& values,
                          int64 capacity,
                          bool fix_start_cumul_to_zero,
                          const string& name) {
    DCHECK_EQ(values.size(), self->nodes());
    self->AddVectorDimension(values.data(), capacity,
                             fix_start_cumul_to_zero, name);
  }
}

%ignore operations_research::RoutingModel::WrapIndexEvaluator(
    Solver::IndexEvaluator2* evaluator);

%ignore operations_research::RoutingModel::RoutingModel(
    int nodes, int vehicles,
    const std::vector<std::pair<NodeIndex, NodeIndex> >& start_end);

%rename("%(camelcase)s", %$isfunction) "";

// Wrap cp includes
%include constraint_solver/routing.h
