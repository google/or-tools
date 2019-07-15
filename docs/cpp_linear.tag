<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="file">
    <name>linear_solver.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/linear_solver/</path>
    <filename>linear__solver_8h</filename>
    <includes id="linear__solver_8pb_8h" name="linear_solver.pb.h" local="yes" imported="no">ortools/linear_solver/linear_solver.pb.h</includes>
    <class kind="class">operations_research::MPSolver</class>
    <class kind="class">operations_research::MPObjective</class>
    <class kind="class">operations_research::MPVariable</class>
    <class kind="class">operations_research::MPConstraint</class>
    <class kind="class">operations_research::MPSolverParameters</class>
    <class kind="class">operations_research::MPSolverInterface</class>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>const absl::string_view</type>
      <name>ToString</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>afc3e3b80841b587c6fbfd9e9f3ec9c59</anchor>
      <arglist>(MPSolver::OptimizationProblemType optimization_problem_type)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a2610f938f233d0adcd3142693f4a2683</anchor>
      <arglist>(std::ostream &amp;os, MPSolver::OptimizationProblemType optimization_problem_type)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a6e3ed7b755e2b756ef48c9b3bad4a780</anchor>
      <arglist>(std::ostream &amp;os, MPSolver::ResultStatus status)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>AbslParseFlag</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a61dc18a85425d0a7cf6aa3e7ce3199f6</anchor>
      <arglist>(absl::string_view text, MPSolver::OptimizationProblemType *solver_type, std::string *error)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>AbslUnparseFlag</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>af04d1dfc591c35038a974202e50e541f</anchor>
      <arglist>(MPSolver::OptimizationProblemType solver_type)</arglist>
    </member>
    <member kind="variable">
      <type>constexpr double</type>
      <name>kDefaultPrimalTolerance</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a07189276cc680928dad51ed197142077</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>linear_solver.pb.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/gen/ortools/linear_solver/</path>
    <filename>linear__solver_8pb_8h</filename>
    <class kind="struct">TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</class>
    <class kind="class">operations_research::MPVariableProto</class>
    <class kind="class">operations_research::MPConstraintProto</class>
    <class kind="class">operations_research::MPGeneralConstraintProto</class>
    <class kind="class">operations_research::MPIndicatorConstraint</class>
    <class kind="class">operations_research::MPSosConstraint</class>
    <class kind="class">operations_research::PartialVariableAssignment</class>
    <class kind="class">operations_research::MPModelProto</class>
    <class kind="class">operations_research::OptionalDouble</class>
    <class kind="class">operations_research::MPSolverCommonParameters</class>
    <class kind="class">operations_research::MPModelRequest</class>
    <class kind="class">operations_research::MPSolutionResponse</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::MPSosConstraint_Type &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::MPSolverCommonParameters_LPAlgorithmValues &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::MPModelRequest_SolverType &gt;</class>
    <class kind="struct">is_proto_enum&lt; ::operations_research::MPSolverResponseStatus &gt;</class>
    <namespace>internal</namespace>
    <namespace>operations_research</namespace>
    <member kind="define">
      <type>#define</type>
      <name>PROTOBUF_INTERNAL_EXPORT_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a2ff46d5dc479b9be7968c15b3f932277</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPSosConstraint_Type</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac4082c18fc997b28960d2a15a27af30b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSosConstraint_Type_SOS1_DEFAULT</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac4082c18fc997b28960d2a15a27af30ba35dfc279dac55f2292c50123bbd65eb4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSosConstraint_Type_SOS2</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac4082c18fc997b28960d2a15a27af30baa35d9c1cb44243e123f7d5993d5b726f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPSolverCommonParameters_LPAlgorithmValues</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_UNSPECIFIED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1a2218d316cfcac5a88342c95b188f3fda</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_DUAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1a129c4c6d32bf9aed2414939cb02ff99a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_PRIMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1a53de34dc95fb67212e335f19dc210516</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_BARRIER</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1a89ff8ffa01928d5993a1414705eecd15</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPModelRequest_SolverType</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca6fab373696058c6e9f279de4a8446411</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca0969851c637668f95c10ddb1ade866a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLPK_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4caa32d84461e16e800e3f996d6347a304d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GUROBI_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca3af34f198d539e787263f9eded0ce0cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CPLEX_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca4bdeae4b1af8d2cd4aab225db4fc0407</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4cac25c4844cbdf1e4d7c7efc11f1f8ebf4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLPK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4caf60a0830addaf4cf00bc59459fa6647e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca0e93bcd472e7a9296ff02058ed60f8d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GUROBI_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4cac8beb7f7b026823a6bc2e4e87f546da6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CPLEX_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca26762918189367f5e171d0e226084d82</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca67639f2cd42e1197b5ad69a004c93ba3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_SAT_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4cac0fedb2082db5e7c96da01b4149c318e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_KNAPSACK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4cabe010aed8c1b29c5a0fd9ac262ce791f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPSolverResponseStatus</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_OPTIMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaac95cb5be9e36b31647dd28910ac6cae4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_FEASIBLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaac7d90afd0518be8cd6433ecad656a83b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_INFEASIBLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa0da2dbf49d011970a770d42141819d0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_UNBOUNDED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaad73de4a0f9908a4c0d11246ecccf32b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_ABNORMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaac77789af50586fb2f81915dd1cb790b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_NOT_SOLVED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa667b6a5ed42c91ea81fa67c59cb3badb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_IS_VALID</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa97ee5aaa7f57f286d4a821dd6e57523f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_UNKNOWN_STATUS</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa84ea2a63b24de389aac6aa33b1203cd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa6ae83516a798f1675e1b4daf0d8ea6b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID_SOLUTION_HINT</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa0f9da70b2f2b1304313c3a2a5f4876b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaab90169f8480eca12c963af5ce50d36aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_SOLVER_TYPE_UNAVAILABLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaafa008125099beaab382c42682be6bbf9</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_NAMESPACE_OPEN ::operations_research::MPConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPConstraintProto &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a9a113c4684d1033ac9473dcb589f122e</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPGeneralConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPGeneralConstraintProto &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>abfcd5bdf253f964bbe89d47f24777443</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPIndicatorConstraint *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPIndicatorConstraint &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>ab171bd5035991fb7ab6e0163679238f9</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPModelProto &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a5c6146771791a1f32bad6494ca1a2bc5</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelRequest *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPModelRequest &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a6ea8278e40d80442b6ab7bbc2e92f238</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSolutionResponse *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPSolutionResponse &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a77c592e83322cb1b241720f74c467285</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSolverCommonParameters *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPSolverCommonParameters &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>af3e03f34b72f583856c6491b279e04af</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSosConstraint *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPSosConstraint &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>ab4154144ff7ab873599743507f1ac14f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPVariableProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::MPVariableProto &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>ae246d1c1c8bbf0af81168fb7621f920e</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::OptionalDouble &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a1a2d4f72f6da5b4955f50f92c57a618e</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::PartialVariableAssignment *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::PartialVariableAssignment &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a2868a3e35d5de439d4817308efe07c66</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSosConstraint_Type_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a69d74b24808a9eba4bcbc04c5bd1f9fb</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MPSosConstraint_Type_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a94d793569692b2bdcb76cf2d7736da05</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>MPSosConstraint_Type_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a0d84cc4ed67dd0a7ccf556176aa9bc1d</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSosConstraint_Type_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a6d1606a9e00c2974c23f2e758924b459</anchor>
      <arglist>(const std::string &amp;name, MPSosConstraint_Type *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab3ee5c7a9f799696432b082fd4835232</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a70bcdf756e44dfd2d5dab2a5cf4cfb9a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac96996b4dbc25690d6d7fe345b364519</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a3b1bc7a63f4a7972004060311346868f</anchor>
      <arglist>(const std::string &amp;name, MPSolverCommonParameters_LPAlgorithmValues *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPModelRequest_SolverType_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad26c438ab5f1b232d7eced80a2780ca0</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MPModelRequest_SolverType_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>af637f39c9ca296bf197d792c62167b7d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>MPModelRequest_SolverType_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5209f68ceef830f109310dc549479a9b</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPModelRequest_SolverType_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aeb81b2591906288f021c0a3e37843b37</anchor>
      <arglist>(const std::string &amp;name, MPModelRequest_SolverType *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverResponseStatus_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7a295b0760db498bc4fa9479bb8c2329</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MPSolverResponseStatus_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ace7f8b02c012c058db64b534e3378f0f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>MPSolverResponseStatus_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a188641a1ab5a4dda11c00a11149b07d4</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverResponseStatus_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a22b5756cf719f9b2d10dae67820cf885</anchor>
      <arglist>(const std::string &amp;name, MPSolverResponseStatus *value)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPSosConstraint_Type &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>adba5596aedf0d949dbafcef74a3f50ff</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPSolverCommonParameters_LPAlgorithmValues &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a29b652c1631e6d387cafbbd19a98952d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPModelRequest_SolverType &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>ab6581d728d80c7e2174e850337fe8bdc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::MPSolverResponseStatus &gt;</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>adc6127e6cad4afd5d1eeecb696e45321</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable">
      <type>const ::PROTOBUF_NAMESPACE_ID::internal::DescriptorTable</type>
      <name>descriptor_table_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>linear__solver_8pb_8h.html</anchorfile>
      <anchor>a73f97af81379c4dd1bb9082b3be9bd25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPConstraintProtoDefaultTypeInternal</type>
      <name>_MPConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a11d06964c51cd718a2a5c620c3289f7e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPGeneralConstraintProtoDefaultTypeInternal</type>
      <name>_MPGeneralConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab18f88184af1e6b0197a98cf0485803f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPIndicatorConstraintDefaultTypeInternal</type>
      <name>_MPIndicatorConstraint_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a1fa4d06ad0beb392a3144747d83fcc2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPModelProtoDefaultTypeInternal</type>
      <name>_MPModelProto_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa214723b84fc52d727efc5067df690e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPModelRequestDefaultTypeInternal</type>
      <name>_MPModelRequest_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5557bc052354d9b956a609d0698281d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPSolutionResponseDefaultTypeInternal</type>
      <name>_MPSolutionResponse_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a9c99a96a8b2fcf4ab6890a4717c92da5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPSolverCommonParametersDefaultTypeInternal</type>
      <name>_MPSolverCommonParameters_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7ece0f2b42b6eaf443223377343e1966</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPSosConstraintDefaultTypeInternal</type>
      <name>_MPSosConstraint_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a3916f807aef0b8a0929c71cb72f8fe2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPVariableProtoDefaultTypeInternal</type>
      <name>_MPVariableProto_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>af3dce953fd737d51dcb003b93452b3b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>OptionalDoubleDefaultTypeInternal</type>
      <name>_OptionalDouble_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5fd6483b24c303a0fbf9ab49846d370c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>PartialVariableAssignmentDefaultTypeInternal</type>
      <name>_PartialVariableAssignment_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac92dae0b80b47779fc1de1bf9e7df9dd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSosConstraint_Type</type>
      <name>MPSosConstraint_Type_Type_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8a26ab806b2722fadd4035cd0be0ae5b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSosConstraint_Type</type>
      <name>MPSosConstraint_Type_Type_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a50ec8ebec75c1daf0e7633cb74ff6657</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>MPSosConstraint_Type_Type_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a43079282dcdc58640a4fb8f3504d9548</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac1eda65381beae08503e8af2b57a0d4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a12a6be7881f2f7dd6e426242c961d5d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a2b0590a3e329a0bb8a10b866c28138a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPModelRequest_SolverType</type>
      <name>MPModelRequest_SolverType_SolverType_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a9eaabd9c53b8aa093483b2c664a405c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPModelRequest_SolverType</type>
      <name>MPModelRequest_SolverType_SolverType_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a42eca5d9d855cdf447e78e17acd87c7a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>MPModelRequest_SolverType_SolverType_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7affd70e5dc61deefab59f4c06149644</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSolverResponseStatus</type>
      <name>MPSolverResponseStatus_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a53c0861628965fd7d72a0816d8575c66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSolverResponseStatus</type>
      <name>MPSolverResponseStatus_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a1315bf58051fbf57733dc025d6994340</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>MPSolverResponseStatus_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac136e7845fbe09520c0e7777d9ae8b43</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>model_exporter.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/linear_solver/</path>
    <filename>model__exporter_8h</filename>
    <includes id="linear__solver_8pb_8h" name="linear_solver.pb.h" local="yes" imported="no">ortools/linear_solver/linear_solver.pb.h</includes>
    <class kind="struct">operations_research::MPModelExportOptions</class>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>util::StatusOr&lt; std::string &gt;</type>
      <name>ExportModelAsLpFormat</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a689d3552f87e89456c0c9a43847c964a</anchor>
      <arglist>(const MPModelProto &amp;model, const MPModelExportOptions &amp;options=MPModelExportOptions())</arglist>
    </member>
    <member kind="function">
      <type>util::StatusOr&lt; std::string &gt;</type>
      <name>ExportModelAsMpsFormat</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aef684073daca7460490db8d881f886e0</anchor>
      <arglist>(const MPModelProto &amp;model, const MPModelExportOptions &amp;options=MPModelExportOptions())</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>model_exporter_swig_helper.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/linear_solver/</path>
    <filename>model__exporter__swig__helper_8h</filename>
    <includes id="linear__solver_8pb_8h" name="linear_solver.pb.h" local="yes" imported="no">ortools/linear_solver/linear_solver.pb.h</includes>
    <includes id="model__exporter_8h" name="model_exporter.h" local="yes" imported="no">ortools/linear_solver/model_exporter.h</includes>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>std::string</type>
      <name>ExportModelAsLpFormatReturnString</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a4d319c19b685fe608fe013b573081351</anchor>
      <arglist>(const MPModelProto &amp;input_model, const MPModelExportOptions &amp;options=MPModelExportOptions())</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ExportModelAsMpsFormatReturnString</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a37abd61c0d982af79257814b6d3a733e</anchor>
      <arglist>(const MPModelProto &amp;input_model, const MPModelExportOptions &amp;options=MPModelExportOptions())</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>model_validator.h</name>
    <path>/usr/local/google/home/lperron/Work/or-tools/ortools/linear_solver/</path>
    <filename>model__validator_8h</filename>
    <includes id="linear__solver_8pb_8h" name="linear_solver.pb.h" local="yes" imported="no">ortools/linear_solver/linear_solver.pb.h</includes>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>std::string</type>
      <name>FindErrorInMPModelProto</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7a27bb74d09b7ba6ea0e97bb572d2755</anchor>
      <arglist>(const MPModelProto &amp;model)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>FindFeasibilityErrorInSolutionHint</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ae4ee4d82cf625670cdc1f52197454654</anchor>
      <arglist>(const MPModelProto &amp;model, double tolerance)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::MPModelRequest_SolverType &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1MPModelRequest__SolverType_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::MPSolverCommonParameters_LPAlgorithmValues &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1MPSolverCommonParameters__LPAlgorithmValues_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::MPSolverResponseStatus &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1MPSolverResponseStatus_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>is_proto_enum&lt; ::operations_research::MPSosConstraint_Type &gt;</name>
    <filename>structis__proto__enum_3_01_1_1operations__research_1_1MPSosConstraint__Type_01_4.html</filename>
  </compound>
  <compound kind="class">
    <name>operations_research::MPConstraint</name>
    <filename>classoperations__research_1_1MPConstraint.html</filename>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a1cc4ca29f46883ebff5bdb2b624318a0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a2ac836dbebc688c3ac5559fc33c20eb7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCoefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a7e9afe55140b2392a99e0e5ad3eab531</anchor>
      <arglist>(const MPVariable *const var, double coeff)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>GetCoefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a7f5fd19dabe1dbb767fc544fd6e95f26</anchor>
      <arglist>(const MPVariable *const var) const</arglist>
    </member>
    <member kind="function">
      <type>const absl::flat_hash_map&lt; const MPVariable *, double &gt; &amp;</type>
      <name>terms</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a2d5d7e32b11a7edb4d810f2f10900b17</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>lb</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a4d254962fb2fe607c875f2b4e33c26ac</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>ub</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a7e655031d93cbf4efbe827e3bd662ae7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetLB</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a85c6d75f990f000e5863b83bd56e0e98</anchor>
      <arglist>(double lb)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUB</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a84c9d35dde8a2e1c7caccf88e9e86d60</anchor>
      <arglist>(double ub)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetBounds</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>abf8bbda7b4d608d011f88d2df83db881</anchor>
      <arglist>(double lb, double ub)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_lazy</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a1341c1b4cf65970cc414b18dfc7b7a52</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_is_lazy</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>aabdeaba6d29ef030605a333cffc20c69</anchor>
      <arglist>(bool laziness)</arglist>
    </member>
    <member kind="function">
      <type>const MPVariable *</type>
      <name>indicator_variable</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a45532e8b56e4b92bb6363fc858d709ff</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>indicator_value</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>ad1b4cc3f73a08cad0716015adffd188f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>index</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>abdf2c9c953fd4d118e7871a716445600</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>dual_value</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>ad1ee565a0efc50e3a515aeca73553493</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>MPSolver::BasisStatus</type>
      <name>basis_status</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a8695f6bddabcff5af750918b919cab7a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>MPConstraint</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>adac818d5ad3d7e51129ba251491a5f46</anchor>
      <arglist>(int index, double lb, double ub, const std::string &amp;name, MPSolverInterface *const interface_in)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>set_dual_value</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a2a521156c4a2eafe918c36fbb386e9e8</anchor>
      <arglist>(double dual_value)</arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>MPSolver</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>ac2c01b4de8f7670e37daa7d42b804dd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>MPSolverInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>ac0aea0786e75adbb2d24c41c15e7456c</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>CBCInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>af5a7cf0c655f37c0b388a2ddcf32ac3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>CLPInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a60944ecdcad88cfb4d4d32feea70c9b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>GLPKInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>ae1a3e0a695903c8e6effd524a7f92784</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>SCIPInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a236f9752f4df4c5134617330a040ec8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>SLMInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a5c083b37243075a00bf909840dc7c933</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>GurobiInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>ac28a56eeedb62d070578a9231f1875ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>CplexInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>ae7cbd08108e1636184f28c1a71c42393</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>GLOPInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a6c754b527a347994b06eeb49a09ac222</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>BopInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>a7383308e6b9b63b18196798db342ce8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>SatInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>acbd4413b1370baca9c45aecb0cb8ebd2</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>KnapsackInterface</name>
      <anchorfile>classoperations__research_1_1MPConstraint.html</anchorfile>
      <anchor>aee1ddf25e86286c16face31551751bda</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPConstraintProto</name>
    <filename>classoperations__research_1_1MPConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>MPConstraintProto</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a82ee586ca987262f6bbc180ba5d0aa54</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPConstraintProto</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aa2fe7dd81be8664cda0904ce46054f8d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPConstraintProto</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>affd68ff314706aad771e2d8ca1adbe79</anchor>
      <arglist>(const MPConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPConstraintProto</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a0f70e1e672ccbd5affd2a5e46a10a254</anchor>
      <arglist>(MPConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a5842ee6b7ed4c268cc41d87464a3b181</anchor>
      <arglist>(const MPConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a5c4fbd893d61dfb5abea6b365afe931e</anchor>
      <arglist>(MPConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ade167500822ccd95ea25ca389c4475f9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>abe045894ee3c1110249de6cfa6f4368b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>af1d13a6df383f47327ee616f8cdcc371</anchor>
      <arglist>(MPConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>MPConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a841ea5eda0b0a05f3a124745afd5cf6d</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ae7deb8233c44f06e4385a02e0fe04b95</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a586d11b330d52732af07a0d599afb45a</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aea296cdb66e72503ce8d17366b2b8bc3</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a9a7e9055566d92d98f47ce4ede21ac30</anchor>
      <arglist>(const MPConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a2659e15a5c5a9e1a15c215009380e573</anchor>
      <arglist>(const MPConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a662d0a48adefea051359e846d688f865</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ab83721c3c648c0eeb5bbf537acf5bd83</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ac4836cca1934e64dbea9e6ef3ce9cdee</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a7393723452a94363ce894a3406065900</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aec473efdfdaa2d67723b072f0e8550e3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a7d9e438875e7c3e8b4190a8854629242</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a26409443fc3814d1bb7f8447f0fbb7a3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ad01855f0c34db1c0f2e42305142ef573</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>var_index_size</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a2cde491b08e4441431c12c762a7076cc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aa526748f0cbc568a82a89720de2e1399</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a780c41f1f1c177b5fbf37219a67d6015</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>acd745a283043504a91ee092119ea5d90</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ab0b9a3c594c44ac6e49a848fc14d2a07</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a77e63b15fcfdc911f245bffb836d61a9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_var_index</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a5a4b26a43b9a108655cce2f3c19cf6f3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>coefficient_size</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a49a41e9c30b728835551f6778afced05</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_coefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a674f003b1363136981c8b6ff2ccd1a9e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>coefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ab7fd245949617780299ea0f0bed446c4</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_coefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ae3972775ecaeda6f9cc94af7d468158a</anchor>
      <arglist>(int index, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_coefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aaca8826ea6581f22fd0dedaca6887d7d</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; &amp;</type>
      <name>coefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a2acf13a458dd64dc62471064c39d0d5c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; *</type>
      <name>mutable_coefficient</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aacb8989ae0e24d68a66ddbf0762b17dc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a0379e4a190793bbd1b9f1ff7bc4a00df</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a2b81bf001e6446167d524d63924a364f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>abbebdd738eedb3590d3d2af853f2a052</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ae1f9fda0852e800f5253a6b401d2fe3e</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a5330be4e21146f52658a247bdbabb934</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a964a9b048ffeffa32bbcbf8485e2a3e0</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a0dcf494a2eb39924bea49de89e3cac94</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a9d6d9d5d6f97aa28b1a51bdcb8933c43</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aa27185b73e53ba34df82530bdf73e4c2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aefeb7d188d2187518ca19598db9fcb81</anchor>
      <arglist>(std::string *name)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_is_lazy</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a1f06e82321c4f697841c49c114fcd0f8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_is_lazy</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a74bae97c5b8276c4ae6e6165f1fecf3b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_lazy</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>add563c3db9265596107e38a0df57f89d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_is_lazy</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a71a038764e673accc0476b1fd8206a06</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_lower_bound</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ab1f474c4998aaf2a11425ebfd869809a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_lower_bound</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ac4a4330b5e8a46272dc5e79f8f42b361</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>lower_bound</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a0e37433ea0f58859b5184da5ee3c7f1d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_lower_bound</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a2f545208fa2ab549b0a34b0cbd54522f</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_upper_bound</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a14355698a6b13b75477e16e7095c4544</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_upper_bound</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ade5fd5169654f17614d3b426829273b4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>upper_bound</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aeeabf803b4f9d75b42bbd2ed06880a60</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_upper_bound</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aaf54b6fd5a4dedb5ad2fdaf1d5e70740</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a07ae8358259bae15bd62f8b12ac63732</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>afff0780dfc42c64fafdf0aefbb5bfc2f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a91d9ea4d8bd90580c6e78e15f166883b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a65e3d2031bdc293c2d85669dc1eca8a8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aaf390249615811d9f1af2e4b51bd0177</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a586cb2b0aec0b4d1d5e92f344da06052</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a0a6b2f25cbacb6871b220f1d55657735</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarIndexFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a479086d965728a2393e8cf081da15ffc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCoefficientFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a2f13cad466cd701f1b187483d59805a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNameFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a441d0fe7455f4ffeddc3a2477adbd022</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIsLazyFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>aeb3995c69c3d24582d11ee28c0523532</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLowerBoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a7b7aaa7430c7639d52507edf2306a4de</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUpperBoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a30f8e7ef7c4951cf8b8a4375b0df4d15</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPConstraintProto.html</anchorfile>
      <anchor>ab27b25852339539546a6b139b7112ddf</anchor>
      <arglist>(MPConstraintProto &amp;a, MPConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPGeneralConstraintProto</name>
    <filename>classoperations__research_1_1MPGeneralConstraintProto.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>GeneralConstraintCase</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2cf31917b69263cfbac0e486ddb6aba1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIndicatorConstraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2cf31917b69263cfbac0e486ddb6aba1a49273ebf624aaccbfa40d1b7e7c0a85f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSosConstraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2cf31917b69263cfbac0e486ddb6aba1af76aca0e4b44619463bc0b0e82737896</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GENERAL_CONSTRAINT_NOT_SET</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2cf31917b69263cfbac0e486ddb6aba1a958b131e29583210144fef16daf97795</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIndicatorConstraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2cf31917b69263cfbac0e486ddb6aba1a49273ebf624aaccbfa40d1b7e7c0a85f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kSosConstraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2cf31917b69263cfbac0e486ddb6aba1af76aca0e4b44619463bc0b0e82737896</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GENERAL_CONSTRAINT_NOT_SET</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2cf31917b69263cfbac0e486ddb6aba1a958b131e29583210144fef16daf97795</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPGeneralConstraintProto</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a4ef6ff6ba1c772ad245949374745abe5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPGeneralConstraintProto</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a6783c5a065b790d843c946d75e624bd3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPGeneralConstraintProto</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a1574f0ff7bd57173445f00124a3ce3cc</anchor>
      <arglist>(const MPGeneralConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPGeneralConstraintProto</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a23ebae958e5b79e041729918317a626e</anchor>
      <arglist>(MPGeneralConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPGeneralConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ade51c5fc9c7d152fc527acec3d061648</anchor>
      <arglist>(const MPGeneralConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPGeneralConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a19ca123af9d0e7864eae82cada2e3a25</anchor>
      <arglist>(MPGeneralConstraintProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ac5d8cfd0c330faeb3ad7d6d9f64e4339</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a68936ea3cd26c1b74b9509b995ecab29</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a6a15885d9dd8dc6ddacdeac999b84d74</anchor>
      <arglist>(MPGeneralConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>MPGeneralConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a156c085640796614ae514830f8aabb9b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPGeneralConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a0c235e52a7fd56d792aec91a50a3a5b3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ac8af879b433a989fc39fcdb127754267</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a4073780b39428a7602616410275209b9</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>afad84cb6995485a207d0804f81436536</anchor>
      <arglist>(const MPGeneralConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a22c151a5abcda31d86d375177ec50941</anchor>
      <arglist>(const MPGeneralConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>abd5067d3256977a140d31c2f9b40a249</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a60de46823c1e2931b4150bd7980c560b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a3ab81e2b0b99952c6e0dbd05ba2eb7ae</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a170ef3d8022f3430cea5be8176dfcd4a</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a6da2c5fcb342b019f938baa45580186d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a17de304caa2df417f3e67ab447e4e0bf</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a32726d5a4bebcdd8a84aef4633275184</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a5b4bf03309e3ca0c52b8a113c1b9eaf9</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a8bba7542eb93ec6f9464c8ee7219199c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a321828218bc588255d5337e6acd9eb48</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a74e9b83f583417d22571efe02b90c5e4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>aeed03354161d5dafd07faf7ff9908d2d</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>aa2dd699453a6ae1e0ec2ff4c5dc57ec1</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ae2f25571556b6694fc6969493a523eda</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ae7f25c54fdd9c84c338c7ec60c97e9f5</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>aa201de18393af8ca448358bdd7218c18</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a8f7b422a685ef384223cf105564f4e7d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a14ed792e09d02949e825225ccdc52380</anchor>
      <arglist>(std::string *name)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_indicator_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>aa87142dc2040bba8672cad97f858fdcd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_indicator_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ac05d6d6c5dbba96126c691912b249981</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::MPIndicatorConstraint &amp;</type>
      <name>indicator_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a10103847e9319221da2749f8efd0ef26</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPIndicatorConstraint *</type>
      <name>release_indicator_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>adfaa2ded770cbaf3a1d4a13b30fd1348</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPIndicatorConstraint *</type>
      <name>mutable_indicator_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a15f1013018bce808656ae4c95415cd69</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_indicator_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a48bf2bb541bea3140542a860bc07595c</anchor>
      <arglist>(::operations_research::MPIndicatorConstraint *indicator_constraint)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_sos_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>aa5a8cc57aa9631b80c4e2be8f249cf38</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_sos_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a97798df187e054cf43b13d982f98f48b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::MPSosConstraint &amp;</type>
      <name>sos_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a48b36b08a303a5e11c7cbceae0747359</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSosConstraint *</type>
      <name>release_sos_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a2f71c0c6408bd30de938edab231d8d47</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSosConstraint *</type>
      <name>mutable_sos_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a8ea86bd6e709db9947a3b6b9ac8eaf45</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_sos_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>aa5844d507b0e982b12946d0c5ccd06d1</anchor>
      <arglist>(::operations_research::MPSosConstraint *sos_constraint)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_general_constraint</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a5351c0cb7e3c235a55235ec3ca2535c2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>GeneralConstraintCase</type>
      <name>general_constraint_case</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a1fa1b2e8991b8063f44e5c9f60b12485</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a04eb965cb31429ace8176986e9e94e96</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a4352cc163cc9112cdfa4a2a9d7a39df7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ac05578dc80d2e0fdb31a38a82d607766</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPGeneralConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>abd12f3b07bf2728bed4448e21636d5a4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>aa4448f7de136867e33f95954998a6934</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPGeneralConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a5495cb1a043b7cfb73cc2cb76af06101</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>af5a0ebb374e39edbf1766525d532db39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNameFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ae057f2125478bc0a6bb8929d6dad7023</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIndicatorConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>ae2594704179fc4c5401f7d83cfb96d35</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSosConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a73e2a1db2b8567b692cb2c6a0196371f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPGeneralConstraintProto.html</anchorfile>
      <anchor>a8f38f5cf55d76f718a960b5b8d67198c</anchor>
      <arglist>(MPGeneralConstraintProto &amp;a, MPGeneralConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPIndicatorConstraint</name>
    <filename>classoperations__research_1_1MPIndicatorConstraint.html</filename>
    <member kind="function">
      <type></type>
      <name>MPIndicatorConstraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ac08487e68504709333737af09be2450e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPIndicatorConstraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a08b1b48c4cf73de8adc896252f6ec515</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPIndicatorConstraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a00e0b5db23c4b4324a07389600408c13</anchor>
      <arglist>(const MPIndicatorConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPIndicatorConstraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ad7e3123d36580743a53821081ad885a6</anchor>
      <arglist>(MPIndicatorConstraint &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPIndicatorConstraint &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a63446fecd1ef3f7ffe6b36aa125f9fc2</anchor>
      <arglist>(const MPIndicatorConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPIndicatorConstraint &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>aefcff32e94dc2a90f584a766c1f4bd99</anchor>
      <arglist>(MPIndicatorConstraint &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a2f69949237763a07a9b9d7bfdb63b0a1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>aa0e3997031cb0436e2160d5de17c3b04</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a8e6f6035dbe17c35778949f6607a7330</anchor>
      <arglist>(MPIndicatorConstraint *other)</arglist>
    </member>
    <member kind="function">
      <type>MPIndicatorConstraint *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a56d217e844d83e83c6f004d6667901e6</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPIndicatorConstraint *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>adbad288f72b75b30616fad06c1c8c5af</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ace0eafff2b3bff385766d8cd99bc4a6c</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a6edc740544090e00a81906975b4478f6</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>aca9514406e2603572b25a944f65f1cb1</anchor>
      <arglist>(const MPIndicatorConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a26986dcee2dca29205399b3b1279a4b8</anchor>
      <arglist>(const MPIndicatorConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a692a63ce43d44284aaf37097aaef659c</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>aeab111b8c411d95addba677867eced0e</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a94e4ece67f6c773bbe1db152d48ee128</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a40f0ae68942231e64fa1806f704fef21</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>af1b8c75cb249cf0b4b3beaca250db644</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>aad0876708fdf8cfb3f69ee02c9000380</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a2cf9578630df932dce4db3227906c402</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a5912ec20abf3de58f4d3852600774125</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_constraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ace76d92053879ffbde31295be1412d87</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_constraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>aa08721d49a0413e856287e18b3d445c1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::MPConstraintProto &amp;</type>
      <name>constraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a02112ccc2d2e12774abeae21e9d39916</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPConstraintProto *</type>
      <name>release_constraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a56b64e1eb3e46fc56fcc2b26bb9e2421</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPConstraintProto *</type>
      <name>mutable_constraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>aa09895a431862860449a5e478006d4f2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_constraint</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a834dd3b173822ac8e3c7ea14321b8a8a</anchor>
      <arglist>(::operations_research::MPConstraintProto *constraint)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_var_index</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>aee14b5535f9e1954fe957ceb9114b004</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_var_index</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ab2205d82bdad160ee2509fdd5fac3f51</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a9619fa3bdf73f8b58f56586aee338610</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_index</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a55ecaaaa4e00ac6e82466e47948b9a15</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_var_value</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a2eb4d6771867dcb48c76f7a8873e34ca</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_var_value</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a7c37e3a5b5b3e5a891d9b2299743d399</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>var_value</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a21d53b500cbd97fe5e31ff23b35a8812</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_value</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a058ad269a90a96ff7a1deba68f032b11</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a6eda95d439b2018075e9a0993b391379</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>afdfce7fd707734ca46b9d34ad7e281b2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a94e119884ac96e355cde0b7f0cd3dfd3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPIndicatorConstraint &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a65c53d4960d9ad7769422d5b47fef0cc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>af877cd6ab03d6462c817ae180c5fa2a5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPIndicatorConstraint *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a4e83a0bb25867fe9de814d46450b9b99</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ac31202232e26a6c704c98be9178ba38c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a45205178099d40e874441fa8124cd450</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarIndexFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ac4dc32ee5078fbb8dea629fa546b5f4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>ac966fa412048b51768b21c43da070ecb</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPIndicatorConstraint.html</anchorfile>
      <anchor>a18c8c95bac078ce4e80b718441462696</anchor>
      <arglist>(MPIndicatorConstraint &amp;a, MPIndicatorConstraint &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::MPModelExportOptions</name>
    <filename>structoperations__research_1_1MPModelExportOptions.html</filename>
    <member kind="function">
      <type></type>
      <name>MPModelExportOptions</name>
      <anchorfile>structoperations__research_1_1MPModelExportOptions.html</anchorfile>
      <anchor>a86112c3c11136b95424b6c60627d91b1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>obfuscate</name>
      <anchorfile>structoperations__research_1_1MPModelExportOptions.html</anchorfile>
      <anchor>af22a80b78769d34299cdc7c39549bb3b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>log_invalid_names</name>
      <anchorfile>structoperations__research_1_1MPModelExportOptions.html</anchorfile>
      <anchor>a7b8fcb7a9d972c70ccb83d34bfd9e207</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>bool</type>
      <name>show_unused_variables</name>
      <anchorfile>structoperations__research_1_1MPModelExportOptions.html</anchorfile>
      <anchor>a5e95cfdaa41bc1970763fbb1910286ac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int</type>
      <name>max_line_length</name>
      <anchorfile>structoperations__research_1_1MPModelExportOptions.html</anchorfile>
      <anchor>af2188b57edde3fa11b664af741cd4d18</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPModelProto</name>
    <filename>classoperations__research_1_1MPModelProto.html</filename>
    <member kind="function">
      <type></type>
      <name>MPModelProto</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>afc19167676980c67f0853f98f290fd19</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPModelProto</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>abbda629d4ea66666e419b406f6dd7942</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPModelProto</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa86070792ae245d44f118327004ee429</anchor>
      <arglist>(const MPModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPModelProto</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9ae0d50d391e9c33085f0d2f62f4f3ca</anchor>
      <arglist>(MPModelProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPModelProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9a347af6feb7c06b20b867b0a1075d13</anchor>
      <arglist>(const MPModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPModelProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a22d17426afe0af4f294f5ba29da522ff</anchor>
      <arglist>(MPModelProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a41446638a635e6a35dfd6d8ca47f2cfc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a0f23afc93086b7c2f026c1f0dc384ab7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a777b7e0a9d5fd43efa4b896c95a53409</anchor>
      <arglist>(MPModelProto *other)</arglist>
    </member>
    <member kind="function">
      <type>MPModelProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a468815c3cb78b068a7a8f04e6586258c</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPModelProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ac71a27e7f7ced602c0c6ff85e74601d4</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ac11f33e444a3256026ed062c8a31d516</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>af00b492bede9cea434c22461ac6a0cd1</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a241d15325442983ec540f5870e9e01de</anchor>
      <arglist>(const MPModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a0ed82623217e44daa96a299cab08c22f</anchor>
      <arglist>(const MPModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a06f1a19ca8d1431eddb455014653699d</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a30c0ad224f92ebd7ac4020fc0bae7545</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa837ba026ea9044e3a137df11021c495</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a6159869e1359e9cfb6cea3430f7704bc</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9741becf8d6a4cfd04eeaace9c65a5c3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a896e358adaed52ffdc7119a11f0f187c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aaebe1b2b0abe2ebb541a2193ac01c5ec</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ac3c9716dc8c9e6deb6c3650177b567f9</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>variable_size</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a14fd1215ea6e13b461b3b64829a2d35c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_variable</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a6f48541d07553cb24ffe701779200375</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPVariableProto *</type>
      <name>mutable_variable</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa55dcad76e104c34495572e6e28de653</anchor>
      <arglist>(int index)</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::MPVariableProto &gt; *</type>
      <name>mutable_variable</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa90a413c453d3b18fa421c3f4a40726e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::MPVariableProto &amp;</type>
      <name>variable</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ac9876e1f7ee63983c14b8b7fa5f7473c</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPVariableProto *</type>
      <name>add_variable</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9eb925d09b08c1016d6e900637e8a21d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::MPVariableProto &gt; &amp;</type>
      <name>variable</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ab5ae318d787951fc33935494bcebc144</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>constraint_size</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>af91908ae4dad316b74410421bf1936fa</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aba350f1a269c535d2d3d0265cf7c351c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPConstraintProto *</type>
      <name>mutable_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a855f85be118baa77b1c2bbe274b499db</anchor>
      <arglist>(int index)</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::MPConstraintProto &gt; *</type>
      <name>mutable_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>acbcf4f8e997d3c59d56ff589273e8b17</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::MPConstraintProto &amp;</type>
      <name>constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9fa3b93c884129f630ddd682366f402e</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPConstraintProto *</type>
      <name>add_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a1b0dbc47df1614cb7b369d6ef9d60dd1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::MPConstraintProto &gt; &amp;</type>
      <name>constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a140bf890bfceafaca6a15cf8f84c4bc1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>general_constraint_size</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a7d522eef9045e2885e66743ad2b84168</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_general_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a57a62999f91d9391421cc7cd1508533e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPGeneralConstraintProto *</type>
      <name>mutable_general_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a40fde17810f546a0c092d0c6ecca7aa0</anchor>
      <arglist>(int index)</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::MPGeneralConstraintProto &gt; *</type>
      <name>mutable_general_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aef2719f6a7ab16f74ba43ed230a30d89</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::MPGeneralConstraintProto &amp;</type>
      <name>general_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ae14ddee93568173b75b63924f88b1883</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPGeneralConstraintProto *</type>
      <name>add_general_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>af33883ff76442f9925a817cabb059dc7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedPtrField&lt; ::operations_research::MPGeneralConstraintProto &gt; &amp;</type>
      <name>general_constraint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a3665914fdbbdc6aec50fb3d72f612ddd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa7de06e4de50da30719a7f026f890496</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a45da2f604022beaee16e5ed797c81cb8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a6850e569a7cfa2964a32554201899737</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a5ac0e95d76d81bd2357a1d3dbedaa932</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa8d1e82150889dfc9c3153487670e28f</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a8b0580d3edebf3f20786081e5f23b85f</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a49d460a0e3d92fc7d284ba4a8d72640d</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a2d85fe9aad80c83e7d904b73da5a06d9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ad3aca9f5ef2959d6e969001fe32110e7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9663569e272e1bb41d68a6d3cb6b6c07</anchor>
      <arglist>(std::string *name)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_solution_hint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aca2749e68edea763a46706361c1bf9d0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solution_hint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a5a68808aaf2b3f48861ed3765a20f01b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::PartialVariableAssignment &amp;</type>
      <name>solution_hint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a294cd3e86115d2db8b9c48a24e3010f0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::PartialVariableAssignment *</type>
      <name>release_solution_hint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a738289e1b4a63ee573e15d8251d1fff2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::PartialVariableAssignment *</type>
      <name>mutable_solution_hint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a2209d493a3b4d27891de688e1077889f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_solution_hint</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>abb0c6d0ba18d52edc99aad81499bafa0</anchor>
      <arglist>(::operations_research::PartialVariableAssignment *solution_hint)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_objective_offset</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a02496c2d2e292ebdef1c95b9d44b1420</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_objective_offset</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a508b09af2a1a7a702ff3d3d15f3cdad8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>objective_offset</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>afe94623b2d2e8cf1902badbf53e4fb5f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_objective_offset</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a7d4cc573df9d697b9abfe13fd31d3fda</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_maximize</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ae27653c6c2edf9cac13c87ece57f4461</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_maximize</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>adf8ffb84f602291e2dc947ddbd9788b5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>maximize</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a76f5d78d924775583e84323cf17afdc3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_maximize</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a1d64908c6eb5b8e9a15902fea80c3057</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>af5e12572814c0270376b485137db2efd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ac1f9d28cbbc9640e80505533e4c23d81</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>ad737b1c9b768a3f41d518f47e57430e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPModelProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>aa0c0432cb0c479ddd35544cfa023fd88</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a033c0190a0c2278e83e311cbbcff8f94</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPModelProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a2ab39b9ef7809eb287a78b11e4e1d8ec</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>acdabf20fa8be8ff8944a42cc73b4a608</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVariableFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9c95f3f52ef05577972f131e2fe20d3c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>af61ec53c83a3e26a39266f4b5dfb5771</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kGeneralConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a727693fc6e01ab88556d31ad489a6bf4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNameFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a4479518628ab3ca572e07638b409b38e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolutionHintFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a1b384ae7a79bf4633cd44369c3998f79</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kObjectiveOffsetFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a23012da9df4a1288f225c1b71b9dfaa9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaximizeFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a27901ac2d990d284f461a716ad01c77c</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPModelProto.html</anchorfile>
      <anchor>a301641c5bae6af6d35fb2e1cdc9ec42f</anchor>
      <arglist>(MPModelProto &amp;a, MPModelProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPModelRequest</name>
    <filename>classoperations__research_1_1MPModelRequest.html</filename>
    <member kind="typedef">
      <type>MPModelRequest_SolverType</type>
      <name>SolverType</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a6a3005022de6d7df32031b2db297017d</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPModelRequest</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a21355b6fb0205dbfd20ae939f52a8362</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPModelRequest</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a78632fd84fc2d22aa67a99c82c7ef2e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPModelRequest</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aa790ada507ed2407bcfa8526fdaa83c8</anchor>
      <arglist>(const MPModelRequest &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPModelRequest</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a47a06f7c07b569b7e820bd9bff5ead44</anchor>
      <arglist>(MPModelRequest &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPModelRequest &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a25dd7d10f6941b9f802956fcf93e6f82</anchor>
      <arglist>(const MPModelRequest &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPModelRequest &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ac762029a62425eaf7e57075d3ea2456e</anchor>
      <arglist>(MPModelRequest &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a22fb1d32fe12402ef84e14a561e31a2c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a412ea0812202862d8d4e5b23eb34075f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ab43b434242a85136e884a6c477e65282</anchor>
      <arglist>(MPModelRequest *other)</arglist>
    </member>
    <member kind="function">
      <type>MPModelRequest *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a6059aff9650b4461a2fed4fba224e325</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPModelRequest *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a612bfb8ca4f8a792949b06ca47c3ca1d</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ad641b13b91314f3309f88a1ebdd70583</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a67451b2fc312a8a6de11dafc8476dc7d</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a5ef2e82abd9f363594b1b999ed62ccda</anchor>
      <arglist>(const MPModelRequest &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a0dd1a5bfa19358067efd89c351cd717f</anchor>
      <arglist>(const MPModelRequest &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ac1ba3033490c1ab7f085817491d22dae</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a7f3a3c66c2cba60ccfb35b961335a37e</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>acd2a42e71ef4c50a4e0826ac273376c6</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a141877501be675c72da09478ce50e55c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a77ea7df548b25370633869f0caf1f921</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>afeafb49a3edf2c668c542d1ec683feed</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ac291e23db35f959dd9bc27656002ba2f</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>afbfa135179cc55eeaec72c15d2b37ca3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a1d6fa833f3f021a66f218e3b844912f9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>abe4d7b688bb45f29e8cb66c4c44666f8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a3f0ea522ebc273f907a96434006134f5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a647c582cb93d3748e3c6e80e32cb26a8</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a592a98b4bf6a8d7a993ae2ede5983450</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ab8c4387591a27bb1b8c50b2f9c776b1b</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a3907db52e3feb776d4ccf6cd69f489ef</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>mutable_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>afee47e519007f2309c89d8f2ff8001f2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a2feff3d15e814351fbd150cd225f8dd2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_solver_specific_parameters</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a6247984f936d1c758908988418f4ca0c</anchor>
      <arglist>(std::string *solver_specific_parameters)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_model</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>add8f266800fdfe160ada2bc3d55345bb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_model</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a118e898bbe95c0a13885c0487b5ef744</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::MPModelProto &amp;</type>
      <name>model</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a2d10e31d0aee82d986b6d6ee36a3b8f4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelProto *</type>
      <name>release_model</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a5032e6543c89f76bd973671586b52092</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelProto *</type>
      <name>mutable_model</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a6400b0b7f9c42f6500222a2a69628737</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_model</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>afa16f5780184e4cc3e9aec1f3af7b937</anchor>
      <arglist>(::operations_research::MPModelProto *model)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_solver_time_limit_seconds</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a8a3976afeaec68a94962b691b640602c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solver_time_limit_seconds</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a5a890ccbf652026e997348100f3a128c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>solver_time_limit_seconds</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a270a2c19f606dcb8f83dddf243b2eda6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solver_time_limit_seconds</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>abcfe8954f7f20ceab98a55487fd977b6</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_enable_internal_solver_output</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>abb2457f3861fd64a669b984c89c8d98b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_enable_internal_solver_output</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a9ff77d5905a6217d48e3416fdda752a7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>enable_internal_solver_output</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a2288363e545507e1f46c67342715ae9b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_enable_internal_solver_output</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a2170e9d9170c52973b46bc5a3ddcf7e3</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_solver_type</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aa4efe4bd8083d797204a5e2302486d08</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solver_type</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a7dec127297cb18e84924ba1ebb94373f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPModelRequest_SolverType</type>
      <name>solver_type</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ab6ade0421447a1ec594e3438d03ee978</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solver_type</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a87941923c7a6f1e0688e41c7ff01defc</anchor>
      <arglist>(::operations_research::MPModelRequest_SolverType value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>add16a1bf2f363b611e9fe4c1e7397e88</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aaf58d06bc141be9c822896176e56f32a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a05ca64f1fb20d0127fb684dbdd291a63</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPModelRequest &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aa6562cc657806d7d8061e4dd1c95327a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aefbe5cc481a5f05affe62fa847364da6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPModelRequest *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ab35dee69590227c9babd67710f15f8c1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>SolverType_IsValid</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a3a55b88af0e6f38169e6adc88d33d341</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>SolverType_descriptor</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ac06437e0133322deba7ad53b3f60e171</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>SolverType_Name</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ae201ed7def637c43d53e242fa41f2d37</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>SolverType_Parse</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a95ec94dc6b3ea38a80ac2bceeb9c4958</anchor>
      <arglist>(const std::string &amp;name, SolverType *value)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a3222e53b28d5734acce946e0fcdaf2a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ad95405e840ce5bae4d43977e2e6407af</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a2ea3ce8df0f748802fb97c0b3a6a5722</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>GLPK_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aba5737f3cad6e067bcfc7dd04c870b63</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>GUROBI_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>abca4692f0fcfa117ca40af1c5c53f917</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>CPLEX_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a080026387b93e030672c47eebe9bcf2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ada47ef0b4153c303f4f8c0ff4518e9cc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>GLPK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aaaf45d1bde8a2dacb3060e75670d2935</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a1641b77916c0270e22357730e4fc4493</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>GUROBI_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a5e2c61bfc40faf93efadff0e59cfdb82</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>CPLEX_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>abe5c46883b619f9adc7c740c76c988c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a5c08074c8f6966c8d4182b0678a7e150</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>SAT_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a4cac26b88bf1179ab56a0208999ead9a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>KNAPSACK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>aa7dc7cc449b2299e191d36b865f3cd5e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>SolverType_MIN</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a5627a9ad23ae98827c388e15553ffe2a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr SolverType</type>
      <name>SolverType_MAX</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a493fc18496160428abe3ce9d2dc53ec8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>SolverType_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ac7cd44b7957b4cee491642df92d7fdb0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolverSpecificParametersFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a648ba13d6fe3f598e479827ee8feeba7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kModelFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a8640e3de7a39446e825a67a9cbaee146</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolverTimeLimitSecondsFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a35bafc1a48f8afda882b032e62c9ade2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kEnableInternalSolverOutputFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a6dd9e79974f131487bcb6ff2dd051d4d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolverTypeFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a574d4b71b338b04ac835da81c30d6f3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPModelRequest.html</anchorfile>
      <anchor>ae42c6d79fd1ec8f8c16c952c77f215d1</anchor>
      <arglist>(MPModelRequest &amp;a, MPModelRequest &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPObjective</name>
    <filename>classoperations__research_1_1MPObjective.html</filename>
    <member kind="function">
      <type>void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a2131c9028ff6d7047c8272c3ea3e62e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetCoefficient</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>aad1b0ca33b3a2c45d91e2875feb98c66</anchor>
      <arglist>(const MPVariable *const var, double coeff)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>GetCoefficient</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>aba6c839cf1c09f5d48ff6072cd6b28c5</anchor>
      <arglist>(const MPVariable *const var) const</arglist>
    </member>
    <member kind="function">
      <type>const absl::flat_hash_map&lt; const MPVariable *, double &gt; &amp;</type>
      <name>terms</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>ae1f99be84ce9efa28b4c1050af954835</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetOffset</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a24611f7b12b571fe1e73b629a8a6c17b</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>offset</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a8d6bb2249af13a783033763d292763d4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>OptimizeLinearExpr</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>ab1829576c2533e5de48b38447d9f6823</anchor>
      <arglist>(const LinearExpr &amp;linear_expr, bool is_maximization)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MaximizeLinearExpr</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a6927f7e28f694c55d72496c94a9a6b01</anchor>
      <arglist>(const LinearExpr &amp;linear_expr)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MinimizeLinearExpr</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a19a354c154bf034196d7a273feeff737</anchor>
      <arglist>(const LinearExpr &amp;linear_expr)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddLinearExpr</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a9e03dec6fb0099d48a119dd525879dd7</anchor>
      <arglist>(const LinearExpr &amp;linear_expr)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetOptimizationDirection</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>add0f9517dc64b1f768952fc490f7be00</anchor>
      <arglist>(bool maximize)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetMinimization</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>af9eea25e667a52dcad270495025e1202</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetMaximization</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a7f1396f72628328fd85ac852191fcc70</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>maximization</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a883fc7ee2a166fc6ff257296f78e9565</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>minimization</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a700788b8583ef4c69730be08a1a3ac28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>Value</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a43fd3a9687cfef2591b22c96cbe02477</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>BestBound</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a1ab47aaf7b73ae8a2664dd262227e3a9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>MPSolver</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>ac2c01b4de8f7670e37daa7d42b804dd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>MPSolverInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>ac0aea0786e75adbb2d24c41c15e7456c</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CBCInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>af5a7cf0c655f37c0b388a2ddcf32ac3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CLPInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a60944ecdcad88cfb4d4d32feea70c9b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>GLPKInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>ae1a3e0a695903c8e6effd524a7f92784</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>SCIPInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a236f9752f4df4c5134617330a040ec8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>SLMInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a5c083b37243075a00bf909840dc7c933</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>GurobiInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>ac28a56eeedb62d070578a9231f1875ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CplexInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>ae7cbd08108e1636184f28c1a71c42393</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>GLOPInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a6c754b527a347994b06eeb49a09ac222</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>BopInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>a7383308e6b9b63b18196798db342ce8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>SatInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>acbd4413b1370baca9c45aecb0cb8ebd2</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>KnapsackInterface</name>
      <anchorfile>classoperations__research_1_1MPObjective.html</anchorfile>
      <anchor>aee1ddf25e86286c16face31551751bda</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPSolutionResponse</name>
    <filename>classoperations__research_1_1MPSolutionResponse.html</filename>
    <member kind="function">
      <type></type>
      <name>MPSolutionResponse</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a833c349e47df157a774d29d2488d74bb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPSolutionResponse</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>af60e9ac94bf195014676464015497259</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSolutionResponse</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a440e5fed34c956e5ca5e0ca468389f98</anchor>
      <arglist>(const MPSolutionResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSolutionResponse</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aa7083de934957361064b85e671d98bf5</anchor>
      <arglist>(MPSolutionResponse &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPSolutionResponse &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a81401dd9366596f4111c545f7517c091</anchor>
      <arglist>(const MPSolutionResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPSolutionResponse &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a304b676e377a361681c228d194a35056</anchor>
      <arglist>(MPSolutionResponse &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a23ff8fa31306a5d90cea682d6484596e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a8c5995c70c6c633299ff1d697b242ae1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a6d6da769d3220182d7a95e99d54da12b</anchor>
      <arglist>(MPSolutionResponse *other)</arglist>
    </member>
    <member kind="function">
      <type>MPSolutionResponse *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a9b6499414284cfed319740ee0cc3c8af</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPSolutionResponse *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a885c043c62c25d6a6ddf2a547c7a94e3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a1ce5838e637ee9511b0deb790015e4a9</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a66afdea65fb57c621879985b47f5d3c3</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a384fe46115cc159fd3b21fc9acd0fafe</anchor>
      <arglist>(const MPSolutionResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a0769e61139812f239c5d955f928e5e60</anchor>
      <arglist>(const MPSolutionResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>abd61a4fdb904666e6b203f5b85f4ddbd</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>addb97e9eeeba0cdaafb182363cbaafb0</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ad5c0d27c677a85de081a39eed017ae06</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>acdd6d121d205c55c28b35fa1b7202bff</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a2b3456ec4e9e0f5dedae13a7c8e7aaa7</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a2dcb4d4fb72ca65ea9eb618c190c123f</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a134ca27f7b3a507da53522b9818cec85</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ad262be4a452cf124c10d0e85a3525e27</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>variable_value_size</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ae4bb0e074498f657cc1426bb5c1368b9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_variable_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a6da91d5cdb532512d4f2efc96c273500</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>variable_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a80f33fec5c1284de87a2fde8742cf53c</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_variable_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a0800460f522ddc4ed4792ee085c5a1c1</anchor>
      <arglist>(int index, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_variable_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a1eab1b57a080d4dd465436cb5ed87fd2</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; &amp;</type>
      <name>variable_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a1a4fdc5be6979047d5ab4bc4f756a089</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; *</type>
      <name>mutable_variable_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a3579d86d93ee10835aa417fce43225cc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>dual_value_size</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ace46a87c1facb8b33911dbb1d2fb526d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_dual_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ab3905acc7754f038751def3d3aa1d3a5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>dual_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a319b5eec9e7d1b4dd16ac00bc638651c</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_dual_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a6d730345a711e3b5d52a7acb0ec76949</anchor>
      <arglist>(int index, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_dual_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>abceb991a518493eb454df9c3272f4cd0</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; &amp;</type>
      <name>dual_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>afffec263de5be060fd683cb7f77f77bb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; *</type>
      <name>mutable_dual_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a38474f62aedfaced9ad77d8e82282d0a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>reduced_cost_size</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a3d86c3b36d523699014252790254826a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a46916fe6ced9e534fad008fb78765ffa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a858be2f1607683ec73a2c056783ad6dc</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aeacb60c3ab22a02359c6edf8730c61a6</anchor>
      <arglist>(int index, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a4f1757e5b80c384f31189ce310c6c4cc</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; &amp;</type>
      <name>reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a9f1cd44e5505ac392d844a2444d1f08f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; *</type>
      <name>mutable_reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a8a57911af8711c387db527c9f9ec2f1f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_objective_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a816e053105d440043ec4486361e62990</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_objective_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a822c9eeeea28341f1c804cffbf78e106</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>objective_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aaa4a14b0f8dd2b72664759b3a364f9cc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_objective_value</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a5f55375af57c2b7e797a992413ae0e8f</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_best_objective_bound</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a241360bf68127d58eddf3ef079859ba0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_best_objective_bound</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aa0ed8dfdd43246e12c02b81d03ae45ba</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>best_objective_bound</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a3aa7ae0547d526475d060edc8b8ca6b2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_best_objective_bound</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a8423de7149da893dc45c125c78dfcd20</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_status</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ad65e4d3267e1d2270b7d1bfb7eb18127</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_status</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a59e89fd692444fb987e92acc632cd1f6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSolverResponseStatus</type>
      <name>status</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a34bfe8d869b513c42a9f086ef8913152</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_status</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>adfd87a796632d91fc9b8c6ff859e8a9f</anchor>
      <arglist>(::operations_research::MPSolverResponseStatus value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a22059d09607c3b8243fa04bcaeb7fddd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aa5b39fa05d67f3fbc906bfc869fff943</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aa62d98a43cf90276fcc4fbd882b2f5f9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPSolutionResponse &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a1435bd9b0243d107f16087c9bbfefa28</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ac491b6603878a670b10abf8ad0464a64</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPSolutionResponse *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>af68dd53a793541c1dddf1cfb5b6def91</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a6e8113932c9b8dbd691046d00573994e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVariableValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ac98c58c65d284c4786e1e67cdb6d24a9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDualValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a99c2d42f522d104b063fdcd410f6849f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kReducedCostFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aee382a95d276dc7aedd71abf85ee7c75</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kObjectiveValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>aa242fccdc5e77e2febdf2d24f2ca0f25</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBestObjectiveBoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ac087d9c26bfdf30a96d8265c7c4effcf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kStatusFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>ac2712ff29ac754732f4041a7e67c1341</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPSolutionResponse.html</anchorfile>
      <anchor>abbb344cf9059573a256d77455a3dfa8c</anchor>
      <arglist>(MPSolutionResponse &amp;a, MPSolutionResponse &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPSolver</name>
    <filename>classoperations__research_1_1MPSolver.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>OptimizationProblemType</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2a91bb43cabe6c49465bd7138189f3ea84</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2a32049e26d1ea6f68624fc478b88d98c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2a357b78ac84d42c93f2be55c89ed685dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2a6f8b8f9d64ae299e8cfccf4917bf5282</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2aaef5b33a2d88606a978524ebc7b1cb7b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ResultStatus</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a7a765c1340de9cc37e22c68a2da7d390</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a5930d45ccc1bc78ad06bcd15cbca6a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a84e96d7264feeb2b6577400bc379d9db</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBOUNDED</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a88c7283cc752b51ed05c21c73f8fe100</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ABNORMAL</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192ae7c84444803ea46da465b68fb6e974fa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_INVALID</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a0ce4e033c6bca973cb026780cdb7daa2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a5c14184aabfffdb489347ab0486c8492</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>BasisStatus</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FREE</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743a642d0b5abe3faed060b4ec237df381a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>AT_LOWER_BOUND</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743a32b9b4f5f09a5dbdeed585318e8d97f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>AT_UPPER_BOUND</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743a578f080f1d30ca7ce7ba6c5b050ddd56</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FIXED_VALUE</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743aeca9b511e54f58239988d6affd62afa8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BASIC</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743a0185d5946c48b9852d8a02a7493f4dcf</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2a91bb43cabe6c49465bd7138189f3ea84</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2a32049e26d1ea6f68624fc478b88d98c9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2a357b78ac84d42c93f2be55c89ed685dc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2a6f8b8f9d64ae299e8cfccf4917bf5282</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a42c406c7e6fba381aa2bb41aae4b44f2aaef5b33a2d88606a978524ebc7b1cb7b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a7a765c1340de9cc37e22c68a2da7d390</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a5930d45ccc1bc78ad06bcd15cbca6a29</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a84e96d7264feeb2b6577400bc379d9db</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNBOUNDED</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a88c7283cc752b51ed05c21c73f8fe100</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>ABNORMAL</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192ae7c84444803ea46da465b68fb6e974fa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_INVALID</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a0ce4e033c6bca973cb026780cdb7daa2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>NOT_SOLVED</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16bca30fdb1b048d987631b757c63192a5c14184aabfffdb489347ab0486c8492</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FREE</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743a642d0b5abe3faed060b4ec237df381a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>AT_LOWER_BOUND</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743a32b9b4f5f09a5dbdeed585318e8d97f2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>AT_UPPER_BOUND</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743a578f080f1d30ca7ce7ba6c5b050ddd56</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FIXED_VALUE</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743aeca9b511e54f58239988d6affd62afa8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BASIC</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7e2a34816b22749e70e23d26f49cf743a0185d5946c48b9852d8a02a7493f4dcf</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSolver</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ab6a8a6c57eefce8c07c8a52e053b035b</anchor>
      <arglist>(const std::string &amp;name, OptimizationProblemType problem_type)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPSolver</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>aee7aedeeff79cd0645a5c7e8c0200834</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsMIP</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a24caaef373d3715d5bce9fb0da2c203d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a1d6101b365c33fb1f73a4c953abeb0ed</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual OptimizationProblemType</type>
      <name>ProblemType</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a54e8b352edd37540f788c3fc473fa875</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a4fb5381d2f4a764660365168622e4955</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>NumVariables</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a0ba0685c817d5c5910c80492dd1a7050</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const std::vector&lt; MPVariable * &gt; &amp;</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a1593ede4c9cd1da430f606127dc9a642</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>MPVariable *</type>
      <name>LookupVariableOrNull</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ae384aa3d9095f883a93f5e2e830e0077</anchor>
      <arglist>(const std::string &amp;var_name) const</arglist>
    </member>
    <member kind="function">
      <type>MPVariable *</type>
      <name>MakeVar</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ad6bb7605c749ba485b040b02a37f6728</anchor>
      <arglist>(double lb, double ub, bool integer, const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>MPVariable *</type>
      <name>MakeNumVar</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a54d66e99fdc2424e812d910e7c2f225a</anchor>
      <arglist>(double lb, double ub, const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>MPVariable *</type>
      <name>MakeIntVar</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>acf84ccc5151ce164a571d2f31f30960c</anchor>
      <arglist>(double lb, double ub, const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>MPVariable *</type>
      <name>MakeBoolVar</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7176be8ce0481d880a8d30a2d7a1c09e</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MakeVarArray</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a2c58f52acd6216131582c60aae3625ee</anchor>
      <arglist>(int nb, double lb, double ub, bool integer, const std::string &amp;name_prefix, std::vector&lt; MPVariable * &gt; *vars)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MakeNumVarArray</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7aebea4f022e7a685322e7db70b76e5e</anchor>
      <arglist>(int nb, double lb, double ub, const std::string &amp;name, std::vector&lt; MPVariable * &gt; *vars)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MakeIntVarArray</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a5eefc88942fd284ff2962564224d5f8e</anchor>
      <arglist>(int nb, double lb, double ub, const std::string &amp;name, std::vector&lt; MPVariable * &gt; *vars)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MakeBoolVarArray</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>af5c55776ce47479f8904480cd815a6d5</anchor>
      <arglist>(int nb, const std::string &amp;name, std::vector&lt; MPVariable * &gt; *vars)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>NumConstraints</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a16a3cee848c033e365ebb1cf50bb97cd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const std::vector&lt; MPConstraint * &gt; &amp;</type>
      <name>constraints</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>aebc45909b1f377ab86295578ec417a17</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>MPConstraint *</type>
      <name>LookupConstraintOrNull</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a299502e4b0e4e6608330fdbe4fb86c94</anchor>
      <arglist>(const std::string &amp;constraint_name) const</arglist>
    </member>
    <member kind="function">
      <type>MPConstraint *</type>
      <name>MakeRowConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a9afcc9a30bf7c360066d7936c121acd0</anchor>
      <arglist>(double lb, double ub)</arglist>
    </member>
    <member kind="function">
      <type>MPConstraint *</type>
      <name>MakeRowConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a9ef93893d198901ce104d74794dde123</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>MPConstraint *</type>
      <name>MakeRowConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ac7dc5e8edf7f3a96c2faadc738d52c41</anchor>
      <arglist>(double lb, double ub, const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>MPConstraint *</type>
      <name>MakeRowConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>abad1058684a6996ba3035c0011b4cc41</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>MPConstraint *</type>
      <name>MakeRowConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ae31ac8b47142ee8685a39a608a9190d1</anchor>
      <arglist>(const LinearRange &amp;range)</arglist>
    </member>
    <member kind="function">
      <type>MPConstraint *</type>
      <name>MakeRowConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a4c54bb9041abac99c35b92dc5386b7a7</anchor>
      <arglist>(const LinearRange &amp;range, const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const MPObjective &amp;</type>
      <name>Objective</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a4834a6747544a7053110a0b20d79dac2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>MPObjective *</type>
      <name>MutableObjective</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a467f89af3bae743dc9d628ee4e74f0c9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ResultStatus</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a1535b2a46d5cff6f9727c08085cfbb1f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ResultStatus</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>acfe9e4c330b12131b53d72f41506ddaf</anchor>
      <arglist>(const MPSolverParameters &amp;param)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Write</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ad40ba327269cfff827f23ac4d94414d9</anchor>
      <arglist>(const std::string &amp;file_name)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; double &gt;</type>
      <name>ComputeConstraintActivities</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ad07e28e347a4b2d94d53ca96ae201d70</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>VerifySolution</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a2a4a0234b5830d4ea82d549b3b6b5baf</anchor>
      <arglist>(double tolerance, bool log_errors) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reset</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a0bade4bbf46f4e35513650d38a0a3208</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>InterruptSolve</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a4413905b5839d17823e756cff10d0ffe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>MPSolverResponseStatus</type>
      <name>LoadModelFromProto</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>af9eb46d78f04fa12da7ac27c14becb7d</anchor>
      <arglist>(const MPModelProto &amp;input_model, std::string *error_message)</arglist>
    </member>
    <member kind="function">
      <type>MPSolverResponseStatus</type>
      <name>LoadModelFromProtoWithUniqueNamesOrDie</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a51b5d57f310db13d800e4440ca3c0d0b</anchor>
      <arglist>(const MPModelProto &amp;input_model, std::string *error_message)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>FillSolutionResponseProto</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a91c69a1b2e9098f3835b9fe1c4fead59</anchor>
      <arglist>(MPSolutionResponse *response) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ExportModelToProto</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a9c8cc8b3c36fe9c08d8e0eefbc98b774</anchor>
      <arglist>(MPModelProto *output_model) const</arglist>
    </member>
    <member kind="function">
      <type>util::Status</type>
      <name>LoadSolutionFromProto</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>aa20b60665bd7e137dac446b2b1400838</anchor>
      <arglist>(const MPSolutionResponse &amp;response, double tolerance=kDefaultPrimalTolerance)</arglist>
    </member>
    <member kind="function">
      <type>util::Status</type>
      <name>ClampSolutionWithinBounds</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a27312cbf1394d779305d016e2ea2753e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ExportModelAsLpFormat</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a393a84863fe633baa3c7af4e71c8b147</anchor>
      <arglist>(bool obfuscate, std::string *model_str) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>ExportModelAsMpsFormat</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ae06ddd4e79e6a985b2644eff14747484</anchor>
      <arglist>(bool fixed_format, bool obfuscate, std::string *model_str) const</arglist>
    </member>
    <member kind="function">
      <type>util::Status</type>
      <name>SetNumThreads</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a6394f5b0c08af038bfd9610d2bc4be90</anchor>
      <arglist>(int num_threads)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetNumThreads</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a88ddf26ba2c9524de319f6f307cfde60</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SetSolverSpecificParametersAsString</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a532d2fb86e5cdc4710e1a168acbbe7f6</anchor>
      <arglist>(const std::string &amp;parameters)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>GetSolverSpecificParametersAsString</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a64bc5e50054e619d3399956df3ed110f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetHint</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>abf4255929ea45766c51cf6138758b277</anchor>
      <arglist>(std::vector&lt; std::pair&lt; const MPVariable *, double &gt; &gt; hint)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetStartingLpBasis</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a11a76240b36c39f04ff64104c482990f</anchor>
      <arglist>(const std::vector&lt; MPSolver::BasisStatus &gt; &amp;variable_statuses, const std::vector&lt; MPSolver::BasisStatus &gt; &amp;constraint_statuses)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>OutputIsEnabled</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>aa90a00b370b9abc4a43bfefd7f6a895b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>EnableOutput</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>abc2c48c807107ccfdfa8c1b50ae16c41</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SuppressOutput</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>abfaa014d3c3ca883e3c8a17110372801</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>absl::Duration</type>
      <name>TimeLimit</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>aae2fbd44c86451dadfc256f000772394</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetTimeLimit</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a47a5516ca826adbc25bd5bf1d7935fd1</anchor>
      <arglist>(absl::Duration time_limit)</arglist>
    </member>
    <member kind="function">
      <type>absl::Duration</type>
      <name>DurationSinceConstruction</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a6a68e7fab8751978c9faa30867f91241</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>iterations</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a15001c5a8f5c0086dddcc4626a5a5ad7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>nodes</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a6837e7545ac2c5cfe95ca9a1c0f013e3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>SolverVersion</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>aee22680c23e591329e9ac50ff78f572d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void *</type>
      <name>underlying_solver</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a3d3bc5e39b383336fb3ca88b1a0c6cc5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>ComputeExactConditionNumber</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a6390f93d9c4e88775d98ec8353ef0979</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ABSL_MUST_USE_RESULT bool</type>
      <name>NextSolution</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a4b11747bf657bf074d1e710121810d13</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>time_limit</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a258c821fae2869693b58440145125aba</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_time_limit</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ae99e29fcc045ab27c8fecbdc422e6133</anchor>
      <arglist>(int64 time_limit_milliseconds)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>time_limit_in_secs</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a0177cee53f52d09df990920d532b0772</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>wall_time</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a1ce69ce989942416f35a7d3577b5edd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>OwnsVariable</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a78606aaae8d97a0def488e696fdb2d2b</anchor>
      <arglist>(const MPVariable *var) const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>SupportsProblemType</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ae96df0f02a46493eba93d2e70709911a</anchor>
      <arglist>(OptimizationProblemType problem_type)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ParseSolverType</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a650dd472cb06be9f9abcf5bc0833437d</anchor>
      <arglist>(absl::string_view solver, OptimizationProblemType *type)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>SolveWithProto</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a3aaa8bdb57173a9d933ed5f62bb60f42</anchor>
      <arglist>(const MPModelRequest &amp;model_request, MPSolutionResponse *response)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static double</type>
      <name>infinity</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a5b15f7248e2b72d474bae0444a613033</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>GLPKInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ae1a3e0a695903c8e6effd524a7f92784</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>CLPInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a60944ecdcad88cfb4d4d32feea70c9b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>CBCInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>af5a7cf0c655f37c0b388a2ddcf32ac3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>SCIPInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a236f9752f4df4c5134617330a040ec8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>GurobiInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ac28a56eeedb62d070578a9231f1875ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>CplexInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ae7cbd08108e1636184f28c1a71c42393</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>SLMInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a5c083b37243075a00bf909840dc7c933</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>MPSolverInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>ac0aea0786e75adbb2d24c41c15e7456c</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>GLOPInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a6c754b527a347994b06eeb49a09ac222</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>BopInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>a7383308e6b9b63b18196798db342ce8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>SatInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>acbd4413b1370baca9c45aecb0cb8ebd2</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>KnapsackInterface</name>
      <anchorfile>classoperations__research_1_1MPSolver.html</anchorfile>
      <anchor>aee1ddf25e86286c16face31551751bda</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPSolverCommonParameters</name>
    <filename>classoperations__research_1_1MPSolverCommonParameters.html</filename>
    <member kind="typedef">
      <type>MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>LPAlgorithmValues</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aa6313a43e806bab53656593a9ae435de</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSolverCommonParameters</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a7029587a338759d9ef1de80b7d9eb028</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPSolverCommonParameters</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a7588f2e39fe3c0965fddf258b044c30e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSolverCommonParameters</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>adddf97d95dd001a5822e1e903452314d</anchor>
      <arglist>(const MPSolverCommonParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSolverCommonParameters</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a7dfb3ed9e85240be0f28d8ee6119c631</anchor>
      <arglist>(MPSolverCommonParameters &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPSolverCommonParameters &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>af84f57cffdf5b072009d138b985fed4a</anchor>
      <arglist>(const MPSolverCommonParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPSolverCommonParameters &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>abea7d3aff5f78fc4eb898376c1f4817c</anchor>
      <arglist>(MPSolverCommonParameters &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a0cc7e44a0b6d195e922182a2cc33e150</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a9dd30a842ddfe37c1d2dd62aaf9c33fd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a0b2027280bb07408f1deb2c0031dde0f</anchor>
      <arglist>(MPSolverCommonParameters *other)</arglist>
    </member>
    <member kind="function">
      <type>MPSolverCommonParameters *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a2cf23de93062aec7b11e0793545a03e0</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPSolverCommonParameters *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a3b01bf7a42e9a5ca34c76aff47c33bdf</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a6c0a9ae90e743a2b57e3928c67757828</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ad3c5810dbf626dbbca40397c5e39d7a2</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a8d562ea03ef8fe1bc9256be00f220542</anchor>
      <arglist>(const MPSolverCommonParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a9bd2064cbf832181ed24ed007c769adc</anchor>
      <arglist>(const MPSolverCommonParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aed0359cec171eabf7abbade42dca41a3</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a52fd88906392a5fe4beba6420e5e71ea</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a2bcacdf9638a127992102e232ec6566e</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a20cc887e783658243973551831da7933</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>abfe30efa928731feb5b57ae0f4c8e3ee</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aed4287682e3ae3514be974adb805a60b</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a1ce2dd3b914799b0f7615ac19343e16b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a53398a85356202f49851c7ccfe16105a</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_relative_mip_gap</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ab43c065bc88472c640d3390c09cd5734</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_relative_mip_gap</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a73d75d67f9fc347719d63c1215987ff2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::OptionalDouble &amp;</type>
      <name>relative_mip_gap</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>acc1c456c8915a916726cb63e970621af</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>release_relative_mip_gap</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aa9ec4345cde03d03b3c08838ec30369a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>mutable_relative_mip_gap</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ad38f5b3e0e8683ff10f03f3a500bca32</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_relative_mip_gap</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a7dd8fd0089cd7d1daf30d410efde5714</anchor>
      <arglist>(::operations_research::OptionalDouble *relative_mip_gap)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_primal_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a77b134a21077007194139b99a9459728</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_primal_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a66a4b7cb8b837910751805c601c78443</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::OptionalDouble &amp;</type>
      <name>primal_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a36127b223027fc058bbf971f0307efae</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>release_primal_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>adb3df3d7b09a472dfa5742229233da26</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>mutable_primal_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a7c2d5e8cde1db9b23187a0fe7fcd4d06</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_primal_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ab1be2b91e2bbe5ee267ee024c4ea9bde</anchor>
      <arglist>(::operations_research::OptionalDouble *primal_tolerance)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_dual_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aba8f0504a8ca0687acafba19891f9fe8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_dual_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aef0f515d0768ea3b47384acd15552721</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::OptionalDouble &amp;</type>
      <name>dual_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aec11862fc49c8ce98432ede53d544c6f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>release_dual_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a279dd3901076daeafbe460fa6936c0d0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalDouble *</type>
      <name>mutable_dual_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ac4053ee2251560ec5cc0fe3deb71fa0d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_dual_tolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a7afc7287898f240e6bb722fbb7ae3d89</anchor>
      <arglist>(::operations_research::OptionalDouble *dual_tolerance)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_lp_algorithm</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ae876c5cd03af14f0908c5ab96b559b9a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_lp_algorithm</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ae16a10ce4e7c3b4b2faa89721960ca4d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>lp_algorithm</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a2c96e4635c2c168f214138eb34d8a3ba</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_lp_algorithm</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a71f6fb311e3d7f04309728fc6d1ba825</anchor>
      <arglist>(::operations_research::MPSolverCommonParameters_LPAlgorithmValues value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_presolve</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ab4cd892db8d5e15f13f57505534133e8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_presolve</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a759a0852aeab4f63ce3ff62ef1f2fff2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalBoolean</type>
      <name>presolve</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a26669e8b4b16b9181bc3b674d8112562</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a0b2e7880d0da6c38a3d3efbc79715439</anchor>
      <arglist>(::operations_research::OptionalBoolean value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_scaling</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ad34b9cdbadb5f32e35a7a4bdd8a226dd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_scaling</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a8c9a43f439b75d4f31e080b31e959d89</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::OptionalBoolean</type>
      <name>scaling</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a8583cdaae51f66fdfbc2f06a4b2ce56a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_scaling</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>adc35e25af2214b8abf74bd57f6344499</anchor>
      <arglist>(::operations_research::OptionalBoolean value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a7fe3425b656ff748d2a54b417683ae1e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a4eb31f0916c4bc2c10aa123f382121d3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a2906af194f9b72a0add2525fcfd737b0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPSolverCommonParameters &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a3a7b9bec5564a208b72fc5246112c165</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a344b175f28e0075bc7c80716c2b1c3e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPSolverCommonParameters *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>add3ad6e9230fcfae52d6f1807bd7b488</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>LPAlgorithmValues_IsValid</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a0137f210096fe7f000d841102036c4cf</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>LPAlgorithmValues_descriptor</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a8e5bd01cce14e19384dbb5932225ce7a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>LPAlgorithmValues_Name</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aab8554e67a204a2407bcae1bd4552c51</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>LPAlgorithmValues_Parse</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a26ec7059979185b161efe1b02bc245f4</anchor>
      <arglist>(const std::string &amp;name, LPAlgorithmValues *value)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a8834cc1a6ae262a7cdb1e9b8ebe3d5d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr LPAlgorithmValues</type>
      <name>LP_ALGO_UNSPECIFIED</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a451e8cae76b11602cfcd00ab705ccda8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr LPAlgorithmValues</type>
      <name>LP_ALGO_DUAL</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a0aa62e04f861e6e60c044b41a936a2ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr LPAlgorithmValues</type>
      <name>LP_ALGO_PRIMAL</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a90e92782dea2a7e3d3ef695468e47ade</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr LPAlgorithmValues</type>
      <name>LP_ALGO_BARRIER</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aa76efeebfa128c524c90366194d7eeb8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr LPAlgorithmValues</type>
      <name>LPAlgorithmValues_MIN</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ae87f3855f26b50a795cc8cdb8cbd4c06</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr LPAlgorithmValues</type>
      <name>LPAlgorithmValues_MAX</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a40277564b0313e56adc6e4f4a48bce85</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>LPAlgorithmValues_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a2a1f085384fb024681c611ca1b2328d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRelativeMipGapFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a8d28e029518e99c4077aff290f7183b4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPrimalToleranceFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>ad503f1cdc67cc527b0b33764bd369b7c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDualToleranceFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>adfdb8a87c8c7cdf67330f779346c807d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLpAlgorithmFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>aa89c71529d1f6f59ec9294db19423a02</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPresolveFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a3a4ebb83702a3a0eb2f10b10445c37ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kScalingFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a9c817f666dcf53cb2a2f362202afa2d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPSolverCommonParameters.html</anchorfile>
      <anchor>a3ef7437ece6efc6c3d73ea07fbef4855</anchor>
      <arglist>(MPSolverCommonParameters &amp;a, MPSolverCommonParameters &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPSolverInterface</name>
    <filename>classoperations__research_1_1MPSolverInterface.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>SynchronizationStatus</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a912394f09fe0aee694df2e9c962853a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MUST_RELOAD</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a912394f09fe0aee694df2e9c962853a0a5e5dccb6be46f13fd046ebf8dd63fc0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_SYNCHRONIZED</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a912394f09fe0aee694df2e9c962853a0abb904e2bd71f6c8f7612f5bb41a0b8f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SOLUTION_SYNCHRONIZED</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a912394f09fe0aee694df2e9c962853a0a1a93fdf313f6e448af723eb80bbbb7ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MUST_RELOAD</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a912394f09fe0aee694df2e9c962853a0a5e5dccb6be46f13fd046ebf8dd63fc0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_SYNCHRONIZED</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a912394f09fe0aee694df2e9c962853a0abb904e2bd71f6c8f7612f5bb41a0b8f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SOLUTION_SYNCHRONIZED</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a912394f09fe0aee694df2e9c962853a0a1a93fdf313f6e448af723eb80bbbb7ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSolverInterface</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a152b3f16428c0a1c58247ba88d95f0a4</anchor>
      <arglist>(MPSolver *const solver)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPSolverInterface</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>acd4eda4c38c00ed2cf7908c29bb74de3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual MPSolver::ResultStatus</type>
      <name>Solve</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a99d3df0f2e02937e56473d9f3df68965</anchor>
      <arglist>(const MPSolverParameters &amp;param)=0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>Write</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a075910a059a214af934af08f3cba7db4</anchor>
      <arglist>(const std::string &amp;filename)</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>Reset</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ad4ce4ee159f4c2db5a02923c886c136b</anchor>
      <arglist>()=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>SetOptimizationDirection</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ac35b40e98fabb8bcd5e62cdd57678ff3</anchor>
      <arglist>(bool maximize)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>SetVariableBounds</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>af80b8873e1b07fedcac24c5704a889cf</anchor>
      <arglist>(int index, double lb, double ub)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>SetVariableInteger</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a5e445125231a8febc619d3a81cb8c12f</anchor>
      <arglist>(int index, bool integer)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>SetConstraintBounds</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ae63369de591dfd558df4c74c1143d84a</anchor>
      <arglist>(int index, double lb, double ub)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>AddRowConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a4c06cfd4d3c34ca0a96ad90f50a1abd8</anchor>
      <arglist>(MPConstraint *const ct)=0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual bool</type>
      <name>AddIndicatorConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>aa38ec74a9f9beb650fbb88dece755ebd</anchor>
      <arglist>(MPConstraint *const ct)</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>AddVariable</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a94980bb46c791363fe9ce55d57e2a79a</anchor>
      <arglist>(MPVariable *const var)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>SetCoefficient</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a3ecddb0dba419349214fd4b021d2b9f6</anchor>
      <arglist>(MPConstraint *const constraint, const MPVariable *const variable, double new_value, double old_value)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>ClearConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a38ad5df166aef299c76fac71eb785523</anchor>
      <arglist>(MPConstraint *const constraint)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>SetObjectiveCoefficient</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ab54c763dfb19f258603711eddfe66bca</anchor>
      <arglist>(const MPVariable *const variable, double coefficient)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>SetObjectiveOffset</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a6a0b90433ed94679a6065000b384fb64</anchor>
      <arglist>(double value)=0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void</type>
      <name>ClearObjective</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a1d0c4924c375cd234f4a17bf2b8c1a8b</anchor>
      <arglist>()=0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>BranchingPriorityChangedForVariable</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a3038e118fcb2ccc9f0e2493cc48a1545</anchor>
      <arglist>(int var_index)</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual int64</type>
      <name>iterations</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>aa7ef7e2f076fc9208d13fdfb4ec33c76</anchor>
      <arglist>() const =0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual int64</type>
      <name>nodes</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a6ca427f3c6266f86e41f4b5b1905c7d3</anchor>
      <arglist>() const =0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual double</type>
      <name>best_objective_bound</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a1e4b520fcd965a7a2b975035e1a906b5</anchor>
      <arglist>() const =0</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>trivial_worst_objective_bound</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>aec492297c32397c8a82490efb952e137</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>objective_value</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a9a71292f92599d3f96dc96543c24673d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual MPSolver::BasisStatus</type>
      <name>row_status</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>aad9285c25e1671edfac7a7748b166770</anchor>
      <arglist>(int constraint_index) const =0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual MPSolver::BasisStatus</type>
      <name>column_status</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a5a2299c6d83d21a0917d0661fb704dbe</anchor>
      <arglist>(int variable_index) const =0</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckSolutionIsSynchronized</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a0fabe855ed7766a6ccddb63cbbfa3bc4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual bool</type>
      <name>CheckSolutionExists</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a8f0df932d3bffc929a8b463fe9697431</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CheckSolutionIsSynchronizedAndExists</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ad792f0af5ffa1072f0d6a4d3d872f73a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual bool</type>
      <name>CheckBestObjectiveBoundExists</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a80e389070e3984f5333c4fd96311231b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual bool</type>
      <name>IsContinuous</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a77db27f245bbe7fec03763cb0f81210a</anchor>
      <arglist>() const =0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual bool</type>
      <name>IsLP</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a8410c2213e0f2faec9a202b5b670ad6c</anchor>
      <arglist>() const =0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual bool</type>
      <name>IsMIP</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a3cb924945c7c8ebd6964ba982ffdf276</anchor>
      <arglist>() const =0</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>last_variable_index</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a4996e4f9b120ab70f4d2382e473dc852</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>variable_is_extracted</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a8245f5863b87aa3f6e9ac19d29531a9a</anchor>
      <arglist>(int var_index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_variable_as_extracted</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a01fb82c6f72457879c0defb22ae9db47</anchor>
      <arglist>(int var_index, bool extracted)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>constraint_is_extracted</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a90947e6cbecd6bfff60feefa9ba37847</anchor>
      <arglist>(int ct_index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_constraint_as_extracted</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a3c7ae564168e67c161f5111a060a604e</anchor>
      <arglist>(int ct_index, bool extracted)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>quiet</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a86ce7eb3c7e1f5ca9822590f489ab708</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_quiet</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>adc4579b45ae778e4c61b5c35cffc0eaf</anchor>
      <arglist>(bool quiet_value)</arglist>
    </member>
    <member kind="function">
      <type>MPSolver::ResultStatus</type>
      <name>result_status</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a0e34bb4ed1518f8104e6f84592e68dc5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual std::string</type>
      <name>SolverVersion</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a5e19196bab184ddaf66a7e34056cab48</anchor>
      <arglist>() const =0</arglist>
    </member>
    <member kind="function" virtualness="pure">
      <type>virtual void *</type>
      <name>underlying_solver</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ad98f058f6721dba0ddeb4f82f7bc98ef</anchor>
      <arglist>()=0</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual double</type>
      <name>ComputeExactConditionNumber</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>afae6a55da4ebeb2d9c6eb55eeccbdc8b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual void</type>
      <name>SetStartingLpBasis</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ae60b6e41d8b3077d982e0347c85b802c</anchor>
      <arglist>(const std::vector&lt; MPSolver::BasisStatus &gt; &amp;variable_statuses, const std::vector&lt; MPSolver::BasisStatus &gt; &amp;constraint_statuses)</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual bool</type>
      <name>InterruptSolve</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a5f9dc671c62b54a3940f2691f9953e67</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual bool</type>
      <name>NextSolution</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a7f6e6fc6c25a04f49e3b95b7510c8369</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int64</type>
      <name>kUnknownNumberOfIterations</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a156255f5e27fd48b7ee43539b52f644f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int64</type>
      <name>kUnknownNumberOfNodes</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a25b846bdb22eb75b6c0f3b9922556c46</anchor>
      <arglist></arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ExtractModel</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a9ae99b901839c7f1e29193d8a619b728</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>ExtractNewVariables</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a945992529aaaa827d7c9307e996ab2ef</anchor>
      <arglist>()=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>ExtractNewConstraints</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a1363e1a1c4a1535b0cb92f46a9efd4b5</anchor>
      <arglist>()=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>ExtractObjective</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a24085346a573695d2f3d224b726c5714</anchor>
      <arglist>()=0</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>ResetExtractionInformation</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a3fd73560f782eaf5093a435c61783d09</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>InvalidateSolutionSynchronization</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a27ef26272b0444403e683b31605d88f7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>SetCommonParameters</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ad237855fa79fc36fbf9baedce589384c</anchor>
      <arglist>(const MPSolverParameters &amp;param)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>SetMIPParameters</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ab7289f57fd0f768453f2817d59a20c80</anchor>
      <arglist>(const MPSolverParameters &amp;param)</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>SetParameters</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a6c1c1fc340f3f15e75745d7e6b0fe3df</anchor>
      <arglist>(const MPSolverParameters &amp;param)=0</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>SetUnsupportedDoubleParam</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a861fc44ad0a6147a79dbcef606d34958</anchor>
      <arglist>(MPSolverParameters::DoubleParam param)</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="virtual">
      <type>virtual void</type>
      <name>SetUnsupportedIntegerParam</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a2b7834a656a48c094c34c13c60bd742c</anchor>
      <arglist>(MPSolverParameters::IntegerParam param)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>SetDoubleParamToUnsupportedValue</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>aa5b34d05b6fbeb881404d4147b63d401</anchor>
      <arglist>(MPSolverParameters::DoubleParam param, double value)</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="virtual">
      <type>virtual void</type>
      <name>SetIntegerParamToUnsupportedValue</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a0e21cafe5af05431fa67ec7e9bfbf5ec</anchor>
      <arglist>(MPSolverParameters::IntegerParam param, int value)</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>SetRelativeMipGap</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a2b6fcec92ec397f38a9657d7d38a9e49</anchor>
      <arglist>(double value)=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>SetPrimalTolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a119e0274303ed2eb0c80fcef960cfff0</anchor>
      <arglist>(double value)=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>SetDualTolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>abc5c8df36220fb85bf2c4f69b959ead9</anchor>
      <arglist>(double value)=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>SetPresolveMode</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a51ae167e111959ecbe3f67e0e12e2f09</anchor>
      <arglist>(int value)=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="virtual">
      <type>virtual util::Status</type>
      <name>SetNumThreads</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ae6b5032ab0257e33006c4dee5249ad21</anchor>
      <arglist>(int num_threads)</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="virtual">
      <type>virtual bool</type>
      <name>SetSolverSpecificParametersAsString</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a9421cf0896b24513fb63283fac87de44</anchor>
      <arglist>(const std::string &amp;parameters)</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="virtual">
      <type>virtual bool</type>
      <name>ReadParameterFile</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>af3bee59eacd9c98d6f9240167cf3051f</anchor>
      <arglist>(const std::string &amp;filename)</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="virtual">
      <type>virtual std::string</type>
      <name>ValidFileExtensionForParameterFile</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a5220692ca2d4674d77045a27317bca3e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>SetScalingMode</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>aaaa84e64f7c29af15440e02189a362da</anchor>
      <arglist>(int value)=0</arglist>
    </member>
    <member kind="function" protection="protected" virtualness="pure">
      <type>virtual void</type>
      <name>SetLpAlgorithm</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a7cde757e7152aa06930afc59709c3c64</anchor>
      <arglist>(int value)=0</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>MPSolver *const</type>
      <name>solver_</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ae27c5da090750971d680f2bf8f4f706b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>SynchronizationStatus</type>
      <name>sync_status_</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>af7655248b40e336f843180b9ef37dc3c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>MPSolver::ResultStatus</type>
      <name>result_status_</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>add7194b3e70938a6bb7fb8e7c6532e56</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>maximize_</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>aaabe8c85cd90dd61e4c9de70667b0c16</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>int</type>
      <name>last_constraint_index_</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a577812bda95732730419025af875deb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>int</type>
      <name>last_variable_index_</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a45aa65ea825c85f885b0d8a0064af2f6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>double</type>
      <name>objective_value_</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a9fbca54e7d25c2bfacfac1e47183e14f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>bool</type>
      <name>quiet_</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ac962e5cc7308644424829668241a8fcf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" protection="protected" static="yes">
      <type>static const int</type>
      <name>kDummyVariableIndex</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a693c6f125a85b70fe94057be1cfd0819</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>MPSolver</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>ac2c01b4de8f7670e37daa7d42b804dd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>MPConstraint</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a24102af97b3c7e803861e1d6983b1fea</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>MPObjective</name>
      <anchorfile>classoperations__research_1_1MPSolverInterface.html</anchorfile>
      <anchor>a77dbe3a653f9c5d30e818000d92d8b17</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPSolverParameters</name>
    <filename>classoperations__research_1_1MPSolverParameters.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>DoubleParam</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3834b74a8764c1c971e10ce9ba2f265e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>RELATIVE_MIP_GAP</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3834b74a8764c1c971e10ce9ba2f265eaca78b129ceb19e286889b3274bb8cab3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRIMAL_TOLERANCE</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3834b74a8764c1c971e10ce9ba2f265ea57e959f9c5ede1f3e683169609fb0488</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DUAL_TOLERANCE</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3834b74a8764c1c971e10ce9ba2f265ea313eaed42edf309599e71ca7563cce22</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>IntegerParam</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRESOLVE</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0a1f92ab900139fd7649eace59942f1e0f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LP_ALGORITHM</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0a28abd1bbac3e861d5a74f841f8263e05</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INCREMENTALITY</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0a0ea983a7a805bc98c2cf9a1000c1ef95</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SCALING</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0ad5871c18d62c5e2e1e48a5420c13942f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>PresolveValues</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a45d822ac67d10cc80b2f70fe6ec555a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRESOLVE_OFF</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a45d822ac67d10cc80b2f70fe6ec555a4a6e658228fc28195fb00675d3dfec2c85</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRESOLVE_ON</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a45d822ac67d10cc80b2f70fe6ec555a4ac8e04ac3295af6c505a22757490bb018</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>LpAlgorithmValues</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab297405e6084c08a9058012361376762</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DUAL</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab297405e6084c08a9058012361376762a2aca0e0bfd9807b5a938bb3bbc2bea07</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRIMAL</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab297405e6084c08a9058012361376762a9fa228e5a51b8e22346b46d0f866e68d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BARRIER</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab297405e6084c08a9058012361376762af55a23df7a5ec299bb9ca742ef9004ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>IncrementalityValues</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad58cb8cf98c288843ab0d1fd644f6116</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INCREMENTALITY_OFF</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad58cb8cf98c288843ab0d1fd644f6116a27013e436bfe1d34ebb6da476c590eee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INCREMENTALITY_ON</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad58cb8cf98c288843ab0d1fd644f6116aef000bbd88ef3ff2715d53a0e1dfbd37</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>ScalingValues</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ae77fef975a893d6118d955ef2eb72ac9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SCALING_OFF</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ae77fef975a893d6118d955ef2eb72ac9a41abc3f52f70490cd997aa8debace24b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SCALING_ON</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ae77fef975a893d6118d955ef2eb72ac9a33cf90a88928c32c5b56c9375d7dfd18</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>RELATIVE_MIP_GAP</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3834b74a8764c1c971e10ce9ba2f265eaca78b129ceb19e286889b3274bb8cab3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRIMAL_TOLERANCE</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3834b74a8764c1c971e10ce9ba2f265ea57e959f9c5ede1f3e683169609fb0488</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DUAL_TOLERANCE</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3834b74a8764c1c971e10ce9ba2f265ea313eaed42edf309599e71ca7563cce22</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRESOLVE</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0a1f92ab900139fd7649eace59942f1e0f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>LP_ALGORITHM</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0a28abd1bbac3e861d5a74f841f8263e05</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INCREMENTALITY</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0a0ea983a7a805bc98c2cf9a1000c1ef95</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SCALING</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8f18a88f586c725cfffc613499926a0ad5871c18d62c5e2e1e48a5420c13942f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRESOLVE_OFF</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a45d822ac67d10cc80b2f70fe6ec555a4a6e658228fc28195fb00675d3dfec2c85</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRESOLVE_ON</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a45d822ac67d10cc80b2f70fe6ec555a4ac8e04ac3295af6c505a22757490bb018</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DUAL</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab297405e6084c08a9058012361376762a2aca0e0bfd9807b5a938bb3bbc2bea07</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>PRIMAL</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab297405e6084c08a9058012361376762a9fa228e5a51b8e22346b46d0f866e68d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>BARRIER</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab297405e6084c08a9058012361376762af55a23df7a5ec299bb9ca742ef9004ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INCREMENTALITY_OFF</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad58cb8cf98c288843ab0d1fd644f6116a27013e436bfe1d34ebb6da476c590eee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INCREMENTALITY_ON</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad58cb8cf98c288843ab0d1fd644f6116aef000bbd88ef3ff2715d53a0e1dfbd37</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SCALING_OFF</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ae77fef975a893d6118d955ef2eb72ac9a41abc3f52f70490cd997aa8debace24b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SCALING_ON</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ae77fef975a893d6118d955ef2eb72ac9a33cf90a88928c32c5b56c9375d7dfd18</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSolverParameters</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>accb1cea31a73bd0b09bb75882baa9e5f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetDoubleParam</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ab297ed394e3996ebb0cc43d931deaa16</anchor>
      <arglist>(MPSolverParameters::DoubleParam param, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetIntegerParam</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>afcfb05b6d356807556f65f1b845897a8</anchor>
      <arglist>(MPSolverParameters::IntegerParam param, int value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ResetDoubleParam</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a7bc16e5febd0604a3400572c0e35c490</anchor>
      <arglist>(MPSolverParameters::DoubleParam param)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ResetIntegerParam</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a100330795edeb8c1b12c7b8ff74611df</anchor>
      <arglist>(MPSolverParameters::IntegerParam param)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Reset</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ada3cdf5254959f2807c3d27bb7e18e27</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>GetDoubleParam</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a4423dd8b21b413522d8b91e4536cfa08</anchor>
      <arglist>(MPSolverParameters::DoubleParam param) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetIntegerParam</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ae9e67424e09a35840b4fda2942b94c6f</anchor>
      <arglist>(MPSolverParameters::IntegerParam param) const</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kDefaultDoubleParamValue</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>af0e4fbb80499756bc3edf88fd495d7f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDefaultIntegerParamValue</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a9bac90a88459ac4dd9629beeada24a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kUnknownDoubleParamValue</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a561c73d056eef04db4b4703abd972868</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUnknownIntegerParamValue</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a6c81985cecabbd5f054974d362db551a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kDefaultRelativeMipGap</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad6b31b12f9e1944b355809efd56fec98</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kDefaultPrimalTolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3369c0b857cf02ddf742acc5ef1feec5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kDefaultDualTolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a15fd57d45c522be9c8340ba9d1244e3f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const PresolveValues</type>
      <name>kDefaultPresolve</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>afc36c21bad2607b8d7bcba63b60d8681</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const IncrementalityValues</type>
      <name>kDefaultIncrementality</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8d266a839e50968c11481a151f80ea9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kDefaultDoubleParamValue</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>af0e4fbb80499756bc3edf88fd495d7f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDefaultIntegerParamValue</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a9bac90a88459ac4dd9629beeada24a66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kUnknownDoubleParamValue</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a561c73d056eef04db4b4703abd972868</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUnknownIntegerParamValue</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a6c81985cecabbd5f054974d362db551a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kDefaultRelativeMipGap</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ad6b31b12f9e1944b355809efd56fec98</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kDefaultPrimalTolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a3369c0b857cf02ddf742acc5ef1feec5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const double</type>
      <name>kDefaultDualTolerance</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>a15fd57d45c522be9c8340ba9d1244e3f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const PresolveValues</type>
      <name>kDefaultPresolve</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>afc36c21bad2607b8d7bcba63b60d8681</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const IncrementalityValues</type>
      <name>kDefaultIncrementality</name>
      <anchorfile>classoperations__research_1_1MPSolverParameters.html</anchorfile>
      <anchor>ac8d266a839e50968c11481a151f80ea9</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPSosConstraint</name>
    <filename>classoperations__research_1_1MPSosConstraint.html</filename>
    <member kind="typedef">
      <type>MPSosConstraint_Type</type>
      <name>Type</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a2a0e1001b3343face886a920fac92833</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSosConstraint</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>afb146038bc0ae4a82443bedf6a41e7d4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPSosConstraint</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>aee50c5d8956f70f535554a14ede3f656</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSosConstraint</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ac68098c6e05517770d3c66006ac053b4</anchor>
      <arglist>(const MPSosConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPSosConstraint</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a7e57cd207c82924a4159b352a493f556</anchor>
      <arglist>(MPSosConstraint &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPSosConstraint &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a2fc979854067dd31752e3eaf93b66ebd</anchor>
      <arglist>(const MPSosConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPSosConstraint &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a799b78f85762ecbd51a3345b3a7cac93</anchor>
      <arglist>(MPSosConstraint &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a48b3d6a1c08cd21402f032f95d6c0e4f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a1fbfb7554d749efcaa370c26a2c91aad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>aeec1fb5f3cf356ad86164078f2b44841</anchor>
      <arglist>(MPSosConstraint *other)</arglist>
    </member>
    <member kind="function">
      <type>MPSosConstraint *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a1b7f11f4c053a2289e49d4d2c4c3af12</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPSosConstraint *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>acc64a83699f56ba1e99d00da5190cb25</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a13952a17e8cf3a867abfb73943ef13fd</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a2eac4fd3aa6fd5bbb858a7d3ac6b76ee</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a684227d3926d81765dca850da5fc819a</anchor>
      <arglist>(const MPSosConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a8595714daf43d02e429a6a6cbbcedfa7</anchor>
      <arglist>(const MPSosConstraint &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a9e2e0e6ae9e3ecabb6ce86e4c28f4566</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a4c0265bebd26eca3e86963f001f57e98</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ae591d9757e75b01442a7e62ec20d601c</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a50a915df6d7fee9267fcd3a7747fe49c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a72bb2fe50efc8606bd474a10b1a70626</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a23045597f1902f84da5d5235dc643018</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a230cb7befbbc7ff449ea7a5a3802f517</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a9c49cb4448a2a0fd0cbb933f09f9591c</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>var_index_size</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ad2ea96b4127eb2944ab2d652491baebd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_var_index</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a4b9a394dd1e97f994cfe1a7379fc2029</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ad4ba491d6656aef2b6ce14d14d99cf01</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_index</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>aab4e3dedb74a0c29d3df022ba9c84268</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_var_index</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a5576bb20bca29ed4eda43e2b4b9247f1</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a03de6d5ff4a7ffb033478f00c71db48f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_var_index</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ae6c69bd4e46582995ce3c7a9c036658b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>weight_size</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a7665f86b3ed723269cac94bf14d01ad0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_weight</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a4a1a33e78aef5b0ff2bf99d7a16e3478</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>weight</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a235a66740de57251cfaa3387d132330f</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_weight</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a63d36464e6a33b178dc315a66b4a9ad9</anchor>
      <arglist>(int index, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_weight</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a3e1617716a9eb5ea110ab0f090397efc</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; &amp;</type>
      <name>weight</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a2e7159181e0b3423d7f19164bce3b876</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; *</type>
      <name>mutable_weight</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a637f8487af7e3f77625713aad7bc4494</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_type</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a1bd1320f7685edd8c5395aa663341c16</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_type</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a42140495e297ef560c2c27253ca10322</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::MPSosConstraint_Type</type>
      <name>type</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a2df361194caf5e4463fbbed84c656fc7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_type</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ab2548485ba08eaa25e2e24199fef91f4</anchor>
      <arglist>(::operations_research::MPSosConstraint_Type value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>af0a9ac6f60f07d13d5bef20ca2b49201</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>aadddf22f38ae842d1365ccc7243db640</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a709b787bea1ada39bcf3eb870de35f7e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPSosConstraint &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a71d5911e13b9bc3867c9ca58dc4d6a59</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a4a4ade79d6848b71ae8e0783ebc0111d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPSosConstraint *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a7a1cacad2e4de04abc4d4bb410d87624</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>Type_IsValid</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>af670b6bfdad221487337ebb353036b75</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>Type_descriptor</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a9aaafc6271ea2df4af1a65cbb8506bcf</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const std::string &amp;</type>
      <name>Type_Name</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ae5f7840e329c8c7286fae9d12755535b</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>Type_Parse</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a0e98d0f7d465cc70fab65f72a3c5bb9f</anchor>
      <arglist>(const std::string &amp;name, Type *value)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ad713beb692d411139f0992a9063eb6ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Type</type>
      <name>SOS1_DEFAULT</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>aa3dab72e0e2b95c0b6e47b73bcc69199</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Type</type>
      <name>SOS2</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ad274af8f614ee6beca8911362bb25c2f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Type</type>
      <name>Type_MIN</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a81c3692f56ba1a2c830fd7cf47d11146</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr Type</type>
      <name>Type_MAX</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a6200a36b5cb91d4e21762c20145086d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>Type_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a7c4a30c7b67b03f1c2f20f4d77a40b8d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarIndexFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a1dbfad5972ab01953869c63701d6ce24</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kWeightFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>ac568532c841abb5799bbba776d3a7692</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTypeFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>abb2aeac1446916d48a40886c558c705b</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPSosConstraint.html</anchorfile>
      <anchor>a5dbc5d574b38cde070463680d87e9cdb</anchor>
      <arglist>(MPSosConstraint &amp;a, MPSosConstraint &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPVariable</name>
    <filename>classoperations__research_1_1MPVariable.html</filename>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a1d74db2a526fd2d8b4e11a4ed4ebf07d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetInteger</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a02131c51847247fc3f620e4ccb439470</anchor>
      <arglist>(bool integer)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>integer</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a044841d06c6de76db6f4978bf57898ec</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>solution_value</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a9c7aabb1c218e733e719ce2966939586</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>index</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>ac9676264a15bac7aae8db95344903b1f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>lb</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a0d581a8129e4fc9779ee0b1172967563</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>ub</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a1eeda5bea30b1e1f354c2919110e2ff9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetLB</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a0d23a28f31a3b667f21034551666430e</anchor>
      <arglist>(double lb)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetUB</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a42590823a1b3343d0459cc7c564b9d69</anchor>
      <arglist>(double ub)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetBounds</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a5608a23aad7a48ea52ba3869b557d036</anchor>
      <arglist>(double lb, double ub)</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>unrounded_solution_value</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>af25c3940fd37705e63340d9269896dc0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a0001ce8a3240bd00b49557594a809cdb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>MPSolver::BasisStatus</type>
      <name>basis_status</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>abb491e237ace37cb5240cde3c3ff2958</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>branching_priority</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a6d0e4798441493244b31f29ee5ac26ad</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetBranchingPriority</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a074f5b3c88658b91bd372a372db63627</anchor>
      <arglist>(int priority)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>MPVariable</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>abba28daa7e558932c40cf1a8ecb30194</anchor>
      <arglist>(int index, double lb, double ub, bool integer, const std::string &amp;name, MPSolverInterface *const interface_in)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>set_solution_value</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a8f6294a83ae2d4bb0e6000981f8651a5</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function" protection="protected">
      <type>void</type>
      <name>set_reduced_cost</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a73eaa62d668195e9d894548d84f75fdb</anchor>
      <arglist>(double reduced_cost)</arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>MPSolver</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>ac2c01b4de8f7670e37daa7d42b804dd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>MPSolverInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>ac0aea0786e75adbb2d24c41c15e7456c</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>CBCInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>af5a7cf0c655f37c0b388a2ddcf32ac3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>CLPInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a60944ecdcad88cfb4d4d32feea70c9b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>GLPKInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>ae1a3e0a695903c8e6effd524a7f92784</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>SCIPInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a236f9752f4df4c5134617330a040ec8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>SLMInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a5c083b37243075a00bf909840dc7c933</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>GurobiInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>ac28a56eeedb62d070578a9231f1875ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>CplexInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>ae7cbd08108e1636184f28c1a71c42393</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>GLOPInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a6c754b527a347994b06eeb49a09ac222</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>MPVariableSolutionValueTest</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a8844020cc1376123531cd53c831acdef</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>BopInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>a7383308e6b9b63b18196798db342ce8a</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>SatInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>acbd4413b1370baca9c45aecb0cb8ebd2</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>KnapsackInterface</name>
      <anchorfile>classoperations__research_1_1MPVariable.html</anchorfile>
      <anchor>aee1ddf25e86286c16face31551751bda</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::MPVariableProto</name>
    <filename>classoperations__research_1_1MPVariableProto.html</filename>
    <member kind="function">
      <type></type>
      <name>MPVariableProto</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a62cf48331dc44923f7c0cc96fae028c2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~MPVariableProto</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ab4c175309211bf770111fd077b13b4f3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPVariableProto</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a2624c6f6a7184dc13aeaf57670a3ba85</anchor>
      <arglist>(const MPVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>MPVariableProto</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a2228aa035c4679076d612ad7a7367a04</anchor>
      <arglist>(MPVariableProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>MPVariableProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>abcbe8216b089c8c7e64e8222625232e3</anchor>
      <arglist>(const MPVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>MPVariableProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aea708da5988649175a66ce565380c369</anchor>
      <arglist>(MPVariableProto &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ae07a0339e2463e1d5ec3891a51c8cdeb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a961a32fc471dbf46aad8f819d540fb77</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a6fbbe71940eb75fa29fc189d15db4cb6</anchor>
      <arglist>(MPVariableProto *other)</arglist>
    </member>
    <member kind="function">
      <type>MPVariableProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a85181da0e2f19f985232a2139bdd4c5e</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>MPVariableProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a1c3425e747a8163b9953d646d080d80f</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a0ffdc54c1073f2e7d20269fb5c6ab074</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a65a3581b22383b59bcb947fdb50fa8ac</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a14e8b27a18065aba5e5bd211e11aa9f6</anchor>
      <arglist>(const MPVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a1fe6eb4b3c4c7c2af434f04925c55356</anchor>
      <arglist>(const MPVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a89fd1748e6452956b64edc8f376cdc45</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aa2f4322f0750640e2e4931f82cce4556</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ae822f2450d8b94912e8b8cd3f5aa98a9</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ada780fdedbed0037b473879f1a07dcd3</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a2bcfb3cc88341f6e751b859b89342e62</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>acdcc296bcb79e339341f609aa7918418</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a91cd03e19b998dc79c8693cad5a510b3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a2130a5e7f9a63b6802b3d1dad012b8ee</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aa7ee7256ce99a9a3c67ea707954b3de3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aa1f8ef8a66e3febe4322ba5487a0db94</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>af6750e75d02b64614ac6cb6424f7b9a0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>addc50daf7db9041c0f58d04cca6cba9b</anchor>
      <arglist>(const std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a16eeb9b0988dc882fd51896da234e28a</anchor>
      <arglist>(std::string &amp;&amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a1c53e1c2a5401f24dd58a90803327711</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a7faf5c9ea2ad411be9b50d92ff26de4a</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ab1261573cff3c47b234a300d34f6452b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a1d607740d3050978d04f31d01296c6b7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a43fda9c3abb29a9a493a7a806b53abb0</anchor>
      <arglist>(std::string *name)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_objective_coefficient</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ad339f6e4dcec1201c4ee91aa1b79fdc3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_objective_coefficient</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aaee6b80ba43b4d5bf7e72cf18f4273f5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>objective_coefficient</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aeef9421fbd1ae56ed0f9cdde3e29732a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_objective_coefficient</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a531854c1172ab6e07ac265718a6f33ec</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_is_integer</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a91affe257e981ebc4454f97899f879ab</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_is_integer</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ad56f145747bfab5ce78e1e5199d195e1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>is_integer</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a8856bc82282f6dbf0bf2539781d4d7ac</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_is_integer</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a40e71bc57f5973531a5768245dc26c3a</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_branching_priority</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aa0a0ecd3ee1ec9c0b997359205dde136</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_branching_priority</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>adade5eb436788cce169656745d5e6733</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>branching_priority</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a9cfaa26d92675300703d8d9621396e18</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_branching_priority</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a3f814beb2c9d25f27e30fff16e104598</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_lower_bound</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a5d69b8518570db9b5c5f7cedaf5b3809</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_lower_bound</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a8fd1b7352faf069ecf048e03824f355b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>lower_bound</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a5ba61bba5dcc8a6decbce2780b775b8f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_lower_bound</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ab3ab7f51a56e972b5cacf8c6496ffe5f</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_upper_bound</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a1746892dd67db5804e2ed67d90c882c4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_upper_bound</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a378ce6efc84ed541ac45c23a6bad24b9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>upper_bound</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aa0d0da3b7d777ff3d0f6368e33db3397</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_upper_bound</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a1481927c09d8e94e157b1a8a212b3152</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a0a2d86822192fdf98f65353ccad5dad8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aa5df5142710261c84c3171996206c349</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>acbc37eb86a54beac068c779801317a68</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPVariableProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a40e2459a522f2e51fd66dfd8d529f631</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a235a0ba956569c56e5c9c50b598594ed</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const MPVariableProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a0bea28b37fd3bb49be397a7a597f702e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>ae06de7d05dd185634d944f74d56ad6d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNameFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a5ccb07923cc9a1bac2c00b216b70192b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kObjectiveCoefficientFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a8cffdd51c38ae1e87144eb234bda245b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIsIntegerFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a295a3114f1d9c1927dda04b9886aa7e7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBranchingPriorityFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a5641d857f33a05f09e190d6ac2c8251b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLowerBoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a73578a474e28aa5d0e8aad3c05424f0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUpperBoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a8e22d9cfbceb3503571503453006d559</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1MPVariableProto.html</anchorfile>
      <anchor>aa521381d2e1830b495734a1ac056b4df</anchor>
      <arglist>(MPVariableProto &amp;a, MPVariableProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::OptionalDouble</name>
    <filename>classoperations__research_1_1OptionalDouble.html</filename>
    <member kind="function">
      <type></type>
      <name>OptionalDouble</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a3d37488ffc31669b71bf38c4145b25a2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~OptionalDouble</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a65f55376681119f710b2297183777bd4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OptionalDouble</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a078f72896ab3eb36213a7553208398fa</anchor>
      <arglist>(const OptionalDouble &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>OptionalDouble</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a13e6dad8632df288416bdd94fd0df1e7</anchor>
      <arglist>(OptionalDouble &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>OptionalDouble &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a40c297e4ce229e0ddef0a3b407d87ca8</anchor>
      <arglist>(const OptionalDouble &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>OptionalDouble &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a504120f95e15a1e71cf209d9b93157b2</anchor>
      <arglist>(OptionalDouble &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a2a93171bf45f55369c758f9337dd65ea</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>af864352c5dafeddce187695a3ae24bcc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a9c75b6f0082ba236ca858815b450caeb</anchor>
      <arglist>(OptionalDouble *other)</arglist>
    </member>
    <member kind="function">
      <type>OptionalDouble *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a2e1f008bad7c5f20d5308223fe66c19a</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>OptionalDouble *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a262eb91b7e1dce29df7ecabf6d43ede4</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>ac3f8e64ebff323cecfa2a41886f86664</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a55e664958ddb866d07aa9e37206d6faf</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a5fd00504567875d0a6092c26f1187bad</anchor>
      <arglist>(const OptionalDouble &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a67b2aea4a6f1ea64df07fc3491316d35</anchor>
      <arglist>(const OptionalDouble &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>abc94f07f3e53c2dc2097b50756dab07d</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>aadebb3f9d31bac8d4ad3e1ad0287b116</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a2cc088c9f621ad09c50c29c78e3781cc</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>aa76eb8f2af8d4e5d404f34495147c2d8</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a2ec26b7b3746d35972473fc0d5c6324b</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a031492fa560f6d525c9ba7068554f8a6</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>abceac6c5e5331b2e7fa0139f44ee549a</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a05ce54d80cb057348f48003b1069a394</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_value</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a185e270d4e9d8953cd2a5dd82ed026b6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_value</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a7edc7e27706b3dffcb5b9546defe4920</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>value</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>adee9ea9aa591eac8d98ae7d7d89a10f1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_value</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>aa6121160f7a2b7d41d1c5277fac45a26</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>af419e2de2e48c7bb65acd185fd7d6146</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a2d4cc857956971d399fd929031fced05</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a93006a7209080ee0afa499beb8aff307</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const OptionalDouble &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a37ab82e4e0ce390a09feb635283e2caf</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a38ed2ac40cef2d7d8f5ffa2c386cd8df</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const OptionalDouble *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a2b0cc17863a09d8d1194625006a0bd3e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a36f2c73846c8a388927003dd80299b12</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a58566ced525f962991a04c3c039a2bbc</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1OptionalDouble.html</anchorfile>
      <anchor>a72787f610d4321655ffac187486bf51e</anchor>
      <arglist>(OptionalDouble &amp;a, OptionalDouble &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::PartialVariableAssignment</name>
    <filename>classoperations__research_1_1PartialVariableAssignment.html</filename>
    <member kind="function">
      <type></type>
      <name>PartialVariableAssignment</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>afa81b93282b4cb5c61ad6d191bd5e73f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~PartialVariableAssignment</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a21123dd6a3a5319843762fbf18335a15</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>PartialVariableAssignment</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a4761c4e92063920a4c7af31cc691bd77</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>PartialVariableAssignment</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a2609899860dcaaee103e3d9d3e7cdb49</anchor>
      <arglist>(PartialVariableAssignment &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad9d885c7b3601ce36c6166cf86b19cc6</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a459706299ff079a574d02101940a53ce</anchor>
      <arglist>(PartialVariableAssignment &amp;&amp;from) noexcept</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a0419d3890bcc24619c692b58e00025d7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::PROTOBUF_NAMESPACE_ID::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a7c27934178b6aabd031f2e9f1c02ab98</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a08277fb56cafd65ca5585ce1bdbcb36d</anchor>
      <arglist>(PartialVariableAssignment *other)</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a03f27ca12071c128ca558ea787edae77</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a5e65fc8f5a57a385c17309ecfe11abfe</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af1c56240332a5de239066e389c0c01c3</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a9ebf517febe19dd2c53887da470c2694</anchor>
      <arglist>(const ::PROTOBUF_NAMESPACE_ID::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a88dcf3fa6267266ccd9815afa6c5e521</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad62c328e09e76ffc39d61aa92d3549ef</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ab76923bdde9b8199e82065ba47abfbf2</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>abd98286b2663f85cc959fd27a9ca042a</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>adef80e177d70aafb97b9cf38f423ece2</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a5e9e54bf2da71ab4eae1f590c6dec976</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ac3ab17b3ed705445bcc813a66a0e0b2c</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a05fff420a4e8d36f31e5d1d6ab218352</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad34186104406bfac678cc65149d23b84</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>aab8fc6a1644e0557c82ecd7b96287e47</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>var_index_size</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad24ac6ef826cf1912280e012d72ec24b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a40014a65f2205e5f6754c7a5b8df3e67</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::int32</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a9464b9b2a128ad6a66ac7b4d30809d36</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a074bc686b6afccbba9bb104205aca909</anchor>
      <arglist>(int index, ::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>adc61faba8d33c752968adfe3a73198a4</anchor>
      <arglist>(::PROTOBUF_NAMESPACE_ID::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; &amp;</type>
      <name>var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af98c698abe34821590c9ab56a3c11524</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; ::PROTOBUF_NAMESPACE_ID::int32 &gt; *</type>
      <name>mutable_var_index</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ac35d334678cc29c83b4d61b8625acaf0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>var_value_size</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ae58185e8029c7642b7fdb6fef574a43b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_var_value</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ade2414edbb9f0e372f989d3276298fec</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>var_value</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a66a76172734f4dee8d1ab190abc1c151</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var_value</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a5d91c2aebebdce36ccf7f53ecbbf31c2</anchor>
      <arglist>(int index, double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_var_value</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ade06b2468c9527c47d29fd25bc733247</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; &amp;</type>
      <name>var_value</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a0c71dc52f489f62a2768f43d5e451933</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::PROTOBUF_NAMESPACE_ID::RepeatedField&lt; double &gt; *</type>
      <name>mutable_var_value</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a2de40e4bc038da3d4be1bea4be33b6e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a24131213b34e356875f2e16df277179f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Descriptor *</type>
      <name>GetDescriptor</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a6c79f62f4da1262acd0413cf0ddfd385</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::Reflection *</type>
      <name>GetReflection</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad3ad7b3e9897348a82dadb7e382ba5b6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const PartialVariableAssignment &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ae458e5ecf055a526f099979c58c1fe0e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>afe03f07357c3e3004bc748276535b692</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const PartialVariableAssignment *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a74f0656a7355b8470a4e631192a6346b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ac677f2680ae29ac4f9d69008b84e09d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarIndexFieldNumber</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>acab46ed9ecd7ccc9d838dfbd38db8faa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af42d8a9e7fbc85605b0821e4e4ea0b37</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend class</type>
      <name>::PROTOBUF_NAMESPACE_ID::internal::AnyMetadata</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a9b35d94da3444084fc3673b7717b6cfe</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af910aca53a97ebc29b0a0b528eeb1671</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af5d0c2dd0559285b7031bfdf619ece69</anchor>
      <arglist>(PartialVariableAssignment &amp;a, PartialVariableAssignment &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>TableStruct_ortools_2flinear_5fsolver_2flinear_5fsolver_2eproto</name>
    <filename>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</filename>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::ParseTableField entries []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>ab0021985b398d91a5038c29af93c5e1d</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::AuxillaryParseTableField aux []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>ae89b6468e71ab96b8719afc2f860ce09</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::ParseTable schema [11]</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>aa1db0c27c3994ce246d23e942c325e0f</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::FieldMetadata</type>
      <name>field_metadata</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>a99802beb583a4ed25707283597242a5a</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::internal::SerializationTable</type>
      <name>serialization_table</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>a2173c6cfbfa8f412b8dd294d6d7798f2</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::PROTOBUF_NAMESPACE_ID::uint32</type>
      <name>offsets</name>
      <anchorfile>structTableStruct__ortools__2flinear__5fsolver__2flinear__5fsolver__2eproto.html</anchorfile>
      <anchor>affa2d00f4e745385ebd6e5f68eb79b4a</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>internal</name>
    <filename>namespaceinternal.html</filename>
  </compound>
  <compound kind="namespace">
    <name>operations_research</name>
    <filename>namespaceoperations__research.html</filename>
    <class kind="class">operations_research::MPConstraint</class>
    <class kind="class">operations_research::MPConstraintProto</class>
    <class kind="class">operations_research::MPGeneralConstraintProto</class>
    <class kind="class">operations_research::MPIndicatorConstraint</class>
    <class kind="struct">operations_research::MPModelExportOptions</class>
    <class kind="class">operations_research::MPModelProto</class>
    <class kind="class">operations_research::MPModelRequest</class>
    <class kind="class">operations_research::MPObjective</class>
    <class kind="class">operations_research::MPSolutionResponse</class>
    <class kind="class">operations_research::MPSolver</class>
    <class kind="class">operations_research::MPSolverCommonParameters</class>
    <class kind="class">operations_research::MPSolverInterface</class>
    <class kind="class">operations_research::MPSolverParameters</class>
    <class kind="class">operations_research::MPSosConstraint</class>
    <class kind="class">operations_research::MPVariable</class>
    <class kind="class">operations_research::MPVariableProto</class>
    <class kind="class">operations_research::OptionalDouble</class>
    <class kind="class">operations_research::PartialVariableAssignment</class>
    <member kind="enumeration">
      <type></type>
      <name>MPSosConstraint_Type</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac4082c18fc997b28960d2a15a27af30b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSosConstraint_Type_SOS1_DEFAULT</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac4082c18fc997b28960d2a15a27af30ba35dfc279dac55f2292c50123bbd65eb4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSosConstraint_Type_SOS2</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac4082c18fc997b28960d2a15a27af30baa35d9c1cb44243e123f7d5993d5b726f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPSolverCommonParameters_LPAlgorithmValues</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_UNSPECIFIED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1a2218d316cfcac5a88342c95b188f3fda</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_DUAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1a129c4c6d32bf9aed2414939cb02ff99a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_PRIMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1a53de34dc95fb67212e335f19dc210516</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSolverCommonParameters_LPAlgorithmValues_LP_ALGO_BARRIER</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a46d924645e62163da6dafc13b827d7b1a89ff8ffa01928d5993a1414705eecd15</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPModelRequest_SolverType</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLOP_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca6fab373696058c6e9f279de4a8446411</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CLP_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca0969851c637668f95c10ddb1ade866a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLPK_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4caa32d84461e16e800e3f996d6347a304d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GUROBI_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca3af34f198d539e787263f9eded0ce0cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CPLEX_LINEAR_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca4bdeae4b1af8d2cd4aab225db4fc0407</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_SCIP_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4cac25c4844cbdf1e4d7c7efc11f1f8ebf4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GLPK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4caf60a0830addaf4cf00bc59459fa6647e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CBC_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca0e93bcd472e7a9296ff02058ed60f8d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_GUROBI_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4cac8beb7f7b026823a6bc2e4e87f546da6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_CPLEX_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca26762918189367f5e171d0e226084d82</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_BOP_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4ca67639f2cd42e1197b5ad69a004c93ba3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_SAT_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4cac0fedb2082db5e7c96da01b4149c318e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPModelRequest_SolverType_KNAPSACK_MIXED_INTEGER_PROGRAMMING</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a15791cd7d877fd8cb7977bbfecd6ce4cabe010aed8c1b29c5a0fd9ac262ce791f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>MPSolverResponseStatus</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_OPTIMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaac95cb5be9e36b31647dd28910ac6cae4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_FEASIBLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaac7d90afd0518be8cd6433ecad656a83b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_INFEASIBLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa0da2dbf49d011970a770d42141819d0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_UNBOUNDED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaad73de4a0f9908a4c0d11246ecccf32b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_ABNORMAL</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaac77789af50586fb2f81915dd1cb790b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_NOT_SOLVED</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa667b6a5ed42c91ea81fa67c59cb3badb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_IS_VALID</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa97ee5aaa7f57f286d4a821dd6e57523f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_UNKNOWN_STATUS</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa84ea2a63b24de389aac6aa33b1203cd4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa6ae83516a798f1675e1b4daf0d8ea6b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID_SOLUTION_HINT</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaa0f9da70b2f2b1304313c3a2a5f4876b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaab90169f8480eca12c963af5ce50d36aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MPSOLVER_SOLVER_TYPE_UNAVAILABLE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8cc975b7db5017319901da0f63a114aaafa008125099beaab382c42682be6bbf9</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>const absl::string_view</type>
      <name>ToString</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>afc3e3b80841b587c6fbfd9e9f3ec9c59</anchor>
      <arglist>(MPSolver::OptimizationProblemType optimization_problem_type)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a2610f938f233d0adcd3142693f4a2683</anchor>
      <arglist>(std::ostream &amp;os, MPSolver::OptimizationProblemType optimization_problem_type)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a6e3ed7b755e2b756ef48c9b3bad4a780</anchor>
      <arglist>(std::ostream &amp;os, MPSolver::ResultStatus status)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>AbslParseFlag</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a61dc18a85425d0a7cf6aa3e7ce3199f6</anchor>
      <arglist>(absl::string_view text, MPSolver::OptimizationProblemType *solver_type, std::string *error)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>AbslUnparseFlag</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>af04d1dfc591c35038a974202e50e541f</anchor>
      <arglist>(MPSolver::OptimizationProblemType solver_type)</arglist>
    </member>
    <member kind="function">
      <type>util::StatusOr&lt; std::string &gt;</type>
      <name>ExportModelAsLpFormat</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a689d3552f87e89456c0c9a43847c964a</anchor>
      <arglist>(const MPModelProto &amp;model, const MPModelExportOptions &amp;options=MPModelExportOptions())</arglist>
    </member>
    <member kind="function">
      <type>util::StatusOr&lt; std::string &gt;</type>
      <name>ExportModelAsMpsFormat</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aef684073daca7460490db8d881f886e0</anchor>
      <arglist>(const MPModelProto &amp;model, const MPModelExportOptions &amp;options=MPModelExportOptions())</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ExportModelAsLpFormatReturnString</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a4d319c19b685fe608fe013b573081351</anchor>
      <arglist>(const MPModelProto &amp;input_model, const MPModelExportOptions &amp;options=MPModelExportOptions())</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ExportModelAsMpsFormatReturnString</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a37abd61c0d982af79257814b6d3a733e</anchor>
      <arglist>(const MPModelProto &amp;input_model, const MPModelExportOptions &amp;options=MPModelExportOptions())</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>FindErrorInMPModelProto</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7a27bb74d09b7ba6ea0e97bb572d2755</anchor>
      <arglist>(const MPModelProto &amp;model)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>FindFeasibilityErrorInSolutionHint</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ae4ee4d82cf625670cdc1f52197454654</anchor>
      <arglist>(const MPModelProto &amp;model, double tolerance)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSosConstraint_Type_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a69d74b24808a9eba4bcbc04c5bd1f9fb</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MPSosConstraint_Type_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a94d793569692b2bdcb76cf2d7736da05</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>MPSosConstraint_Type_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a0d84cc4ed67dd0a7ccf556176aa9bc1d</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSosConstraint_Type_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a6d1606a9e00c2974c23f2e758924b459</anchor>
      <arglist>(const std::string &amp;name, MPSosConstraint_Type *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab3ee5c7a9f799696432b082fd4835232</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a70bcdf756e44dfd2d5dab2a5cf4cfb9a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac96996b4dbc25690d6d7fe345b364519</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a3b1bc7a63f4a7972004060311346868f</anchor>
      <arglist>(const std::string &amp;name, MPSolverCommonParameters_LPAlgorithmValues *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPModelRequest_SolverType_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ad26c438ab5f1b232d7eced80a2780ca0</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MPModelRequest_SolverType_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>af637f39c9ca296bf197d792c62167b7d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>MPModelRequest_SolverType_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5209f68ceef830f109310dc549479a9b</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPModelRequest_SolverType_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aeb81b2591906288f021c0a3e37843b37</anchor>
      <arglist>(const std::string &amp;name, MPModelRequest_SolverType *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverResponseStatus_IsValid</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7a295b0760db498bc4fa9479bb8c2329</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::PROTOBUF_NAMESPACE_ID::EnumDescriptor *</type>
      <name>MPSolverResponseStatus_descriptor</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ace7f8b02c012c058db64b534e3378f0f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>MPSolverResponseStatus_Name</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a188641a1ab5a4dda11c00a11149b07d4</anchor>
      <arglist>(T enum_t_value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MPSolverResponseStatus_Parse</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a22b5756cf719f9b2d10dae67820cf885</anchor>
      <arglist>(const std::string &amp;name, MPSolverResponseStatus *value)</arglist>
    </member>
    <member kind="variable">
      <type>constexpr double</type>
      <name>kDefaultPrimalTolerance</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a07189276cc680928dad51ed197142077</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPConstraintProtoDefaultTypeInternal</type>
      <name>_MPConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a11d06964c51cd718a2a5c620c3289f7e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPGeneralConstraintProtoDefaultTypeInternal</type>
      <name>_MPGeneralConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab18f88184af1e6b0197a98cf0485803f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPIndicatorConstraintDefaultTypeInternal</type>
      <name>_MPIndicatorConstraint_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a1fa4d06ad0beb392a3144747d83fcc2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPModelProtoDefaultTypeInternal</type>
      <name>_MPModelProto_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aa214723b84fc52d727efc5067df690e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPModelRequestDefaultTypeInternal</type>
      <name>_MPModelRequest_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5557bc052354d9b956a609d0698281d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPSolutionResponseDefaultTypeInternal</type>
      <name>_MPSolutionResponse_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a9c99a96a8b2fcf4ab6890a4717c92da5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPSolverCommonParametersDefaultTypeInternal</type>
      <name>_MPSolverCommonParameters_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7ece0f2b42b6eaf443223377343e1966</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPSosConstraintDefaultTypeInternal</type>
      <name>_MPSosConstraint_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a3916f807aef0b8a0929c71cb72f8fe2c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>MPVariableProtoDefaultTypeInternal</type>
      <name>_MPVariableProto_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>af3dce953fd737d51dcb003b93452b3b6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>OptionalDoubleDefaultTypeInternal</type>
      <name>_OptionalDouble_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5fd6483b24c303a0fbf9ab49846d370c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>PartialVariableAssignmentDefaultTypeInternal</type>
      <name>_PartialVariableAssignment_default_instance_</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac92dae0b80b47779fc1de1bf9e7df9dd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSosConstraint_Type</type>
      <name>MPSosConstraint_Type_Type_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a8a26ab806b2722fadd4035cd0be0ae5b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSosConstraint_Type</type>
      <name>MPSosConstraint_Type_Type_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a50ec8ebec75c1daf0e7633cb74ff6657</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>MPSosConstraint_Type_Type_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a43079282dcdc58640a4fb8f3504d9548</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac1eda65381beae08503e8af2b57a0d4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSolverCommonParameters_LPAlgorithmValues</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a12a6be7881f2f7dd6e426242c961d5d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>MPSolverCommonParameters_LPAlgorithmValues_LPAlgorithmValues_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a2b0590a3e329a0bb8a10b866c28138a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPModelRequest_SolverType</type>
      <name>MPModelRequest_SolverType_SolverType_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a9eaabd9c53b8aa093483b2c664a405c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPModelRequest_SolverType</type>
      <name>MPModelRequest_SolverType_SolverType_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a42eca5d9d855cdf447e78e17acd87c7a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>MPModelRequest_SolverType_SolverType_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a7affd70e5dc61deefab59f4c06149644</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSolverResponseStatus</type>
      <name>MPSolverResponseStatus_MIN</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a53c0861628965fd7d72a0816d8575c66</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr MPSolverResponseStatus</type>
      <name>MPSolverResponseStatus_MAX</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a1315bf58051fbf57733dc025d6994340</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>constexpr int</type>
      <name>MPSolverResponseStatus_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ac136e7845fbe09520c0e7777d9ae8b43</anchor>
      <arglist></arglist>
    </member>
  </compound>
</tagfile>
