<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>
<tagfile>
  <compound kind="file">
    <name>cp_model.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/sat/</path>
    <filename>cp__model_8h</filename>
    <includes id="cp__model_8pb_8h" name="cp_model.pb.h" local="yes" imported="no">ortools/sat/cp_model.pb.h</includes>
    <includes id="cp__model__solver_8h" name="cp_model_solver.h" local="yes" imported="no">ortools/sat/cp_model_solver.h</includes>
    <includes id="model_8h" name="model.h" local="yes" imported="no">ortools/sat/model.h</includes>
    <includes id="sat__parameters_8pb_8h" name="sat_parameters.pb.h" local="yes" imported="no">ortools/sat/sat_parameters.pb.h</includes>
    <includes id="sorted__interval__list_8h" name="sorted_interval_list.h" local="yes" imported="no">ortools/util/sorted_interval_list.h</includes>
    <class kind="class">operations_research::sat::BoolVar</class>
    <class kind="class">operations_research::sat::IntVar</class>
    <class kind="class">operations_research::sat::LinearExpr</class>
    <class kind="class">operations_research::sat::IntervalVar</class>
    <class kind="class">operations_research::sat::Constraint</class>
    <class kind="class">operations_research::sat::CircuitConstraint</class>
    <class kind="class">operations_research::sat::TableConstraint</class>
    <class kind="class">operations_research::sat::ReservoirConstraint</class>
    <class kind="class">operations_research::sat::AutomatonConstraint</class>
    <class kind="class">operations_research::sat::NoOverlap2DConstraint</class>
    <class kind="class">operations_research::sat::CumulativeConstraint</class>
    <class kind="class">operations_research::sat::CpModelBuilder</class>
    <namespace>operations_research</namespace>
    <namespace>operations_research::sat</namespace>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9c0ae0d048a431656985fc79428bbe67</anchor>
      <arglist>(std::ostream &amp;os, const BoolVar &amp;var)</arglist>
    </member>
    <member kind="function">
      <type>BoolVar</type>
      <name>Not</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5e3de118c1f8dd5a7ec21704e05684b9</anchor>
      <arglist>(BoolVar x)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a57b8aabbc5b3c1d177d35b3ebcf9b5fa</anchor>
      <arglist>(std::ostream &amp;os, const IntVar &amp;var)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae9f86b31794751c624a783d15306280c</anchor>
      <arglist>(std::ostream &amp;os, const IntervalVar &amp;var)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>SolutionIntegerValue</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aeaed9bdf2a27bb778ba397666cb874d7</anchor>
      <arglist>(const CpSolverResponse &amp;r, const LinearExpr &amp;expr)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>SolutionIntegerMin</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a671200a31003492dbef21f2b4ee3dcbd</anchor>
      <arglist>(const CpSolverResponse &amp;r, IntVar x)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>SolutionIntegerMax</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ec893fa736de5b95135ecb9314ee6d8</anchor>
      <arglist>(const CpSolverResponse &amp;r, IntVar x)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SolutionBooleanValue</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afa415e372a9d64eede869ed98666c29c</anchor>
      <arglist>(const CpSolverResponse &amp;r, BoolVar x)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>cp_model.pb.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/sat/</path>
    <filename>cp__model_8pb_8h</filename>
    <class kind="struct">TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</class>
    <class kind="class">operations_research::sat::IntegerVariableProto</class>
    <class kind="class">operations_research::sat::BoolArgumentProto</class>
    <class kind="class">operations_research::sat::IntegerArgumentProto</class>
    <class kind="class">operations_research::sat::AllDifferentConstraintProto</class>
    <class kind="class">operations_research::sat::LinearConstraintProto</class>
    <class kind="class">operations_research::sat::ElementConstraintProto</class>
    <class kind="class">operations_research::sat::IntervalConstraintProto</class>
    <class kind="class">operations_research::sat::NoOverlapConstraintProto</class>
    <class kind="class">operations_research::sat::NoOverlap2DConstraintProto</class>
    <class kind="class">operations_research::sat::CumulativeConstraintProto</class>
    <class kind="class">operations_research::sat::ReservoirConstraintProto</class>
    <class kind="class">operations_research::sat::CircuitConstraintProto</class>
    <class kind="class">operations_research::sat::RoutesConstraintProto</class>
    <class kind="class">operations_research::sat::CircuitCoveringConstraintProto</class>
    <class kind="class">operations_research::sat::TableConstraintProto</class>
    <class kind="class">operations_research::sat::InverseConstraintProto</class>
    <class kind="class">operations_research::sat::AutomatonConstraintProto</class>
    <class kind="class">operations_research::sat::ConstraintProto</class>
    <class kind="class">operations_research::sat::CpObjectiveProto</class>
    <class kind="class">operations_research::sat::DecisionStrategyProto_AffineTransformation</class>
    <class kind="class">operations_research::sat::DecisionStrategyProto</class>
    <class kind="class">operations_research::sat::PartialVariableAssignment</class>
    <class kind="class">operations_research::sat::CpModelProto</class>
    <class kind="class">operations_research::sat::CpSolverResponse</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::CpSolverStatus &gt;</class>
    <namespace>operations_research</namespace>
    <namespace>operations_research::sat</namespace>
    <namespace>google</namespace>
    <namespace>google::protobuf</namespace>
    <member kind="define">
      <type>#define</type>
      <name>PROTOBUF_INTERNAL_EXPORT_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a198722177a36417069228aec0f9d97d6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>DecisionStrategyProto_VariableSelectionStrategy</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_FIRST</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea5e00b7cd6b433ec6a15ff913d3b2c3f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_LOWEST_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea0b1d456b36749d677aa4a201b22ba114</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_HIGHEST_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea79fc0af04ed454750ecb59dc5a748e88</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MIN_DOMAIN_SIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea18e573e60bf8dde6880a6cfb9f697ffc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MAX_DOMAIN_SIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea9bc8cd090f555c04c4fb8ec23838dc30</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea77405cd855df69ed653be2766be0a1af</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148eadecec94c9d1599ecbdfdab2f7cfcb7aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>DecisionStrategyProto_DomainReductionStrategy</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_MIN_VALUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4a2f416e6e94f971bfbb75ba25e7f7b760</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_MAX_VALUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4ac22896facd05595ce84133b3b3043685</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_LOWER_HALF</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4ab63e61aebddafddd1496d6ab577dab53</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_UPPER_HALF</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4ac41d0ba8114af7179c253fda16e517ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4a82875a7d185a8f87d56cb0fb0f37f72a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4ac1c76a18c1405c9569b8afca29919e48</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>CpSolverStatus</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNKNOWN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12a693e3d1636a488a456c173453c45cc14</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_INVALID</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12acb3300bde58b85d202f9c211dfabcb49</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12ae4d551fa942cba479e3090bb8ae40e73</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12a185c2992ead7a0d90d260164cf10d46f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12a78e9c6b9f6ac60a9e9c2d25967ed1ad0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CpSolverStatus_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12a443f059ef1efc767e19c5724f6c161d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CpSolverStatus_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12ae535ad44840a077b35974e3a04530717</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddDescriptors_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>cp__model_8pb_8h.html</anchorfile>
      <anchor>a38bf680499d9a614d825dfa5a7a689a6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::AllDifferentConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::AllDifferentConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab6c5ebe14cfc68d93a5f60686f2ae22d</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::AutomatonConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::AutomatonConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a47da04ba2be147be8b0a249d1127175f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::BoolArgumentProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ad530047c3866901687cad573a8902a36</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CircuitConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab8dbc8cdb17b07a5682228a84ca326a7</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitCoveringConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CircuitCoveringConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a65ba1bb90bf8b69684824af54ed34061</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a194b7268d38aea43cf720189f2c7d933</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpModelProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpModelProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>aad6b5a46ab5d2233f555b7eaa7f9dc8b</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpObjectiveProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpObjectiveProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>afbd10e0381bdcea8db6a4b8b1ddda5b4</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpSolverResponse *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpSolverResponse &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>abdc2b7a036c638cad9b003b8e2ae38fb</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CumulativeConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CumulativeConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a8c7246d8fad339bf133ecf5ce8b70e6f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::DecisionStrategyProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab396a7c48de804df389f1fde37cd4aed</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto_AffineTransformation *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::DecisionStrategyProto_AffineTransformation &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a8dafed95c6efbf6296753a9a90923388</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ElementConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ElementConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a4e0bfccb327b7e1ef475d48d813554ac</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntegerArgumentProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ad1456edebbb93b07e4cb7b231c6d5d1c</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerVariableProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntegerVariableProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a8e446e46683177ee44ab293e2c35231b</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntervalConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntervalConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a23fd3de0c47884bbebb25116ece5c2d7</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::InverseConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::InverseConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a6fd2c00fa691e2d0a3ec45cf883dfdf5</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::LinearConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::LinearConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>abe77e6dc60fd9e0d5c696b1b55c2fccd</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlap2DConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::NoOverlap2DConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab081583e505c7c4003cc7981f7bd354f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlapConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::NoOverlapConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>aa61f6fa8185bc8617023420148f33045</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::PartialVariableAssignment *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::PartialVariableAssignment &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a9605edce6c8d1b9f2b465ea3cf193e72</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ReservoirConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ReservoirConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>adb8db465df82459433570257339128c1</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::RoutesConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::RoutesConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ae061245ac4989a9fa86f211ccf1a94bb</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::TableConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::TableConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab58f5023a24725742e59513c8a5785e2</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9644b126f05b927a27fc7eba8e62dd57</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af66c861360ab3857d0bb2d53fde74bca</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2262e194393851724b02211c34c57457</anchor>
      <arglist>(DecisionStrategyProto_VariableSelectionStrategy value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af25eeda8a253dce34e0b0e98f69031ad</anchor>
      <arglist>(const ::std::string &amp;name, DecisionStrategyProto_VariableSelectionStrategy *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af161ecb897e60ce83c87c17d11ae7d91</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3bef95d750e0d2c4dcbf9944a6147232</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a15661f91c1c5635b462c569097268773</anchor>
      <arglist>(DecisionStrategyProto_DomainReductionStrategy value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab65df8c02daf63542fcee35b0a9f7779</anchor>
      <arglist>(const ::std::string &amp;name, DecisionStrategyProto_DomainReductionStrategy *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CpSolverStatus_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8f7f7995f8e9a03c15cdddf39b675702</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>CpSolverStatus_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aef4cfe27470b9d29843e9394cb75f33a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>CpSolverStatus_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a168a8ab6018d96c83fbd0d0ee03e087c</anchor>
      <arglist>(CpSolverStatus value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CpSolverStatus_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a49458d152506001af5ad6ad1b7c8576e</anchor>
      <arglist>(const ::std::string &amp;name, CpSolverStatus *value)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a932a088438a4a18cac0d84a50f9cef93</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab7bf2119b197f54b7cfb237d392a3b31</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::CpSolverStatus &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a61f6bf84c590e6ff99427d674d30cc9c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable">
      <type>AllDifferentConstraintProtoDefaultTypeInternal</type>
      <name>_AllDifferentConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad5cadc3f160d3e34ef323536a36578ce</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>AutomatonConstraintProtoDefaultTypeInternal</type>
      <name>_AutomatonConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a89e105e8d30d25c4c680294fe7d572c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>BoolArgumentProtoDefaultTypeInternal</type>
      <name>_BoolArgumentProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad05e4bcf8c4464c50e1f1b8af2b81ad2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CircuitConstraintProtoDefaultTypeInternal</type>
      <name>_CircuitConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6a9352c8a15382c9206993a807ca1f97</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CircuitCoveringConstraintProtoDefaultTypeInternal</type>
      <name>_CircuitCoveringConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adc89524c8aab967f7d4a66bd3ec70bca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>ConstraintProtoDefaultTypeInternal</type>
      <name>_ConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a946e95ccf1a9faf8270238f5c5b301fb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CpModelProtoDefaultTypeInternal</type>
      <name>_CpModelProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ace223c8e846b17ef993566562cec8dda</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CpObjectiveProtoDefaultTypeInternal</type>
      <name>_CpObjectiveProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>acfdc8eaa58fc4cf8b103821df60cd4e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CpSolverResponseDefaultTypeInternal</type>
      <name>_CpSolverResponse_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a13b87f99bbea144cc07cdcd2095ab601</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CumulativeConstraintProtoDefaultTypeInternal</type>
      <name>_CumulativeConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aac6a8bda3dfe9f06ab9e4b5d0273df53</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>DecisionStrategyProtoDefaultTypeInternal</type>
      <name>_DecisionStrategyProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a1d42bd587a5323aaf16295be1dfa1455</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>DecisionStrategyProto_AffineTransformationDefaultTypeInternal</type>
      <name>_DecisionStrategyProto_AffineTransformation_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad0110b5023e714ba7608ca6393a28aee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>ElementConstraintProtoDefaultTypeInternal</type>
      <name>_ElementConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4ef77bd2a03378993af8582adc081ae6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>IntegerArgumentProtoDefaultTypeInternal</type>
      <name>_IntegerArgumentProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3dc76ede4b7ff0d2c5bd425c834e1a1b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>IntegerVariableProtoDefaultTypeInternal</type>
      <name>_IntegerVariableProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a44161c9b8ede2f098f009c6980c489a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>IntervalConstraintProtoDefaultTypeInternal</type>
      <name>_IntervalConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4215dda19ecaf7d9b3437190df671cbb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>InverseConstraintProtoDefaultTypeInternal</type>
      <name>_InverseConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4903b3b9596898e507eadb8642d73b7d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>LinearConstraintProtoDefaultTypeInternal</type>
      <name>_LinearConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a35f06e6b931d091b424f42c8db845273</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>NoOverlap2DConstraintProtoDefaultTypeInternal</type>
      <name>_NoOverlap2DConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afc421996f32997364f39272a061499f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>NoOverlapConstraintProtoDefaultTypeInternal</type>
      <name>_NoOverlapConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a75a5dfa26b4dc21981f4c6cc46ae9c43</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>PartialVariableAssignmentDefaultTypeInternal</type>
      <name>_PartialVariableAssignment_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5fe88249a924da9eac41aefea5ddabed</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>ReservoirConstraintProtoDefaultTypeInternal</type>
      <name>_ReservoirConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0865a57214595b3a38ceee49543b4a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>RoutesConstraintProtoDefaultTypeInternal</type>
      <name>_RoutesConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae1bf1cf3f7f77485b9d4c7ab4d6894ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TableConstraintProtoDefaultTypeInternal</type>
      <name>_TableConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a1b5b8679bd9fed7c991d05c09cf01466</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a59941f8a574d610fbd0d2766daf437e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa936a57453c9681bab32e74a3747c5f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0fe139f7887fdce2f0d82ba7bfe3b761</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8d89ba785675bf6374b216c6880cf89d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2446fab2d79c5ef3d9ab370d8be7519b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a98b9900acdb468cd47a37be6ec6fecce</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const CpSolverStatus</type>
      <name>CpSolverStatus_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a39d6196edcd5c594db5524b4fd1a9cad</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const CpSolverStatus</type>
      <name>CpSolverStatus_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad61de2d59ad12b07b65b1b2497542ea2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>CpSolverStatus_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9309f1a918471faabd064037b40b3a2a</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>cp_model_solver.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/sat/</path>
    <filename>cp__model__solver_8h</filename>
    <includes id="cp__model_8pb_8h" name="cp_model.pb.h" local="yes" imported="no">ortools/sat/cp_model.pb.h</includes>
    <includes id="model_8h" name="model.h" local="yes" imported="no">ortools/sat/model.h</includes>
    <includes id="sat__parameters_8pb_8h" name="sat_parameters.pb.h" local="yes" imported="no">ortools/sat/sat_parameters.pb.h</includes>
    <namespace>operations_research</namespace>
    <namespace>operations_research::sat</namespace>
    <member kind="function">
      <type>std::string</type>
      <name>CpModelStats</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a287579e5f181fc7c89feccf1128faffb</anchor>
      <arglist>(const CpModelProto &amp;model)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>CpSolverResponseStats</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac2d87e8109f9c60f7af84a60106abd57</anchor>
      <arglist>(const CpSolverResponse &amp;response)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse</type>
      <name>SolveCpModel</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9d67b9c66f1cb9c1dcc3415cd5af11bf</anchor>
      <arglist>(const CpModelProto &amp;model_proto, Model *model)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse</type>
      <name>Solve</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a09d851f944ab4f305c3d9f8df99b7bf8</anchor>
      <arglist>(const CpModelProto &amp;model_proto)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse</type>
      <name>SolveWithParameters</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa3062797aa0396abf37dbcc99a746f12</anchor>
      <arglist>(const CpModelProto &amp;model_proto, const SatParameters &amp;params)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse</type>
      <name>SolveWithParameters</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af52c27ecb43d6486c1a70e022b4aad39</anchor>
      <arglist>(const CpModelProto &amp;model_proto, const std::string &amp;params)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetSynchronizationFunction</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad04337634227eac006d3e33a7028f82f</anchor>
      <arglist>(std::function&lt; CpSolverResponse()&gt; f, Model *model)</arglist>
    </member>
    <member kind="variable">
      <type>std::function&lt; void(Model *)&gt;</type>
      <name>NewFeasibleSolutionObserver</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aacf15d440f0db4cd0a63c8aebe85db6d</anchor>
      <arglist>(const std::function&lt; void(const CpSolverResponse &amp;response)&gt; &amp;observer)</arglist>
    </member>
    <member kind="variable">
      <type>std::function&lt; SatParameters(Model *)&gt;</type>
      <name>NewSatParameters</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10700832ca6bc420f2931eb707957b0b</anchor>
      <arglist>(const std::string &amp;params)</arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>model.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/sat/</path>
    <filename>model_8h</filename>
    <class kind="class">operations_research::sat::Model</class>
    <namespace>operations_research</namespace>
    <namespace>operations_research::sat</namespace>
  </compound>
  <compound kind="file">
    <name>sat_parameters.pb.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/gen/ortools/sat/</path>
    <filename>sat__parameters_8pb_8h</filename>
    <class kind="struct">TableStruct_ortools_2fsat_2fsat_5fparameters_2eproto</class>
    <class kind="class">operations_research::sat::SatParameters</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_VariableOrder &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_Polarity &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_BinaryMinizationAlgorithm &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseProtection &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseOrdering &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_RestartAlgorithm &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatAssumptionOrder &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_SearchBranching &gt;</class>
    <namespace>operations_research</namespace>
    <namespace>operations_research::sat</namespace>
    <namespace>google</namespace>
    <namespace>google::protobuf</namespace>
    <member kind="define">
      <type>#define</type>
      <name>PROTOBUF_INTERNAL_EXPORT_ortools_2fsat_2fsat_5fparameters_2eproto</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>af6cf58e28c0974a7acdafa7d639296f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_VariableOrder</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7500a48ce324f0ef41f39e45f60f214d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7500a48ce324f0ef41f39e45f60f214da92760d7186df85dfd6c188eae0b9b591</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_REVERSE_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7500a48ce324f0ef41f39e45f60f214da941215af97625c63a144520ec7e02bfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_RANDOM_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7500a48ce324f0ef41f39e45f60f214da8de6cbc54e325b78d800c8354591d726</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_Polarity</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_TRUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2a6145ecb76ca29dc07b9acde97866a8ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_FALSE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2a204c91561099609cdf7b6469e84e9576</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_RANDOM</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2afaf662755a533bc2353968b4c4da4d32</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_WEIGHTED_SIGN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2af9a6fbf18fc3445083ca746b1e920ca6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_REVERSE_WEIGHTED_SIGN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2a77094f18176663ceea0b80667cf917a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ConflictMinimizationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23bae1bd62c48ad8f9a7d242ae916bbe5066</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_SIMPLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23bac1adcdd93b988565644ddc9c3510c96c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_RECURSIVE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23bacf7f9f878c3e92e4e319c3e4ea926af7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_EXPERIMENTAL</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23ba52b205df52309c4f050206500297e4e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_BinaryMinizationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_NO_BINARY_MINIMIZATION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffa5cefb853f31166cc3684d90594d5dde9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffacefb9cb334d97dc99896de7db79a2476</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffac586955ded9c943dee2faf8b5b738dbd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_WITH_REACHABILITY</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffa30c30629b82fa4252c40e28942e35416</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_EXPERIMENTAL_BINARY_MINIMIZATION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffaeb6a38e1f5f44d7f13c6f8d6325ba069</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ClauseProtection</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad24109146a44723f1c95b7d3f226fcc1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad24109146a44723f1c95b7d3f226fcc1a1739f0f3322dc59ebaa2fb9fa3481d6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_ALWAYS</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad24109146a44723f1c95b7d3f226fcc1aa7de36c91e9668bd4d3429170a3a915a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_LBD</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad24109146a44723f1c95b7d3f226fcc1a4ce148354b01f5b1e2da32e7576edaa3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ClauseOrdering</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a31ad84fa962b626887890dd76a53c74a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseOrdering_CLAUSE_ACTIVITY</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a31ad84fa962b626887890dd76a53c74aaaab0bb6b57e109185e6a62d5d0271a04</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseOrdering_CLAUSE_LBD</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a31ad84fa962b626887890dd76a53c74aa2dcf758b7ee7431577e2aa80a60b163e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_RestartAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_NO_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a698c5900a88697e89f1a9ffa790fd49f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_LUBY_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a0fcf1821b877dd61f6cfac37a36a82d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_DL_MOVING_AVERAGE_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a89e7ee47fc5c826c03f455f082f22c70</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_LBD_MOVING_AVERAGE_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a5d2302ed4086b87cadaad18aa5981aed</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_FIXED_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a353691b5a40f70fe5d05cc01bdf22536</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_MaxSatAssumptionOrder</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a683a06394b218203a4517b19468df400</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_DEFAULT_ASSUMPTION_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a683a06394b218203a4517b19468df400ab0500c1196441cd7820da82c2c1baf6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_DEPTH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a683a06394b218203a4517b19468df400a61bc7845a56fecefcc18795a536d5eb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_WEIGHT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a683a06394b218203a4517b19468df400a44da070df5c6e2443fa1c00b6c25893f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_MaxSatStratificationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab342c0ecaab53f1e8e6cf05ca513b8d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab342c0ecaab53f1e8e6cf05ca513b8d5a5bb7f0a112c4672ea2abec407f7d384c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_DESCENT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab342c0ecaab53f1e8e6cf05ca513b8d5a0c67cde78d6314de8d13734d65709b3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_ASCENT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab342c0ecaab53f1e8e6cf05ca513b8d5adf547628eb3421e641512aeb95b31912</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_SearchBranching</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_AUTOMATIC_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4bac23498a3951b707b682de68c3f2ef4ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_FIXED_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4ba4b402cda1dee9234ecc9bf3f969dae9c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PORTFOLIO_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4ba79d67aaf6b62f71bbddd9c5177ebedc1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_LP_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4bac0ee72ff494861f949253aac50496f42</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PSEUDO_COST_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4ba0959d8f131e2610b97a8830464b2c633</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PORTFOLIO_WITH_QUICK_RESTART_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4ba28a2409f7a5ca2ecd6635da22e4e6667</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddDescriptors_ortools_2fsat_2fsat_5fparameters_2eproto</name>
      <anchorfile>sat__parameters_8pb_8h.html</anchorfile>
      <anchor>a68b07c45a03772248435665ca63fb33a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::SatParameters &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a5e3ba24b3733dab6d3aac5a8deec1fc6</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_VariableOrder_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a711b59624fbd706f0754647084c665d8</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_VariableOrder_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a63d00708775015b761d79d26958ae008</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_VariableOrder_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aea11eda3bbcc4f79baab267009d28df6</anchor>
      <arglist>(SatParameters_VariableOrder value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_VariableOrder_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a628f11b71a7acbabf2c7eb0a55ebf04e</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_VariableOrder *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_Polarity_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4585806adf77d6f7a56bd21230a31175</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_Polarity_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab019ee0753776b26fed17764e82d23e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_Polarity_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aeb3937db56cace9b52fbb3ada9bfea73</anchor>
      <arglist>(SatParameters_Polarity value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_Polarity_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aacbaf337b8a87121b647c838bef22e1b</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_Polarity *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a90d6f173fbfa33e26ff6508013c81ffd</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0405a68dd3f67e20ca8c7b12d45cb870</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a729819ca8e41e5a7c95a32da63d75804</anchor>
      <arglist>(SatParameters_ConflictMinimizationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a184421f59216ca2ef58f282236cf8bc3</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_ConflictMinimizationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_BinaryMinizationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3e37f554c39fbb05faf07674ac550f47</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_BinaryMinizationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac6854e48c578db9f71a0c4a95dc95279</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_BinaryMinizationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af7cc36dac69bb4b7d7d5dacbf37e57ba</anchor>
      <arglist>(SatParameters_BinaryMinizationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_BinaryMinizationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa0292e780dbe4984839ecad4b44fccf0</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_BinaryMinizationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseProtection_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac1aa9d5ea93fbc96a68237c2beda3836</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_ClauseProtection_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afdfdf216dea1b6ca3cb4c816396f7493</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_ClauseProtection_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa46504e5e34f1716ac37b78ddc08b060</anchor>
      <arglist>(SatParameters_ClauseProtection value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseProtection_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a1558fb6c8e007b75889204116c149f78</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_ClauseProtection *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseOrdering_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa6f7c43106217e8a55877110b7d87e7c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_ClauseOrdering_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a38f0d79ca92d2252d62d8db8dfd1556a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_ClauseOrdering_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a56d5fe6aa184be05f6092ab990f5250e</anchor>
      <arglist>(SatParameters_ClauseOrdering value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseOrdering_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad674863df7b9117f210c945f2674db58</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_ClauseOrdering *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_RestartAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab199957e5457d8356687f12d67d1aaac</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_RestartAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa2f24a25dc16dd685917069e6bb22b0b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_RestartAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9703a0efa39a7877735205de9a006c0f</anchor>
      <arglist>(SatParameters_RestartAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_RestartAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0960de8f477819a400cbd3a41062b9a2</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_RestartAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatAssumptionOrder_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4104fcd7cb88b2edc4cbc86e6b331cdf</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_MaxSatAssumptionOrder_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a828b06b1d9e9e57276c5092899592cd2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_MaxSatAssumptionOrder_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3a48b4e764d3598485a64075cee904fa</anchor>
      <arglist>(SatParameters_MaxSatAssumptionOrder value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatAssumptionOrder_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3bc3e149fd0e1959e5805d7ad73ccff2</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_MaxSatAssumptionOrder *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5fcee51ba7784a7c403731301af6e14c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a52f132562a3089063ffa35dc1c54f21b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a08c2c94217816891bec7180e5f6b50d3</anchor>
      <arglist>(SatParameters_MaxSatStratificationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>affb8017c363df7be4c369908a6e1f90f</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_MaxSatStratificationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_SearchBranching_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9018824bcc1b169f32af87ad4faf7561</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_SearchBranching_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a136b498c164dea9e5a9829d1590cec7b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_SearchBranching_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aaf4dfaa6a41d60012b210e5587cbbf51</anchor>
      <arglist>(SatParameters_SearchBranching value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_SearchBranching_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a992a00120a7ac841217f4561576cc354</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_SearchBranching *value)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_VariableOrder &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>affaa52e637aa5700a18de9de15d50802</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_Polarity &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a7ce45884a30882618460722b3f5b6a63</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a865dfb0c8234642d80df4b0f91914a54</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_BinaryMinizationAlgorithm &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a53653792bc958c9d29044e3ab139254b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ClauseProtection &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ad34fd79c2176b0344e2a5a649cfcca5e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ClauseOrdering &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a5f0b29c0076ec4e010aeeb8bdb500a02</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_RestartAlgorithm &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>aa75c01445cca67a20648e0162d4d01b3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_MaxSatAssumptionOrder &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>afa03ee919b72e3665921728ee15be26d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a86ff4f7acb5908cc179e592e6dae80e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_SearchBranching &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a20bb637648e3cdc283c4e71ce06a3956</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable">
      <type>SatParametersDefaultTypeInternal</type>
      <name>_SatParameters_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae6d7897cec550c4b33117827b971e421</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_VariableOrder</type>
      <name>SatParameters_VariableOrder_VariableOrder_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afebbdcc35f1ea46b6b36b02942a45718</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_VariableOrder</type>
      <name>SatParameters_VariableOrder_VariableOrder_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa37fa90963dd6767336794ec9ddd88a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_VariableOrder_VariableOrder_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0dbcc1f155896d126ee866c6fa7cdbca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_Polarity</type>
      <name>SatParameters_Polarity_Polarity_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7dcc06ad16f29c763ef71c12e33428d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_Polarity</type>
      <name>SatParameters_Polarity_Polarity_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa9e4f6913a334312075d8b06e4a8f481</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_Polarity_Polarity_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa038a595c0924ec0a6b6d1df43a47a92</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ConflictMinimizationAlgorithm</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a797fae7f793822a392093b2c0e0583df</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ConflictMinimizationAlgorithm</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad89401863cafbbb42117a67da51a9c7a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8bb3e1b9fc46859bcb473d877fdf81f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_BinaryMinizationAlgorithm</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af3993105042b8a18ef4c48af71dbfae6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_BinaryMinizationAlgorithm</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac3979ff56f4e8e1a3827ffe9a1cfd953</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4603f2b46b1da66b7f160b501802a571</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ClauseProtection</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a50a54b74c5a02bf787d5161be8496a65</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ClauseProtection</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a315f1d416996e0e1df6cf7c5f22c4c83</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a177a5f526c9aaf4dde0ae3d973a0a1c6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ClauseOrdering</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa6f7a09fe0ece7997430857d3d2b06d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ClauseOrdering</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8939c25bb2dba51fc7d410b379ca4b95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7bd8956745a9e0f194935411ad26a7a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_RestartAlgorithm</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad069012c7b5da25c0b506d93f249ae3c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_RestartAlgorithm</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae5654991d85d76a2bb57727e534aca69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9fc9eb8a69f68bb8c56f718d2905cccf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_MaxSatAssumptionOrder</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8276145f0fea9405a17ea4e15437c370</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_MaxSatAssumptionOrder</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a253ac635cb2d948d9e6f7ddbdf50deb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab876374ba4d61ed8a8f5ab1647214f57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_MaxSatStratificationAlgorithm</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a81ef72928f25bf91f9459e95b30f60a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_MaxSatStratificationAlgorithm</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af77a4c44b279a8ed20ba62fe6855f3d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aeb0e2efc07e4da53cac4c129726d25c2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_SearchBranching</type>
      <name>SatParameters_SearchBranching_SearchBranching_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a759737df3763d9079011350ee71b933f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_SearchBranching</type>
      <name>SatParameters_SearchBranching_SearchBranching_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae811ceae8f8230a59d40b5effad594af</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_SearchBranching_SearchBranching_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a82110fc37ba023a467574052d75d507b</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="file">
    <name>sorted_interval_list.h</name>
    <path>/Users/lperron/Work/or-tools/ortools/util/</path>
    <filename>sorted__interval__list_8h</filename>
    <class kind="struct">operations_research::ClosedInterval</class>
    <class kind="class">operations_research::Domain</class>
    <class kind="class">operations_research::SortedDisjointIntervalList</class>
    <class kind="struct">operations_research::SortedDisjointIntervalList::IntervalComparator</class>
    <namespace>operations_research</namespace>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5c341d9214d5d46014089435ba0e26d3</anchor>
      <arglist>(std::ostream &amp;out, const ClosedInterval &amp;interval)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aaa301d39d2a9271daf8c65e779635335</anchor>
      <arglist>(std::ostream &amp;out, const std::vector&lt; ClosedInterval &gt; &amp;intervals)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IntervalsAreSortedAndNonAdjacent</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab8c23924c4b61ed5c531424a6f18bde1</anchor>
      <arglist>(absl::Span&lt; const ClosedInterval &gt; intervals)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>abebf3070a940da6bf678953a66584e76</anchor>
      <arglist>(std::ostream &amp;out, const Domain &amp;domain)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::AllDifferentConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>AllDifferentConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>afa374362ff2ec8d60e5c421e54b6a8a8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~AllDifferentConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a0e2f7dc53c99244558213cf95867b151</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>AllDifferentConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a26fea2882d6146622fff824680664334</anchor>
      <arglist>(const AllDifferentConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>AllDifferentConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>ad4bf1bfe0a0aa3f19e6b5dc7159adb30</anchor>
      <arglist>(const AllDifferentConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a6a9aed54518ace24b21f1c97dad50e14</anchor>
      <arglist>(AllDifferentConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>AllDifferentConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a9d39bf2a98cfbbc78cd1c3b1c79e3fae</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>AllDifferentConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a3f257dab16292c7831f7361aef07625d</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>afb358ed428e2e634fabf33b60f99d149</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a43898ffb224792fda6b89f64411a60f8</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a6e634b216b297afc7daab20e4d4df6aa</anchor>
      <arglist>(const AllDifferentConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>ab5b2993c4ea616b2c2f7f858c5879f1c</anchor>
      <arglist>(const AllDifferentConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>aaddbe0d802e4082ef20b54714f729d9a</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a03f21d65761f0771abb669fa9aead776</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>aebd51dada93f067e7c9f84fbff787b5b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a3a28d885a00ca704abf9bbd609e30090</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>ab7b8130120e80ca58bc306d702a250e0</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a308313c0054b1c5b7bf34ec4f540f249</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a0064fce1beae7a9a46176c1050ac5fc3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a603e743d00fcfb3ca7a4f92e67de73a3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>vars_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a7a9333c7f1acab5f529ced5c134a0526</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a6e37adc908b3f8e82a6eda54c0fd56e7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a3d78b15020b25d11c0907e92f7c3b566</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a336ec9541dce5445385a90d03c2ab273</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a5e343db89b4f96f073219ff96cbf1ed1</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>adbe8562cfce680587f33ec3377bf9a6a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a5fe82b6bcc56f466ed48bf5891f3b617</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a7f5b7ef264feb768b76df854f4a63f39</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const AllDifferentConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>aa86d19876167cc651ab7c7f91813cf11</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a20f5b23bd98aeff3e7de3b247547d0de</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const AllDifferentConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a2f260cd00606d62f67eb3cd8a5dfb00b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a58f503a20854e14c4f88516be9e6a7fb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>a0ab3526b503dbd92ed23ad2d929f03e6</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1AllDifferentConstraintProto.html</anchorfile>
      <anchor>aaa3305f1fd5a03f4eb7996c2a2aba0a9</anchor>
      <arglist>(AllDifferentConstraintProto &amp;a, AllDifferentConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::AutomatonConstraint</name>
    <filename>classoperations__research_1_1sat_1_1AutomatonConstraint.html</filename>
    <base>operations_research::sat::Constraint</base>
    <member kind="function">
      <type>void</type>
      <name>AddTransition</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraint.html</anchorfile>
      <anchor>a4fa8634eeba27c91397c58105ff50eb7</anchor>
      <arglist>(int tail, int head, int64 transition_label)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9052e9e1dd8248433909b5542f314add</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>ab6457905c9a8cd1c5f92738d57e6f298</anchor>
      <arglist>(BoolVar literal)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9401ab195650160402df5b61f8ac9bda</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aeaf30f4ee7d141e68905f1ac2432b937</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ConstraintProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aa0b277df64333f670b66c8d5295b8250</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>acaa17b2fbfd62f6845329ae944835654</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ConstraintProto *</type>
      <name>proto_</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9d74c3d77f601020ab87700745f830ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraint.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::AutomatonConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>AutomatonConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a2ce37e9a01698c28e3918fea2380b34a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~AutomatonConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a2bcaf0024a666930de570132899432f3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>AutomatonConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a55f2533461576fb4106ce5e8c79e712e</anchor>
      <arglist>(const AutomatonConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>AutomatonConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>acb72109275f0bbd30408de1bcf0eeacc</anchor>
      <arglist>(const AutomatonConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a0aa92588fd47a629b96696e25dd6300b</anchor>
      <arglist>(AutomatonConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>AutomatonConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>aff155b45acfb3df83388e54a20b84420</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>AutomatonConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ac9c293fb6841816d4911c2f1290e1754</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a19b7f343b8d97295cd1669c93cbec5f2</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a40dd164a5aebba8849cccdc52b0a2f4b</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a15b619d6ee471826fa715465ae242ae4</anchor>
      <arglist>(const AutomatonConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>aaa030cd3ff45507264753f8b0b10d252</anchor>
      <arglist>(const AutomatonConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a9a044943acecb41edd5ea95d6b321ed9</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a8878a085c2b4478c553749bac6725edc</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a951d2ca862980a508c22c5e0308278d9</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>aab3fb4fe814c6edcba5e71d8fdfaa63b</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a006fe8a819c993ad062c8d4bd18083dc</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ad34c59148c5364923d67f967682cea4d</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ad3328ed391a2d36ad716d60d910bcdb3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ad94224e07479c7e60dbe62515a3ff409</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>final_states_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ac99e1afae75590e25d661f1137da0ba8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a7083c52be05ded4ea61e630caa50bc4a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a7608e2255874168b59392f72cb753447</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a80ed7ff85b8efd1815d86586068c4b76</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a0dff0dadcfc0fa8c28c8ac1fa094b778</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a043b5f8d4dced524e49a99b4a53fa07f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_final_states</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a32c8de8beacb484cff5d46efba7ee59c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>transition_tail_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>add75ec952b964800c3a18adb171d09ec</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a95b5195c56298dbfae5f770ed360a341</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>aa7d0d3d3943614c389782d117262f1e6</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a96e3c98ba5837f23106681f0db79560e</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a59b9541b95f5f3f798dbecd104e31a11</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>adf3352df0fdb68b199240d941a2acb47</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_transition_tail</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a4c04bcfc62345b462205b2779600d9f4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>transition_head_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a6728dfcf5656948276eb264330581fd8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a14890736d4144d5d0500007c66c250f4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ade960984611c1148c2bf4d8d33c661b2</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a8f85400789546a6557933cd3d4544118</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a58a98d5c7fd069b1b2951b8239db3006</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a31a2f332cf3239dc51b35c8fd4c085d6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_transition_head</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a40d46599946d091ddaed7d1b8a4c0ce0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>transition_label_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>acfe5b84916bdea1c88761d9313af37e4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a03d458adcd99f56890b91bdafd07933f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a43b569d25ec0ab27d4b422c67e26dc0b</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ac9244949573e2ed7874c351cae0b07e2</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a073dbd97e67b045e5a92e5e3839be2b5</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a801d47702cc27485643dba6b9c342429</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_transition_label</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a01636f4466914de4f4faaeed4be942e5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>vars_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a53a302dce7c81b492f48e64d181fdb63</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a2bd463b949a988e39d0d48b557c1ba67</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ace455118d811041fc43444902f78cfc0</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a70a7ca8986dd225fbf8f4b88835f98e8</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ae831863cb9f82a9ec4ac78ae8d2eabee</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ac080d419e3df06c0e526c304be0398ba</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a1eff50f92f57709e19816fe5eb38c3d7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_starting_state</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>acb7af1f1b3e2085c4fc287d24c969927</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>starting_state</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a4f0070663b33142e48c4d67622bec0b7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_starting_state</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ab967d11422ab695be4918379f20b70d0</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a3c88a9d69667ec90b35c18c3869e9536</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const AutomatonConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a55d75e066622788e5c181dac8c008bc3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ae0e9c59d0fb6ecfedba625909970b89a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const AutomatonConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ab01d12ceba022e6cbac43b994e2f989e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a387ca7a7c92210a71e8c77629eadd560</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kFinalStatesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a2b50dfa699e33a00007187246d440403</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTransitionTailFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a79a4872e3a0d000ff7a62b728f0be592</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTransitionHeadFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>aec94b3112b64bf15d42a2f06d3cd58fe</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTransitionLabelFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>ad42d3c684b92af8eb39541c92976479d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a499c2db091213dae28610e24433d5667</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kStartingStateFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>a7f92fefd240bd66168e393acaa6c4d31</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1AutomatonConstraintProto.html</anchorfile>
      <anchor>afe5304b03b26f7f806e85d9af6e439ab</anchor>
      <arglist>(AutomatonConstraintProto &amp;a, AutomatonConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::BoolArgumentProto</name>
    <filename>classoperations__research_1_1sat_1_1BoolArgumentProto.html</filename>
    <member kind="function">
      <type></type>
      <name>BoolArgumentProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>aa6d7164d8f0e2932c3f5e9f19074f744</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~BoolArgumentProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a358ed03085bd4ed48d3504ceff622780</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>BoolArgumentProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>ad3578c907f4521c8e9677d6868b9427d</anchor>
      <arglist>(const BoolArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>BoolArgumentProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a9bb2617efbb9575da8fc1d4cf01af39f</anchor>
      <arglist>(const BoolArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a3914a36d19690e3df25bb7b4e7ed1c79</anchor>
      <arglist>(BoolArgumentProto *other)</arglist>
    </member>
    <member kind="function">
      <type>BoolArgumentProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a43d3bfd3136b34018452bbddcb96d030</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>BoolArgumentProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>ad71453c1e6cf339ee64682c236414895</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>add1109346f6be8042ba61c50d9a1693c</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a7cd712133e4d51f544270bbc03043bc6</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>ae8c1b738c76f2366fc317e7c092df9c1</anchor>
      <arglist>(const BoolArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a29ceab5bb3c648acf7cf73cb8322e8e8</anchor>
      <arglist>(const BoolArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a15cad7b5f5252937821fd4d6d9f9b2f4</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>af787de2b844643c16104056bf79ab97b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>aae91c259f6cd19cf9251bd5ff5870f0b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a0ed2d0310bea9145b889990c36ea6407</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>acf1bd48d0a8e0a45f0ca59be3c9176af</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a1bcc5e52d8ab9d48de1e58857b7207bb</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>afe0fff5867a98c14d6d29ba4720071ce</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a88a222d89d9c63750694affc2a16a369</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>literals_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a81914ffb56d793ba98c8633026bd8cf9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a5256bdb96599195f9b9271412b0a48fd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a3f16f613b34bcf50af48d7485ebb35c1</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>adfdf82f30184efad642acf6159fcfe9a</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a749ebb82bf43204a4e1ea9cb17c533cc</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a73001359fb4c2ea4644c08122967b05b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a40332ca462fb011f1a83a39295e9b630</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>aa7da3152a8f00b1cbd80cb886bce1db2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const BoolArgumentProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a17325964be2cd6a3cc1ec1d2f9652107</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a7bd57a2b7a336d7e53b36212fcb5c834</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const BoolArgumentProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a1e56fdb95df22a4766a67ae9bbb61591</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a07c8eb97a8c1865d856c6600728251f8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLiteralsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a264798808af6ae84c09a6f047980025b</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolArgumentProto.html</anchorfile>
      <anchor>a5de194fae79eeb9b54d960d21d113787</anchor>
      <arglist>(BoolArgumentProto &amp;a, BoolArgumentProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::BoolVar</name>
    <filename>classoperations__research_1_1sat_1_1BoolVar.html</filename>
    <member kind="function">
      <type></type>
      <name>BoolVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a8467b4b5dffef99ffb96ef6b9b4a4097</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>BoolVar</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a1963637fcd9bfe8f9bd85a0971c0270d</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>abcebeff89abbdb6b0b812616f1517f25</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>BoolVar</type>
      <name>Not</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>ac5a3346c2302559c71bd9cd1e989edf9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator==</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>afb7fdd0dab72ba28030fb6d03ce5c32f</anchor>
      <arglist>(const BoolVar &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a6e9d4868f30b80fa5c37ac8991726110</anchor>
      <arglist>(const BoolVar &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>afb03d8ed70e426b0f7b83c76fce3c68f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const IntegerVariableProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a379713d334c199eeb834c338385293ba</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerVariableProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>ae7e96dfb8ae534a787632d78711f9a44</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>index</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a27d52277902e0d08306697a43863b5e8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CircuitConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a946eae8a695dfad4799c1efecec379e6</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>Constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a697ed9eaa8955d595a023663ab1e8418</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>IntVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a34419e55556ff4e92b447fe895bdb9c3</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>IntervalVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>afc7f9983234a41167299a74f07ec6622</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>LinearExpr</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a7678a938bf60a5c17fb47cf58995db0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>ReservoirConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>ae0ff478f6506cb705bbc1737598276f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend bool</type>
      <name>SolutionBooleanValue</name>
      <anchorfile>classoperations__research_1_1sat_1_1BoolVar.html</anchorfile>
      <anchor>a8391a20c25890ccbf3f5e3982afed236</anchor>
      <arglist>(const CpSolverResponse &amp;r, BoolVar x)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CircuitConstraint</name>
    <filename>classoperations__research_1_1sat_1_1CircuitConstraint.html</filename>
    <base>operations_research::sat::Constraint</base>
    <member kind="function">
      <type>void</type>
      <name>AddArc</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraint.html</anchorfile>
      <anchor>a9ee6aa474b9e4c2bcf8fab717079704d</anchor>
      <arglist>(int tail, int head, BoolVar literal)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9052e9e1dd8248433909b5542f314add</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>ab6457905c9a8cd1c5f92738d57e6f298</anchor>
      <arglist>(BoolVar literal)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9401ab195650160402df5b61f8ac9bda</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aeaf30f4ee7d141e68905f1ac2432b937</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ConstraintProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aa0b277df64333f670b66c8d5295b8250</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>acaa17b2fbfd62f6845329ae944835654</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ConstraintProto *</type>
      <name>proto_</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9d74c3d77f601020ab87700745f830ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraint.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CircuitConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>CircuitConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ac03c25224efaf68cb37bf98ed55607ec</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~CircuitConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a3736b5b621d7a4b3605ac433b6382957</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>CircuitConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>aec23bde0c2a7502ff946c4423641abd3</anchor>
      <arglist>(const CircuitConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CircuitConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ac2b3d1c86cae0843cb1b90ad512a485a</anchor>
      <arglist>(const CircuitConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ab1c3303746b39a3d342e45f19a811140</anchor>
      <arglist>(CircuitConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>CircuitConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a6c8172fd753d71de2ca23661777bbda7</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>CircuitConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ab44585f7a5855d888e1f7b9b6e923eed</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a7aa2c01b5ad538488d678c00c1e9c0f0</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a81fafe099df488db67ca908097324839</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>abcd4e0a410ac9673f6f6712ea7054325</anchor>
      <arglist>(const CircuitConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ab160b48ca874381ff6b41dd56b1c7938</anchor>
      <arglist>(const CircuitConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ad70d2820838a83df3348e4dcd1b20cea</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a0f06b46a64f75615a4a2c49db992481f</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>aba954be1d46b388b7d3065635f71f326</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a9ee7a4d988128cb5b3d3c5795e9628b9</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a678875e4c352af0223e22ae4d6205423</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ac930eec00068f2bc78e8846589f0eee9</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a90aa18f88888ace0d623de979f7c398d</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a616915ddd9c051553257da27d6626d14</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>tails_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a57607035d4858f8ef2e01a22fff82439</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a9edc97dfbac3dc05fb3ae0404581d6b2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ae4c70a436cb759f1aa65e64652de574a</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a360bf3fdf81562acae6740542f62d96a</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a34bd0d3c0fcf3bc84a925c6043e333ff</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ab030bad5974eb5c6748f8fb02ee1dc4e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a8445c2a968481ce74e53bae45148a473</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>heads_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a4f13a443bccc6025d789530f9c1f8424</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>adfb00c3666338f5bded103a6c5d04b8e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a7d9717048673653429237fb3a2cdd9e1</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a7e8fc502eec008fd4dbeffb1dccd4938</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a934fa3aeaeb55e19742e07813cd283ef</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>aefa5a6533fbf4b7b116e763067be3d6c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a9e5d797bed65aa26e8add80266749d02</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>literals_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>ac9af2e517541f34a816b08876e7bf897</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a8497d861f72f9440c9f57e5202a2c690</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>aa154f4ee20de7b05fd19987ea35c1a59</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a330cd6d9c5d1d0b9eadccf208c32d4c7</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>af9a48fb2a07e20f850c529f7a76aa26f</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a67c6013435a3684f201e1255768029ce</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a38a95c5e65fbf2ffb20f1d54eab900c8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a71a68d9e1f208f445411a521b3c004dd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CircuitConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a533b4d7ddbe62501bdbc1dbc0757b158</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a8f89111afdbc96248e6ceb54cafe47b5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CircuitConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a4f99cdabbc4a106fb43c80697145bf40</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a11a07cf84b1b816316bf2027e70ab5e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTailsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a97650e3f1e5e6e75690bd1a8edc2f7b0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kHeadsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a6b73f88461df2b0d76c8675ef2a3455f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLiteralsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a2d76ed5befb82bafa1780691d6e1fea9</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitConstraintProto.html</anchorfile>
      <anchor>a3f29fae2e2b1458bafebce6492c8350a</anchor>
      <arglist>(CircuitConstraintProto &amp;a, CircuitConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CircuitCoveringConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>CircuitCoveringConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a18f7844e9f186e5fbc933a07e4b60cc4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~CircuitCoveringConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>aa7d06305f269b95c8f0916c11c030886</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>CircuitCoveringConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a8aaa75ccc3693b2cadf9a61a3142dd49</anchor>
      <arglist>(const CircuitCoveringConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CircuitCoveringConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a7923ca37bb6e6c8a86928e95ede9eede</anchor>
      <arglist>(const CircuitCoveringConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a53e35c361f142f5d263af6122c2cd1fc</anchor>
      <arglist>(CircuitCoveringConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>CircuitCoveringConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ae9dae8863a44b93144a4a09693a912ec</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>CircuitCoveringConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>aebd09027ebd0f9aa811cb79663961ac8</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>acd2b5c5d7f23d9f4522b208909f2e637</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a0730512bbaf709aea4661d0dc96aad81</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ae2ad0379b25a0e3cfefe079d5590775a</anchor>
      <arglist>(const CircuitCoveringConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ab8c29293be1cbc7305cd3d6834567e5b</anchor>
      <arglist>(const CircuitCoveringConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a633979cc780157b04496cfef86de26ea</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>abee12d9695573cecfa922cc630900bb2</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a78d5dba85d260f0ae28db4e8bfcb59fa</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a6df68edcf297586760f81fe8dc06e6e2</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a3e5412f253d404a7d7ccddb9b523eca1</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ae50eccc06cd67c0985936e744f7f4883</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a634b2023dc3d99e11a8bdc314cc6e3da</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a18da503c7a02cbc861ec7c061d25e9c8</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>nexts_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ae81a5330d9c4578a872554f767a95030</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a702407bd83369a9f351bfeca7d70d9a4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a345efbf6fed0f58374bb58083c021a15</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a2d3aa7f19f696d1a4bdf6d7c2826cb59</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a5db8ca7f4e96ce3b3a3396aa397f5351</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a54e42a780e65edfc9c5a1a44ffd572da</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_nexts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>aaa54f6a6d1916e53cad22f46bb2a4909</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>distinguished_nodes_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a91305e4fa3c1579cc39428a3b701fa35</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a1b772f12f1d739f8664093caab32492f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a70b107b5eb2e8170009f089209220cdc</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a5205db0efa49b6c873824ce9b326cd63</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>adf3da7be113000ed83607e3116ce8741</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a860f8ef11d9677f4fb412c260082f524</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_distinguished_nodes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ad60ef4ef646a2db977c7817270d4ff1a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>adbb601898963a0ffe3e010989cc1ca67</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CircuitCoveringConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a00ce285601d62105dd0c050374821ee4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a77003f4a28115587497843e1b86fe1ca</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CircuitCoveringConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ae878ab3d55408227172e06d3128f791b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a92608cdb80815a28da2a1be947994d27</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNextsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a0a92e93b4764d23f4356d960ebc0ced9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDistinguishedNodesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>a425c18334e10877278812faa278192fb</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CircuitCoveringConstraintProto.html</anchorfile>
      <anchor>ac4168eb8c043a305f923cbdb229dfb2b</anchor>
      <arglist>(CircuitCoveringConstraintProto &amp;a, CircuitCoveringConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::ClosedInterval</name>
    <filename>structoperations__research_1_1ClosedInterval.html</filename>
    <member kind="function">
      <type></type>
      <name>ClosedInterval</name>
      <anchorfile>structoperations__research_1_1ClosedInterval.html</anchorfile>
      <anchor>ac88ed3db6ef14b84e38b5d89d39aaa04</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ClosedInterval</name>
      <anchorfile>structoperations__research_1_1ClosedInterval.html</anchorfile>
      <anchor>ae79c6820d4e756c8805ef8f3dac20655</anchor>
      <arglist>(int64 s, int64 e)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>structoperations__research_1_1ClosedInterval.html</anchorfile>
      <anchor>a45209c3e31f989a7d5b3c7f8a15f6164</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator==</name>
      <anchorfile>structoperations__research_1_1ClosedInterval.html</anchorfile>
      <anchor>a5e40a5426c5178a044f8147e05a57e67</anchor>
      <arglist>(const ClosedInterval &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator&lt;</name>
      <anchorfile>structoperations__research_1_1ClosedInterval.html</anchorfile>
      <anchor>aec81d0fbae7b13a421f4aebc0285df2d</anchor>
      <arglist>(const ClosedInterval &amp;other) const</arglist>
    </member>
    <member kind="variable">
      <type>int64</type>
      <name>start</name>
      <anchorfile>structoperations__research_1_1ClosedInterval.html</anchorfile>
      <anchor>add276e813566f507dc7b02a1971ea60b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>int64</type>
      <name>end</name>
      <anchorfile>structoperations__research_1_1ClosedInterval.html</anchorfile>
      <anchor>a650dc1bae78be4048e0f8e8377208925</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::Constraint</name>
    <filename>classoperations__research_1_1sat_1_1Constraint.html</filename>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9052e9e1dd8248433909b5542f314add</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>ab6457905c9a8cd1c5f92738d57e6f298</anchor>
      <arglist>(BoolVar literal)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9401ab195650160402df5b61f8ac9bda</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aeaf30f4ee7d141e68905f1ac2432b937</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ConstraintProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aa0b277df64333f670b66c8d5295b8250</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>acaa17b2fbfd62f6845329ae944835654</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" protection="protected">
      <type></type>
      <name>Constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9a6b9b664d4d0d56e8c0d14c8ad3bad9</anchor>
      <arglist>(ConstraintProto *proto)</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ConstraintProto *</type>
      <name>proto_</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9d74c3d77f601020ab87700745f830ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="protected">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::ConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1ConstraintProto.html</filename>
    <member kind="enumeration">
      <type></type>
      <name>ConstraintCase</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kBoolOr</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a705bb6ca71f5af4bc065f01fdd3e6bfa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kBoolAnd</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ac21140bc25c184d332f57f1d725e38a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kAtMostOne</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a104ac051e3fac45d118336fafcd78bfc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kBoolXor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ad987062c36dc563894f2a3d26197500e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntDiv</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a35fd22077d30d054670d016ede906acd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntMod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ac7bbacce3d7eb4fc277a51a65cfe0702</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntMax</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a8ad369fa4f923910360c564bdb7d8762</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntMin</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a9ec63c50679c0039a12e29226f226527</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntProd</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ac895ea97ae4e81a42cc9b2fdfd1030ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kLinear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a7fdfe348c47fb1b603ece24d1ebaa579</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kAllDiff</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ae9e579cb7ddd6426d9a0e14764c741a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kElement</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ad0ca39d67db616b9882a3519577acbfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kCircuit</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92af1487093aa6682e397319c8764b9ee00</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kRoutes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a1a0f4bf1d276c8925468553869e13785</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kCircuitCovering</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ac31f3d955393c8475ff900a0b895dc03</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kTable</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a497212ead868a867a2fd85dee6fd05cc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kAutomaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a642b33c3b02ca487eb0aa00a089538ce</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kInverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a55c8140a2905eb6f14420003b5c2f521</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kReservoir</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92aedb4cff3209c9d64a1f575e83564d429</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kInterval</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a7fb600421a46d673bc2add6f400164d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kNoOverlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a4b2a9828c1ffaae1a8462362a1c28a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kNoOverlap2D</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a6821b17ef82cf675d5f5c4011e4df114</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kCumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a3986d7b34549a1cdf7c2a8d3151d6569</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CONSTRAINT_NOT_SET</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a6930b48c82c46169a6cbcf8428ae757c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kBoolOr</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a705bb6ca71f5af4bc065f01fdd3e6bfa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kBoolAnd</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ac21140bc25c184d332f57f1d725e38a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kAtMostOne</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a104ac051e3fac45d118336fafcd78bfc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kBoolXor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ad987062c36dc563894f2a3d26197500e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntDiv</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a35fd22077d30d054670d016ede906acd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntMod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ac7bbacce3d7eb4fc277a51a65cfe0702</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntMax</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a8ad369fa4f923910360c564bdb7d8762</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntMin</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a9ec63c50679c0039a12e29226f226527</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kIntProd</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ac895ea97ae4e81a42cc9b2fdfd1030ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kLinear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a7fdfe348c47fb1b603ece24d1ebaa579</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kAllDiff</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ae9e579cb7ddd6426d9a0e14764c741a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kElement</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ad0ca39d67db616b9882a3519577acbfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kCircuit</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92af1487093aa6682e397319c8764b9ee00</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kRoutes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a1a0f4bf1d276c8925468553869e13785</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kCircuitCovering</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92ac31f3d955393c8475ff900a0b895dc03</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kTable</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a497212ead868a867a2fd85dee6fd05cc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kAutomaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a642b33c3b02ca487eb0aa00a089538ce</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kInverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a55c8140a2905eb6f14420003b5c2f521</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kReservoir</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92aedb4cff3209c9d64a1f575e83564d429</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kInterval</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a7fb600421a46d673bc2add6f400164d1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kNoOverlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a4b2a9828c1ffaae1a8462362a1c28a22</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kNoOverlap2D</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a6821b17ef82cf675d5f5c4011e4df114</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>kCumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a3986d7b34549a1cdf7c2a8d3151d6569</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CONSTRAINT_NOT_SET</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76cca486a4920b723b07a9a3a7eb3c92a6930b48c82c46169a6cbcf8428ae757c</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa628f3ef0e0d0c55a0dccf97ec232432</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~ConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aca53fb5a4f68fc1e76308cc4e2c8fe2d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a4445e9c8d129bbdebe140a7c4548ac6c</anchor>
      <arglist>(const ConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3a8b3abb61dc03fc158995e8a642a52c</anchor>
      <arglist>(const ConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ab38234cd745e7718479c1190684c3074</anchor>
      <arglist>(ConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>adabc7caebc27504dfb2777ec4b5cb9c0</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a912cd7bfaef8d9fc7d84e2d018717886</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>af211fe01e91b6e7a72e3c26f1ec703b4</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a2b77692bab4e194cc9b0176728023a59</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a04352802d1d8a3ad245ae9ad9e633c90</anchor>
      <arglist>(const ConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a20f50dd9fdc3d0f5a18753d1afcfc816</anchor>
      <arglist>(const ConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a28ef8fea92c19bfa1539a11cfd78c6ef</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>adaf1e2ed016dcbdae3846cb5dd6a4330</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3a541675e89688d089ad6efbbdd60925</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3cf90604f80fdf917e71beaf9e4a87f0</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a555a6c06001400bb65fc7126e023bcb9</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a17dfaf4a23a22ef75948187a00515ae6</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aef2bfeeda4c457d5b815191a78613004</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a0fd5b8a1e6c42ec4bf9f7067f60984d1</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>enforcement_literal_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ada17d138a6873ebfc0e1e177ea44c1a4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a85645c71e824bd3c863f89f6b2a024dc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aad8a9f4db9671f2ba09b7cc5f80de829</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a0511c64e1a26240948da5f9c88dd9c10</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ad436d37ed3f40815cc2c988656940a13</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a896083969ba6c2e5ac1312c456199a9f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_enforcement_literal</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8ded2b15353596d604787ffd5fc8599a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6fe2a4cda5e554408466838cb36b33f9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a95f1be1dd6ac795a3df9c3a0e170528f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7cfbf8832eedc78348b1518fdfc71433</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a91f047ceac8b25cd82c5073ab3cebc54</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a9d454afc0f3570545ae5acf267084c95</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>adbdd4f5efeab12b810f875b2492a663c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a042ffd63999a1573d23d2af6b3d28e8f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a0b5d2693fd55e990ae477fa73c24c599</anchor>
      <arglist>(::std::string *name)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_bool_or</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8eeb6ccaf041efbef3dcac3d8d369c51</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_bool_or</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a19d5119ec6a645926d6d46c2a184aaac</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::BoolArgumentProto &amp;</type>
      <name>bool_or</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a754492eba7a8f5c3c8f96848facc71c7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>release_bool_or</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a62f2ecbd3538bebd072d29c3b4fd3d92</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>mutable_bool_or</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ad64dfd534d8e4d9c738ecb39430a4e89</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_bool_or</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae9bc04148c3e407f788c0719504323cb</anchor>
      <arglist>(::operations_research::sat::BoolArgumentProto *bool_or)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_bool_and</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6d05b7c78cc7c9ea4adaf410bb0ab086</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_bool_and</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a2b328a572737cfc26823c98bcec6ec40</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::BoolArgumentProto &amp;</type>
      <name>bool_and</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>acd1eb701663490f35a869ae0029821a9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>release_bool_and</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3ccec574fa60b9de955695227a2efd23</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>mutable_bool_and</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7f9733e7139e307759fc4602dfd0b56a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_bool_and</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a0dccc441215330271deb5c98b51a9e4c</anchor>
      <arglist>(::operations_research::sat::BoolArgumentProto *bool_and)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_at_most_one</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a4d27d7f212e20be9bed29b988a228ea1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_at_most_one</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa459a0b9c801b03a74d89884073420bb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::BoolArgumentProto &amp;</type>
      <name>at_most_one</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a03c603d6b4eeab5423acacc1f98496b5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>release_at_most_one</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ab851a997d7fb3cc3377e5cc7ac8088d6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>mutable_at_most_one</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ad5a86f793f0fec20827f758347aca07e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_at_most_one</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa1ccddfbfc49e86adf46ee7dcf782b28</anchor>
      <arglist>(::operations_research::sat::BoolArgumentProto *at_most_one)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_bool_xor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae33c7520fa3a6010d01b0bed238a41a3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_bool_xor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ade7b3062d3d4cd50a8a771f5c623467e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::BoolArgumentProto &amp;</type>
      <name>bool_xor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a27b1a116b55d8003acd879e0c9af5f54</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>release_bool_xor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a346ae96c2bacba32a16e3526e491d9e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>mutable_bool_xor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a0ee6dafe035cf2a2b34de199c3e070fb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_bool_xor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a5499c4d8c62e5fddd76edae19b28c859</anchor>
      <arglist>(::operations_research::sat::BoolArgumentProto *bool_xor)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_int_div</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8e694024366c39609e83916bf228525c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_int_div</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a1a49ba721ab0d72719427e2ea63a2cfd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::IntegerArgumentProto &amp;</type>
      <name>int_div</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac98aedbccc413ad565665104385eb8b9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>release_int_div</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aba5451e0cf15021d15ef93dd0ecfd2c6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>mutable_int_div</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a965593a260f98b72401c6dd591a1c478</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_int_div</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a79e178989442f33a380e4e1e09675eeb</anchor>
      <arglist>(::operations_research::sat::IntegerArgumentProto *int_div)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_int_mod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae27d2c57e5fcf3ece47493864e05e6c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_int_mod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8a52cf64c8840a2996a35e320c079304</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::IntegerArgumentProto &amp;</type>
      <name>int_mod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ada28832d5c3177a8d643b3fe60d85525</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>release_int_mod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aea04332e976da951abe82bbc9d111865</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>mutable_int_mod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a9e17fd7855d21b3c061e523f4c17ffcd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_int_mod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ab69ee8bfb94cc03e06224489d9601fc5</anchor>
      <arglist>(::operations_research::sat::IntegerArgumentProto *int_mod)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_int_max</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a15dc6cc84c0b2c8e75f4b9f869ea4bdd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_int_max</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae097560547ce4f1c8fac9e5c43398f81</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::IntegerArgumentProto &amp;</type>
      <name>int_max</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae868376b0fb6f39a92b2de852dfcf528</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>release_int_max</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a640d36ed728390f7e10b94884e90ea45</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>mutable_int_max</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a11a14f59bc17176e5fb38f4705803437</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_int_max</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aae61c045e02d39891ecb5895bd52d2b3</anchor>
      <arglist>(::operations_research::sat::IntegerArgumentProto *int_max)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_int_min</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ab797b2456d12310663e86385a30ef92e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_int_min</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a54adc16f1f475237bda78939bf9ef2b6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::IntegerArgumentProto &amp;</type>
      <name>int_min</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac22fa70288a89ea56585f776bd083757</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>release_int_min</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7fa575785f3d16348d2d062dcd6d00ad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>mutable_int_min</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ace0dfba4cd6fe07b264bc3f00a61e357</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_int_min</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3a7d708a1b6b811428425c944b2a4261</anchor>
      <arglist>(::operations_research::sat::IntegerArgumentProto *int_min)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_int_prod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>adb4b75e20479a3a3bac243fd4d4a03ec</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_int_prod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac11569d8f764f319a79168b4152be94b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::IntegerArgumentProto &amp;</type>
      <name>int_prod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3df8e61dddf8563c43760238caf53564</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>release_int_prod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa8a6058adda8a5fe3fd4e3cf58f1ffc7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>mutable_int_prod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6f8c9a1b4fc19f1bda65d0831c37480f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_int_prod</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a00125c011fa695eb6febc1c309e63a60</anchor>
      <arglist>(::operations_research::sat::IntegerArgumentProto *int_prod)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_linear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>abd5b36c1c0e1e2a0f4303dc7598bbcc4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_linear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a33e78410bd3b735ca279c41818daa690</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::LinearConstraintProto &amp;</type>
      <name>linear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>abdd556609679a9dd5d55808714a9ccd6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::LinearConstraintProto *</type>
      <name>release_linear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7a2afe4818cafb9d335eb8c8d65ea495</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::LinearConstraintProto *</type>
      <name>mutable_linear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa70cf5d09d837abbe42bae58e70ebca0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_linear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a01a753b8ddf9d293498dcaf960970c48</anchor>
      <arglist>(::operations_research::sat::LinearConstraintProto *linear)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_all_diff</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a89ea5c26f5cfaacb41885e21b0739318</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_all_diff</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a10ee3f265f74a6e8eeb345eb9e92b815</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::AllDifferentConstraintProto &amp;</type>
      <name>all_diff</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae970851ed15ddb7c62e8c3c30f5b050d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::AllDifferentConstraintProto *</type>
      <name>release_all_diff</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a2f17eaf7115a57ea973dd6f0696d0e06</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::AllDifferentConstraintProto *</type>
      <name>mutable_all_diff</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a084726006cfced96fb4287ed3eea412b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_all_diff</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a971d4bb38f3ce6e6f05b0bd90e8cc1e0</anchor>
      <arglist>(::operations_research::sat::AllDifferentConstraintProto *all_diff)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_element</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a627718808956f9cb524bd2c14ebeb0c4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_element</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6430185c94e453e61ee566034b0992e1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::ElementConstraintProto &amp;</type>
      <name>element</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a9f1abbc633e56b7b348d3b609ead7acc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ElementConstraintProto *</type>
      <name>release_element</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a500f8a08b6b4cefb0a97b6e099b14ce2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ElementConstraintProto *</type>
      <name>mutable_element</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>addb2cf23713cb60d8616735504e91872</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_element</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a4cc74717757be245b38fbd3cc9510a97</anchor>
      <arglist>(::operations_research::sat::ElementConstraintProto *element)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_circuit</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a02b63f2b7366e5a96c07d7e6d73aabbf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_circuit</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a1703e9ccd8b4242d429eed2bd489e356</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::CircuitConstraintProto &amp;</type>
      <name>circuit</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>afa730516e6940d146615bbe424b3c9ea</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitConstraintProto *</type>
      <name>release_circuit</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a9240cbc42e2246a0e063f7251dd940aa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitConstraintProto *</type>
      <name>mutable_circuit</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>abe777d7758df71582184306ba8c5da7f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_circuit</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ad855e9d8c1b392615686e1cf8dbad634</anchor>
      <arglist>(::operations_research::sat::CircuitConstraintProto *circuit)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_routes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a1c4a90046e3aa8a141cedc6c1e288d92</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_routes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8b6942181a96fa5846db02593033bb4b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::RoutesConstraintProto &amp;</type>
      <name>routes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a297017471bd201fbe1a9a4f52c30e9da</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::RoutesConstraintProto *</type>
      <name>release_routes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a4a94142f808ed752ede3fdae935dff8d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::RoutesConstraintProto *</type>
      <name>mutable_routes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>acef74e462acb705571c58402daccd50e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_routes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a43ffbdd6ff2f9bfa820e3dda7c69e49c</anchor>
      <arglist>(::operations_research::sat::RoutesConstraintProto *routes)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_circuit_covering</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8915ad7bc02b1cc182b748f2e2a04560</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_circuit_covering</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a02270d1584e5e9455f2e2cc29bf4c6b5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::CircuitCoveringConstraintProto &amp;</type>
      <name>circuit_covering</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7629bb3aa48dcbdce9da36c54105ccaa</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitCoveringConstraintProto *</type>
      <name>release_circuit_covering</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a278b495f8ddd14f3acb86b75d32f2e85</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitCoveringConstraintProto *</type>
      <name>mutable_circuit_covering</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ad914f6fea2f7b7a17ef042aa08361f90</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_circuit_covering</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aad655b896d353fa0df1303dd819e42fd</anchor>
      <arglist>(::operations_research::sat::CircuitCoveringConstraintProto *circuit_covering)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_table</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aeb08b4a9be82558eb8b8addc6d1cf5ff</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_table</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a74d6706101d4479131d9bb7e7bc9cdbe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::TableConstraintProto &amp;</type>
      <name>table</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aba9c5d11cb96089802b971e4cde83d42</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::TableConstraintProto *</type>
      <name>release_table</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a55eb257594f88832d263858f5e8dcbf8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::TableConstraintProto *</type>
      <name>mutable_table</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a166b08fc0567630f2552a03d58993a31</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_table</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a12fff3aa9f1aadd9e1eb2d023328e990</anchor>
      <arglist>(::operations_research::sat::TableConstraintProto *table)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_automaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7e7543fa5d1aba41534ca4852b1300d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_automaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a62b9b8410dac5bfe9a6ed0847c15c4c0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::AutomatonConstraintProto &amp;</type>
      <name>automaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6a44efc50a6d420dde804b2c13a29d2d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::AutomatonConstraintProto *</type>
      <name>release_automaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8e4a8b7e77ee1f85ea1fbc8d779470aa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::AutomatonConstraintProto *</type>
      <name>mutable_automaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a77f4ca4f6e1d27b8be0a97bdc466757c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_automaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ad690d8dc521a1a7eff040cd75bc6d061</anchor>
      <arglist>(::operations_research::sat::AutomatonConstraintProto *automaton)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>af600f40a1add13e35a9cb4fd5535254c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ad13881856cc0e4dc3185bbee36aa6527</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::InverseConstraintProto &amp;</type>
      <name>inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a552864982e1aac5d5b9fd81f2411b610</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::InverseConstraintProto *</type>
      <name>release_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ab068ab670b940effbccb19eb240e3af3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::InverseConstraintProto *</type>
      <name>mutable_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a37e03920cb15a23dbbdc0dc713829695</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a99bbc4d81db8b146bcf5485eb3885a62</anchor>
      <arglist>(::operations_research::sat::InverseConstraintProto *inverse)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_reservoir</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a72469434295122d4bdccf2986c3bd385</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_reservoir</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>af902a3a65702888a4529f4117a5604bb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::ReservoirConstraintProto &amp;</type>
      <name>reservoir</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a83d29e180d4186e53e1d286f711ffce0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ReservoirConstraintProto *</type>
      <name>release_reservoir</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3786f26c22e5f492c29c392a3ac9cefa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ReservoirConstraintProto *</type>
      <name>mutable_reservoir</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a908bb0d4164b848a84057736b4a8c724</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_reservoir</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ab9798c39d2f8a9b708ea485edc615d0d</anchor>
      <arglist>(::operations_research::sat::ReservoirConstraintProto *reservoir)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_interval</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a906c8887a15a9e2e062e3c94e0485af8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_interval</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a34d38697419b83574126ade5a3343ae3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::IntervalConstraintProto &amp;</type>
      <name>interval</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ade0baf9bbe5b09d470ab30ae8b730cc4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntervalConstraintProto *</type>
      <name>release_interval</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a8664980a825a616233930f9b6529cfce</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntervalConstraintProto *</type>
      <name>mutable_interval</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a86f1152bd1888743f98a99b789d3295b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_interval</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a4900ad42598ced45bf0dcafaa13834f5</anchor>
      <arglist>(::operations_research::sat::IntervalConstraintProto *interval)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a2b5e2fd804e863cc9610fb0cdfd5d6cd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6b7cda7ca614d61c7d30bc7504beed98</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::NoOverlapConstraintProto &amp;</type>
      <name>no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a28a10d059e4d7ca2af29486c6bf3797c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlapConstraintProto *</type>
      <name>release_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac46d571f03e55688721d3a8fa86a935b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlapConstraintProto *</type>
      <name>mutable_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>afb596d34d84e861a2295ff3550db4c86</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac43a15f789057ccd03d25225811f4579</anchor>
      <arglist>(::operations_research::sat::NoOverlapConstraintProto *no_overlap)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_no_overlap_2d</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>af158df9c07131ce5b103cbf94bd9d42b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_no_overlap_2d</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>abcb1ff6ac7cf6b45215b62deb5f32ab6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::NoOverlap2DConstraintProto &amp;</type>
      <name>no_overlap_2d</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a94a7627048af8685d765c873f685f167</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlap2DConstraintProto *</type>
      <name>release_no_overlap_2d</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a2c25158af83e9cf5adac4daf3432dda5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlap2DConstraintProto *</type>
      <name>mutable_no_overlap_2d</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa2e8622d488f2bf1b7a15031eef3c3d8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_no_overlap_2d</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae5f3f51b59a1d676368d619011ed5127</anchor>
      <arglist>(::operations_research::sat::NoOverlap2DConstraintProto *no_overlap_2d)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_cumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a5d955b99d5d06123b64685022b2e0e9d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_cumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a321e8a0e5d4b7e6f2dc6326468712846</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::CumulativeConstraintProto &amp;</type>
      <name>cumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a15912fac98ec813ba33511cdcd822eb9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CumulativeConstraintProto *</type>
      <name>release_cumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a70e56256d09e73b0d260974e421f4541</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CumulativeConstraintProto *</type>
      <name>mutable_cumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6a7efe03d69f3f9e62c947264be11aae</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_cumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac4389cff5ade3f8aa8676338593c1bac</anchor>
      <arglist>(::operations_research::sat::CumulativeConstraintProto *cumulative)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a38e1a9bae801a20c0e53d6e641f2266a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>ConstraintCase</type>
      <name>constraint_case</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a08e8450b51a1cca8d87966eec73a3c5c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a70b6960ffe28e5091a48a9ef5641c0eb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a6418169b66b7c446772bc96bdccadc6d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa8803a53504ca66c79280126febce054</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a99ba01adbb6e53724371a73b20d3d030</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a05456fe94d9d3faadbe82adf75dfd092</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kEnforcementLiteralFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a885d5eff5834669d4530d60229d0cafe</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNameFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a94e106954629e7915d651f69cdb8d840</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBoolOrFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a97258948e7274277dbfe0e3abc212b3c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBoolAndFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>af912de3fadfeccaa8cd0752a3bdbcf7e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAtMostOneFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7a1157f4641665b8de2f2a775aeb8a67</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBoolXorFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>acc755737adc1475c9122062d325e79fc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIntDivFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>afa4ca15e85aa42caa479dc427f2f6ae6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIntModFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ac99b0684244b5c4b59b2c08652cf4357</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIntMaxFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7ae1372250adbdc1ed846a532b7d5bbb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIntMinFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a4cd8784612e115cc60aee0dad6b1e61d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIntProdFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>ae6db68f568300ad894ec1374e350c538</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLinearFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a76f012cccdad501b9233a33d15582572</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAllDiffFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a55acb9786dfd3d5006e126d5c6ef892a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kElementFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a503c0d40d6d4d912c631f9db8314b941</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCircuitFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a2134a22b274fb6f603caf140c3303cc8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRoutesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa254a93166f6c631d9daf99bd8f94587</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCircuitCoveringFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>af01618478588d3efae9e1a66eab51fb2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTableFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa4e3896b0665bf4b39b442b67b8c9399</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAutomatonFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a338adf39e1fbb0cbeabb42acb0781da1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kInverseFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a63ba14faa7112beed8b1459910f48e4f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kReservoirFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>aa3be03774f769cdd2a1e138493dee736</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIntervalFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a20641009a768b0c458a93a7637042311</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNoOverlapFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a7e0021d4dc9b5d2793298bc06ba0f056</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNoOverlap2DFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a3063681fb867d8da0f5512e81bbcd6e8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCumulativeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a9c53395c32bcae6681fca96aa1038a5b</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ConstraintProto.html</anchorfile>
      <anchor>a42cd6e1de56b3b4b6141435ac47d9c19</anchor>
      <arglist>(ConstraintProto &amp;a, ConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CpModelBuilder</name>
    <filename>classoperations__research_1_1sat_1_1CpModelBuilder.html</filename>
    <member kind="function">
      <type>IntVar</type>
      <name>NewIntVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a0887a2fe4518bde7bbde18f592b6243f</anchor>
      <arglist>(const Domain &amp;domain)</arglist>
    </member>
    <member kind="function">
      <type>BoolVar</type>
      <name>NewBoolVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a8fc4a0c717f985687d63a586dba04641</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>IntVar</type>
      <name>NewConstant</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>adac551c8b80fc7bdd7b30779fd20a4ea</anchor>
      <arglist>(int64 value)</arglist>
    </member>
    <member kind="function">
      <type>BoolVar</type>
      <name>TrueVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a6dc655a67c5213fcefb82a213dac5e2c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>BoolVar</type>
      <name>FalseVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a9a53531099bebddbf54dd15418817326</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>IntervalVar</type>
      <name>NewIntervalVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a4e1c85e161ee8e50f2f2162cd7294d03</anchor>
      <arglist>(IntVar start, IntVar size, IntVar end)</arglist>
    </member>
    <member kind="function">
      <type>IntervalVar</type>
      <name>NewOptionalIntervalVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a99c82eca478306942b3aed3372b38384</anchor>
      <arglist>(IntVar start, IntVar size, IntVar end, BoolVar presence)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddBoolOr</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>ae8bd984917b305dc49abae6c19b69ea3</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddBoolAnd</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a3088d984ab4874140f7c367dc457ac0f</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddBoolXor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a18d2ca2be01dd3e67893f4e1dbe4af43</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddImplication</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a43ca3f9c073ea5078c1abd3bb0c563d4</anchor>
      <arglist>(BoolVar a, BoolVar b)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddEquality</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>ad941d4f0156fc746c4ed12790bce7af7</anchor>
      <arglist>(const LinearExpr &amp;left, const LinearExpr &amp;right)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddGreaterOrEqual</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a7a718730caef4f258e1cbbb2e3e3b452</anchor>
      <arglist>(const LinearExpr &amp;left, const LinearExpr &amp;right)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddGreaterThan</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>acf4c5429ec08207e147b65bd1330ba92</anchor>
      <arglist>(const LinearExpr &amp;left, const LinearExpr &amp;right)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddLessOrEqual</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a4f1c8c11f9f840728e5c037249192b8f</anchor>
      <arglist>(const LinearExpr &amp;left, const LinearExpr &amp;right)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddLessThan</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a7cf9ff9df25ff433286b4f5bda41f990</anchor>
      <arglist>(const LinearExpr &amp;left, const LinearExpr &amp;right)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddLinearConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a562a899753d60f28ae87ecb93e96b797</anchor>
      <arglist>(const LinearExpr &amp;expr, const Domain &amp;domain)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddNotEqual</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>aa64c33dd1487bf4f0d575edf33ef2dc9</anchor>
      <arglist>(const LinearExpr &amp;left, const LinearExpr &amp;right)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddAllDifferent</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a605cc0b904f4d9b2de5fffbf6fa40c68</anchor>
      <arglist>(absl::Span&lt; const IntVar &gt; vars)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddVariableElement</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a001974a3f1f5e9d791ae10cd435f07cf</anchor>
      <arglist>(IntVar index, absl::Span&lt; const IntVar &gt; variables, IntVar target)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddElement</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>ada1b4fad9b4f017f9009ce3761123a8b</anchor>
      <arglist>(IntVar index, absl::Span&lt; const int64 &gt; values, IntVar target)</arglist>
    </member>
    <member kind="function">
      <type>CircuitConstraint</type>
      <name>AddCircuitConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>ad5ec615a9107ebcb8a7516bb3ccfbcd2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>TableConstraint</type>
      <name>AddAllowedAssignments</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a7d05d91ffdd70f16ad170e25fd47e200</anchor>
      <arglist>(absl::Span&lt; const IntVar &gt; vars)</arglist>
    </member>
    <member kind="function">
      <type>TableConstraint</type>
      <name>AddForbiddenAssignments</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a05b1310e7cfde91fbdc10798a84a2345</anchor>
      <arglist>(absl::Span&lt; const IntVar &gt; vars)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddInverseConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a0c391768bc423a43875a7867ee247a4b</anchor>
      <arglist>(absl::Span&lt; const IntVar &gt; variables, absl::Span&lt; const IntVar &gt; inverse_variables)</arglist>
    </member>
    <member kind="function">
      <type>ReservoirConstraint</type>
      <name>AddReservoirConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a5d2c35d16d6b9cb25254ca6d3b963ac8</anchor>
      <arglist>(int64 min_level, int64 max_level)</arglist>
    </member>
    <member kind="function">
      <type>AutomatonConstraint</type>
      <name>AddAutomaton</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a5738c98c07c2e0ec747877eb3813a134</anchor>
      <arglist>(absl::Span&lt; const IntVar &gt; transition_variables, int starting_state, absl::Span&lt; const int &gt; final_states)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddMinEquality</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a967f11af5e1cfb143514e09925628be5</anchor>
      <arglist>(IntVar target, absl::Span&lt; const IntVar &gt; vars)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddMaxEquality</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a902eb5d208511f7da9cdd9cde9a79c45</anchor>
      <arglist>(IntVar target, absl::Span&lt; const IntVar &gt; vars)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddDivisionEquality</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>adffb8e57735762a6f321279f2e60ae65</anchor>
      <arglist>(IntVar target, IntVar numerator, IntVar denominator)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddAbsEquality</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>aa1eae45130c127fe6cac9805736216ef</anchor>
      <arglist>(IntVar target, IntVar var)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddModuloEquality</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>abd73201c6fbc455ca4783ff99ca2eed1</anchor>
      <arglist>(IntVar target, IntVar var, IntVar mod)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddProductEquality</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a991b6a2a16def3962ccc5727004638db</anchor>
      <arglist>(IntVar target, absl::Span&lt; const IntVar &gt; vars)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>AddNoOverlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a89c4590eaf404f0ef3b80d4ce584fbda</anchor>
      <arglist>(absl::Span&lt; const IntervalVar &gt; vars)</arglist>
    </member>
    <member kind="function">
      <type>NoOverlap2DConstraint</type>
      <name>AddNoOverlap2D</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a10d61bc6bc9584cadfc0b87537ada9eb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>CumulativeConstraint</type>
      <name>AddCumulative</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a6620906cabb980393d9433df9a7f7b70</anchor>
      <arglist>(IntVar capacity)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Minimize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a0faf578c69fe9ae80ee0ea9f671dc5e7</anchor>
      <arglist>(const LinearExpr &amp;expr)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Maximize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a3559ac1f9f840b2d5637f1d26cd18f0b</anchor>
      <arglist>(const LinearExpr &amp;expr)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>ScaleObjectiveBy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>ac93a7c7467278afb9eac2bb4a8dec6d3</anchor>
      <arglist>(double scaling)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddDecisionStrategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a4d0cfb231f4bed2420d0aff928f3a980</anchor>
      <arglist>(absl::Span&lt; const IntVar &gt; variables, DecisionStrategyProto::VariableSelectionStrategy var_strategy, DecisionStrategyProto::DomainReductionStrategy domain_strategy)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddDecisionStrategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a81d8ef14e29732b81f56778090bfa547</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; variables, DecisionStrategyProto::VariableSelectionStrategy var_strategy, DecisionStrategyProto::DomainReductionStrategy domain_strategy)</arglist>
    </member>
    <member kind="function">
      <type>const CpModelProto &amp;</type>
      <name>Build</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a8e1b64644f124be491431bbae9d5d843</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const CpModelProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a791f54d4afefc05d6462fa9a9f1f304d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>CpModelProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a4b3320604b4344b5bea17c5fae1ed7ce</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CumulativeConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>a9d31ad87d4edee55fc3cb5e239077720</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>ReservoirConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelBuilder.html</anchorfile>
      <anchor>ae0ff478f6506cb705bbc1737598276f4</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CpModelProto</name>
    <filename>classoperations__research_1_1sat_1_1CpModelProto.html</filename>
    <member kind="function">
      <type></type>
      <name>CpModelProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a3a6863cd8da35857ee1f4a7f4eecdcf4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~CpModelProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a581b38d56c54d82d6a423a4e0d53c428</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>CpModelProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a8d94acf76d5ec3e2c7041eb2a6523247</anchor>
      <arglist>(const CpModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CpModelProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a7006db70a302c79981b9660bbe246958</anchor>
      <arglist>(const CpModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ae19b07a23175dc8868ddb41b86fca418</anchor>
      <arglist>(CpModelProto *other)</arglist>
    </member>
    <member kind="function">
      <type>CpModelProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a5a2f738f83003403a34641886d8ab5fc</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>CpModelProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>af9bea058cd2cf13e0433cfd3bcc35be9</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ae235c19572e0a8d48b42e9c31ab78b0b</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a25c173cba523e37dfbb7190edc8f3073</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a0730b50923f4a8db369fd5b72e8f9158</anchor>
      <arglist>(const CpModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ab70cec34be482a99dd67e513d4d0e189</anchor>
      <arglist>(const CpModelProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ad4f8e50f3dbc53f66500166566a25322</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aef12d3f93b57b1e454b5133479043f3f</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a8c291169971c79711a156b73747d13e3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a5a05243a19689d990534a2f09ca22898</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a98978db1c3b796c8849bfe0e7ffb159d</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a6e32a52282d87eb1a2e5bc21bfd60180</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a83c440eb955944077880bf5eb881c763</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a391a9b50628ddd3e3c7ab3bdd76ca21c</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>variables_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aabdfc884176585b79f65cb603c2171ce</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a481b1c7de97cede6106505b57b934d2e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerVariableProto *</type>
      <name>mutable_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a16b8cc58fa3e670712e9cfe342e61be9</anchor>
      <arglist>(int index)</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::IntegerVariableProto &gt; *</type>
      <name>mutable_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aff4a670eb1ff293d739a513b02cc9fc2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::IntegerVariableProto &amp;</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ab0dda4e799f065179f785ede9a0a2540</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerVariableProto *</type>
      <name>add_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ae13cc27e3f950e477d93af7243678eed</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::IntegerVariableProto &gt; &amp;</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ae89b32d1155e884b833edf049b201fc6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>constraints_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a6c07b425cf6992974fd2fea324a09018</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a64d13d61b9464ac98aad9659c7772a7c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ConstraintProto *</type>
      <name>mutable_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ad81813da437a67ae5f1a28b8fe290614</anchor>
      <arglist>(int index)</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::ConstraintProto &gt; *</type>
      <name>mutable_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a70fe8587686d1a4f37077be394689a5f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::ConstraintProto &amp;</type>
      <name>constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aef37b42d42f179a384a7cf514c58ba5f</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ConstraintProto *</type>
      <name>add_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a78bf9851b0383163d8c329d5e2e49d29</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::ConstraintProto &gt; &amp;</type>
      <name>constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ac1ff55db77b00aaf4544c150d117fc93</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>search_strategy_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a7664d357b05809f85f8fc57b8f392f27</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_search_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a1309970796fa7f2700ee1c65ea3e95e3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto *</type>
      <name>mutable_search_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a1ce4cbc7e9e322f32f6506857df5eb2b</anchor>
      <arglist>(int index)</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::DecisionStrategyProto &gt; *</type>
      <name>mutable_search_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a089063f63c2e05389038cf4b5013047f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::DecisionStrategyProto &amp;</type>
      <name>search_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aed202906f50cae994afe3b22ee127188</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto *</type>
      <name>add_search_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a7dd859d4f12c6eb072d4bde18c079eb8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::DecisionStrategyProto &gt; &amp;</type>
      <name>search_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a29ac54c8b2662ac8d6b0fe482bf8c7e1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aea06a33306cfcc59a3883605eae88ae1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a3ea206928327e817ef207fd22d5eb51d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ac90647359d886780a3479b14929277fa</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a0e2f76fec48748171562c5487befd14c</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a9f6a25a2e5c97ddc8249e75c3e8304fc</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ad57481a60fda7d4d85bad549b7ce97ed</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>af43f89c8f28f6162f97c906bf51925aa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a0a2acf3624890052ba0776f4cb24e636</anchor>
      <arglist>(::std::string *name)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a4b14df8e53579aa0d04cd3afa1deac65</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a549d3a431dc7805c24113a73c247b589</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::CpObjectiveProto &amp;</type>
      <name>objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aefef9641465bac65a80ebc7bae6fca42</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpObjectiveProto *</type>
      <name>release_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a53b2d50c3c5bb97bb699fd1104cce289</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpObjectiveProto *</type>
      <name>mutable_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a0433e54c873c86a851045f285094d862</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ac8c9efd6c1c1c1277169e1b6825c128f</anchor>
      <arglist>(::operations_research::sat::CpObjectiveProto *objective)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_solution_hint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a239b08538bb8d00a5ad6be06352e4b9e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solution_hint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a33550fc75c4e81b2b07b57257e281442</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::PartialVariableAssignment &amp;</type>
      <name>solution_hint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a55742aeabceb438456622936acfdcf5e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::PartialVariableAssignment *</type>
      <name>release_solution_hint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a587344b4588cbf94ced74470484e7f1f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::PartialVariableAssignment *</type>
      <name>mutable_solution_hint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ad017198cb8da599254e1b567089a579b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_solution_hint</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>ac5c8adcf1815ef6e824f5aeee16be357</anchor>
      <arglist>(::operations_research::sat::PartialVariableAssignment *solution_hint)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>abeb0e6462cc596404e3c23684fb76ce6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CpModelProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a2a76d26e21db4f7dbabf47ce56e14cff</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a460c24450ee234ed7107612bba219874</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CpModelProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>aaaff450069b51136ac66c47da10e4150</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a841b288514817e8b69334f464abba834</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVariablesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a2d76e6041716e8bec03abff55da7898d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kConstraintsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a4c1a14de2fadf9805b396eb35b3cc8a8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSearchStrategyFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>af73a70abb66aae35b70e1cfd9bd0cd86</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNameFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a60bffa6248898aefddf2f219e1de5603</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kObjectiveFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a2f5cc41ad6ec0a688bd0c1b26f887c63</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolutionHintFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a016501c8207a07bdb7ae1f63e7b58b40</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpModelProto.html</anchorfile>
      <anchor>a934d9868f4bfcada979a310ea97ce987</anchor>
      <arglist>(CpModelProto &amp;a, CpModelProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CpObjectiveProto</name>
    <filename>classoperations__research_1_1sat_1_1CpObjectiveProto.html</filename>
    <member kind="function">
      <type></type>
      <name>CpObjectiveProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a620838bd6b5a457ad34413779c887ebd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~CpObjectiveProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a6bea91804357f9ea297ca7103e62e7d5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>CpObjectiveProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a986e1db3d16a938a79de066bf2267676</anchor>
      <arglist>(const CpObjectiveProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CpObjectiveProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a94eb71df33b1b12bd25c19840e09ec61</anchor>
      <arglist>(const CpObjectiveProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a41d50989e1178b8a17a3b81da6ae87f5</anchor>
      <arglist>(CpObjectiveProto *other)</arglist>
    </member>
    <member kind="function">
      <type>CpObjectiveProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a30a53cda9025d2dcb13b0e3829c8f683</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>CpObjectiveProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a59fdb7aeb16e0749952ad2d2894c7dc5</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a1a3b2a49a3a3f3dc2a8a467d8bb000da</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a16b3d70ac402e8bbb9b712c9ba4f7931</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a10fc26ec15976625937085e3391b22b2</anchor>
      <arglist>(const CpObjectiveProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>acf6e32b5f5fa3a80631511213cab224e</anchor>
      <arglist>(const CpObjectiveProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ab03e7e5ae7254f1801eab53f7fad0fea</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a12d7f812453d90f0817ff8b813b3c1eb</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>aac875b2a52a25f603afe00f1e7fbc85e</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>adb93a3069513773e4ae2d5a2ab4fe1d9</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a80a3dd769e2954937a625714253c0816</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a185e720a7608443d599e574ecb16c212</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a36064aad65cc24fed204f87490770ec3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ac3c8fbf32858b2eccbb47b7e08d927d4</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>vars_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a6953d6ac4f587760b73093bc042ead8d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a1b8300110c8ebc0ba49b79862f0bdcaa</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ae0767a5ed87b48013cca110686f95a0d</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a90c549e8764b32f322f51f2ac3df7090</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a3bfff65947efdceb3cb71aee6277321a</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a8d6228eb6697463c62782dbdd59e61ee</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a81713efe54f8e325fcef24608c95aa2f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>coeffs_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>aacf4c11bd3601c752879650eeb7a23fc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>aaec1809299acb1c9d00804e4cbb0d7ee</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>aaf63bf3b72009f6ad8fb868286e1d7aa</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a7d87ecea77ff3f1e0e95b90b43c0537e</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ac0a8b1e3648120495074c53a53c7f853</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a5aeea6f884c8f8ef34dc65aa6d6fb045</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a98a338c74125708ff0e1f5d7e13176e7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>domain_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a95eba9b14144bafff777d9e8d6fba5c3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a882f85c944fd411cb8790486077d2b92</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a882a3c06be3bfcccc52fc729e31aae71</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a3a256036c6d0f35a390b0301ad53272d</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>af22e8e07cea0ee83e347e83ad4afcc91</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ac768393d3f03b5b37a9fc17a84cc6fb7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a6c04700d32629f2d84f164013d142100</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_offset</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a8df90ca9dd35a6487eebecb2912867bf</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>offset</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a0b597569cbc9b6ffe67e4ea305f5502f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_offset</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ac22b63c8b32dee15c16f7641455def50</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_scaling_factor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>abc64f08187fb49197f1532e5472f17ff</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>scaling_factor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a5f5cfd59f86f5639add0563573fb4272</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_scaling_factor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a08859db702c2230862ee64643ac2359a</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a7f902003a1bc9cfb75c39770fe0f724c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CpObjectiveProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a5ae43f92a69bcb77da0482a7d06b6816</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a89c0e86e4ed6005898f613b7063d7efd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CpObjectiveProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a3c6fdf99559c082a388918e9ae1331a8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>af1bffd868afdf3a4fd307ff87cb0c175</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>acd178030a57356735a90ca13790e18e7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCoeffsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a9ed8cd2c7baa42d2adf867e67b261373</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDomainFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>ae977c190764af3d6b8bf909d668051ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kOffsetFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a44dc31204c1bcb76742ed5b19cb0ffca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kScalingFactorFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a8b940c45613b3d3e54249c54ad1a3b2f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpObjectiveProto.html</anchorfile>
      <anchor>a87cd08dbce056654f4fda7da1018240f</anchor>
      <arglist>(CpObjectiveProto &amp;a, CpObjectiveProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CpSolverResponse</name>
    <filename>classoperations__research_1_1sat_1_1CpSolverResponse.html</filename>
    <member kind="function">
      <type></type>
      <name>CpSolverResponse</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a7533dc6bf9b4cd31c3831f05fd96e32f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~CpSolverResponse</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad10a69d040a520925b7b8cf2483a18fe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>CpSolverResponse</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a5463f3cb40b155256f8b70e137831053</anchor>
      <arglist>(const CpSolverResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2440c897e10a4669c114233b20c83572</anchor>
      <arglist>(const CpSolverResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a13fbcccd93fe1aa45ef24fc24ac5eec8</anchor>
      <arglist>(CpSolverResponse *other)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a0ae8141b90f2eb0dc9b2c1a7335e657a</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2d52be86282bcfc6f32c450f238151db</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a52398ec87a8eef51792a6d1d5ccac222</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aedc8a90df52dc4fa12df7d45837158e6</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ac958eb487e6a724b253b6a0b1bbcd04a</anchor>
      <arglist>(const CpSolverResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>adb42e4e72d9cafe3b0f678b7ae8912a5</anchor>
      <arglist>(const CpSolverResponse &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a20f3134be24b60cc89f859f0e786f9bd</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad627400c97bda3f8d3f239db636d7984</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a92b9a9292a30d28b7255189c660751a9</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad11a23afc428cf6f15b37ea6058b1148</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a6beb02f0e89a3f8c14c6d8c4c0c65510</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a7faaf04ada1ae344cb1961adbce17557</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aed6a825b81a8bf2fbbd2f16f23d48491</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a06943f9db338eb2d8e9ec0bb59568a38</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>solution_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ac80fa3122294b5afd18d690dc4f8da01</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aada3b809e04f9bfb9b8c8edcfbb63052</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad2aea702a7bb2f89b141dd215889b303</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a3bed20993a23e4289e1cc4e8040df19e</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a146a48ba2588d930b60a2322b25cb941</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ae81a5faff5f0737e33313a2014a910a8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a6a98194fe4ecb943c42253f50d4927b2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>solution_lower_bounds_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aa27ed063d0d32735aaee639b63bde40d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aec7e29b71d3cb1be95372d0cc31e6605</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad9b271303d6457fd16e3734c200a1896</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a111fb1a1152fd9253ee989cf37dc3cf6</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a7530e4b6478cda7692ba9abed5bd83da</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ac1603ca53ec276b56fad7cd49f4c514d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_solution_lower_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a642e0f917b425929feee0718975db212</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>solution_upper_bounds_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aa7b1273e37e36b92856801a2002f8fb4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ac56da3a2a222fd777380deacdb62181e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a7847cb3bf67c9d6cb3d529f0bd13f4e0</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a5a3bfa474c45f6b439114f6dc99859ef</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>afd43c080ad197f5f2b56c6e02b7892e9</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a06d1a1b86b070012e2d3daf535e575fb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_solution_upper_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a9f4b8686f5aa21a686284bdd555400d9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>tightened_variables_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a10957df3ad171812c136f5ec2ee6133e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_tightened_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a3fa217fe7e8527d8aa10c1a48ceed791</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerVariableProto *</type>
      <name>mutable_tightened_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a5498e1deb65e13696947219c6dc4929b</anchor>
      <arglist>(int index)</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::IntegerVariableProto &gt; *</type>
      <name>mutable_tightened_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aec506bbb3cf249e382ae7dd934b3bb73</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::IntegerVariableProto &amp;</type>
      <name>tightened_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a04eaf4fec82c981aefd7193c4ad27136</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerVariableProto *</type>
      <name>add_tightened_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ae3a8d933bc96bc411aa283b0a5ae53a5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::IntegerVariableProto &gt; &amp;</type>
      <name>tightened_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a96a43fb3e1cadb8cbd38a1d6d39cc07e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a300de1d1026383c58ecbe3c51be7febd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a08f559112ba62830cc2bc71853e874c5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aadc89ed59fea8a3f2a683215df897325</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a672ca3aaf1c1edb4a6394dfff847fcfe</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ab00253b2bbbd54e718584fb72c55c7b1</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>mutable_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a7007d548e08343070631d76e8608150c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aac94fa47e35567ed306c239b87d4b542</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_solution_info</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a92261c5bd393b986560be9dcfbed8f5b</anchor>
      <arglist>(::std::string *solution_info)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_objective_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a88b05cc454e570e869cd06a46cf9b649</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>objective_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad7227954cb9e6d46f71a0c86aef23c5d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_objective_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a98e40b4e96dc27df6b48519c51f4386a</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_status</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>af3d6089fc8b5fcae996639b09fb799cd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpSolverStatus</type>
      <name>status</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a62b908faa95a5d39a98a4d25362fa92f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_status</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a62a6114efcebe1f88e8a48c311ea2b2c</anchor>
      <arglist>(::operations_research::sat::CpSolverStatus value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_all_solutions_were_found</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>afd2e2976721753a7ee1c5b95e09b59e7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>all_solutions_were_found</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ace1da02cda722b2f39096e496dccd8ee</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_all_solutions_were_found</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a5579227d76199aefaa7caf12d1f6038b</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_best_objective_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a67b4a954f2e109df30270b4d93597e81</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>best_objective_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a6dcad9fae32425632ccabec70215c66d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_best_objective_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a870f65a87b364046814585200ae9aa3c</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_booleans</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a3bb99f57f6a3f7b8685324307e406bb9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>num_booleans</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ae28cd938c13b82c21b8a7db8c2d1ea1e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_booleans</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a620dd7f4c6098bd16fb6350fc05b712e</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a818361f6305c54210b3e41051ed822be</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ab46b0c684717e6beb1f70422ff1370b7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a8ea2cdb806e113bcc07df0f4743a9e55</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_branches</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>afb45b3e52697edae151112d72d357052</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>num_branches</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a0230bf1cb96e8cb86e43df75ec77dafa</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_branches</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>adab0c2dac568e1252fafa57675f6b454</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_binary_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ae674cc8d35deb0b290dbefc52be06026</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>num_binary_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>adfee8503447839921133b90d36113c87</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_binary_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a90d5547f0b9438c6e39be3c7c12d6c3a</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_integer_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a22ab55fb4c3769bb5d9b30830c8cb2b1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>num_integer_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aacb7bbf40a532dac20ec63bddccaf31e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_integer_propagations</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a1d245a3aa20f475e5f6ad19b8268649b</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_wall_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aed4b19f1cd10eab401e57e987e8badc4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>wall_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ab2511bc344b6ba7aaf8099e36e8278e9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_wall_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a8436b4625b35f50d14d801b5d015159c</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_user_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a53b303773fee1a228d3d7a6f6c99c437</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>user_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aed41c39ab4a816b8fad7cd76018edcf5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_user_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a65348dbb198c0177ce5c1b1947b5b916</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_deterministic_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a65699715fa9e478e31a5bf12f6154913</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>deterministic_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a354d9e195cc5ab0335cb17568552e6a3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_deterministic_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a01144ebd72e69016e7695793feba23c7</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a3869369049ac1e6f4d9707054002d95c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CpSolverResponse &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>af3803f6ba5a19de049f31362452725d4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a2394145469ceb6f9ef7fa0d505ae98a6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CpSolverResponse *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a85332793da5848376a8b777b1c64e5b9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ade7cb13b9b5c928f68104af4e10500bd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolutionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a9ae3661185729b78f14faa1527c78983</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolutionLowerBoundsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aa281d07caeeefd770935f86f6596c0bc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolutionUpperBoundsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aa6cb4b1c2314086e150b39c72521ef3f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTightenedVariablesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a60ade5cc3ad900dd6cf9daf2a191e727</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSolutionInfoFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>aa7972cf2565b480664b3944af5803ac6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kObjectiveValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a19038bdb37547f17672c3dd99c4d0342</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kStatusFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a7e1aba2bd7b3dc22290e42c5c04be024</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAllSolutionsWereFoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a950bcdf35e2ca769fa0dc44f6f183b7a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBestObjectiveBoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ab02ebb794c8dde5c4dc9ce9d3ac5b464</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNumBooleansFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad98245f572ddeb2e90738dccd1de4d4f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNumConflictsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>afb27711b2d082d1c467a42e1ee05d6d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNumBranchesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a4cd01ad4c27b9497df040454df90d1ec</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNumBinaryPropagationsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a8909a22b5f35b39f96f48ce23f2e706d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNumIntegerPropagationsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>ad919e41605d21cc83b7dcdf7c5029115</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kWallTimeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a15ce7d0fe6b337270735f9cce14d94b8</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUserTimeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a6a22c2a70b1e1e8d808347a82e6ab1b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDeterministicTimeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a6dc68531ed444656ec912a0ad1053b05</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CpSolverResponse.html</anchorfile>
      <anchor>a18137eef7618a47d519524eaca7eb565</anchor>
      <arglist>(CpSolverResponse &amp;a, CpSolverResponse &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CumulativeConstraint</name>
    <filename>classoperations__research_1_1sat_1_1CumulativeConstraint.html</filename>
    <base>operations_research::sat::Constraint</base>
    <member kind="function">
      <type>void</type>
      <name>AddDemand</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraint.html</anchorfile>
      <anchor>aded0689c7c92b1a7739758150131b531</anchor>
      <arglist>(IntervalVar interval, IntVar demand)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9052e9e1dd8248433909b5542f314add</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>ab6457905c9a8cd1c5f92738d57e6f298</anchor>
      <arglist>(BoolVar literal)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9401ab195650160402df5b61f8ac9bda</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aeaf30f4ee7d141e68905f1ac2432b937</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ConstraintProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aa0b277df64333f670b66c8d5295b8250</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>acaa17b2fbfd62f6845329ae944835654</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ConstraintProto *</type>
      <name>proto_</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9d74c3d77f601020ab87700745f830ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraint.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::CumulativeConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>CumulativeConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ae070c3b60fe5a6a05ffbb0e34d559589</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~CumulativeConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a5411745888efd9a79aa1a68d4c491915</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>CumulativeConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ae1ffebcaaf80d97ec2bdbe569a9feb3a</anchor>
      <arglist>(const CumulativeConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>CumulativeConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a83c1d1b1cb5722859bcaaea1887c2f22</anchor>
      <arglist>(const CumulativeConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a44a9f88b285af258ad1177dbadfd2443</anchor>
      <arglist>(CumulativeConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>CumulativeConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>acbbd61c32d285a810ce257cf6e7a77e7</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>CumulativeConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a2a673fd709e990dfa57cb0e599d693c9</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ab6fe2ba4ee8caf4ed7dc4e11714a5190</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac40424f16725018ee78242edb99ad15a</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a3878a0590161c52b18314e838b3b89a5</anchor>
      <arglist>(const CumulativeConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a4368c9e1fa379b09d2a873d4d17130e4</anchor>
      <arglist>(const CumulativeConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a64423562c98904b9d423176a4519b51a</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a09c8a74b7bd8d2c523e1d2aa0d5b40c1</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac4b579174094eea57176676f38503720</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a8d43f75020ba77c89e2d73f9c1eeceee</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a23b74fe5f2eb24d6c252d7e04c9e083c</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a237821ec81d4b61da4eec3a3e467c853</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac6a5586e329674017f92c35e6be5e2f8</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a64a1e9dc003e6e2ae0f96c80b0a6258f</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>intervals_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a5a2883283b3b03cda7ad8975d70aae5e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ada52406c692d73c66ac6069095cafff9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>aa9afcc60fd9b45fb8f54b7f8a2afda90</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a15c1e77f058b40205c1452742b71be25</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac77f7363de61db1fcbe7386938da6dfc</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>acc120c3c6c78d57b5cbed290d7a4bc80</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a8f68267bcc49c542d3ce63bf6300970b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>demands_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a5fd084c2ffff13383a2006406e2f86e4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>af59b3fc1afa7e4184ddf0aaf9d1d56e5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a03b79ce802caab3b536150f1726d38b8</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a5fc5084550aefacd79ed6b68fd8f859e</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a646544a220adb95bc37ff6bad40b533f</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a3f1ae271a5938c29d4b5116104c45a1a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>aad3d5d2a6dd5a7d3c811c267689836ed</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a446329def5e87893a31218536fdbebc1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a5f1d6d16c39512f4373b99aca911c14e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a26b5a8f47eed479a97b3378cbaa3c2b5</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a3a26ed131e1cf6738b365721eb6c3a31</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CumulativeConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>affd5016d492791b7c4e3b3cc7fa331c1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac008ee34e8f3597c831e1b4635bd6a43</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const CumulativeConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>abdfc05846fc09eba657ac359cc3c056c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a5c11d9fcd1b9a18ae690aa71c34269cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIntervalsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>ac6e04da45dcc667e610878f782ec3f20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDemandsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a6895649388baddf2a97b60a3294be82f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCapacityFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a037378ee39d381e18d6380ad7311e95e</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1CumulativeConstraintProto.html</anchorfile>
      <anchor>a6a4b23a149db96745f82f89624196f9c</anchor>
      <arglist>(CumulativeConstraintProto &amp;a, CumulativeConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::DecisionStrategyProto</name>
    <filename>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</filename>
    <member kind="typedef">
      <type>DecisionStrategyProto_AffineTransformation</type>
      <name>AffineTransformation</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a86c0cd58b5bd2ab789e6bfaf4e97bce5</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>VariableSelectionStrategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a4c64edc035542ff6aef6f47211cbf550</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DomainReductionStrategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a5cde9528d5186d24091f5da459f9bdd5</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DecisionStrategyProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a32bf9edadbe7857b200bc8edddfe84a6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~DecisionStrategyProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a8f62a1b4120a911232366ac0f39770e1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DecisionStrategyProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>abf44f1ced4d67ea2cb91f2fdf566d273</anchor>
      <arglist>(const DecisionStrategyProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a76e3c16a78d21b34412985b57171ac38</anchor>
      <arglist>(const DecisionStrategyProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a710d0e4ff26908331f916642b1ef4b02</anchor>
      <arglist>(DecisionStrategyProto *other)</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a993f96447601f9cbbebb6b8851c697ca</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a3b494bc9dfb5ad3ff5fcad476a3ef382</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a41c27f71953534889372f99ce47a56e3</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ac4844c71a78ba1389aee54e8b39ae4b2</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>abcc80090ea541a942f49c31f56c704f4</anchor>
      <arglist>(const DecisionStrategyProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a6c2dbfc80c17d77443844a77801221f7</anchor>
      <arglist>(const DecisionStrategyProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a0d0f9c94f3cd539dc66c97f5bbcb3233</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>afa66852bab4ff2bd2f291925791fcb86</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>affe7a238666024e771ccfaf84e19fd38</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ae5de369220b5bb023198fcc4b3aafc92</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a9911042a6619cdc224ed08785939f1f2</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ab5a1e5067aab871c7cc6fd7f0aa2d9bd</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a94e44281175e85257bdc857f9eb69524</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a9369d733971f70e9173d08d70ae5b1a9</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>variables_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a1f64a7778ecb7422eab78f668443894f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>aa52a5aeeae0f396d22a94f8acfbb05d0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a4aaaa5d8695a9d0e013299a5d521df00</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ab833ea02af51e8dfc759d8139cda9c6b</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a0be21f9fe3ee6280fb456838bb99d6a0</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a70c8eb816c8d6fb1306d907a99733d16</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a36a148fe7b6102c9bb1418bcc8d775a6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>transformations_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a41aff7631befd63e889128d950bb3d5c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_transformations</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ad983c89c32202349e759154d2ace687a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto_AffineTransformation *</type>
      <name>mutable_transformations</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a2f5cf96ec368babcddf2126305546920</anchor>
      <arglist>(int index)</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::DecisionStrategyProto_AffineTransformation &gt; *</type>
      <name>mutable_transformations</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a8a8b7602404b2bb2d19759579e2db903</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::operations_research::sat::DecisionStrategyProto_AffineTransformation &amp;</type>
      <name>transformations</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a498d9dabe4708b44f525195d3380bfb6</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto_AffineTransformation *</type>
      <name>add_transformations</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a0c7fb75bffeee9198040855658bb140d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedPtrField&lt; ::operations_research::sat::DecisionStrategyProto_AffineTransformation &gt; &amp;</type>
      <name>transformations</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a4e81fcf3a0d2688d7f75ef6e1b4ecbe9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_variable_selection_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a5af585c946040df63cbdf1e4a1886e61</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>variable_selection_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a2785e12ded72da3b8e531a30814b5f07</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_variable_selection_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a2d4a1299e0bd08a10ebf0366917f73c8</anchor>
      <arglist>(::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_domain_reduction_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>af14a6633ff76fa169c68e5920561a67f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy</type>
      <name>domain_reduction_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a68c96f139f4f0d2817932c4eac5996a0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_domain_reduction_strategy</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a6a8e209f0514b67a37cb187d528a42fe</anchor>
      <arglist>(::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a330a653504fd83b1f854347437ea1e74</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const DecisionStrategyProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>adbf7220f215c0e12215891da8ba121b0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>af082c198c7b1c76d754e059f9ebae543</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const DecisionStrategyProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ae54cf7d2c00a226de3ffa0d0a53525f4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>VariableSelectionStrategy_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>af74aa29a56afd5bb4039d5b82d221ae6</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>VariableSelectionStrategy_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a06ea5e492ddac4f52085bb1671a8afc0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>VariableSelectionStrategy_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a21aecb659761e692c96a4ea4adc5e32f</anchor>
      <arglist>(VariableSelectionStrategy value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>VariableSelectionStrategy_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>afd73728c190663a8f436f331e603fc9b</anchor>
      <arglist>(const ::std::string &amp;name, VariableSelectionStrategy *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>DomainReductionStrategy_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a3272dfe841f631b8498e4415bdee7370</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>DomainReductionStrategy_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a4e46902d1b2467d16056547d1de55a9f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>DomainReductionStrategy_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a15d31a74ad4c60a52b3a7dee91392e56</anchor>
      <arglist>(DomainReductionStrategy value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>DomainReductionStrategy_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ac960a4909cff78b3e5d35da220d0db00</anchor>
      <arglist>(const ::std::string &amp;name, DomainReductionStrategy *value)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>af6aba7f7dbe7d04ac19fc9d50daa2ae5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableSelectionStrategy</type>
      <name>CHOOSE_FIRST</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a7afb92d202ee0e1953c122f0a9443926</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableSelectionStrategy</type>
      <name>CHOOSE_LOWEST_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ac997cb60c8654c076f7865725778b477</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableSelectionStrategy</type>
      <name>CHOOSE_HIGHEST_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a95a33611fb2c05051d2c4f3e07e28970</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableSelectionStrategy</type>
      <name>CHOOSE_MIN_DOMAIN_SIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a143556cc1a403e35fe2a07da641c25af</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableSelectionStrategy</type>
      <name>CHOOSE_MAX_DOMAIN_SIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a642b56c3730fda6a8143f39f5edea6cd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableSelectionStrategy</type>
      <name>VariableSelectionStrategy_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ab31a7b2dd8658deb5b249a1f455144df</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableSelectionStrategy</type>
      <name>VariableSelectionStrategy_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a553727aa97199f0258705fe53a0238bf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>VariableSelectionStrategy_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>ac2f16b0f2e786318452bc0e2b3bab41b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const DomainReductionStrategy</type>
      <name>SELECT_MIN_VALUE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a4e09ce3aa9616c482f74699eaf5eb2b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const DomainReductionStrategy</type>
      <name>SELECT_MAX_VALUE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a1edc6f84a39ce65a0bdf357987f36b7e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const DomainReductionStrategy</type>
      <name>SELECT_LOWER_HALF</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a2315bf69248382bc6f39afc16c5b8824</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const DomainReductionStrategy</type>
      <name>SELECT_UPPER_HALF</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a9767c6dbc86303846faef44dd8b9bf29</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const DomainReductionStrategy</type>
      <name>DomainReductionStrategy_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>addad991a7c937a33b1fd0e2598c35ee4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const DomainReductionStrategy</type>
      <name>DomainReductionStrategy_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>af13b04b95701563d5725e5579ab825b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>DomainReductionStrategy_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a421bebdebe01b0a17c66273c34a4a17f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVariablesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a004b55bcc264a61c1a2edc2241278518</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTransformationsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a346331ff5f36c6f480f58a9a01592f0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVariableSelectionStrategyFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a44cbb05a441e224a013dd3c1357eb522</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDomainReductionStrategyFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>a58047bbf6614804d5d5fa952196fcc12</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto.html</anchorfile>
      <anchor>af6178b9dcf983043f520ec8bd077b29a</anchor>
      <arglist>(DecisionStrategyProto &amp;a, DecisionStrategyProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::DecisionStrategyProto_AffineTransformation</name>
    <filename>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</filename>
    <member kind="function">
      <type></type>
      <name>DecisionStrategyProto_AffineTransformation</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a7fa49e7b014c702cf48c90d462be9da8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~DecisionStrategyProto_AffineTransformation</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a5984cb216d72d0cc0f6a78a84fca61fb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>DecisionStrategyProto_AffineTransformation</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>aab5b1acef154cd65bc4fa6a5ea4d0506</anchor>
      <arglist>(const DecisionStrategyProto_AffineTransformation &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto_AffineTransformation &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>aea5d25cccdfdf1d280f98e086aad7fad</anchor>
      <arglist>(const DecisionStrategyProto_AffineTransformation &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a78d05592fce785a852a25642c8e442ca</anchor>
      <arglist>(DecisionStrategyProto_AffineTransformation *other)</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto_AffineTransformation *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>af5e5d038691db7c89ef2ceaff91a2603</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>DecisionStrategyProto_AffineTransformation *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>adb550ca0135fd189e16f618f37a85042</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a223dc130d3965201a75209ab012f6640</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a68b9639649903c49911bd85ce7c96590</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a23b43cb20ba95d738932cb6f1e107a97</anchor>
      <arglist>(const DecisionStrategyProto_AffineTransformation &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a44b928d7b1ac65827e93a42f450a267f</anchor>
      <arglist>(const DecisionStrategyProto_AffineTransformation &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ac0aff07aa5dea3578be94a1675a3921a</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a1849e711681cbbc217c9d5b65c04fe50</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ac4084ce9174cb821bd2c754856833042</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a07ca39cab35d0ead52ece73779d40559</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a469e3c984d966012c16cf94bea9b317a</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>aa66e3480e592e48ce6e8dd02314e68dd</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a9dd505f4987383d0a6e07b4062c7b7ea</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a04def99dc038571ef4c0e25ff3322be4</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_offset</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>adb8272b32d7d9c4af52ddbf4a1e20669</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>offset</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ae39aacc07452153596ef17d32184e8b2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_offset</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ab03976ef449a0b3d80d9cb38c138b6fe</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_positive_coeff</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ad66affdb829c9b143457e2226f26a587</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>positive_coeff</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ae95ba4b997bdaa600e038d5dcbaf4646</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_positive_coeff</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a029ae67aeb4b793a334e1760f13130e6</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_var</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>abc55c23a2546a5a045fcce0ea702e9a9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>var</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a63190c693edd4b147374e56bd924a6a0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_var</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a9946da51d64e13193a4ba2db1dae47f9</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a8abc988bbeb85c0493c429e62ee7351a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const DecisionStrategyProto_AffineTransformation &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>aedf80eda26adac66a1f9226933eadf1b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a0f55734005dc5dfcaab338b782de350f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const DecisionStrategyProto_AffineTransformation *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a6250874e5d24e03482b39b3d4c47d28e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a9ba3ed4b809aba64d7da0a176f6d7756</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kOffsetFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a3163b9050e719af1b4a3dea6b1ee429b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPositiveCoeffFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>ab50ea38055b3f291e7a8376248cc0086</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>a97613b1cd1584f0e10a88b1461db2881</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1DecisionStrategyProto__AffineTransformation.html</anchorfile>
      <anchor>aa06405236ef94f8f4ebdc39946746a13</anchor>
      <arglist>(DecisionStrategyProto_AffineTransformation &amp;a, DecisionStrategyProto_AffineTransformation &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::Domain</name>
    <filename>classoperations__research_1_1Domain.html</filename>
    <member kind="function">
      <type></type>
      <name>Domain</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a8026c98d5255e56fc3f224be687a07c3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Domain</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a4604d54b843d603bde4f76ef853464e6</anchor>
      <arglist>(int64 value)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>Domain</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a8c763d670dcbb2656b8b708bf50d7262</anchor>
      <arglist>(int64 left, int64 right)</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; int64 &gt;</type>
      <name>FlattenedIntervals</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a2de0f6b61253050ad242107ce42ba825</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsEmpty</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a7f13c8b45290e4cf8889c4e677b0cd85</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>Size</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>afc9e0537e74ddbb050a001b96f4ca05c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>Min</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a9ed997be60938397604bc2761b0c6775</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>Max</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a23a06dbf9d08cc91d8de1fe86b8bccc9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>Contains</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>aca55d3d10ee99aeab77e457d216d8c02</anchor>
      <arglist>(int64 value) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsIncludedIn</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a770758f5d9d0569a04c3e203cb2ce216</anchor>
      <arglist>(const Domain &amp;domain) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>Complement</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>ac6beb8d0bb66ee165d94623d5a704abd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>Negation</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a4b24ac9e300406590558a82ae378db1e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>IntersectionWith</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a4bf09b90ae38afd5bd3aa87ed5e0dc04</anchor>
      <arglist>(const Domain &amp;domain) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>UnionWith</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>ac33030581b21c1e9fc6ec13a0486c28e</anchor>
      <arglist>(const Domain &amp;domain) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>AdditionWith</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a714a1473bb78dab3195bd5cd5e90af42</anchor>
      <arglist>(const Domain &amp;domain) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>MultiplicationBy</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a0d8ff8a937591b31265b86db6353fa34</anchor>
      <arglist>(int64 coeff, bool *exact=nullptr) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>RelaxIfTooComplex</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>ab06560137156458393e8f44acfd01712</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>ContinuousMultiplicationBy</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a5fa203b31737f4c452a0b04779ce45a8</anchor>
      <arglist>(int64 coeff) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>ContinuousMultiplicationBy</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a4c1fd36870e1dbf2029c462d3fb3d517</anchor>
      <arglist>(const Domain &amp;domain) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>DivisionBy</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a9f653103edc20fa77ecdd7708c76d037</anchor>
      <arglist>(int64 coeff) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>InverseMultiplicationBy</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a3b6727dfefc8413b855926e1c4d27da6</anchor>
      <arglist>(const int64 coeff) const</arglist>
    </member>
    <member kind="function">
      <type>Domain</type>
      <name>SimplifyUsingImpliedDomain</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a0217b50fb7a5dc20f697ef3d0b14ec41</anchor>
      <arglist>(const Domain &amp;implied_domain) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>ToString</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a94b8d91180431ec7bf1073c2a8538f70</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator&lt;</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a27c69d2928356d00ee0624323e116fc8</anchor>
      <arglist>(const Domain &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator==</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>acf15b6795a380eeb7521e1a8e15ee5d9</anchor>
      <arglist>(const Domain &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>afc2fcedb7011ed72d191ca8be79e2ec7</anchor>
      <arglist>(const Domain &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>NumIntervals</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a7c440e0dfafdb9c1299576f17a52b34e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ClosedInterval</type>
      <name>front</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>aede4d7f8e8486355c2eb4d9604ed20db</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ClosedInterval</type>
      <name>back</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>ac61a6df7b21becb7105b149b35992fbb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ClosedInterval</type>
      <name>operator[]</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>ad3bfaa1ea6a1fce5a0c5a8ee61bfca3f</anchor>
      <arglist>(int i) const</arglist>
    </member>
    <member kind="function">
      <type>absl::InlinedVector&lt; ClosedInterval, 1 &gt;::const_iterator</type>
      <name>begin</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>abae404f5f46ad1b7cd495000aec19639</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>absl::InlinedVector&lt; ClosedInterval, 1 &gt;::const_iterator</type>
      <name>end</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a5c5e5505773dda5ca11ff5fddd70ae9b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>std::vector&lt; ClosedInterval &gt;</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a153837b45a0e9ddf5620679f243baa96</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static Domain</type>
      <name>AllValues</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a5d6343e6f8f0356f0270b25e76aa03b2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static Domain</type>
      <name>FromValues</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>afb43df1420e70ff6feda3557a1142dfc</anchor>
      <arglist>(std::vector&lt; int64 &gt; values)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static Domain</type>
      <name>FromIntervals</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a3f44e38ea10baff35e9d8f210e5900ed</anchor>
      <arglist>(absl::Span&lt; const ClosedInterval &gt; intervals)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static Domain</type>
      <name>FromVectorIntervals</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a5840372e81d2500f8aeb77d880d22bc6</anchor>
      <arglist>(const std::vector&lt; std::vector&lt; int64 &gt; &gt; &amp;intervals)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static Domain</type>
      <name>FromFlatIntervals</name>
      <anchorfile>classoperations__research_1_1Domain.html</anchorfile>
      <anchor>a130878d13a4dd79365b1ecfa171b4714</anchor>
      <arglist>(const std::vector&lt; int64 &gt; &amp;flat_intervals)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::ElementConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1ElementConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>ElementConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a631cec815893f790c6753ba674a06239</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~ElementConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a2e17b6142b53eee4772b947208d04c9e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ElementConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a55ff85a05735674a84182418dd7abd7a</anchor>
      <arglist>(const ElementConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>ElementConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>aa2ca4dac3acc0a8ef884dff558557a29</anchor>
      <arglist>(const ElementConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a546c40d2e499ceb7955feeaf990ef90e</anchor>
      <arglist>(ElementConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>ElementConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a3deca8acab0095581d819368aae04248</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>ElementConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a759a3f69b0754034975105a16063e047</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a9c014184068264a1c55be99de09d4651</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a4169d794ab045ff25ddb1147e7812b79</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a813fe93764e7687cad537ffe00b90138</anchor>
      <arglist>(const ElementConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a24bdeb0801d2591b9283351e2784acaa</anchor>
      <arglist>(const ElementConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a4bd069fa505e10e875625677d372f0b5</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a3e6e7788addc352b1018c7c2713f5e5a</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a45e86c58ebe5c6628f0e2bbdc8c34ddc</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a6c26da3253d27069a6ba5c07aac7cee2</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a5ab887f8e95475e4f3e8275c3e126ebe</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>af51600c6d50a29b7001015858f0d0a16</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a56d8fc24f19c4d6d8e6a0dc99284c5e4</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>adfb1e1f941c82e3550fc43d72e9c1216</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>vars_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a21632504d4c2b5c87237ce3c6590b609</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a269eff06a9821a1f44338f3f2b80f842</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>abe4127e35dfcd544b7dcc931e4e88660</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a876c375489c917850b5e76e23e373c7b</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a46c2fea6c935fd20d8a2f3d9de11a6e7</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>ae39f0d776d112940fdaf596b30bd3b34</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a53e4c4c9b7495247955bcbab46eb873c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_index</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>ae9e8d14347bf8c2a5a7b9d0b2c66504b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>index</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>aca1a47927851ab518313e1a6072760eb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_index</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a77b626d7f7d907976c99d28a74285af7</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a6a7c4c7bf8c071597ed13c253233fee4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>target</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a5c79758d73f37756e372d564111ae4cf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>aa9ee8af8d0dfb4299f94b7a62a8100c1</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>add9d068973de2c1bf6b3708ec39e55ad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ElementConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>af6c3039b69da2798b17dd5f1968f62c1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a933c1e24a90eea57e5bd29fe4edaaaa5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ElementConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>ad41dc02d5aeab347ca57e32caed5d7c3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a110fccac183697993275cd7ab2816888</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a8febb9ad1c6a5cc5f1d0119fba3a4114</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIndexFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>ab4013f0edc3b9fe2c941a622b632b97f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTargetFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a7cd85d7ba41be706936fd7a4884703ea</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ElementConstraintProto.html</anchorfile>
      <anchor>a6925dbe53f54f70dce4ee62ab187e907</anchor>
      <arglist>(ElementConstraintProto &amp;a, ElementConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::IntegerArgumentProto</name>
    <filename>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</filename>
    <member kind="function">
      <type></type>
      <name>IntegerArgumentProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a89b5ebd8abdd0a5d981444799b03ce20</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~IntegerArgumentProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>aeb09033c4c23063a3236115ceb62b7b7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IntegerArgumentProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a47a7dbc5bb4df16ddaa6b46e5bfc3c85</anchor>
      <arglist>(const IntegerArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>IntegerArgumentProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a229cb29935b0f965cb141e4bb8205c8d</anchor>
      <arglist>(const IntegerArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a6ddcbb7d1fee25398fea86075b788ba7</anchor>
      <arglist>(IntegerArgumentProto *other)</arglist>
    </member>
    <member kind="function">
      <type>IntegerArgumentProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a264cfb0e35fa39f399e1843008d74d24</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>IntegerArgumentProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a76ca6999090bf52a1db53d006e802507</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a74e341eb9426f7c62ebbf138ca20e6c7</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>aeef522ce8ceb7ff532b0f16d6932d9ba</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a1c03778ccd431ac4442e2b6c9d819dfa</anchor>
      <arglist>(const IntegerArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>af87ad325ba4e1482c20cf99423b73889</anchor>
      <arglist>(const IntegerArgumentProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a4a4d81402251a95ad6f245a942c67510</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ab37c43782161cae24433530ddb6e1147</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a9e745ef33fd4232b6eb54166d238a9f1</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a85bcf08d0fe22d2958f4d8831a3d5fe7</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ab28d2c3ae2073a1924d5c3fb73e41631</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ac3ff8fcbe2dbfb6df7171f2b54aabc66</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a5dc3a40dc56da6219825d385d3fef126</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a2f3c1b6efd6a3be7eb83f20967e59161</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>vars_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a987d3ddf0c5960bb841053f5ded1c382</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>afc94662ced7da530a66864f8cdc453dc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a1a3f2e28db44d5ca4d6aafcd49d3cc23</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a682692d89dec3e3f389bd6647948bfcc</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ac063816c65abd07aa7e4032be313e41b</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a34eede096385cba135ab1366ea33f611</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a89f8a595403a84c32bf33b084e6ddbab</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a0e8d6e038cab213caf8b638259dbbf43</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>target</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>af319ee6e9b868be040000bc1949fae73</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>ad57b8078e97eede9d3d141236c461a00</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a3011bb913f22bd13d846b669e377b5e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const IntegerArgumentProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a4555bb08b03d362709c06afceecf9b72</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a0f3e921010da1541aa6170f2ca1461ce</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const IntegerArgumentProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>af673a3abb65dc5abe8b49d4bf2b83a49</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a88e93551d624f3fc4eee441fe0d21883</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a4411ff6c08f72fe1bce1e74ea0dd15b1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTargetFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>adf1c6910e32cf68a6cd8d7a6e98ef5d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerArgumentProto.html</anchorfile>
      <anchor>a7c5414a3ac06608f669faad83493c347</anchor>
      <arglist>(IntegerArgumentProto &amp;a, IntegerArgumentProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::IntegerVariableProto</name>
    <filename>classoperations__research_1_1sat_1_1IntegerVariableProto.html</filename>
    <member kind="function">
      <type></type>
      <name>IntegerVariableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a1ad794b118e3d66c349a2d0eb057f138</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~IntegerVariableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a64bd210f6d48b604b77a262ae49b602e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IntegerVariableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a5d8475bb303bde7fd2460ad41d35cc1b</anchor>
      <arglist>(const IntegerVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>IntegerVariableProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ab076d7e334e142ce3357cedc15798eaf</anchor>
      <arglist>(const IntegerVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ac8d061362d12b56ff220e9d9fc57295b</anchor>
      <arglist>(IntegerVariableProto *other)</arglist>
    </member>
    <member kind="function">
      <type>IntegerVariableProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a336b5d0fd409eec9f72b7947c8d5b1cd</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>IntegerVariableProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>acdc5f2ad592f40617d2f84ceb5be569f</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>afcf6300004357f465c6e6315d01967f7</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a7ca0cf1946f0f1dcf6b1517aad039799</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ae2fd71480599fac8358a31fb651ee5ee</anchor>
      <arglist>(const IntegerVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a3b3baee36a5027ba27b2b569b29b7143</anchor>
      <arglist>(const IntegerVariableProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a6172284f67e8a51e31226f50069e5d69</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a9a4b14264828c2fb51573d8763a62638</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a0c6e241e79bee882467b080f997ad0b7</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a2d8ae61a69b08ce0186846047d975b43</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a0a5581f80e905856028fc85b2c0cbd5e</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a123e9f5b2d3cf3ece40031783258db6d</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a45443494264347c5930f0b39c86dbdc0</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a561f827883d918424566ce05bcf2d5ca</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>domain_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a40b4a0b7f404f81300a8352b8695df3e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a58a35012b1533d941280131768911de3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ac203aeb218d4d5a6d2026ab68a790c54</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ad79f95d0310e4137558b94dcb1741df2</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a6a5111609ed5868ec8e9e94ea3b97112</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a6d81b77d35652f06d0472ef4528c41e9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>af26837227cf0258ec2a5870f2601972d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a8b9b59675a969b5bb475a2d5a40941e8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>abd07f5dd77698e902a520047a90a0492</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a81fc369d0e0fcc136fb25e7be2e4ee01</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>aba387597c11c8baf616378e0262c40e5</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a0f22a1fe9d4f8faa79aa655370e3f683</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>mutable_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a5e5e4fd6b4fbf6677cbc2005166ce610</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ac78972e516dc09a05ff3e418f19cc9bf</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a5f683aaca013088f86b74be3880f71cb</anchor>
      <arglist>(::std::string *name)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a0e1b5f12ed881beafa471d7005c6beb0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const IntegerVariableProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a50345f852d51f94122e19bb93c4d6b89</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>aa78c65ab03a4a2d67d5ee1f2314dec3e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const IntegerVariableProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a4df628b8bc0660695c13d6941de61332</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a8499966ee2513b45d9679c755acaa922</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDomainFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>ae00b4b317c18cb2cf4a01f93af7791e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNameFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a1f7faafa7f13e865c8f4d3e8a230d4d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntegerVariableProto.html</anchorfile>
      <anchor>a9d8670e9216e8e15b77c504761de6af4</anchor>
      <arglist>(IntegerVariableProto &amp;a, IntegerVariableProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>operations_research::SortedDisjointIntervalList::IntervalComparator</name>
    <filename>structoperations__research_1_1SortedDisjointIntervalList_1_1IntervalComparator.html</filename>
    <member kind="function">
      <type>bool</type>
      <name>operator()</name>
      <anchorfile>structoperations__research_1_1SortedDisjointIntervalList_1_1IntervalComparator.html</anchorfile>
      <anchor>ab348be2102e6d84cf3d994dac9afd657</anchor>
      <arglist>(const ClosedInterval &amp;a, const ClosedInterval &amp;b) const</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::IntervalConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>IntervalConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a82bbafa809815efaddf785284939f01d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~IntervalConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a3860d2a92b34f75e8ca10f754b0e400b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IntervalConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>aaa7e2916614116deeee0a4b5556a02cf</anchor>
      <arglist>(const IntervalConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>IntervalConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a512114cdcc8ed5ad5b2c92c06feacca8</anchor>
      <arglist>(const IntervalConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a2dd3021930c090e887def9771011f477</anchor>
      <arglist>(IntervalConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>IntervalConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>aa4d83dc7e7995cd257f54c738a243ef3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>IntervalConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>acbd8dcd7118281c66a7f94df85fde083</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a3f218b44b69f869446c3465b3b78ef0c</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>aba2137e936d77191e3283e936baebeb5</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a94dad3797ac5c9bc499523d1a4f8986e</anchor>
      <arglist>(const IntervalConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a5b063e411e7507db953f59667ae63c56</anchor>
      <arglist>(const IntervalConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ad4e8ccf02542a24d5c33ecd249068d72</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a851e04b3a19c42de40f5a89a6a678c16</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>aa8ccb5585943e262339c737809abc4f1</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a4675a33cac1bc4fe78a251a284630862</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ab8290a970a9967efaa5ec2797c70a40b</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a2900e8b555936ca6a05c4e37e2a9215b</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>aa454d8b5e115eae06da9654f2e21fff7</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a1c916ba369039e6c78d13ab14c5772d9</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_start</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a40d7a74197dedf7af11d23b63d711590</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>start</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a78d1b1dc724b19f85af0bc314c711a9f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_start</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a5ae68d04d4dbce1475944b1578be3519</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_end</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a78d30b34b538515e18369d5e0a1d268a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>end</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ae7a98f61a1c3ff325ef0372076cf0143</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_end</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ab34c04000082a5f0c8cff30dd95494f6</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ab47d61370dcce69cc0cbeb1609410165</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>size</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a90a8e4c6bec3046aa21724629b82b143</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>af7e414b30614b4b3dff461e4c58ffa1a</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a6035b88733f83c4218a8e355cb85d8eb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const IntervalConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a3dce18fbb3d5444e3dd3c50b53e55224</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ada8a58157226d27d22a2da2996e0a398</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const IntervalConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a8cc94d9f884f97a13c1d2a3cc51795e6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a10acf28f717f46698db8c61f6a067468</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kStartFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a7e943695dcb37a762241567cd4eb74d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kEndFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>a20142e7cf494b41ddba5c9625bb7a08c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSizeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ad4ce0a19246e4f29943ece3ca17d69a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalConstraintProto.html</anchorfile>
      <anchor>ae70e4641ca0bdfade09b12fb784dccff</anchor>
      <arglist>(IntervalConstraintProto &amp;a, IntervalConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::IntervalVar</name>
    <filename>classoperations__research_1_1sat_1_1IntervalVar.html</filename>
    <member kind="function">
      <type></type>
      <name>IntervalVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a106c293c6b0cac8589bc6b5b4ff0446c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>IntervalVar</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a1b7333dffeb56f1cffe35973cab19dd1</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a48f98ff3c12aecf540170647a72ce860</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntVar</type>
      <name>StartVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a6228ce653636516ab2b2f760aa61a57e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntVar</type>
      <name>SizeVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a9decc39f3f2079f78cdebd974972bc0f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntVar</type>
      <name>EndVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>ac86513192443e57e505b8e8c9ffb77f2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>BoolVar</type>
      <name>PresenceBoolVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>aa5cc77b54d51bda6a6c8e30907b9a917</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator==</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a34a66e31983270cb695c271d0b869ab3</anchor>
      <arglist>(const IntervalVar &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a6816a7260a80aa691b7cc1e748323d21</anchor>
      <arglist>(const IntervalVar &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>af90bf96ccc72778be5ebd9668e10d842</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const IntervalConstraintProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a34c3fc0d93697326a7e398cd45b1374d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntervalConstraintProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a4d10907c6da83ee20c29312f1064361f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>index</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>ac591e644d995d2520e859ee639695754</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CumulativeConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>a9d31ad87d4edee55fc3cb5e239077720</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>NoOverlap2DConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>abdbbe5d06195ef1dc4c30ad25b9017ac</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntervalVar.html</anchorfile>
      <anchor>ad73e372cd9d1def69624f85777393123</anchor>
      <arglist>(std::ostream &amp;os, const IntervalVar &amp;var)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::IntVar</name>
    <filename>classoperations__research_1_1sat_1_1IntVar.html</filename>
    <member kind="function">
      <type></type>
      <name>IntVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a47cdd55b99ca5d29b194f54b14889819</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>IntVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a2fdcfe89481f7f3e4cce763459764d78</anchor>
      <arglist>(const BoolVar &amp;var)</arglist>
    </member>
    <member kind="function">
      <type>IntVar</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a5f1761c6d2c5f7908f5f92bb16b91de9</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>aa8460c813c17ec5b7a137739c448bb98</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator==</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a23d836e740ab297549905c5fa8539ba5</anchor>
      <arglist>(const IntVar &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>operator!=</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a824aadc0688ab57929ae744b1f1a7a26</anchor>
      <arglist>(const IntVar &amp;other) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>aac78f1c00b73fbad7bd6577181f537fb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const IntegerVariableProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a426492195e6cdd88354def292ffa112f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>IntegerVariableProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a68349a30f6936d8f5a3d00d342ec5f3a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>index</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>ade91cda36a02fffbd115f1ec65746af1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CumulativeConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a9d31ad87d4edee55fc3cb5e239077720</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>LinearExpr</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a7678a938bf60a5c17fb47cf58995db0c</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>IntervalVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>afc7f9983234a41167299a74f07ec6622</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>ReservoirConstraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>ae0ff478f6506cb705bbc1737598276f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend int64</type>
      <name>SolutionIntegerValue</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a64bd6fadf44a9840c837cc701b2b9043</anchor>
      <arglist>(const CpSolverResponse &amp;r, const LinearExpr &amp;expr)</arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend int64</type>
      <name>SolutionIntegerMin</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a8ec929aea42c9e50e2f1daf56525e379</anchor>
      <arglist>(const CpSolverResponse &amp;r, IntVar x)</arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend int64</type>
      <name>SolutionIntegerMax</name>
      <anchorfile>classoperations__research_1_1sat_1_1IntVar.html</anchorfile>
      <anchor>a79061f94ca7a97d0616f8b270358c771</anchor>
      <arglist>(const CpSolverResponse &amp;r, IntVar x)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::InverseConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1InverseConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>InverseConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ab7d828a78e4d21daf17fcf6e98d824e7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~InverseConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a6ed47b0136919d6b8f0ccc6db0662b88</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>InverseConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a0a02bcb13639cdedc21734ddce306721</anchor>
      <arglist>(const InverseConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>InverseConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a3c836696a5468e2fae84c8e227997719</anchor>
      <arglist>(const InverseConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a421f809860ebd2e28f2e864b2951e06a</anchor>
      <arglist>(InverseConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>InverseConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>aeaa8d077b7635d3823fbfda86e6e57b0</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>InverseConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a3d286c359ae5f9d363525347b696264f</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a4ae985803f8ec8e2d3413f1929811d09</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ae2aa02fea74fade45bf32abc7b90e534</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a6f55549ebc9adcdc5e0dd763662aa4db</anchor>
      <arglist>(const InverseConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a71a6601a61d293034da8195f8e93814c</anchor>
      <arglist>(const InverseConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>aed01494d682b2b1d3015cc852d172e12</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>abbc1edcee82145402d9e10911b478d13</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a4ac325430c499d2cf1953ea464f79c07</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a836b638b2f4ae43a5039081357f79106</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a18b0a1eb0dfa05523c4cd7630a36094b</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a3fe245041d484a22a8e9fde8b1815667</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ad8afc40bf050a234c043de0ca8b286a3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>afb33eb21ec05e18d35cc7c9e06cb6278</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>f_direct_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a1823ca067b7bfce79a9e6e66d8e27360</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a0fcf67dc19f8818ad8527cbe68018258</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a17851fed0bc9677d5ae52d4988fa4e0b</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>af130f4b550988b7dfd18635bc57e8b6e</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>aab56f0400d82e12b68f0b7a2551a5b94</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ac130e23a053a7d45bb8f12c5eac51f6a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_f_direct</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a03ff372c76c76ea3ac043bc6151fdfdd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>f_inverse_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a65e0def917909eb24602b82e39576994</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a78af06f99ada7de6b94e79f975ec0577</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>aa243449e607387892fa83f1eed645aab</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a87c52075d7dc27ae402f71bc97117460</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a26393486a91d83ad6d2c167cd027b139</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ae37f0c34a86cf741682454fc88d6d1d7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_f_inverse</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a0b57a04347b29e484f1064ee2c7e9a67</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a001b76a0d0bde4d748267318c4e9cf3b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const InverseConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a0cbe721c733514011934b36993967a4e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a342fc48632f11772ecca5729c485286b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const InverseConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a324502773047121717185ff1c366e45e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a391763debdbec5e02fd3453ab0069082</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kFDirectFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a5741e971804b802a8066ba77a33f2c8e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kFInverseFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>ab0597e9cffe28f5bbda69518082774c0</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1InverseConstraintProto.html</anchorfile>
      <anchor>a3d2e4c9a5495ee646ed491c114f81529</anchor>
      <arglist>(InverseConstraintProto &amp;a, InverseConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::CpSolverStatus &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1CpSolverStatus_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1DecisionStrat411f5031253253c57f3c5f378dfc1bf0.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1DecisionStrat9163b6fc058f1feefc5796666205cadb.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_BinaryMinizationAlgorithm &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParametersb465c243b8d7fa2fa0ca5cc28bee453c.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseOrdering &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__ClauseOrdering_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseProtection &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__ClauseProtection_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters18e480b3e82e25979893dc86f9997a3d.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatAssumptionOrder &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__MaxSatAssumptionOrder_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters66ef3c834a5336a1d6bbbf0bf3479a8a.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_Polarity &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__Polarity_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_RestartAlgorithm &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__RestartAlgorithm_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_SearchBranching &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__SearchBranching_01_4.html</filename>
  </compound>
  <compound kind="struct">
    <name>google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_VariableOrder &gt;</name>
    <filename>structgoogle_1_1protobuf_1_1is__proto__enum_3_01_1_1operations__research_1_1sat_1_1SatParameters__VariableOrder_01_4.html</filename>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::LinearConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1LinearConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>LinearConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a6aa278f389f1ff7352951759cb35e9f7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~LinearConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a8f372b8f76be749f79febb9c3efe2e9c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>LinearConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>af0ba80b16019522fc72df842bf793486</anchor>
      <arglist>(const LinearConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>LinearConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a9c8f76e5cbd2626626c02fc2cc95ee93</anchor>
      <arglist>(const LinearConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a542f5e8ae5d4f6497dae61eec6526a83</anchor>
      <arglist>(LinearConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>LinearConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aabb7e6500c398c4768d3bdbf72fdaf78</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>LinearConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ab41b4e7d447883313ad3cf79a7bf1333</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ae902ec0b85ad20e45f1d1aca587154af</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a04d16ca877e9d32d2b308535ab194ad4</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a9b6b120d04037212c10d65f1c165eac2</anchor>
      <arglist>(const LinearConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>afb1c98d55bcdeed480ec0890b5f30946</anchor>
      <arglist>(const LinearConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a7a1a23133926471a14e931fbe81e3433</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ab0572f27f07a8e6fc86e1e0e17736e27</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a99f98832afd422c959bdc222a0ed1c4d</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a59345ee7a0f4fb46cfea81f5706a44c9</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a26d94e1077abb1d225dc1cff79115828</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a1f72815b3f0c64cdeacf2b6588de27ca</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a0d290df52c40d482c2d0f9aa84761980</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a9dd65cbbc0e23fe45e0b56b07ba41a97</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>vars_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a68b05913498bab89ba6e13474c71901b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a6a416958361de15588476ed10b875e4d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ad5a96be4b085b8079f88acc078c1206e</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a48153b0908ab52ef751af444a8bb21f2</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aa32001e1d34909075d470e6c340f23f9</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a9b89a74a9285096815daaae3cb88cef1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a0cd04adfd0040fd66dad694659a5e8e4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>coeffs_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a8feeb0891af5e423e4db0a0a600f9a30</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a7304f5884dd32bf6477aaa3df31db010</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a59420f1aa75b90ae1ebbfbece5a62cc6</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>acfe7f207f1c13152768aa22c336ccafa</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a16ded2172bd8e0eb24b8d6ddec8ed509</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>adffffa27012872d39a6a849846c1a967</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_coeffs</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aec1b4c510553db2ba1298b3861349a89</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>domain_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>add30ee0c22588d8ae37828bf09af8f0b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a55c162a3077e0af1ee778a4b052af1cc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a6b59fe1edcaae5ee12ff2ad388406703</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a5ece7ea8553ce7f2ca44f2a1887a9d41</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aea11befb2a8995c54d9f9aec3e0ddd1f</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aac55042e1e2d3c7ba00701f7db1b5bad</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_domain</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a4ece75ec9fd0878e6da9bab1d74ddaad</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a10c127f451771e37f5aa0cf185d09c34</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const LinearConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a0aa021a204830124c46e1f7057dff2d9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ad30d392cf1e85346e00c567f5e3b3925</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const LinearConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a58c19ef752f3bf6d7e6808eafd958f10</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>aeac61c8b2838f8f6b0ce023139d2c4ae</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a0b620fd97b2605f6306fadefe54e5aa7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCoeffsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ab1282b6f5ecf6f68d384694966264e4c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDomainFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>ad5430f9ab23f7a653a862667cdafb3f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearConstraintProto.html</anchorfile>
      <anchor>a88e40540b7363ae519958485bef87b7e</anchor>
      <arglist>(LinearConstraintProto &amp;a, LinearConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::LinearExpr</name>
    <filename>classoperations__research_1_1sat_1_1LinearExpr.html</filename>
    <member kind="function">
      <type></type>
      <name>LinearExpr</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>ab33a810593c0a9f585133edcb22deb55</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>LinearExpr</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>aa9dc77d3e71adbebbebbaec9df2890e7</anchor>
      <arglist>(BoolVar var)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>LinearExpr</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>abce3810df8307fd1d04f49b36a2e6693</anchor>
      <arglist>(IntVar var)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>LinearExpr</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>ab2654673474a3bdd47ddf4be52892292</anchor>
      <arglist>(int64 constant)</arglist>
    </member>
    <member kind="function">
      <type>LinearExpr &amp;</type>
      <name>AddConstant</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>abed3c016b025d92058b1c29ddeef9341</anchor>
      <arglist>(int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddVar</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>afb9c31fb1176a9ba22d4b82fa285a5c7</anchor>
      <arglist>(IntVar var)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddTerm</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>aa8bfd52517f0e1ca2a9adef474f1ff0c</anchor>
      <arglist>(IntVar var, int64 coeff)</arglist>
    </member>
    <member kind="function">
      <type>const std::vector&lt; IntVar &gt; &amp;</type>
      <name>variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>af5805ba35a6efa9460c5d8eab8301172</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const std::vector&lt; int64 &gt; &amp;</type>
      <name>coefficients</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>a6f0e8040bcb0ee633efd0862c660cbf4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>constant</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>a050775df6d69660af8f78d577fd327cc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static LinearExpr</type>
      <name>Sum</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>a74026647307b38916135e8c3dad3421f</anchor>
      <arglist>(absl::Span&lt; const IntVar &gt; vars)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static LinearExpr</type>
      <name>ScalProd</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>a3b49fe9924ad61a609f65f4a7bc4c861</anchor>
      <arglist>(absl::Span&lt; const IntVar &gt; vars, absl::Span&lt; const int64 &gt; coeffs)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static LinearExpr</type>
      <name>BooleanSum</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>a0a6ff6ac94b7e556ff06df6f8211182f</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; vars)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static LinearExpr</type>
      <name>BooleanScalProd</name>
      <anchorfile>classoperations__research_1_1sat_1_1LinearExpr.html</anchorfile>
      <anchor>ae62c3c0da3b623e5e43530c08f7bf379</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; vars, absl::Span&lt; const int64 &gt; coeffs)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::Model</name>
    <filename>classoperations__research_1_1sat_1_1Model.html</filename>
    <member kind="function">
      <type></type>
      <name>Model</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>a7d97534275c629f2917ed5a029e2e2c5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>T</type>
      <name>Add</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>a059b9d223761f2b9cc82df4871ae36fa</anchor>
      <arglist>(std::function&lt; T(Model *)&gt; f)</arglist>
    </member>
    <member kind="function">
      <type>T</type>
      <name>Get</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>a162bdd37a619fd04f9085005008951d9</anchor>
      <arglist>(std::function&lt; T(const Model &amp;)&gt; f) const</arglist>
    </member>
    <member kind="function">
      <type>T *</type>
      <name>GetOrCreate</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>af4eb422b7cfd963c58d65b18b4e47717</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const T *</type>
      <name>Get</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>a211cf867e9edd220616c0a8f6ba4b71d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>T *</type>
      <name>Mutable</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>a6226de9875c58b81f461c123577d1689</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>TakeOwnership</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>a6411a892fd57781615c9edf80081026c</anchor>
      <arglist>(T *t)</arglist>
    </member>
    <member kind="function">
      <type>T *</type>
      <name>Create</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>a08f2ee3dc9fa03be18a4c38304e068d9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Register</name>
      <anchorfile>classoperations__research_1_1sat_1_1Model.html</anchorfile>
      <anchor>a78f476ca154e64d281ae07efd825a779</anchor>
      <arglist>(T *non_owned_class)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::NoOverlap2DConstraint</name>
    <filename>classoperations__research_1_1sat_1_1NoOverlap2DConstraint.html</filename>
    <base>operations_research::sat::Constraint</base>
    <member kind="function">
      <type>void</type>
      <name>AddRectangle</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraint.html</anchorfile>
      <anchor>a7e76dae6971e2f38651b7eb8411ebe63</anchor>
      <arglist>(IntervalVar x_coordinate, IntervalVar y_coordinate)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9052e9e1dd8248433909b5542f314add</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>ab6457905c9a8cd1c5f92738d57e6f298</anchor>
      <arglist>(BoolVar literal)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9401ab195650160402df5b61f8ac9bda</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aeaf30f4ee7d141e68905f1ac2432b937</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ConstraintProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aa0b277df64333f670b66c8d5295b8250</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>acaa17b2fbfd62f6845329ae944835654</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ConstraintProto *</type>
      <name>proto_</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9d74c3d77f601020ab87700745f830ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraint.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::NoOverlap2DConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>NoOverlap2DConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ae79da90e540613ae91251219a7be385a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~NoOverlap2DConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a274e12a01a253d559bbc6dbb999bd1d6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>NoOverlap2DConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a74419288557fa7c4af86a8e14390c183</anchor>
      <arglist>(const NoOverlap2DConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>NoOverlap2DConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ad88c52d26f57a52b144b97ba00b6a2a5</anchor>
      <arglist>(const NoOverlap2DConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a6ea6a527e48c326cade52a10d83fb33c</anchor>
      <arglist>(NoOverlap2DConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>NoOverlap2DConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>adc19a13803cd640dc4a091b9903c417f</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>NoOverlap2DConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ad7a049528458b3b9c0455b27de6c024d</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a7897fe7bac6dbe40208fec8325ae3cf3</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a43acbe1879cfdb4b722f16c68bb263ba</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>aabd798be7248cbd1d0016d407447be8f</anchor>
      <arglist>(const NoOverlap2DConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a2fc360b1a6b5542702db46148016a070</anchor>
      <arglist>(const NoOverlap2DConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a91b73889113ab1d64da4836756a03fde</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a62de9cfe75c8023815395877c0ee5123</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a0148d7428af7402e50e00956c9d0c8ee</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a1c19ae90b0599197513636fb370b248e</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ac78acd732cc4e84afb030b521a43440a</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a2a37fb5869f6a4525bd40fa3fb9f3962</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a465b73ed4018e8283a261711fa8e580b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a81d7728125f4b346abfe4d6100a5cb09</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>x_intervals_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ac860c1cad219b0ff79c5fb4f0e8ce80e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a0aae2e04181e8167d4a4aa6253aac4d2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>aa323713564d4ce5445e52ded60280fac</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a8ec0a47cb3dc9d0fe7e79e85a1920ec3</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>aa3c5873d02689551bd0fce71e2a6da68</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a7b757d2993665d27d387826bb3183dc3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_x_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a516e0a79d0644a258d56e08e1702a74d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>y_intervals_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a1c15fb954fc95592cbe2e7fd7dd2aec9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a201476b5aa7e694f7402bd78ec0e0497</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a989843a92948d851842487a176f29e64</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>aba72a8b560eac9f7ff8bdf3db9072184</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a8a18ca5d782eed72f077a109a68c2ae1</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a3239a5b449447ea3d4c20409ac3e8f55</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_y_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>acdeafb6ab842902069739409c4a6ad53</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a8f4236e14af67c64c5fe6629eebda8d1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const NoOverlap2DConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ab2efc66c37a80c1b22ed751ce438536c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a184e14bf889fad6ec203f2953b1d22b9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const NoOverlap2DConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ada59ab4d6bf176f3f229437cb926d218</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a099870133e8d4107a98e53cfbfd0576a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kXIntervalsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>a51b8f56bf916d208488ad933cd74463d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kYIntervalsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>ae6eec5b5ce752cf2544d0bcb11c9420d</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlap2DConstraintProto.html</anchorfile>
      <anchor>aabd9aa50228fae717e9aabf279e070e5</anchor>
      <arglist>(NoOverlap2DConstraintProto &amp;a, NoOverlap2DConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::NoOverlapConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>NoOverlapConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a3cb7f1013994d7e198fc73a6eb18f898</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~NoOverlapConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>abd05e8027e974dcc60ee79c8b8b31a86</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>NoOverlapConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ae93b6510609ddfc26ecccbca53a49f8d</anchor>
      <arglist>(const NoOverlapConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>NoOverlapConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a933306ebe007ad7a65f1de3a7573f65c</anchor>
      <arglist>(const NoOverlapConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>adff384da8f24c37e8fa24b70d0181090</anchor>
      <arglist>(NoOverlapConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>NoOverlapConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ab34751ce8c1acf5ba28cd3fed14cff49</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>NoOverlapConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ae4ab756015c6fe91b28bef8cb564875e</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ac408f8f251dbec2cd66fd01f544f090f</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a82fff3fe9cccd939c37191281fff59d0</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a78e6245d980965c64866247e4937b6e4</anchor>
      <arglist>(const NoOverlapConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ac8d3732f525ad6dd68eee94641db16a4</anchor>
      <arglist>(const NoOverlapConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a7bf7fbb9deaae728708ac4c118b151f6</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a742e2a11500ccfa545610dba11b0c92f</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a00ce85466f96ddbd0403676fe309cee5</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>aa833dd34de2c1675e2f543dda02f13a0</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a3ead9492b767534c84dff72f653f3157</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ab6f003d4d36be3a8c1b1db9e8ad5b093</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a50e6bcc2746b0bf6b477768041e75433</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>aed7e1998bfdce75439bf491bfd478ca3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>intervals_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a1ac25b6f425989928b67c89b13812fc8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a1f74ac7a87587704ed1e311662304493</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ac84f031376543b28989cb29efaf9147a</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>aa0b4dd1b79bc252098a014fc35c70687</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a31a037f85fee9f86f38ed200709a20ad</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>afd16d8f988f54ec74c7d364b6f41ccf6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_intervals</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>aa96e2a817b536b884b555dbd2de5ef20</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a4ed7e38a09fa28cb33aea04f0063eb2d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const NoOverlapConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>abd24fbb1eede6d3471863e1d1cf4f364</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>af6786bf2e1cabef7f5d5baf7594c1fc0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const NoOverlapConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ab266135920a8bbcc22ca11b3cdd16a41</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ae3cb9ef14ad154247e09d8b38543665c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kIntervalsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>ac21fdddf5a859ef216febf27ac926c2f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1NoOverlapConstraintProto.html</anchorfile>
      <anchor>a5ab1e2486c7f1264ac6e899a734c70ba</anchor>
      <arglist>(NoOverlapConstraintProto &amp;a, NoOverlapConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::PartialVariableAssignment</name>
    <filename>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</filename>
    <member kind="function">
      <type></type>
      <name>PartialVariableAssignment</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a4a430a3f6329d617d044ef61ebe62a26</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~PartialVariableAssignment</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a8a65f74b9f4b7c4165ddbdf41a6b63d7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>PartialVariableAssignment</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a9088222d4aa6200aa28e3ddc4c1232b9</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a3822c2fde39cab2adc595da8c1b2f45f</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad023f7ee2c2798f1491f399609f4edfb</anchor>
      <arglist>(PartialVariableAssignment *other)</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>afdf31ddc59f13c39f52d0fd754d6b391</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>PartialVariableAssignment *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a31739266b4221c58a2dc368145429912</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a1ff4b3c9103559f9ed69404ae856455a</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a00e35fd12d4b2ab4591b5931411c2f7d</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>afd9125bbfc9da8c7c42db5f6c271f84a</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a4958d02b419507028b7c2e7d71a84e49</anchor>
      <arglist>(const PartialVariableAssignment &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a9b0373039a407f0d38780be3fffcdccd</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a49a58a52cbc1de932e7b436d8483a285</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ab10be08be206f7aa13bb04dc3673150c</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ace99e96915e11ded3b0ae3a0c771ac78</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>aafb238a4b36d251ba8ff92bf923c3142</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a62695a07a62febcf46e9422602cb2806</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ad7cdd1c7cf1a05dc5600ec22f8b284c5</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a416f12a73fef0d9366506138d6637628</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>vars_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a3b080dacafdc9c5e8859d576cb7ce05b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a4e6c2edf140237d587b97681b6e07f70</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a89b52919f7dd874129a967be48033812</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a23b5abbc7f688acbdf704b02da0e88dc</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a4c3a9db1e8c4cf4af54ae5ea107aa360</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a090c69ab85bb86f553cfc464e1c60184</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a6fdb6116aeb56b2c1ab15d659e875407</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>values_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a8d6fca42b6ea4558e41766f427fd632c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a2e7ba61a72bc28ec69a3be7a3f84f169</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a99fa87887ea0a77c7ab403deb82ff3ed</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ae41776bb85571069aa5d878b0229cb57</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ab6137cdf2d56c02368f0e391290f0a40</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af974a112432d21c7de6764d7c54ba706</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a72d9beefc5c5592d84e1ca1280329210</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a9a1f68ba47740bd8a790c9a8b7e923ee</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const PartialVariableAssignment &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>aafc487ae943ce13ba17c459b6581d300</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ac8156cf148ab48425f1242d7d2672d80</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const PartialVariableAssignment *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ae4c3d1bb909cfbf07490b8d9b41851f4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>ae2f78fe5305979a7b754b8005c14e01d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>acb2e96ad90618f84b04a37ae8b592f32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kValuesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>a0aa3eb65b93085dbcc7e6fad7cb1b76f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1PartialVariableAssignment.html</anchorfile>
      <anchor>af5d0c2dd0559285b7031bfdf619ece69</anchor>
      <arglist>(PartialVariableAssignment &amp;a, PartialVariableAssignment &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::ReservoirConstraint</name>
    <filename>classoperations__research_1_1sat_1_1ReservoirConstraint.html</filename>
    <base>operations_research::sat::Constraint</base>
    <member kind="function">
      <type>void</type>
      <name>AddEvent</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraint.html</anchorfile>
      <anchor>aff0e9a5c156c176def60cf2985919bd6</anchor>
      <arglist>(IntVar time, int64 demand)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>AddOptionalEvent</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraint.html</anchorfile>
      <anchor>aad9028f0c33c7d4799891b9f742148b6</anchor>
      <arglist>(IntVar time, int64 demand, BoolVar is_active)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9052e9e1dd8248433909b5542f314add</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>ab6457905c9a8cd1c5f92738d57e6f298</anchor>
      <arglist>(BoolVar literal)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9401ab195650160402df5b61f8ac9bda</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aeaf30f4ee7d141e68905f1ac2432b937</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ConstraintProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aa0b277df64333f670b66c8d5295b8250</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>acaa17b2fbfd62f6845329ae944835654</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ConstraintProto *</type>
      <name>proto_</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9d74c3d77f601020ab87700745f830ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraint.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::ReservoirConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>ReservoirConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a435b2c70e9e1c12ea336ac6a77a84e70</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~ReservoirConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a0098113084b1b26338fee9667bdb85eb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>ReservoirConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ae58932d5ffa16a78b62aa7e72c79d72a</anchor>
      <arglist>(const ReservoirConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>ReservoirConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ad0daab530049b740b2c4ad4dc71813ae</anchor>
      <arglist>(const ReservoirConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a8f334a0a31b86879a5cf1e2926533293</anchor>
      <arglist>(ReservoirConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>ReservoirConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ac0bff8622d15607b97eeb66031731458</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>ReservoirConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a28bc8e2515658aa794a1e4ccd904b0f5</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a4a0cba3048560f8d31003af2fa84a02f</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a3151e59aba8a904b228b5d5464d37606</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a5712cacd5ce9441ed8d1ce9910439fd0</anchor>
      <arglist>(const ReservoirConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a47b1c6ab7ad8b7d4010fb2f826d1ee25</anchor>
      <arglist>(const ReservoirConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ad35ec910f37d27499aade5759f3bdf75</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>af8b7953bfa6710e092216b84267fe2a8</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ae62268a015fb12edd2364e54dc48f4e3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a78e9bb83a34bbd0f2b15165f23146bd7</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a9cfeaa3ec46716cedfa08b9b3c2e8514</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a043bfa1898a2233049986581d48f50f7</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aa65d88f2677784e0a8839a0f638f3361</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a48ec7007a9339df9d683b10e8312b0ed</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>times_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a57b796114e91487aff6f28e43e636aac</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a0179cc5fa3528d5b303dfa6e5e1492e2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a26e9cc7b3b9bdbfef2b00e16e73ba3ef</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a3542c5ee0934923a4db3951898491eba</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a4ad9c91833bd1b77bc51f2fd075732b7</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a5ce08a0d494a171bab04798bb3fcf2e8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_times</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a15658578766363f6241daa606faba152</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>demands_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a734c7b3754b1ae719cee7617acd75709</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aa9dcc7df20645baf72ca6cd9e8c19e6f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a37ea75f2fdc29bd5180317efb8e65ca4</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a3b34d2b6434970d2403f09e6f6adc83b</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aec1a9af37b5eda800df7954c397af66a</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a604d5115f71bf66e2a50c8565fa8dd96</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a3c22c0918a20ba08b415033e4135b46e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>actives_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aaee8b2a879ba80aa472b95820de3b6f3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a7ba63a69669155c8cd21c6054e408659</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>acecfe2d5a55deaa5b06992a93d6484c9</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a090c46af8c3b859cef9d741bd486ef60</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ab9cbf0a91106598317a46cad679c260c</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a6b599aedc9ba9cbb64bd6e9b0da62c15</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_actives</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ae866b73e50c9941517ef282f99051c41</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_min_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>adb5c56615fd76768b05d8b2a46cfea74</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>min_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aa88c25852b8b1e025befca7d8b2a09c2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_min_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a174662a19893f1bb6d74f75c9835cc67</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>af4ebdf4db00d24477b32c8dbc8d6f0be</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>max_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a6a495f736292659fea2144bb36c830ca</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ad72cd85207cf349db67caa0983c2f769</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a09ca0e48042eca20291deaae1a6ca0ab</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ReservoirConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a5f86f39ab1cfecc905b579329596e65f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a82d9a2956750c54f9aefbe81e271cb27</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ReservoirConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ad6a81ee34f164adcf8baab09dd6d2b2b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>acfe7e632606da73ddb155f946b686d32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTimesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ac11e64fbb00ed4a9b416eeee62f7d8a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDemandsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a2b38d2329cc0a540f0a14df8932ca007</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kActivesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>a835f07389166ce234319a6658eef103a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMinLevelFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>aad6599fa35f799568c6c635a36ad49eb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxLevelFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>ad20adf9c695eed8f822db87a15788751</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1ReservoirConstraintProto.html</anchorfile>
      <anchor>acfd202ff58fd87038a27b2130a413097</anchor>
      <arglist>(ReservoirConstraintProto &amp;a, ReservoirConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::RoutesConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>RoutesConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>acf8c0c1c206a33598a1adf22ec39af43</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~RoutesConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a708ec9dab68a48918d20317ee2eeb4bb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>RoutesConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a910cd5c634c78b4f7496ebbdb0710a0f</anchor>
      <arglist>(const RoutesConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>RoutesConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a07107e6e4490559714e67f598f5dc6e3</anchor>
      <arglist>(const RoutesConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a9616b4554f381c785930725c4efa26b1</anchor>
      <arglist>(RoutesConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>RoutesConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ac906a08518a22bdc77ecb56b551c9390</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>RoutesConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ad01d19c48930b971dd7226b887b09c71</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ab2803f84af6dd5e0001d2d0f684c7592</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a3a96c0f0aa10ebe43ae2244c64a78ffd</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a162fba0ad4785c443c76de4793dd5840</anchor>
      <arglist>(const RoutesConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ab9bc8d2e0833773f48964e1ad95a5a7f</anchor>
      <arglist>(const RoutesConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a2c04636a8a8ff61fe36f424d82d4989a</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>af2fcd64a6a59460b16a4c4288d80a6e2</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ab5c021a74232e20c60b0da8aaf8e069b</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a7cb2f719c82beae54dcad6a7662ed16a</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a86185387b22e8aa017c5cbe73bd43cd9</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>abfe1146ccc9b45f0c75b465e465a1e4b</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aa649b5d02b12d1644fa24838c6e7eb05</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aeba75ace214d57312dc1c68ffcfd6d0d</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>tails_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ad6188a63e90c028bf7d01db17ab68f30</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a0522c23b674c00249aeeb20f76f4a821</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ac999451b8005028f6d9980c40cc5b50e</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a3ec99719358ad6c69af6178c32810c00</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a9dc96a8d328728424eef7c1bc39f55e6</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ab1f643c6f14740a188afa86a0e2fa58c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_tails</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a11e644ad3de4c039b0e33305ab84a08e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>heads_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a6d12c7861c832016d9fe1e966ab3ffb5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a0d8a94e4dad92a92e25ff6deae5c5064</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a5d37fd5ecf7ccd06400b8b1ddf26fc2b</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a89e25008788a355ede317f8ed37f5d3e</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a2a657c65614fd2013ddbcd59a116159d</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a57b0428b6ce971792f597d0be198214a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_heads</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a135a0af61eb4d06812e95266aaaf7cb5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>literals_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a399aaf0578bb74021f08cee00779d38f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ad4d901785b5c3f64491ffd89b301c5bc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a9cd602b59502cbf1b17a4ef1b143bbe9</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a24a2b81b63ebeb44e8b451b9d5133dbd</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a730a4d2d29418698249237a9dc6542e4</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a7036fdc5b463044eb61b6875f97f3a38</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_literals</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a273ef0b5026940a0a5b3a902651499ce</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>demands_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>abe2878ef55e9ed85292ea9d4d86d100f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>abee4ed6e50c3a32bbda6218c4f27bcfb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a9cf398907e3b671a4594e5554d70c977</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a1cb301f8a82a2f1c985ad0fb3bfddd6a</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a63a12f09c4bc8fdde43ac4b559763a89</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a9bf414d09754d276cfd22def447eb5e5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_demands</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aa11316cc6cbd19d3f3c6ed14945785de</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a1ef1c7fd0ab292fdbe73f1349c4ad72e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aacff77e53d0072434980333c61b82d39</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_capacity</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a7ac72a8ce8d4b71770a5ed34aa84e79e</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>aa9f3001289f8819fe09292568f90d325</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const RoutesConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ac9dc769ea9aa7a14723f8c9392b2be28</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a20868ff6445da44dc1967f8a3afa050e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const RoutesConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a6c422af74ccf72d6f0eb8bd398ac77b8</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>adbdbd3b74649a6ef965ad69fb1119eef</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTailsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a45f350d59bb481ced9ff17e6917cdc5f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kHeadsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a261b47d657c736e4adae6eff7c454974</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLiteralsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>acb8b933104a691e4205dfa82ab50ead9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDemandsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>abb9f956c251d806fe4a250c03ac61199</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCapacityFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>a281c1cad6b3dd7607dfbb18eaff68077</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1RoutesConstraintProto.html</anchorfile>
      <anchor>ac91d73b61ee144ff7a168c0a1c97ba12</anchor>
      <arglist>(RoutesConstraintProto &amp;a, RoutesConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::SatParameters</name>
    <filename>classoperations__research_1_1sat_1_1SatParameters.html</filename>
    <member kind="typedef">
      <type>SatParameters_VariableOrder</type>
      <name>VariableOrder</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a57f2442d6b42157926aeacfac88ef7b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_Polarity</type>
      <name>Polarity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a23c9f2eaf05f470e387cdc82528cb1f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_ConflictMinimizationAlgorithm</type>
      <name>ConflictMinimizationAlgorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a741c32c1eee2e8c92074da63c3a101b2</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_BinaryMinizationAlgorithm</type>
      <name>BinaryMinizationAlgorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3935f8c53db1e05ff6a813d9636b3232</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_ClauseProtection</type>
      <name>ClauseProtection</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1c601aefbe23b3e8797c4e0ba93017a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_ClauseOrdering</type>
      <name>ClauseOrdering</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a233c9a666d0c3e012b079c0465b00790</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_RestartAlgorithm</type>
      <name>RestartAlgorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa939e5e33b3c280a50d902d01c6dbb0b</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_MaxSatAssumptionOrder</type>
      <name>MaxSatAssumptionOrder</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8e87f43c84566548d51c94adbfd9dfb9</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_MaxSatStratificationAlgorithm</type>
      <name>MaxSatStratificationAlgorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4b5b89fbef56678b4897f57c53963a89</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>SatParameters_SearchBranching</type>
      <name>SearchBranching</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3ebc49c3ff673e80e7f9815a7b7a116a</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SatParameters</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af4d8f5e09bbec71ebd9ee03c8ef5a120</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~SatParameters</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0bdde063de9e457141af35f04dd8ebf1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SatParameters</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa44f94048ae596b4fdd88e39d6564d05</anchor>
      <arglist>(const SatParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>SatParameters &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0967633b9de4be2869282456e56c5064</anchor>
      <arglist>(const SatParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::UnknownFieldSet &amp;</type>
      <name>unknown_fields</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4989838448ff19efcdda63bff1d28860</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>inline ::google::protobuf::UnknownFieldSet *</type>
      <name>mutable_unknown_fields</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0957a9798bf5266a0ee61d76ce364eb9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7594cd5cfdca68f080e03c83d4dae5b5</anchor>
      <arglist>(SatParameters *other)</arglist>
    </member>
    <member kind="function">
      <type>SatParameters *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3c252cb65df7d764af952eacadee60df</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>SatParameters *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acd1d8a1dc86d4c0e02117cbb9e3f674d</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ada7a766ae7013e03ae3c90634af38d22</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a983f1d5469855684789e594c76f6ff5e</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5d0fc3a39a124846c7c8b40139cc80c0</anchor>
      <arglist>(const SatParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeee4f4a3d73f9ef2a1dcd26ae312b5bc</anchor>
      <arglist>(const SatParameters &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a19785c82a2483991b95b3ffbcd4c2d60</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8c3459512aaab547b347409cc325cda2</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a49bb75e2befe21afa0307b7d0c0452b0</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af4a604adc8e1840cd94d6de32cb0e2d3</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a39019f6e657462912af22ba5d6bb90df</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9ca12b6a1ff8f10f85559b0416440463</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac73a18b3427663e2549d5e21c9b909e3</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0401247b7ccc60f440834f79400577c2</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>restart_algorithms_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a75ea12358ac15a525596aa39da3bf603</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a17f2516b6cb73932b6431a467754c97f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_RestartAlgorithm</type>
      <name>restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6f1eea27610a9744333457be0510443f</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4595a19a4713f27b81d97b3a07b88338</anchor>
      <arglist>(int index, ::operations_research::sat::SatParameters_RestartAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a649423537b705b6efaea2c2049bffa58</anchor>
      <arglist>(::operations_research::sat::SatParameters_RestartAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; int &gt; &amp;</type>
      <name>restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adde98dc970f6376b3548d1b3d74cc6ef</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; int &gt; *</type>
      <name>mutable_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6145a44b08812a703c9b205c8f7f3567</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4f9e587b77d231773ca277bff246beff</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3b5a81677e6cc163c3913e9f83ae3d63</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3631c23795ee76fb52cd04798a2dd415</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3f35c980529ab98b1a27395e28410ca0</anchor>
      <arglist>(const ::std::string &amp;value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac930d4c88c8dd62f5ebed21d307a53c3</anchor>
      <arglist>(const char *value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad460bcf19bcfb1cf9e7d36cc832f5a6e</anchor>
      <arglist>(const char *value, size_t size)</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>mutable_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeca983c469ec5a7c9a155040bf790fe4</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::std::string *</type>
      <name>release_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad52b0ed823c5c768b125c9568bf5a379</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_allocated_default_restart_algorithms</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4fcefde6fc8457f40a5b646e6cd62669</anchor>
      <arglist>(::std::string *default_restart_algorithms)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_preferred_variable_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad9cfcec2147dca61c8e74a54f69edbad</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_preferred_variable_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a13fa94fe576a74c66201757ebfb7ad5d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_VariableOrder</type>
      <name>preferred_variable_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8dc12c4b807995d1aa4627e447b43877</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_preferred_variable_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a46a41f7be959128859f9f14ddc5f097d</anchor>
      <arglist>(::operations_research::sat::SatParameters_VariableOrder value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_sat_assumption_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa7650618aa4fe4d337f7646c339891d1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_sat_assumption_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab012debdeac7f661961fce1b41a2a34f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_MaxSatAssumptionOrder</type>
      <name>max_sat_assumption_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3b4bcbab0e1731232d2c0a443d2d7119</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_sat_assumption_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa14ee618146695101ef7e6bfd69b0477</anchor>
      <arglist>(::operations_research::sat::SatParameters_MaxSatAssumptionOrder value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_random_branches_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a69ed15ec6026dfa46c8c8da6da848931</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_random_branches_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a524f65ee1c15bd43e8fa6d24e83e51bb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>random_branches_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac54298c32a174441baaedbc174e56dc9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_random_branches_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2d67ce107021e88c3c156d5a244ed62b</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_random_polarity_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a14917e5d77a88b0557aef5b6d73d721a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_random_polarity_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa293c388ba7c6f8fe9e1f90e1767da1e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>random_polarity_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a980dd0dad9c150aeb156f26d8481b19d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_random_polarity_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a830be06bc76d1cfc2de6f1e2d8b350a5</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_pb_resolution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a452ba56aa4b446b26b702b8fd5a0cbc2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_pb_resolution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6cecd5c3387027dc79887afc37a1debe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_pb_resolution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7d726d6194c36ed64e583c496df7f395</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_pb_resolution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac5992a1984d9a84c783d833e943d6b87</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_minimize_reduction_during_pb_resolution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5c603dafeb96eed6a014658d88f0e920</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_minimize_reduction_during_pb_resolution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8c51fad5c136b7b6cf5ed0c49ace71ae</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>minimize_reduction_during_pb_resolution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4dfa3625510be333847f19ecd1239a41</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_minimize_reduction_during_pb_resolution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac948afe4059f6f36eb496e78335cb3ca</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_sat_reverse_assumption_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afd5d859471f19b413da081a9533502c0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_sat_reverse_assumption_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a102f9e3b46404f564525c2fd5131c217</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>max_sat_reverse_assumption_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a483db0e7475b907348fae889ca7009a2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_sat_reverse_assumption_order</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab0d636c2904b2819bb0aa6262b135a65</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_overload_checker_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad018b89dea2159e6a881a6a96a6514ec</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_overload_checker_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0a9db31587a59f4ffaa4dbb174df4424</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_overload_checker_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6ecaf50307219f2da780c0d94fca721d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_overload_checker_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aec673646ee9ca4e2a72d531286640d0d</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_clause_cleanup_protection</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a93dc8db0c3ef1d4cbe06a81b635976eb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_clause_cleanup_protection</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a27a2985040ab2fd525b837113294642d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_ClauseProtection</type>
      <name>clause_cleanup_protection</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab68a3e3c275ec350b98d747cc3c68dec</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_cleanup_protection</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1a21dd120b5c1811a15027ebb7e4c846</anchor>
      <arglist>(::operations_research::sat::SatParameters_ClauseProtection value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_clause_cleanup_ordering</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5ada19dd0a4d348ca4c768706688035d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_clause_cleanup_ordering</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8efeb31ff478af53dece575ddf3712e0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_ClauseOrdering</type>
      <name>clause_cleanup_ordering</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4e07b1df49807ae6984a3cec5e912890</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_cleanup_ordering</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a39475d439aa38ebe84bbfa3f68ae47aa</anchor>
      <arglist>(::operations_research::sat::SatParameters_ClauseOrdering value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_erwa_heuristic</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4d9114d2ac6f5686f37312a2dbd32cb2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_erwa_heuristic</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a735dde6e294058b8d4f5c6116a51eaac</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_erwa_heuristic</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8ea993e459628622948ba2f5a8319c5a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_erwa_heuristic</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1031ae7a2d2fa7e98a94a5e9ddf4f573</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_also_bump_variables_in_conflict_reasons</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac159de93968b297fac81c7948991a6a6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_also_bump_variables_in_conflict_reasons</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab875be7f29ab25ca13c091f4e4fc5f71</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>also_bump_variables_in_conflict_reasons</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aff8890384ab9957b9a6582e163b5868d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_also_bump_variables_in_conflict_reasons</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a327dccfe7d0cc96dbc86f207134e1a9d</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_blocking_restart</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3df2f36e7b7b63d1922738aec516c726</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_blocking_restart</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a287555f322246bd57878f7632d12fb2b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_blocking_restart</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a20f03fe926f9912336bfecde38b3d431</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_blocking_restart</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5b13453f8b580ed7b8e8369f653d5e7a</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_log_search_progress</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae6773193d40ed6c682adcf527716b69b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_log_search_progress</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2bda86fe30714cd506bce3de4f32a395</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>log_search_progress</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9bb66488b1e39abfd99c601d13d73ffe</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_log_search_progress</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac1a2d9752fe2878fe265b2b68a57c42c</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_strategy_change_increase_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af49c58eddefe486c4d3205d8c59a0f34</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_strategy_change_increase_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0509d6fd2e4a2ae577e659e4f41d61cc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>strategy_change_increase_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a317783e0b74181b6562207fa709979a5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_strategy_change_increase_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a32541e21965ec9413a7aa7c3377b162f</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_num_conflicts_before_strategy_changes</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a657b8b1a3d50afc4be6175d2a244f4d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_conflicts_before_strategy_changes</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae6b2df2e56502a6cde03525959bf8261</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>num_conflicts_before_strategy_changes</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4949c084392f4007e76d6c91b598a0c1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_conflicts_before_strategy_changes</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab954d5ba1420dffde08f95a737070b05</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_search_branching</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abb18c00f70cd1ba260c5972418c13f64</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_search_branching</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab1037626aa53652711ac3042db4dee13</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_SearchBranching</type>
      <name>search_branching</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac349e5bab7c4d219226d6fa0b3640cb9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_search_branching</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a26ea5d1445e6356d6f0534be32aba7ec</anchor>
      <arglist>(::operations_research::sat::SatParameters_SearchBranching value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_initial_variables_activity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a560f9b043fbbc8ee287035956760b6ee</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_initial_variables_activity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4da5f70705c61d1f4a95d18ffa2bd75c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>initial_variables_activity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a729c68650b549aea62fe937dc9a326be</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_initial_variables_activity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a304bd45d3c310cdc540b18bd754e8113</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_exploit_best_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9de3c5f96b0ca544acd7fb20ba31ad0e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_exploit_best_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3443567f3260b0fd825d334b27e09e8c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>exploit_best_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0ae0b2bc8aa340d565d20a48ca2993aa</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_exploit_best_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2b0c09acebdc1829d7df993790ec79d6</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_optimize_with_core</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>add5c5873bb09ddc8e12346ea1ba813d0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_optimize_with_core</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af2e8ee84778686e9789a5ba0ce7c383f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>optimize_with_core</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8e7473ea41dbf33ccba88340165a41bd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_optimize_with_core</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa38a51abe6aae43e843c8c1ae77611d3</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_optimize_with_max_hs</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2ba8ea927fd1ccb99979ed3a0354b246</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_optimize_with_max_hs</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1af4ec5318ae8d10197f7d734ef080a0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>optimize_with_max_hs</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af3a1ed149e11916ac95cafbecc6e4142</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_optimize_with_max_hs</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7367690953b34a66ede243221b882cce</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_enumerate_all_solutions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a32264920249bfae4bab1a09cfc63cc88</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_enumerate_all_solutions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a854f8cc2dbe640ec5ef9e8fb29b685fe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>enumerate_all_solutions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a42c6626281bb07de74f2b48d5687bad2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_enumerate_all_solutions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae77ff8a4a6599aee304a5fa8fc8974c1</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_timetable_edge_finding_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0986680bcf1538a55e9f4c34eb319b2c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_timetable_edge_finding_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1971b073e183edabaf7bc59f46358d0c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_timetable_edge_finding_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a953b222c7fcb34e9729a1e09928b7c8b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_timetable_edge_finding_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad2186802e41fbd4b40393594d7bb9911</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_only_add_cuts_at_level_zero</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a67656b86657f259cd2242eaf7e5840bc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_only_add_cuts_at_level_zero</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a722f4187972fc8a2c4c49fa2011b40dc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>only_add_cuts_at_level_zero</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6640268cf41dee98c18683cb710ca8c5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_only_add_cuts_at_level_zero</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1fc15036affa6211d5d431a14b976b9b</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_add_knapsack_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afa4aead81722bdb0056014bac13141c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_add_knapsack_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a04c63b40595db6488c1bfb0a3b101bac</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>add_knapsack_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa9d1f7d5ab75524506aa61259a9def86</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_add_knapsack_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a92921ca8bf6c096980da55da47422c40</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_add_cg_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a38608a2a8876aa04d1116fe98b1a3bf0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_add_cg_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2e7e06416c6d33832c3db6ff92ed7199</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>add_cg_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a05b0283ab0ec514dc5491661dbc2e34b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_add_cg_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acf1bfa02f21378b89106cd3f86406d70</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_search_randomization_tolerance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa057214fddeb6e75d095b56a94c9403b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_search_randomization_tolerance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a164e3ac70c57a482d49f4931d40f4bdb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>search_randomization_tolerance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a35e52748cfd2aebf785980f151b32cd2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_search_randomization_tolerance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aaca922e8870efcfe27d122a82b369a87</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_num_search_workers</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1084ac9a4eddadaea6a7b3fca0ea6ce0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_num_search_workers</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a57ed4de8cbb62926aa513f2555a7ff1a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>num_search_workers</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a20d149f7b5ab8da7326739acf96a0691</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_num_search_workers</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a43558c4a0596a883357bf880fc97538d</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_fill_tightened_domains_in_response</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a358af413cfd511dee33a72a78e05c2d3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_fill_tightened_domains_in_response</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae68a4393badfcb1511441d4fd5f11846</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>fill_tightened_domains_in_response</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1acc23d23a7829cbee39c33156714b37</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_fill_tightened_domains_in_response</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac56636f4118df9ae7b49a89ff73c6073</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_stop_after_first_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>add539afedb092c2b2d4bfbb323d572fe</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_stop_after_first_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae58a24e4bfca8efd6c1b75a21d6d1b5e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>stop_after_first_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a78c6b78e31d41a45a9506692473f85b6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_stop_after_first_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4a0f153cf20dfb8ec439f6c10193130f</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_lns</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a33f975ecb6da863de946b7398f228e9e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_lns</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abcc5309b38ea9669f96fae65205c820d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_lns</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2db2d05db9b2cd0d43e898c5c7a99f2e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_lns</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9ff124ca5208529e25e80dd038a833d6</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_lns_focus_on_decision_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a283055babc73ebef42ca0b98f0b42613</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_lns_focus_on_decision_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2cd63c92bd839e841c0dd17b69c90cbc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>lns_focus_on_decision_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5b3eedeae3778d2f47b5e658e5894736</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_lns_focus_on_decision_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4e65354c4d93976325d953e366718d17</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_rins_lns</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afe58ed279e3d20a901d37511a3f909e7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_rins_lns</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af08e4ab996af1aa521993c004738bf4c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_rins_lns</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4ff91e7794cb0ea0407949ddaa4f258d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_rins_lns</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af476cc00b893ebfeb5ab7e6d69e2ac92</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_randomize_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac10d3776f2e92ffa9fde04a56ae06145</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_randomize_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7a918a01963b3ec258f000e074fe37b7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>randomize_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aaecf2ed3801a14f4ef2aa8fa418eb8c0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_randomize_search</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeec72bea45ed9de54d9fc17cb511b8f7</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_combined_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0fac786be2bdbb5d93781523b3f4ce64</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_combined_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ade4963c7a1a6d8c0b7ecc20ce142a94e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_combined_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3fc6d6586318ccabe142e90b27d97507</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_combined_no_overlap</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac3327bb59d47a20656eed1666e84d4c1</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_min_orthogonality_for_lp_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad7af5d2e5696028919b11bdfe008346b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_min_orthogonality_for_lp_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5f643d045e63b5873f1fa94bbecf0849</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>min_orthogonality_for_lp_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a043ad75614acafc414ffd4947fafaa34</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_min_orthogonality_for_lp_constraints</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6d2f4faa402690f8463b8f25657bf5c0</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_mip_max_activity_exponent</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a68115cbd3b1924199858e2c261439276</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_mip_max_activity_exponent</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a86e972bdac7cf4b3129c5b8ca1bf9751</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>mip_max_activity_exponent</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aff86b511d8b6786752926351510eea5f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_mip_max_activity_exponent</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad7b83e8141859ebbde292bb0a5593848</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_initial_polarity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a55850aeddb1632f4d2d32b5ae2eba9da</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_initial_polarity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2547a17b67f7b86ee17d6cff38f51e06</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_Polarity</type>
      <name>initial_polarity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab8cb28da16ab4054c04f29aa8344e3bc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_initial_polarity</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4fd96ffb9a098f7da94484dc9a42b2ba</anchor>
      <arglist>(::operations_research::sat::SatParameters_Polarity value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_minimization_algorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae6c1fb9bd4beb319f8b099623ade7b9d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_minimization_algorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a722215b21a1eb801988c35f594451bb3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm</type>
      <name>minimization_algorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad1a1c7311c39fb7f232f2c7dfc62aac0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_minimization_algorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a16d6c3b9a0212e2833f71ecff1bf91e7</anchor>
      <arglist>(::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_clause_cleanup_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5a05a3198cba3da4f499a94bcb3ce803</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_clause_cleanup_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae8db44027f0fa813e3a411022d330737</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>clause_cleanup_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab0ccb4457e0ad167e46b6c3a06e6fa5a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_cleanup_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa6c6dac098960b53295cbf681794adc3</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_clause_cleanup_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ada787c10593d9bd85153d118f1e2a5d5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_clause_cleanup_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2738833d93835a92cf657d50e1f9a5d3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>clause_cleanup_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1e7bff24d803180e6c60f3bbc8399d2c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_cleanup_target</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a81576b47a1f47c5f43656a93ef2195b5</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_variable_activity_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae41941f027549f65d64e1dc1104ec427</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_variable_activity_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adb2b78f7b832bc6cfa4ef2d5950a8a9b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>variable_activity_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1ab7be255f9a11320608a814dab947fd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_variable_activity_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2905415a8af6aa9ad11c283d2b69d227</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_variable_activity_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acd40d65c7a601067882e36a0204cc3a0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_variable_activity_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5ac5befe3d1a6b7469f839d6e0ce4885</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>max_variable_activity_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6a89d4f794f345a38c89519ed2ba2daa</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_variable_activity_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa8e130cd366acbb9459268e32c74b33b</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_clause_activity_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abf8484485f5cf2e48e33b1b020a45509</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_clause_activity_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a560291edbfbc41393633abb9e20170ef</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>clause_activity_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af18f073a1e84bb745fcb6bbf7a29c50b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_activity_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4056e3d075da45b212d1f16863eb788f</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_clause_activity_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a94c4134ed54093241cddaebfb55956bf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_clause_activity_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aabd109345395ca1922df6105d1a9d8a3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>max_clause_activity_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3d789de270371a45e88644a3aef30e2e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_clause_activity_value</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9c11974dcc3b6f5b85aec5777cf50938</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_glucose_max_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2818fe9823201cee220befd3b021925d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_glucose_max_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac50b21223e5197c319b0f5b195a415ee</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>glucose_max_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a22c515ce5771ece8e310d275f01250e6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_glucose_max_decay</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6dcdff4249400fd5368540713be3f8b0</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_glucose_decay_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeec669701e40645a7fa8bfb19e4aaf9d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_glucose_decay_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5bc5f4eb3ef83a29230e0e45ef5c9ed2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>glucose_decay_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aade052995d2374250e7cbe687e83a0a0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_glucose_decay_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a569d7643d2bcc53bd9a96ac86c4840cd</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_glucose_decay_increment_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0a67015e554a7508a96d22855b72c1d2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_glucose_decay_increment_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5c440b6cec4dd2336418834f7a6b3c26</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>glucose_decay_increment_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2164f40a11d9c0fc18d3343f210662f4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_glucose_decay_increment_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a157dd38edaf7918be0e4b459edbe0da1</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab528b49e9311d739e41f71452d18a7ca</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abebc7fbe70ab8cbdbbdaaff5cc7e821b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5be6deac1efe31d76db6726e660905e8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa31156bf26261533816877f72939b2d4</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_random_seed</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a652e9bcc32f0aca6f5b001647530c6c8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_random_seed</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2627995032320b53ab21b83b0a42a4c6</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>random_seed</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8793fe1ef6b2c3f25392fe5c5d02406c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_random_seed</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8f00b9736678a1480237eec03be56fdc</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_binary_minimization_algorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad00d9960daccace7592dd09cb1f7a32c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_binary_minimization_algorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af991ef9b25c2d9e0b50a20f56d415698</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_BinaryMinizationAlgorithm</type>
      <name>binary_minimization_algorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5a2a20f25b804c464dc05a077d2aea3f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_binary_minimization_algorithm</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a32d2d2e2c489cad358ff440925713198</anchor>
      <arglist>(::operations_research::sat::SatParameters_BinaryMinizationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_time_in_seconds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acb70ec53e2fbfce24b4daa28c28234ef</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_time_in_seconds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a472fdb8907f0d63a137689a6ebaf9452</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>max_time_in_seconds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0e2df6c1d4dac1eade627d27eb8ef90c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_time_in_seconds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a22f22991bd7f54a5de6d7413670c3fae</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_number_of_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab67b7f2f1f74515809844d9c1dd820d9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_number_of_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a871c3c25d81801477726175df429f33c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>max_number_of_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2b8343a6e7cc8d92d5a109f174fca34d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_number_of_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aec741a938e27028b5981dcbd5b34d09a</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_memory_in_mb</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a98af6d355d236e5019064bd6568aa03b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_memory_in_mb</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a07897b95903ebe91e548811cdac628c1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>max_memory_in_mb</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9b2aea19ec06c2c4be018ff4c7e47e0d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_memory_in_mb</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acb651eaf2ec51630e6f667316f531def</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_phase_saving</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5a22883237835df1899680c39e8659e4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_phase_saving</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac789d6c7627cb8bb50029478a1902aff</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_phase_saving</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7c115ad36b8acca68ebd7fa4c43b599f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_phase_saving</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5fb4b17080a070e4a0ffd113b08f13e7</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_subsumption_during_conflict_analysis</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3fe58ea59aced6a80fb7115467b52366</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_subsumption_during_conflict_analysis</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab75158069ed59aaf11552e68a32304eb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>subsumption_during_conflict_analysis</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a78dcafedfc1c172f4cabfd673ab0aa66</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_subsumption_during_conflict_analysis</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6d08144376c4d2708282b1b4f05b2027</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_treat_binary_clauses_separately</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8d2f08e836c02f7734b448884e752e82</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_treat_binary_clauses_separately</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a25cb0e2a2ccc17d516c818085913de9d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>treat_binary_clauses_separately</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a84c4633786d2c47cf206fb51f196db2d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_treat_binary_clauses_separately</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a46d0957a994186a33bda8a18825718a0</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_count_assumption_levels_in_lbd</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1c7c33b1ec87f1d7180cf94a7c0d9ecb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_count_assumption_levels_in_lbd</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aebc7547adfaddafac2d002b97130af78</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>count_assumption_levels_in_lbd</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a142f1ac6bba8910786bbd7dae6746c78</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_count_assumption_levels_in_lbd</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a580ab7d8dfaaaf29cab99db9c2d785dd</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_pb_cleanup_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aace1d4b76ac84331576659d76fbee204</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_pb_cleanup_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae6b730d49066570ee5d60b056baf7929</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>pb_cleanup_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6cbb2e430e3383fb9b99b67d52762fa7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_pb_cleanup_increment</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1de6629d3f45b80ff26d4bccc14b4ecc</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_pb_cleanup_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af177dc4936047691ad433a7d640e1530</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_pb_cleanup_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4cadd40582fd60f94ea274a501ace62f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>pb_cleanup_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a84f08656ad47d865024b505172327b3f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_pb_cleanup_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a33dafca132f425b24a0acd336244e6e9</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_sat_stratification</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af7c121d3e2f7942aa72f2b7aa2306b54</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_sat_stratification</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a676d1e4254d425a50c6ee6e399a92f7b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm</type>
      <name>max_sat_stratification</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3d0f20419385d42ec4d41c11bf64643c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_sat_stratification</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab2874dc3d44b18d5a0ced1818e70d369</anchor>
      <arglist>(::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_presolve_bve_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aae816aa278e0eef64c3c82fd34155b83</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_presolve_bve_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa13751ba3a7b8457b6deaaa168f79b2d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>presolve_bve_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a73249750d34559a087ac41e5bdf54017</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_bve_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3bf9c24ff70bea71327a2b114c41bfe8</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_presolve_probing_deterministic_time_limit</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0719d0ab83f83913cc6ed45159462352</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_presolve_probing_deterministic_time_limit</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4c7d1d4e89ea2882b4d6fecabc305a99</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>presolve_probing_deterministic_time_limit</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad5d1c0e05c689cef947022cf02ef253c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_probing_deterministic_time_limit</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3775d9fc573397d21b54330fbfd90f49</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_presolve_bve_clause_weight</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9ad426a407a899f4465eb75ffe5347bf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_presolve_bve_clause_weight</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af7a16cb2b70ccd7174f02f2839c18a51</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>presolve_bve_clause_weight</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adac48d8340e5d1e35f4ea7b62f574359</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_bve_clause_weight</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a84c96de45986c3680f738acfbe1019e1</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_clause_cleanup_lbd_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af5a5fcd74ebf108e1b48ff75c50b6bb8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_clause_cleanup_lbd_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a96193482110fea743c54dfc903a7d929</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>clause_cleanup_lbd_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2f7f7060e4c4c613917889e877c27f97</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_clause_cleanup_lbd_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a95e7a399fd787d4385f1b816b1feed3d</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_presolve_blocked_clause</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9e79e320d91d0302e8b63d6e80b393b7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_presolve_blocked_clause</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af3e049387370c472db3d1ce71e827f7a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>presolve_blocked_clause</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac79cf37d8aa6886804a81eac6ff33a4b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_blocked_clause</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a95c875cb40876fa3e9f6cf93650e6593</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_presolve_use_bva</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a78038c00f6656576301de7754665b8b2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_presolve_use_bva</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa55a957fb94ac214946c6793d6c2ef65</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>presolve_use_bva</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af43b0ab37d3a1bc4972b29eb90686a15</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_use_bva</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8a4f420cea0226ca20b74604a61bddd2</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_optimization_hints</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aba1f5eb52a39b2f28217082dfb034670</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_optimization_hints</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9aab6866816cf54407fd54582c5273d9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_optimization_hints</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1531ac19e851bed12ea02f0f39831870</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_optimization_hints</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a222072ad9d1da0f2abbdf9d964ccbae7</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_minimize_core</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aaf5e7e2b8beed7c403d539ade27e6c61</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_minimize_core</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ace09b6eac0574e22e50f262344e1347f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>minimize_core</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab1743ecb3831f63271f0f2bf485b73b9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_minimize_core</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5e0a4c638e042afcb3eb2dea50d1f147</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_restart_running_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1e0264d5bfed3f6ae8939fecae653ac1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_restart_running_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab1a4d21deac2e58b558ffa51b38cb8df</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>restart_running_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7d895b8a64cb84c9012eafb8039aacbe</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_restart_running_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad88983ceb72252974624c21b5b07c277</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_restart_dl_average_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae0df8d9e2542a960023416138144f01c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_restart_dl_average_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa153ae3e4cebd0cea0613cfd871449fe</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>restart_dl_average_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac3ab4a01e2b9ab820d05a9739e48b922</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_restart_dl_average_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af9f85d67c60935ad86e285d164f08451</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_blocking_restart_multiplier</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2cef0df4a7bc8d94e9112aacb02bb7ad</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_blocking_restart_multiplier</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a845d8935c3aa4cc2e3dda12a513d6389</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>blocking_restart_multiplier</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0046fce1fe08613b45b64d4754a39048</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_blocking_restart_multiplier</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a32ae538a46fb4e2fac415fcd802a70c8</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_deterministic_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac395a32ae8fdbd8f97f4ed8925b23857</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_deterministic_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab5d5d31aebc607fc08249a4a9cd103e0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>max_deterministic_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6e17c1ac59d6077b10573699959321c6</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_deterministic_time</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1e10c985af96ea97e505e8a46fd8599b</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_blocking_restart_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab2491dc199f47aef147ada19b0350dd4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_blocking_restart_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a159ce3755d4304de78d18d4054c5c0f1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>blocking_restart_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac562190b40c8922ca90b23f9d7927bd3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_blocking_restart_window_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac16bea982f01e50a6317d32d474339db</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_presolve_bva_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa12f85f1edac3ee8b135651b973189e8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_presolve_bva_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a79820be6a45846d26727272a075e51cb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>presolve_bva_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3791dcd2af8b19dd689fcbdbfd9c1a48</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_presolve_bva_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acabf4a4dde1b035f6df59e3843590703</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_restart_lbd_average_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad25b9f63a0f3c13fa0b1c71a98fc3ae9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_restart_lbd_average_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a821b1420ce44dfa1e933b0f458e1a986</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>restart_lbd_average_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab5527bd7be13fbf0fc9ce737a3d1d0b8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_restart_lbd_average_ratio</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a48ec41da62217bf918297510abd49fe6</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_find_multiple_cores</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6f092573145682ac8fb2f11b8bcaa6a5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_find_multiple_cores</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a161ced07e4452a55e5015b39fb719a5f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>find_multiple_cores</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa7cff1ef9161b5813a5c288229081ed7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_find_multiple_cores</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3654b833d9c55c4150966eb791a7f832</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_cover_optimization</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab0efdc6312c1d7e0716b468a8fa2d4fa</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_cover_optimization</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a91eba0cc0072303d649a463fb1deb715</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>cover_optimization</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa62fa42cc92062e9b088e38a49bac1c4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_cover_optimization</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2ae64cbb21430f6f32bbec81aa0717d1</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_precedences_in_disjunctive_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a370696cf99fa031ee67927752475ae8f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_precedences_in_disjunctive_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aaee4bda10cfc0fec0e4e788a72f11c31</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_precedences_in_disjunctive_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2a0ccf77cd58ba4937c44a580f063238</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_precedences_in_disjunctive_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7cdff8beb1fe04edfb177e2ad1f489d9</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_disjunctive_constraint_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a653c1af8aabc7bce4a7ab4f7e068ebfc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_disjunctive_constraint_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2d9a062a188135b7ba435535967c5204</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_disjunctive_constraint_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad38cd6ceca712bdb36dbf7d7be470997</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_disjunctive_constraint_in_cumulative_constraint</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adecd25365b2276e87d1d78ef1c5206bd</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_linearization_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3fa28c852425b90a9484d30f46679a96</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_linearization_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6df334d097afe2696bd1a173a80e8fb0</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>linearization_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad4a07a8fb6acf0a027563d1d4a133a83</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_linearization_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7f10929649ecda23f27a44c0ac975817</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_num_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1c3c67c4f76c82ea561567f09cc8c589</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_num_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a331deb162d70ca1793393c9931da0274</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>max_num_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5c2a0955211d922f4a3aba089b85af80</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_num_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9d0dfd588027e1ccf4e2a0b8d2846dc6</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_minimize_with_propagation_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5e38be5fa903efc083e1dc8818ce4f28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_minimize_with_propagation_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac527e419c766a18bc49a4e0466b8f3b3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>minimize_with_propagation_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1833d328fea0ceb1ec02765a3512d38c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_minimize_with_propagation_restart_period</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad628d7514b4068b42f9b9c0a7ec018d5</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_minimize_with_propagation_num_decisions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac93ad9b7175538e185406f9661e16ab9</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_minimize_with_propagation_num_decisions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8d79b8045f0cca2d7686eaf02f5f82d1</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>minimize_with_propagation_num_decisions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae1a4415a99796a9fd749e99e6b6a4f54</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_minimize_with_propagation_num_decisions</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af1fa079c985591e5d0001ddcff49faeb</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_binary_search_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a70d41475d2fc1466eedd19e80151cd7b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_binary_search_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7e149f88fc0075c98c16248252662d53</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>binary_search_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae3833e782e4b08278db0e93a75be0c0f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_binary_search_num_conflicts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a71bb9a8c458757ab5a41ccd90b0049b6</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_lns_num_threads</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa16bd22e21ab9837d86ae62e74073b7a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_lns_num_threads</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4fc3ba31a568695116ec3d82a0cf131d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>lns_num_threads</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad207affa2571674283639fa5693a1109</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_lns_num_threads</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a12df387cf27bf7466fe3e02b0d3cad4c</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_exploit_all_lp_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a01149d364c5dafc503cf3e065c684818</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_exploit_all_lp_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a69c75c42b89c78020d3aba350fad8c57</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>exploit_all_lp_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a42408080a77032f2b2201d06e58a0f6e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_exploit_all_lp_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7aa58f43a87d551f9bc0a75c97df8476</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_exploit_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac06d87416a568cfd3d2510fd93eb00dc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_exploit_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad218eff075b3081fbea55e698aea1169</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>exploit_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acd5ac8d4f358bb1ea9c213b373065ced</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_exploit_objective</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4e91c83ef16a5db6d3c8f0976216fac6</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_cp_model_presolve</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac3e0972e9f62fb9d5df54bc988781a67</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_cp_model_presolve</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa12dd9baffdee29099b29d2cce701f60</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>cp_model_presolve</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afe03e03821950cf85145722fcef8edc2</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_cp_model_presolve</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6856f241c4ccd9a18b6c77d7be16e117</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_cp_model_use_sat_presolve</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a128494fff93d0582d869ad24897917b0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_cp_model_use_sat_presolve</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a82fbe3478903f34425741affbdc2eb0d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>cp_model_use_sat_presolve</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab80c45c958e9500bac82b17d5100e2f7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_cp_model_use_sat_presolve</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1fb6a2768738dc5088a5197b6b9354fe</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_boolean_encoding_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a756b16f25abb7e1ffab126d2332e15d0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_boolean_encoding_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a35ce99858f0afc9945bc4fe1f22e4e6d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>boolean_encoding_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa67dd1d9130c57bf709b7b3603d7057e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_boolean_encoding_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a06724d9c7bcc779776da20675c5f6719</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_instantiate_all_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7b5a1d6c4d59f0409429736965a713d8</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_instantiate_all_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae2a8e08d0c4b1f8c420bbe26245509a5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>instantiate_all_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a40084b6cd3b84973a6d0af0bb9cd319b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_instantiate_all_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acbbdda6a89dce59207f8f63f673409a7</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_auto_detect_greater_than_at_least_one_of</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5a43541e065e0afa28ab7e0d3afd3d28</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_auto_detect_greater_than_at_least_one_of</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2e219ada7d08f51818d997e7e0db7c18</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>auto_detect_greater_than_at_least_one_of</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a72147df93c16bf7eb0ce8c1eb4f6c021</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_auto_detect_greater_than_at_least_one_of</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2b3c869376acb8cfc9c72fa2d6a4b807</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_share_objective_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa41424293281fe80b7a4e72e34def998</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_share_objective_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acf7d32c05386dea647e4ddcb35d660f2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>share_objective_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a448095398811fb6c477460b7fb6b36cf</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_share_objective_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac46e44880847d39e5e1d4d5c48bf81ce</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_share_level_zero_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9bfdf7076dea955c13fe197a6228e5d1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_share_level_zero_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a079a98cf62a9c8dcb6c3d37b0fa2e518</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>share_level_zero_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad997788e114f1bec07d5ff4e7d0f08e0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_share_level_zero_bounds</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5ac91626b814e4c587c15e04d4b19bfa</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_optional_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7f895da906479be37a0fa925c65a919e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_optional_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae47f567d8c8f82fada73347d29b253c5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_optional_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7a5f4c6f5d6f57b15dd98ad63de8acf1</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_optional_variables</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a88306976162c40099a868c7a44aae596</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_exact_lp_reason</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5e035ce620456899ec8de1b3f226c45b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_exact_lp_reason</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa771eb136ae84dd984dc9b10892633d7</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_exact_lp_reason</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a62d4b73c33078a073cbfeee9ff560998</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_exact_lp_reason</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a97156b400b2f40de83044bc1357358ee</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_cp_model_probing_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae26158b8feb367dc1255f7d2a2cf880a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_cp_model_probing_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9fb96d63468ceb1891f5624b3a2262af</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>cp_model_probing_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a56e16db387858b22278343db567e05de</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_cp_model_probing_level</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a06a1176010691d9644aa6fb5a85bfcab</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_add_mir_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a376e6ea815537cd2e97feb2f0e691942</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_add_mir_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adcf97ed1a1ea5ede79aeaf331c9592d5</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>add_mir_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab8cb2bad5c31fe58b2ee94344891f60e</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_add_mir_cuts</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4ecfa46b49fb90be62cfc4a7ef4450dd</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_use_mir_rounding</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a63878829db966b5e438590bd50b14cf4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_use_mir_rounding</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aab537a0adeb1c5d20511dbc7cdc49821</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>use_mir_rounding</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac79caa8b7a4c29ca5c8eb1bae0646b18</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_use_mir_rounding</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a78e02a718d4f859c50b8914a9e4b0fdd</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_add_lp_constraints_lazily</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae27fc5f446a70f1960b94dd1669da9a0</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_add_lp_constraints_lazily</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad543207d30019dc1fe988d3af4374774</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>add_lp_constraints_lazily</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2019d199f858dd546ccd593b26ab198c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_add_lp_constraints_lazily</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad257f8729ea29346b8e9698272ba35f8</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_exploit_integer_lp_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ada1123197d9468202b3e32d3202c4ac7</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_exploit_integer_lp_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8a698230c7b1915c3ffba4243745e9df</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>exploit_integer_lp_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7735e6e231740c615486fd50300c4292</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_exploit_integer_lp_solution</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8b0175b0d535952628e64fd8a17df1de</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_integer_rounding_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7afb62ad9e9d6be5a26d00b383affd49</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_integer_rounding_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2d19d28ab90f0193d49b6d9dadf3b2da</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>max_integer_rounding_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a60af9685480f6db8877875d1fea1a061</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_integer_rounding_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2b7a438fe3f4521f16570f74fb048389</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_max_inactive_count</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7e28ac8ff60b0c553bc068f9c97a5833</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_max_inactive_count</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad6e2cc39f1ffd9593d470b154f41f87a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>max_inactive_count</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa7d4e1ac370b64b682865b9f7de95c4c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_max_inactive_count</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a93a9fe4e73991610bc81842b7d31a3ff</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_constraint_removal_batch_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abaf3de71f74efcbf186d4cd6927d1b4a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_constraint_removal_batch_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa3932668de1670b6b933659f6d0662c2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>constraint_removal_batch_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8d64d46cbece5f00483662b8590283a3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_constraint_removal_batch_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a713538029f2cd48b7a7f3dd2e6464d1e</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_pseudo_cost_reliability_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adc59cdf3d720dff16eaea2dbf19028bb</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_pseudo_cost_reliability_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0dacbfbb1822ae81089ad93331f2e542</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>pseudo_cost_reliability_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab4dba107d7ac74f7661c1ca42ef8766b</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_pseudo_cost_reliability_threshold</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2a75515bb216f21bf399aa402981dc03</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_mip_max_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a257509be4db1116c941dd94b856f2cd4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_mip_max_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a27794b668f9d30a6724b3aede96960c9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>mip_max_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa6461a9fae2498fcfef87d62003aaddc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_mip_max_bound</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac7dce2260cf497d7bbebffa2f05aef75</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_mip_var_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abb6257387749b0032b2efa236633182d</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_mip_var_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>affcb6c0106f7088c7c3e0cdfc56d4857</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>mip_var_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad1cb1ba28f3f67e0ff6775beb10f2a1c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_mip_var_scaling</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab1b440ae58ef6ff056f94df6dcdbd357</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_mip_wanted_precision</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ada287fe650af1cca5fabbc291f1ac4f5</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_mip_wanted_precision</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abbeaa5c6c461596c03bbecc059219f32</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>mip_wanted_precision</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa064e3b6cf8c17ce47d96e43b4de4476</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_mip_wanted_precision</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>affa96fa5200f4c9bc31bd993491aa198</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>has_mip_check_precision</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9f2aabe7bd42a273829d8914606a2e6c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_mip_check_precision</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6675a4da295058dccce7b0dc410a1e0c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>double</type>
      <name>mip_check_precision</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a20e09d2229f97e58fff20a1486511394</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_mip_check_precision</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a30429f43611d2decc0e537421f7c5c0f</anchor>
      <arglist>(double value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aaed79e80ca86d9fb3638f5a9e2b32ac2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const SatParameters &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac474f81bae9a999247622271427161b3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac19faad538e99354ef962f1c490fb1cc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const SatParameters *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8e5209e73e62a8eaf07549fca0ef17ac</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>VariableOrder_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9a22b6196297096eb54a8d6375275112</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>VariableOrder_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afe4d4a18e63003d315d561d6e49ea2ca</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>VariableOrder_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a12cbd225eb2ca67e70f5a579546633a0</anchor>
      <arglist>(VariableOrder value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>VariableOrder_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a71447e0d785124feaec093711f57854b</anchor>
      <arglist>(const ::std::string &amp;name, VariableOrder *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>Polarity_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1143efde7d013f8220838ea2ae71c887</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>Polarity_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a43c446efc51c8bd3e5fea028c66cd730</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>Polarity_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5a9c5543625d0b37fe4e64a809e45f52</anchor>
      <arglist>(Polarity value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>Polarity_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0452cd51658f6c3edc48b14dca6fb143</anchor>
      <arglist>(const ::std::string &amp;name, Polarity *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ConflictMinimizationAlgorithm_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3b399cf7366676017073c0f36773138c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>ConflictMinimizationAlgorithm_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeb399103bedf4b050dc9ea68ebf46e1e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>ConflictMinimizationAlgorithm_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac4fdc023451067acb40ce0ab6acb13a9</anchor>
      <arglist>(ConflictMinimizationAlgorithm value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ConflictMinimizationAlgorithm_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ade8d4fe9dae2d95d04cbb75d94b882d8</anchor>
      <arglist>(const ::std::string &amp;name, ConflictMinimizationAlgorithm *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>BinaryMinizationAlgorithm_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a99a3bc35f1ee438d5034f173476c2232</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>BinaryMinizationAlgorithm_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8966e1478b1c92b8d3fb9b0ef2bd6c42</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>BinaryMinizationAlgorithm_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0020d12962e21d30b735dd18426d26d9</anchor>
      <arglist>(BinaryMinizationAlgorithm value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>BinaryMinizationAlgorithm_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a04afc825b2be6b6406f21a5e40d34119</anchor>
      <arglist>(const ::std::string &amp;name, BinaryMinizationAlgorithm *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ClauseProtection_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad2d5ffae53df56c4630ffc343851142c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>ClauseProtection_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a32b6ca5af81080412353ca50282cc557</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>ClauseProtection_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4e7e26d7d49a1cfdfacaf2238608ef6e</anchor>
      <arglist>(ClauseProtection value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ClauseProtection_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a75c70671cc09c6f0c1df11731e11f441</anchor>
      <arglist>(const ::std::string &amp;name, ClauseProtection *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ClauseOrdering_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab95be218b14b8d6d9bbf28a09f545a75</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>ClauseOrdering_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aea6353f3cda72a34a89116be6087c164</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>ClauseOrdering_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a437df16b9ca15e73e188e94a5363c5b4</anchor>
      <arglist>(ClauseOrdering value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>ClauseOrdering_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac4b379625407a03531940c603f6b33ea</anchor>
      <arglist>(const ::std::string &amp;name, ClauseOrdering *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>RestartAlgorithm_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6bc12f6092ceecd9403233f081e9ebcd</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>RestartAlgorithm_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3dc05fa5cdc2f995e2218f7c9bf73a37</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>RestartAlgorithm_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1c8b0f7a8aa9583ac016d9054f7b597c</anchor>
      <arglist>(RestartAlgorithm value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>RestartAlgorithm_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3202c943977c622d095ec4ffbcceffc4</anchor>
      <arglist>(const ::std::string &amp;name, RestartAlgorithm *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>MaxSatAssumptionOrder_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2ae770ff4ce1ef6d5c32cad224cd8fa0</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>MaxSatAssumptionOrder_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adc71dcb0b31c6d95033985af7135c87b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>MaxSatAssumptionOrder_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aea935941ff08301dd73f60930c35aad0</anchor>
      <arglist>(MaxSatAssumptionOrder value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>MaxSatAssumptionOrder_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a696b6d3f48af46ab32e35eab6beb44ee</anchor>
      <arglist>(const ::std::string &amp;name, MaxSatAssumptionOrder *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>MaxSatStratificationAlgorithm_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a266e70e69fc4191560e696d697d1f39b</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>MaxSatStratificationAlgorithm_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5c5cb3a3fb9ffc034ba78b9d02accef2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>MaxSatStratificationAlgorithm_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8e7c15deb129bd4ce0e63c0817bfeda0</anchor>
      <arglist>(MaxSatStratificationAlgorithm value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>MaxSatStratificationAlgorithm_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a09be63eaddc70dc7e4d5b6cf0a0e5a26</anchor>
      <arglist>(const ::std::string &amp;name, MaxSatStratificationAlgorithm *value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>SearchBranching_IsValid</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae849deb5401a33e85b973ef423548f83</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::EnumDescriptor *</type>
      <name>SearchBranching_descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a131679cbb21f9404089fba7c0beec697</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::std::string &amp;</type>
      <name>SearchBranching_Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a138705335715574c8c51c90da336bd53</anchor>
      <arglist>(SearchBranching value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static bool</type>
      <name>SearchBranching_Parse</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a094f12329b6c0d449d2d74bd6a424bbd</anchor>
      <arglist>(const ::std::string &amp;name, SearchBranching *value)</arglist>
    </member>
    <member kind="variable">
      <type>static ::google::protobuf::internal::ExplicitlyConstructed&lt;::std::string &gt;</type>
      <name>_i_give_permission_to_break_this_code_default_default_restart_algorithms_</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a87cc48db7a45852491e2c497b1351c0a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afe1d2252f7a9c2589c01399d4453d51c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableOrder</type>
      <name>IN_ORDER</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aab7c39c7967989bc649796f7d0895f11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableOrder</type>
      <name>IN_REVERSE_ORDER</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abd72c94ebf8070bb83c7c65cb40344a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableOrder</type>
      <name>IN_RANDOM_ORDER</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7026e9d849b6edc6004041381574bed7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableOrder</type>
      <name>VariableOrder_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a096aacfba17e165157532d06fb0d5a97</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const VariableOrder</type>
      <name>VariableOrder_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a751655caacc2971f471ff5a696622a93</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>VariableOrder_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a22ed1e37a58cc754ceecaa75ff8869a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Polarity</type>
      <name>POLARITY_TRUE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4d725db13d7dfeb831b8fd3a7555b14b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Polarity</type>
      <name>POLARITY_FALSE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa313ce97ef3038b27bdf7d51f02cc09e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Polarity</type>
      <name>POLARITY_RANDOM</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5cf220ea3cfb9980aa790af3c9e9569f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Polarity</type>
      <name>POLARITY_WEIGHTED_SIGN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac90816275d11367a0e0c32c5b26158a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Polarity</type>
      <name>POLARITY_REVERSE_WEIGHTED_SIGN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa6742ee90c34b205a5c0cfe17191ab68</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Polarity</type>
      <name>Polarity_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a84a5fee9d824f0d8aebb23185757de39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const Polarity</type>
      <name>Polarity_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa34eda498b6cc1768de190f6b1bdc1bf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>Polarity_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad35f31dfec1e267469d3516355378631</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ConflictMinimizationAlgorithm</type>
      <name>NONE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acfe8f027464d9683d13a613f6e64a888</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ConflictMinimizationAlgorithm</type>
      <name>SIMPLE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0dea1ec8fc4091058590032d8866a104</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ConflictMinimizationAlgorithm</type>
      <name>RECURSIVE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab0cb198805b7f786d8812e735bb08a94</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ConflictMinimizationAlgorithm</type>
      <name>EXPERIMENTAL</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aeaef638996b6f51cebebbc73658f5aad</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ConflictMinimizationAlgorithm</type>
      <name>ConflictMinimizationAlgorithm_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9fdd8a0fb42b0abe3ade65879a54e858</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ConflictMinimizationAlgorithm</type>
      <name>ConflictMinimizationAlgorithm_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a06b4cdfe9d6815efa2db7339340129b4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>ConflictMinimizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a99e141ddd096a6d07b42823fa6e8f3ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const BinaryMinizationAlgorithm</type>
      <name>NO_BINARY_MINIMIZATION</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af40696f27c44fe19b067c6d8bb2da17b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const BinaryMinizationAlgorithm</type>
      <name>BINARY_MINIMIZATION_FIRST</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a69007d20eb576d240318e5e7a6bde061</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const BinaryMinizationAlgorithm</type>
      <name>BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa79b9c9d2c5a5f57f6dfe8063bcaed32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const BinaryMinizationAlgorithm</type>
      <name>BINARY_MINIMIZATION_WITH_REACHABILITY</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a59f1b5df734beb3bba823a8903c87678</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const BinaryMinizationAlgorithm</type>
      <name>EXPERIMENTAL_BINARY_MINIMIZATION</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6918b20342fb3a7e1aed146137ab2f64</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const BinaryMinizationAlgorithm</type>
      <name>BinaryMinizationAlgorithm_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aef8529ff207405baba83f4680ed0f383</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const BinaryMinizationAlgorithm</type>
      <name>BinaryMinizationAlgorithm_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adf8dd60fb14b4f08cd5f651b38f79c77</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>BinaryMinizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5d3c53e24031b3a21e7147275ad29b39</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseProtection</type>
      <name>PROTECTION_NONE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aefc25ec61b528fcd02226705d5ea6217</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseProtection</type>
      <name>PROTECTION_ALWAYS</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a941cb40b2f39ef4efeebb1e70f8ff937</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseProtection</type>
      <name>PROTECTION_LBD</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8efc4b1f266490d69e39018ae138949b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseProtection</type>
      <name>ClauseProtection_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae316c121bce984d78d3b24c13a841f58</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseProtection</type>
      <name>ClauseProtection_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6635ac1dde37a5f23419e6e377d1e4bf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>ClauseProtection_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5aff806eddf4c716950dd02389957201</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseOrdering</type>
      <name>CLAUSE_ACTIVITY</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3993fcfa9890a765a3525ba8f53c7538</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseOrdering</type>
      <name>CLAUSE_LBD</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a882ac7be48af6c9c5e4d9701b27faf21</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseOrdering</type>
      <name>ClauseOrdering_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac65f112c37e68734baa6537ac4a904e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ClauseOrdering</type>
      <name>ClauseOrdering_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0e75cfa647164057bbd3b316a837fddd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>ClauseOrdering_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a07da5cc47f8e6cc6db6be5e055580ab5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const RestartAlgorithm</type>
      <name>NO_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a039ae13cd6b782e1f99678d4d28015a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const RestartAlgorithm</type>
      <name>LUBY_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3b12089e6ab77ba0ccce005404b6bdaa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const RestartAlgorithm</type>
      <name>DL_MOVING_AVERAGE_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0a52bb15631c83768272904bfcfe9624</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const RestartAlgorithm</type>
      <name>LBD_MOVING_AVERAGE_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a548bf4dbe7fe1edc79c4b0ff0ccb1e8b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const RestartAlgorithm</type>
      <name>FIXED_RESTART</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a72bdf170f7b41fc06597fbcc01fe3025</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const RestartAlgorithm</type>
      <name>RestartAlgorithm_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abb7bcdf9aad20babd08b21fabc28848d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const RestartAlgorithm</type>
      <name>RestartAlgorithm_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8dbc16942213b9aaf1b2dc8f887ca2b4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>RestartAlgorithm_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae721a71cfc3a975c1204bdb7d4f008d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatAssumptionOrder</type>
      <name>DEFAULT_ASSUMPTION_ORDER</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad7b7f934e8517cf796868eae9b3d2754</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatAssumptionOrder</type>
      <name>ORDER_ASSUMPTION_BY_DEPTH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6c958cd2b6193a939404d328049b2c42</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatAssumptionOrder</type>
      <name>ORDER_ASSUMPTION_BY_WEIGHT</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a241a9b2e33328b7462841a00de8b98ac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatAssumptionOrder</type>
      <name>MaxSatAssumptionOrder_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a70bb2afa1b27329c604e668f04d92e69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatAssumptionOrder</type>
      <name>MaxSatAssumptionOrder_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1c83568d861a69277f8048192f6bbc05</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>MaxSatAssumptionOrder_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a549d159c9ca81f44357b8b0aaee3754d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatStratificationAlgorithm</type>
      <name>STRATIFICATION_NONE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a41af0908f9b89fe55649767609040acb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatStratificationAlgorithm</type>
      <name>STRATIFICATION_DESCENT</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af8c3f740d6416c55d029b1fc1cc208f6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatStratificationAlgorithm</type>
      <name>STRATIFICATION_ASCENT</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a52849202bd1f8caa584392c7894676da</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatStratificationAlgorithm</type>
      <name>MaxSatStratificationAlgorithm_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aaeecf950f2739bbf56ce999d68d13309</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const MaxSatStratificationAlgorithm</type>
      <name>MaxSatStratificationAlgorithm_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3264acff21635894db666781f77205f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>MaxSatStratificationAlgorithm_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6876ba2c6d0ad20aea70014f09f2f267</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SearchBranching</type>
      <name>AUTOMATIC_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac9deaec5fe4bf4180c402e61d23b82f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SearchBranching</type>
      <name>FIXED_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4890ad17eafc5aa7b1dcd7818a18d601</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SearchBranching</type>
      <name>PORTFOLIO_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afb1ea4767376332394160940313806ac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SearchBranching</type>
      <name>LP_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab4804e8357aac35b14a37dfd871d3ce2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SearchBranching</type>
      <name>PSEUDO_COST_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1e2441ec2726b8957f3a9b0cc6ac5b4f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SearchBranching</type>
      <name>PORTFOLIO_WITH_QUICK_RESTART_SEARCH</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6eda43a2cf694cb7856dde222e1fc87b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SearchBranching</type>
      <name>SearchBranching_MIN</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad6fefc3c15637bd5b72f5658cf70d80b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const SearchBranching</type>
      <name>SearchBranching_MAX</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adb4b9d057fde96f94f7c0bab1d203346</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>SearchBranching_ARRAYSIZE</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a93e71a28216f0a05511cc130748fb96e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRestartAlgorithmsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af406978849dbaf1f05a18efaad2f007a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kDefaultRestartAlgorithmsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa18b9339a967048dd17e6ad8966007ab</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPreferredVariableOrderFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac6096a631e14783bf6093a9fbc81946f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxSatAssumptionOrderFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a240437a4d6b5d1effdb6275738c5eaa7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRandomBranchesRatioFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a09fc51778ff5af0e840ee8135e1b3b20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRandomPolarityRatioFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3a81f3cc125d801cbeb65a65f7e80eb2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUsePbResolutionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae02370c1bfb0e8d9839c92d8d057fb11</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMinimizeReductionDuringPbResolutionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9349fac1453b77592ef8ab1fab57a10b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxSatReverseAssumptionOrderFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad2a4d61d0ffc23be2d34c48f81ec9cf0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseOverloadCheckerInCumulativeConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a415e27d44533108ee47e0c984306691e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kClauseCleanupProtectionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8f3de5a600785985e5ddad0a80066333</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kClauseCleanupOrderingFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a790c3a37c12afeb44e6b0b2dd08778b7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseErwaHeuristicFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2e8655cf53b8967091193fb91734c7f5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAlsoBumpVariablesInConflictReasonsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aefca97e1e69617e21fee4c937df712e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseBlockingRestartFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a816fb45e88a2d65ae8974edd40278a73</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLogSearchProgressFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3fafefd6a11b06d24c3d86aabab96f46</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kStrategyChangeIncreaseRatioFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afe812a51c52ba091a8b723ecb9eef7b3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNumConflictsBeforeStrategyChangesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a208e33992f0ed37dc27aef55428300d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSearchBranchingFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae360fe68ef818842aeda8c5ffbd44325</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kInitialVariablesActivityFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>acbe59660690fe3b487d644239d169582</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kExploitBestSolutionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a22f1ca83c99ef1d393e1cadf067ce649</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kOptimizeWithCoreFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad30000218a9902a47c3199f15e5acfcd</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kOptimizeWithMaxHsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad36f3824084eabd2707844617bcb0b1d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kEnumerateAllSolutionsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a726b7d0448386547c4584189bf39b0a0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseTimetableEdgeFindingInCumulativeConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aef0dc3877fb1c91c20a8894c0169f9ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kOnlyAddCutsAtLevelZeroFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a62a00fdc77c5f05a27612544f1211a7f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAddKnapsackCutsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a86a234a126daf33bb2c92c9c26316193</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAddCgCutsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0055ab3fd1ddc3d483982c86bc40addc</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSearchRandomizationToleranceFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a69a80db1e62d7f5d68c64594af003633</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNumSearchWorkersFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a43fcfbbe1a76b248772e258ce94c3dc2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kFillTightenedDomainsInResponseFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abfd68ddad1aeee5fb1a9a7998d673067</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kStopAfterFirstSolutionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8cfe5215106bf66580e10df762d8df14</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseLnsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab09a7ef3e5f8c1cd0cfce826993f56d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLnsFocusOnDecisionVariablesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a28b7f6e706775e4aca56efa199f24895</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseRinsLnsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a23df77ed9b4c43561c0b77d774186703</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRandomizeSearchFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1c5b8a718fe62e6426ee9d44207ed459</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseCombinedNoOverlapFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0fce1691547c5fac1bad1e5247a0ec08</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMinOrthogonalityForLpConstraintsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a82c16fd139d1d22bc3e3e128a0b6367e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMipMaxActivityExponentFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abcf76f881b9962d96f0baf096eae78be</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kInitialPolarityFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a40b1f1fc705be587ab8403dcb72cc64d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMinimizationAlgorithmFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a196fc810175e7ea00a062ce08900db5b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kClauseCleanupPeriodFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adc73d56348c84c4b8320476a068776d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kClauseCleanupTargetFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a96ff9c0cf9eeda87f5b74dc448d6f8a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVariableActivityDecayFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3a1dde65b02a8e3879b317f6297fc684</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxVariableActivityValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8f444cb50707fe4ea62d3fea778a0101</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kClauseActivityDecayFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a28f1a3da8023995652635d5e80e2e414</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxClauseActivityValueFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>abf78313f09e9309d3737e0a45a008d81</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kGlucoseMaxDecayFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab102db024a333b6d30951a64c6c570f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kGlucoseDecayIncrementFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>adb991538acefb7b841eceda74bd09dad</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kGlucoseDecayIncrementPeriodFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab6bd36ab5e8ac74142aca989ed63d427</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRestartPeriodFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aed44c38247322dd8841dad88c93ced63</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRandomSeedFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2ce767a0c9a98c33f7694d052d051538</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBinaryMinimizationAlgorithmFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3dd7eda0cca39fe6162cdaab1feae91f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxTimeInSecondsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a6a3f411ee8002c8162f380d6a0108867</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxNumberOfConflictsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a607fa966f475c19710044b12f6949bf0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxMemoryInMbFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a379946c64e5b975bd65828a5b53368e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUsePhaseSavingFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0b576e388d027f867ffcfa7c502110fa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kSubsumptionDuringConflictAnalysisFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8cf1dd5098d87cad2eec2b16b64de124</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kTreatBinaryClausesSeparatelyFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aafa7017995df0a226a5d767acaea64fa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCountAssumptionLevelsInLbdFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af9971f7d7c6b62ada1b44fad08ecd3e1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPbCleanupIncrementFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3886c59f2c1646fee2edfb881f3fc587</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPbCleanupRatioFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ae5400508093140a13156f1ef01f418da</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxSatStratificationFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a33faf4f3bcff96f0102884483264e7fe</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPresolveBveThresholdFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9c50ae63bf0b2e21fb79208240cbdc85</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPresolveProbingDeterministicTimeLimitFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a150c09000b2685971f8835fe40d895f9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPresolveBveClauseWeightFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2a9eb3419a8c704edf57a5ce411214ef</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kClauseCleanupLbdBoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afeda0c2eebd8b2dbf1b90cc22998e9d6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPresolveBlockedClauseFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a38adc13c08d63cbd90fd628e1962c738</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPresolveUseBvaFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>addfd4f6c3878fc8e598fefea7dd24176</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseOptimizationHintsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4cbdfb50d91fbf0b4696c88475540857</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMinimizeCoreFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a4039a8baf98b25e39f40539ed26d3fba</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRestartRunningWindowSizeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7e8a63a5ec12cfbc18ec13994eee4ceb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRestartDlAverageRatioFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a512767e3c3d5d19bf4a578d63a9b05a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBlockingRestartMultiplierFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0ffa5f515b8f6f150435baaec6797bb5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxDeterministicTimeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a22e17703669bb6aba37ca9f3f14adda0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBlockingRestartWindowSizeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a7c319281fb3e6e20b066ab9e06540f32</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPresolveBvaThresholdFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2ceb1aecb2eb3a62699db14f9b2c8673</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kRestartLbdAverageRatioFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa657344ae91604666c7dd3f07110dcf5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kFindMultipleCoresFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a8a3ff5a9f7ca07ad320f8de27dd11a58</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCoverOptimizationFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2634c1499952d10cea22a5d1ab19c73c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUsePrecedencesInDisjunctiveConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0358c4037b7b7cb9f643e2acccaa0a8e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseDisjunctiveConstraintInCumulativeConstraintFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa71d4a2192cbd114147053b8b2745543</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLinearizationLevelFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aae8c81000a2228572ddc74e3e9c415d4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxNumCutsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aefa6114bd3c7b0578efb5ec00614f3ac</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMinimizeWithPropagationRestartPeriodFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ad1cd45388a7e26653eab139dac6f1ed7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMinimizeWithPropagationNumDecisionsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a961fcd17f83b9be7e1f95c77452c572c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBinarySearchNumConflictsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a3b5ecd6ba591e92b8d3474b6717fdb2b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kLnsNumThreadsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a90c8b989cc5e7f1c0c02a24718d5ff87</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kExploitAllLpSolutionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa6520cf3a4a925d78e5e58e9024356e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kExploitObjectiveFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5ac25d04135b98ac44a5c81890d5decf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCpModelPresolveFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a56caad7ec47914cfebe6ba0109399208</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCpModelUseSatPresolveFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a960c984d17615b6a0a60b2301094266d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kBooleanEncodingLevelFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a647017b4c2949fff125306b7c209df3e</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kInstantiateAllVariablesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0bc0f8e47b7b03ac681ff57ada4217e3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAutoDetectGreaterThanAtLeastOneOfFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac9e4b7f758aeb090cb697e2c4554fa9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kShareObjectiveBoundsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9238452bb7b724e4d2985c484574c6f4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kShareLevelZeroBoundsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2729588aea2170294ae939f89d5730fa</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseOptionalVariablesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ab235b1dadc66cfdcc2028440e484fff4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseExactLpReasonFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>ac7683b3a2936667b0717e99694dfab20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kCpModelProbingLevelFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>afbf561236292602be18854f2bb02f29b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAddMirCutsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af2ca8644501d2e1dccf12eb02413fba9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kUseMirRoundingFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9867f6aec9f6d3b99b7137234b4f331f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kAddLpConstraintsLazilyFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a216226e11c16f5aaf91b743a9498d75d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kExploitIntegerLpSolutionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a890e3e04001aab3224a754f07592ec9f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxIntegerRoundingScalingFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a2bf7fbe9acaffe79c4300ec8f3ae2162</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMaxInactiveCountFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a32bffbc55e2a874adc8c1280963dff20</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kConstraintRemovalBatchSizeFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>af1ce6e04c2532019f78de4d6d03235a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kPseudoCostReliabilityThresholdFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1e22c32f0841fa785744eca0e5177c0d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMipMaxBoundFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a5495443c4f03536e063974b5a39d04e7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMipVarScalingFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a9fc3c9ced470583594048a471748ec4b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMipWantedPrecisionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>aa559086c79e7d1dab18575ee351054e4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kMipCheckPrecisionFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0a19f4ba1b97ab1cd80e227b41e4fbae</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fsat_5fparameters_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a1f3d169deac565ec8fc486cd2aaf6270</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1SatParameters.html</anchorfile>
      <anchor>a0e1f473202adcb674fa3c816fdd7c707</anchor>
      <arglist>(SatParameters &amp;a, SatParameters &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::SortedDisjointIntervalList</name>
    <filename>classoperations__research_1_1SortedDisjointIntervalList.html</filename>
    <class kind="struct">operations_research::SortedDisjointIntervalList::IntervalComparator</class>
    <member kind="typedef">
      <type>std::set&lt; ClosedInterval, IntervalComparator &gt;</type>
      <name>IntervalSet</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>aa2245d981f2499b2948864f9e717b638</anchor>
      <arglist></arglist>
    </member>
    <member kind="typedef">
      <type>IntervalSet::iterator</type>
      <name>Iterator</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>aad3f138b3bec53adabe26c6d8a3f8b4b</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SortedDisjointIntervalList</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a1d800ef9b7bcda7b2bd88941e63e9c0d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SortedDisjointIntervalList</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a1112195975448318ee0d3a0ca17d8077</anchor>
      <arglist>(const std::vector&lt; ClosedInterval &gt; &amp;intervals)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SortedDisjointIntervalList</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a85b8188b348ebff82e5ff38ef49da201</anchor>
      <arglist>(const std::vector&lt; int64 &gt; &amp;starts, const std::vector&lt; int64 &gt; &amp;ends)</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>SortedDisjointIntervalList</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>ac2174bbccd9beafcfc3e3009264b968f</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;starts, const std::vector&lt; int &gt; &amp;ends)</arglist>
    </member>
    <member kind="function">
      <type>SortedDisjointIntervalList</type>
      <name>BuildComplementOnInterval</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>ac13367be885072fd7068f25fc9b726af</anchor>
      <arglist>(int64 start, int64 end)</arglist>
    </member>
    <member kind="function">
      <type>Iterator</type>
      <name>InsertInterval</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a3ff158f563c55210b76f860b30274daf</anchor>
      <arglist>(int64 start, int64 end)</arglist>
    </member>
    <member kind="function">
      <type>Iterator</type>
      <name>GrowRightByOne</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>aede8a3e747198ce6223d9f2ed788a3b3</anchor>
      <arglist>(int64 value, int64 *newly_covered)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>InsertIntervals</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>ab2f22ce88ed4581b3e4859a2e4504e9e</anchor>
      <arglist>(const std::vector&lt; int64 &gt; &amp;starts, const std::vector&lt; int64 &gt; &amp;ends)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>InsertIntervals</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>ad937d5864aa7a209f5c565a49daad904</anchor>
      <arglist>(const std::vector&lt; int &gt; &amp;starts, const std::vector&lt; int &gt; &amp;ends)</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>NumIntervals</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a6d5b1054cf1c7338bfd929bf2460140c</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>Iterator</type>
      <name>FirstIntervalGreaterOrEqual</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a32e16ab60594417366e26339a1db03f3</anchor>
      <arglist>(int64 value) const</arglist>
    </member>
    <member kind="function">
      <type>Iterator</type>
      <name>LastIntervalLessOrEqual</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a0b57e677ac61bf5768d95bc64c0a4e4a</anchor>
      <arglist>(int64 value) const</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>DebugString</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a403ba2c2147b5f9086ca988671dd00fd</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const Iterator</type>
      <name>begin</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>aa0c89218fd155f0cbe89cbfa05e35afc</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const Iterator</type>
      <name>end</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>ae7c2e3e4d3cddf60fd1e5340a956e716</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ClosedInterval &amp;</type>
      <name>last</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a25eab92a71b4bb1fa45e9ce37c9b51f4</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>ab770345669a4b37c7df83dee5ebceb04</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1SortedDisjointIntervalList.html</anchorfile>
      <anchor>a92df806fd079d25dbff73b57d36485f0</anchor>
      <arglist>(SortedDisjointIntervalList &amp;other)</arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::TableConstraint</name>
    <filename>classoperations__research_1_1sat_1_1TableConstraint.html</filename>
    <base>operations_research::sat::Constraint</base>
    <member kind="function">
      <type>void</type>
      <name>AddTuple</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraint.html</anchorfile>
      <anchor>a90017a38e8ac8eaf4644bdce5e5e1420</anchor>
      <arglist>(absl::Span&lt; const int64 &gt; tuple)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9052e9e1dd8248433909b5542f314add</anchor>
      <arglist>(absl::Span&lt; const BoolVar &gt; literals)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>OnlyEnforceIf</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>ab6457905c9a8cd1c5f92738d57e6f298</anchor>
      <arglist>(BoolVar literal)</arglist>
    </member>
    <member kind="function">
      <type>Constraint</type>
      <name>WithName</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9401ab195650160402df5b61f8ac9bda</anchor>
      <arglist>(const std::string &amp;name)</arglist>
    </member>
    <member kind="function">
      <type>const std::string &amp;</type>
      <name>Name</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aeaf30f4ee7d141e68905f1ac2432b937</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>const ConstraintProto &amp;</type>
      <name>Proto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>aa0b277df64333f670b66c8d5295b8250</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>ConstraintProto *</type>
      <name>MutableProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>acaa17b2fbfd62f6845329ae944835654</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="variable" protection="protected">
      <type>ConstraintProto *</type>
      <name>proto_</name>
      <anchorfile>classoperations__research_1_1sat_1_1Constraint.html</anchorfile>
      <anchor>a9d74c3d77f601020ab87700745f830ad</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend" protection="private">
      <type>friend class</type>
      <name>CpModelBuilder</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraint.html</anchorfile>
      <anchor>ae04c85577cf33a05fb50bb361877fb42</anchor>
      <arglist></arglist>
    </member>
  </compound>
  <compound kind="class">
    <name>operations_research::sat::TableConstraintProto</name>
    <filename>classoperations__research_1_1sat_1_1TableConstraintProto.html</filename>
    <member kind="function">
      <type></type>
      <name>TableConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a16e43cab707033fdf695a0495dd6d8bb</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" virtualness="virtual">
      <type>virtual</type>
      <name>~TableConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>af94d1e4fd8f9d37f713239b7c7057831</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type></type>
      <name>TableConstraintProto</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ab8922334fd4044502e91f4e4389eabae</anchor>
      <arglist>(const TableConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>TableConstraintProto &amp;</type>
      <name>operator=</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a1c026db493b5064e9ce685013912e67f</anchor>
      <arglist>(const TableConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>Swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a1b5c2aeb972d2fb796abd5332db49cad</anchor>
      <arglist>(TableConstraintProto *other)</arglist>
    </member>
    <member kind="function">
      <type>TableConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a1b28444b983563a9b2242d6601cf81d8</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>TableConstraintProto *</type>
      <name>New</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ac32c367b2e64008e84ed459e6b36b337</anchor>
      <arglist>(::google::protobuf::Arena *arena) const final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a29595acc87491e4baa443dff8a553085</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a1c8f7f2a4102935aab12103884c93f6a</anchor>
      <arglist>(const ::google::protobuf::Message &amp;from) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>CopyFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a3c984e0c045468bb116785cb126dfed1</anchor>
      <arglist>(const TableConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>MergeFrom</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a20e715aca6f96aaccddbe64a5d710e49</anchor>
      <arglist>(const TableConstraintProto &amp;from)</arglist>
    </member>
    <member kind="function">
      <type>PROTOBUF_ATTRIBUTE_REINITIALIZES void</type>
      <name>Clear</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>af4b28509dc8689461709c0127f4853f1</anchor>
      <arglist>() final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IsInitialized</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a0c3fb26434ef06d7a71e6c47d21e839d</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>size_t</type>
      <name>ByteSizeLong</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a08692ee327925e51c39c148f1d5a6daa</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>MergePartialFromCodedStream</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a4acfab8c4cb91a62bdb315ee01cbc399</anchor>
      <arglist>(::google::protobuf::io::CodedInputStream *input) final</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SerializeWithCachedSizes</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>aa3d5038f619f4d26df68912643c9c19b</anchor>
      <arglist>(::google::protobuf::io::CodedOutputStream *output) const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::uint8 *</type>
      <name>InternalSerializeWithCachedSizesToArray</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>aad65e0b4426550b418a8581478c6a956</anchor>
      <arglist>(::google::protobuf::uint8 *target) const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>GetCachedSize</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>aa4c3d4029ea446b64a0f569058bf7ce6</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::Metadata</type>
      <name>GetMetadata</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>af70593a7881923531e6ec0b6bd633b0e</anchor>
      <arglist>() const final</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>vars_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a8916d9c73976298b3417d1c95db1b7e3</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a3d09f346c980a6d11cc9897b084334dd</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int32</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>af08a9d644b6c6b919a525d93eaecdc7f</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a99f424f79fa01716a8973836a6403c9a</anchor>
      <arglist>(int index, ::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a7816ae8513ee482a4388cb15208a2482</anchor>
      <arglist>(::google::protobuf::int32 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; &amp;</type>
      <name>vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a7ff12db3864cbbbd9ea188f0ea1b131a</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int32 &gt; *</type>
      <name>mutable_vars</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a1f5e2d436e0bc4c401097d5afc844efc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>int</type>
      <name>values_size</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>aa5f68514950fc8b3893411a889477e31</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a2749177005e30925464a17eb760d8e2d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::int64</type>
      <name>values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ad44be45403c52030c76eb10d03dc287c</anchor>
      <arglist>(int index) const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>aae2e73a33d7caf8229a98777b764038b</anchor>
      <arglist>(int index, ::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>add_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a5b3b062000ff1eccafe2da65be87ec2c</anchor>
      <arglist>(::google::protobuf::int64 value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; &amp;</type>
      <name>values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a704cd614efff326dd36876f219f8aeed</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>::google::protobuf::RepeatedField&lt; ::google::protobuf::int64 &gt; *</type>
      <name>mutable_values</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a5a023305323cffb9478f0bd32376e2b9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>clear_negated</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ac4e476033d9763fbb9262227431988fc</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>negated</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a399b9373cde8f9b9b12477f04674445f</anchor>
      <arglist>() const</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>set_negated</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ae2b3c294de412dda1f23c4b6285291f5</anchor>
      <arglist>(bool value)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::Descriptor *</type>
      <name>descriptor</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a348929fafab761e7a99660c2b81cc05f</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const TableConstraintProto &amp;</type>
      <name>default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>af2a784dd805035380e82f86c3333994a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static void</type>
      <name>InitAsDefaultInstance</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ae50dd96ebfe7243c9ccbabef50e02a5b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const TableConstraintProto *</type>
      <name>internal_default_instance</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ac94e8ee9c10f721d70842012ea869aba</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static constexpr int</type>
      <name>kIndexInFileMessages</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>a5fd83c0d5d8e0a4a2f04f0a19b15f416</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kVarsFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>ae14094907d6df98818e142ca972242b5</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kValuesFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>aec84e813091702a88437f3f7a2d32a9b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const int</type>
      <name>kNegatedFieldNumber</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>aa34757807168e251188f600630f9f8b9</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend struct</type>
      <name>::TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>afb8396aa773b2cf0b644f1ddf0f6f75f</anchor>
      <arglist></arglist>
    </member>
    <member kind="friend">
      <type>friend void</type>
      <name>swap</name>
      <anchorfile>classoperations__research_1_1sat_1_1TableConstraintProto.html</anchorfile>
      <anchor>adb928cd62412b93fef5e35aaa9723660</anchor>
      <arglist>(TableConstraintProto &amp;a, TableConstraintProto &amp;b)</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>TableStruct_ortools_2fsat_2fcp_5fmodel_2eproto</name>
    <filename>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</filename>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::internal::ParseTableField entries []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>ae2bf684da069ec17ebefa296adb22993</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::internal::AuxillaryParseTableField aux []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>a4a8ba0ded625913812da9d0b273f5c7f</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::internal::ParseTable schema [24]</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>aadbc72e8b80df0e57b1d038e2ebb5a5a</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::internal::FieldMetadata</type>
      <name>field_metadata</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>a2a956171ed889dbcef47d7258c058b89</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::internal::SerializationTable</type>
      <name>serialization_table</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>ac9df3dc13cf6f89405c66a081ffe833d</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::uint32</type>
      <name>offsets</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fcp__5fmodel__2eproto.html</anchorfile>
      <anchor>ace6ed23e4e261a5f5b41ec94ebe7ff29</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="struct">
    <name>TableStruct_ortools_2fsat_2fsat_5fparameters_2eproto</name>
    <filename>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</filename>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::internal::ParseTableField entries []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a45e4823caaa375e4256d4bef9f4ecc88</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::internal::AuxillaryParseTableField aux []</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a8e3aaa6a265314766ef0b541cfaf56e9</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="function" static="yes">
      <type>static const ::google::protobuf::internal::ParseTable schema [1]</type>
      <name>PROTOBUF_SECTION_VARIABLE</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a591b5d07c121c984719dbf4fcdd8cd50</anchor>
      <arglist>(protodesc_cold)</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::internal::FieldMetadata</type>
      <name>field_metadata</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a9d4b81cf8b41b0cfc7e999500b7d70f7</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::internal::SerializationTable</type>
      <name>serialization_table</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a1b323b885ae3884bcc86749b33bbbd04</anchor>
      <arglist>[]</arglist>
    </member>
    <member kind="variable" static="yes">
      <type>static const ::google::protobuf::uint32</type>
      <name>offsets</name>
      <anchorfile>structTableStruct__ortools__2fsat__2fsat__5fparameters__2eproto.html</anchorfile>
      <anchor>a0f4088c97bd013efcbc107dcb32d3fbd</anchor>
      <arglist>[]</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>google</name>
    <filename>namespacegoogle.html</filename>
    <namespace>google::protobuf</namespace>
  </compound>
  <compound kind="namespace">
    <name>google::protobuf</name>
    <filename>namespacegoogle_1_1protobuf.html</filename>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::CpSolverStatus &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_BinaryMinizationAlgorithm &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseOrdering &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ClauseProtection &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatAssumptionOrder &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_Polarity &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_RestartAlgorithm &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_SearchBranching &gt;</class>
    <class kind="struct">google::protobuf::is_proto_enum&lt; ::operations_research::sat::SatParameters_VariableOrder &gt;</class>
    <member kind="function">
      <type>::operations_research::sat::AllDifferentConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::AllDifferentConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab6c5ebe14cfc68d93a5f60686f2ae22d</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::AutomatonConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::AutomatonConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a47da04ba2be147be8b0a249d1127175f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::BoolArgumentProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::BoolArgumentProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ad530047c3866901687cad573a8902a36</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CircuitConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab8dbc8cdb17b07a5682228a84ca326a7</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CircuitCoveringConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CircuitCoveringConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a65ba1bb90bf8b69684824af54ed34061</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a194b7268d38aea43cf720189f2c7d933</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpModelProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpModelProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>aad6b5a46ab5d2233f555b7eaa7f9dc8b</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpObjectiveProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpObjectiveProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>afbd10e0381bdcea8db6a4b8b1ddda5b4</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CpSolverResponse *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CpSolverResponse &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>abdc2b7a036c638cad9b003b8e2ae38fb</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::CumulativeConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::CumulativeConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a8c7246d8fad339bf133ecf5ce8b70e6f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::DecisionStrategyProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab396a7c48de804df389f1fde37cd4aed</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::DecisionStrategyProto_AffineTransformation *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::DecisionStrategyProto_AffineTransformation &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a8dafed95c6efbf6296753a9a90923388</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ElementConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ElementConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a4e0bfccb327b7e1ef475d48d813554ac</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerArgumentProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntegerArgumentProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ad1456edebbb93b07e4cb7b231c6d5d1c</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntegerVariableProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntegerVariableProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a8e446e46683177ee44ab293e2c35231b</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::IntervalConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::IntervalConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a23fd3de0c47884bbebb25116ece5c2d7</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::InverseConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::InverseConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a6fd2c00fa691e2d0a3ec45cf883dfdf5</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::LinearConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::LinearConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>abe77e6dc60fd9e0d5c696b1b55c2fccd</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlap2DConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::NoOverlap2DConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab081583e505c7c4003cc7981f7bd354f</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::NoOverlapConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::NoOverlapConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>aa61f6fa8185bc8617023420148f33045</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::PartialVariableAssignment *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::PartialVariableAssignment &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a9605edce6c8d1b9f2b465ea3cf193e72</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::ReservoirConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::ReservoirConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>adb8db465df82459433570257339128c1</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::RoutesConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::RoutesConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ae061245ac4989a9fa86f211ccf1a94bb</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::TableConstraintProto *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::TableConstraintProto &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab58f5023a24725742e59513c8a5785e2</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::DecisionStrategyProto_VariableSelectionStrategy &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a932a088438a4a18cac0d84a50f9cef93</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::DecisionStrategyProto_DomainReductionStrategy &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ab7bf2119b197f54b7cfb237d392a3b31</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::CpSolverStatus &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a61f6bf84c590e6ff99427d674d30cc9c</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>::operations_research::sat::SatParameters *</type>
      <name>Arena::CreateMaybeMessage&lt;::operations_research::sat::SatParameters &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a5e3ba24b3733dab6d3aac5a8deec1fc6</anchor>
      <arglist>(Arena *)</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_VariableOrder &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>affaa52e637aa5700a18de9de15d50802</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_Polarity &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a7ce45884a30882618460722b3f5b6a63</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ConflictMinimizationAlgorithm &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a865dfb0c8234642d80df4b0f91914a54</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_BinaryMinizationAlgorithm &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a53653792bc958c9d29044e3ab139254b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ClauseProtection &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>ad34fd79c2176b0344e2a5a649cfcca5e</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_ClauseOrdering &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a5f0b29c0076ec4e010aeeb8bdb500a02</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_RestartAlgorithm &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>aa75c01445cca67a20648e0162d4d01b3</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_MaxSatAssumptionOrder &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>afa03ee919b72e3665921728ee15be26d</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_MaxSatStratificationAlgorithm &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a86ff4f7acb5908cc179e592e6dae80e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const EnumDescriptor *</type>
      <name>GetEnumDescriptor&lt; ::operations_research::sat::SatParameters_SearchBranching &gt;</name>
      <anchorfile>namespacegoogle_1_1protobuf.html</anchorfile>
      <anchor>a20bb637648e3cdc283c4e71ce06a3956</anchor>
      <arglist>()</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>operations_research</name>
    <filename>namespaceoperations__research.html</filename>
    <namespace>operations_research::sat</namespace>
    <class kind="struct">operations_research::ClosedInterval</class>
    <class kind="class">operations_research::Domain</class>
    <class kind="class">operations_research::SortedDisjointIntervalList</class>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>a5c341d9214d5d46014089435ba0e26d3</anchor>
      <arglist>(std::ostream &amp;out, const ClosedInterval &amp;interval)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>aaa301d39d2a9271daf8c65e779635335</anchor>
      <arglist>(std::ostream &amp;out, const std::vector&lt; ClosedInterval &gt; &amp;intervals)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>IntervalsAreSortedAndNonAdjacent</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>ab8c23924c4b61ed5c531424a6f18bde1</anchor>
      <arglist>(absl::Span&lt; const ClosedInterval &gt; intervals)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research.html</anchorfile>
      <anchor>abebf3070a940da6bf678953a66584e76</anchor>
      <arglist>(std::ostream &amp;out, const Domain &amp;domain)</arglist>
    </member>
  </compound>
  <compound kind="namespace">
    <name>operations_research::sat</name>
    <filename>namespaceoperations__research_1_1sat.html</filename>
    <class kind="class">operations_research::sat::AllDifferentConstraintProto</class>
    <class kind="class">operations_research::sat::AutomatonConstraint</class>
    <class kind="class">operations_research::sat::AutomatonConstraintProto</class>
    <class kind="class">operations_research::sat::BoolArgumentProto</class>
    <class kind="class">operations_research::sat::BoolVar</class>
    <class kind="class">operations_research::sat::CircuitConstraint</class>
    <class kind="class">operations_research::sat::CircuitConstraintProto</class>
    <class kind="class">operations_research::sat::CircuitCoveringConstraintProto</class>
    <class kind="class">operations_research::sat::Constraint</class>
    <class kind="class">operations_research::sat::ConstraintProto</class>
    <class kind="class">operations_research::sat::CpModelBuilder</class>
    <class kind="class">operations_research::sat::CpModelProto</class>
    <class kind="class">operations_research::sat::CpObjectiveProto</class>
    <class kind="class">operations_research::sat::CpSolverResponse</class>
    <class kind="class">operations_research::sat::CumulativeConstraint</class>
    <class kind="class">operations_research::sat::CumulativeConstraintProto</class>
    <class kind="class">operations_research::sat::DecisionStrategyProto</class>
    <class kind="class">operations_research::sat::DecisionStrategyProto_AffineTransformation</class>
    <class kind="class">operations_research::sat::ElementConstraintProto</class>
    <class kind="class">operations_research::sat::IntegerArgumentProto</class>
    <class kind="class">operations_research::sat::IntegerVariableProto</class>
    <class kind="class">operations_research::sat::IntervalConstraintProto</class>
    <class kind="class">operations_research::sat::IntervalVar</class>
    <class kind="class">operations_research::sat::IntVar</class>
    <class kind="class">operations_research::sat::InverseConstraintProto</class>
    <class kind="class">operations_research::sat::LinearConstraintProto</class>
    <class kind="class">operations_research::sat::LinearExpr</class>
    <class kind="class">operations_research::sat::Model</class>
    <class kind="class">operations_research::sat::NoOverlap2DConstraint</class>
    <class kind="class">operations_research::sat::NoOverlap2DConstraintProto</class>
    <class kind="class">operations_research::sat::NoOverlapConstraintProto</class>
    <class kind="class">operations_research::sat::PartialVariableAssignment</class>
    <class kind="class">operations_research::sat::ReservoirConstraint</class>
    <class kind="class">operations_research::sat::ReservoirConstraintProto</class>
    <class kind="class">operations_research::sat::RoutesConstraintProto</class>
    <class kind="class">operations_research::sat::SatParameters</class>
    <class kind="class">operations_research::sat::TableConstraint</class>
    <class kind="class">operations_research::sat::TableConstraintProto</class>
    <member kind="enumeration">
      <type></type>
      <name>DecisionStrategyProto_VariableSelectionStrategy</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_FIRST</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea5e00b7cd6b433ec6a15ff913d3b2c3f3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_LOWEST_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea0b1d456b36749d677aa4a201b22ba114</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_HIGHEST_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea79fc0af04ed454750ecb59dc5a748e88</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MIN_DOMAIN_SIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea18e573e60bf8dde6880a6cfb9f697ffc</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_CHOOSE_MAX_DOMAIN_SIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea9bc8cd090f555c04c4fb8ec23838dc30</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148ea77405cd855df69ed653be2766be0a1af</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_VariableSelectionStrategy_DecisionStrategyProto_VariableSelectionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10a32f85785b62ba65343391e575148eadecec94c9d1599ecbdfdab2f7cfcb7aa</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>DecisionStrategyProto_DomainReductionStrategy</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_MIN_VALUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4a2f416e6e94f971bfbb75ba25e7f7b760</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_MAX_VALUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4ac22896facd05595ce84133b3b3043685</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_LOWER_HALF</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4ab63e61aebddafddd1496d6ab577dab53</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_SELECT_UPPER_HALF</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4ac41d0ba8114af7179c253fda16e517ca</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4a82875a7d185a8f87d56cb0fb0f37f72a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>DecisionStrategyProto_DomainReductionStrategy_DecisionStrategyProto_DomainReductionStrategy_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adb7c9ce3ef722957ff56d0875e802fb4ac1c76a18c1405c9569b8afca29919e48</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>CpSolverStatus</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>UNKNOWN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12a693e3d1636a488a456c173453c45cc14</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>MODEL_INVALID</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12acb3300bde58b85d202f9c211dfabcb49</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>FEASIBLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12ae4d551fa942cba479e3090bb8ae40e73</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>INFEASIBLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12a185c2992ead7a0d90d260164cf10d46f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>OPTIMAL</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12a78e9c6b9f6ac60a9e9c2d25967ed1ad0</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CpSolverStatus_INT_MIN_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12a443f059ef1efc767e19c5724f6c161d9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>CpSolverStatus_INT_MAX_SENTINEL_DO_NOT_USE_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2392f4581af743a0af577069f99fed12ae535ad44840a077b35974e3a04530717</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_VariableOrder</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7500a48ce324f0ef41f39e45f60f214d</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7500a48ce324f0ef41f39e45f60f214da92760d7186df85dfd6c188eae0b9b591</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_REVERSE_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7500a48ce324f0ef41f39e45f60f214da941215af97625c63a144520ec7e02bfb</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_VariableOrder_IN_RANDOM_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7500a48ce324f0ef41f39e45f60f214da8de6cbc54e325b78d800c8354591d726</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_Polarity</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_TRUE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2a6145ecb76ca29dc07b9acde97866a8ee</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_FALSE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2a204c91561099609cdf7b6469e84e9576</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_RANDOM</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2afaf662755a533bc2353968b4c4da4d32</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_WEIGHTED_SIGN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2af9a6fbf18fc3445083ca746b1e920ca6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_Polarity_POLARITY_REVERSE_WEIGHTED_SIGN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>add5353b524feae6119c2a8220f1ca3d2a77094f18176663ceea0b80667cf917a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ConflictMinimizationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23bae1bd62c48ad8f9a7d242ae916bbe5066</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_SIMPLE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23bac1adcdd93b988565644ddc9c3510c96c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_RECURSIVE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23bacf7f9f878c3e92e4e319c3e4ea926af7</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ConflictMinimizationAlgorithm_EXPERIMENTAL</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ab616f5071426513fb5d7dd88f2b23ba52b205df52309c4f050206500297e4e5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_BinaryMinizationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ff</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_NO_BINARY_MINIMIZATION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffa5cefb853f31166cc3684d90594d5dde9</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffacefb9cb334d97dc99896de7db79a2476</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_FIRST_WITH_TRANSITIVE_REDUCTION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffac586955ded9c943dee2faf8b5b738dbd</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_BINARY_MINIMIZATION_WITH_REACHABILITY</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffa30c30629b82fa4252c40e28942e35416</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_BinaryMinizationAlgorithm_EXPERIMENTAL_BINARY_MINIMIZATION</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af925d9b6f4730729d935c3fad014c4ffaeb6a38e1f5f44d7f13c6f8d6325ba069</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ClauseProtection</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad24109146a44723f1c95b7d3f226fcc1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad24109146a44723f1c95b7d3f226fcc1a1739f0f3322dc59ebaa2fb9fa3481d6b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_ALWAYS</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad24109146a44723f1c95b7d3f226fcc1aa7de36c91e9668bd4d3429170a3a915a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseProtection_PROTECTION_LBD</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad24109146a44723f1c95b7d3f226fcc1a4ce148354b01f5b1e2da32e7576edaa3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_ClauseOrdering</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a31ad84fa962b626887890dd76a53c74a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseOrdering_CLAUSE_ACTIVITY</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a31ad84fa962b626887890dd76a53c74aaaab0bb6b57e109185e6a62d5d0271a04</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_ClauseOrdering_CLAUSE_LBD</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a31ad84fa962b626887890dd76a53c74aa2dcf758b7ee7431577e2aa80a60b163e</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_RestartAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_NO_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a698c5900a88697e89f1a9ffa790fd49f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_LUBY_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a0fcf1821b877dd61f6cfac37a36a82d8</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_DL_MOVING_AVERAGE_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a89e7ee47fc5c826c03f455f082f22c70</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_LBD_MOVING_AVERAGE_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a5d2302ed4086b87cadaad18aa5981aed</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_RestartAlgorithm_FIXED_RESTART</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a192662f710ae6cff2e00eff50ce55ac3a353691b5a40f70fe5d05cc01bdf22536</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_MaxSatAssumptionOrder</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a683a06394b218203a4517b19468df400</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_DEFAULT_ASSUMPTION_ORDER</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a683a06394b218203a4517b19468df400ab0500c1196441cd7820da82c2c1baf6f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_DEPTH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a683a06394b218203a4517b19468df400a61bc7845a56fecefcc18795a536d5eb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatAssumptionOrder_ORDER_ASSUMPTION_BY_WEIGHT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a683a06394b218203a4517b19468df400a44da070df5c6e2443fa1c00b6c25893f</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_MaxSatStratificationAlgorithm</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab342c0ecaab53f1e8e6cf05ca513b8d5</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_NONE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab342c0ecaab53f1e8e6cf05ca513b8d5a5bb7f0a112c4672ea2abec407f7d384c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_DESCENT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab342c0ecaab53f1e8e6cf05ca513b8d5a0c67cde78d6314de8d13734d65709b3a</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_MaxSatStratificationAlgorithm_STRATIFICATION_ASCENT</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab342c0ecaab53f1e8e6cf05ca513b8d5adf547628eb3421e641512aeb95b31912</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumeration">
      <type></type>
      <name>SatParameters_SearchBranching</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4b</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_AUTOMATIC_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4bac23498a3951b707b682de68c3f2ef4ba</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_FIXED_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4ba4b402cda1dee9234ecc9bf3f969dae9c</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PORTFOLIO_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4ba79d67aaf6b62f71bbddd9c5177ebedc1</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_LP_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4bac0ee72ff494861f949253aac50496f42</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PSEUDO_COST_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4ba0959d8f131e2610b97a8830464b2c633</anchor>
      <arglist></arglist>
    </member>
    <member kind="enumvalue">
      <name>SatParameters_SearchBranching_PORTFOLIO_WITH_QUICK_RESTART_SEARCH</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad9191a142de9cc0cca4248601387cb4ba28a2409f7a5ca2ecd6635da22e4e6667</anchor>
      <arglist></arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9c0ae0d048a431656985fc79428bbe67</anchor>
      <arglist>(std::ostream &amp;os, const BoolVar &amp;var)</arglist>
    </member>
    <member kind="function">
      <type>BoolVar</type>
      <name>Not</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5e3de118c1f8dd5a7ec21704e05684b9</anchor>
      <arglist>(BoolVar x)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a57b8aabbc5b3c1d177d35b3ebcf9b5fa</anchor>
      <arglist>(std::ostream &amp;os, const IntVar &amp;var)</arglist>
    </member>
    <member kind="function">
      <type>std::ostream &amp;</type>
      <name>operator&lt;&lt;</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae9f86b31794751c624a783d15306280c</anchor>
      <arglist>(std::ostream &amp;os, const IntervalVar &amp;var)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>SolutionIntegerValue</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aeaed9bdf2a27bb778ba397666cb874d7</anchor>
      <arglist>(const CpSolverResponse &amp;r, const LinearExpr &amp;expr)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>SolutionIntegerMin</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a671200a31003492dbef21f2b4ee3dcbd</anchor>
      <arglist>(const CpSolverResponse &amp;r, IntVar x)</arglist>
    </member>
    <member kind="function">
      <type>int64</type>
      <name>SolutionIntegerMax</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8ec893fa736de5b95135ecb9314ee6d8</anchor>
      <arglist>(const CpSolverResponse &amp;r, IntVar x)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SolutionBooleanValue</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afa415e372a9d64eede869ed98666c29c</anchor>
      <arglist>(const CpSolverResponse &amp;r, BoolVar x)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>CpModelStats</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a287579e5f181fc7c89feccf1128faffb</anchor>
      <arglist>(const CpModelProto &amp;model)</arglist>
    </member>
    <member kind="function">
      <type>std::string</type>
      <name>CpSolverResponseStats</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac2d87e8109f9c60f7af84a60106abd57</anchor>
      <arglist>(const CpSolverResponse &amp;response)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse</type>
      <name>SolveCpModel</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9d67b9c66f1cb9c1dcc3415cd5af11bf</anchor>
      <arglist>(const CpModelProto &amp;model_proto, Model *model)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse</type>
      <name>Solve</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a09d851f944ab4f305c3d9f8df99b7bf8</anchor>
      <arglist>(const CpModelProto &amp;model_proto)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse</type>
      <name>SolveWithParameters</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa3062797aa0396abf37dbcc99a746f12</anchor>
      <arglist>(const CpModelProto &amp;model_proto, const SatParameters &amp;params)</arglist>
    </member>
    <member kind="function">
      <type>CpSolverResponse</type>
      <name>SolveWithParameters</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af52c27ecb43d6486c1a70e022b4aad39</anchor>
      <arglist>(const CpModelProto &amp;model_proto, const std::string &amp;params)</arglist>
    </member>
    <member kind="function">
      <type>void</type>
      <name>SetSynchronizationFunction</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad04337634227eac006d3e33a7028f82f</anchor>
      <arglist>(std::function&lt; CpSolverResponse()&gt; f, Model *model)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9644b126f05b927a27fc7eba8e62dd57</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af66c861360ab3857d0bb2d53fde74bca</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2262e194393851724b02211c34c57457</anchor>
      <arglist>(DecisionStrategyProto_VariableSelectionStrategy value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af25eeda8a253dce34e0b0e98f69031ad</anchor>
      <arglist>(const ::std::string &amp;name, DecisionStrategyProto_VariableSelectionStrategy *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af161ecb897e60ce83c87c17d11ae7d91</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3bef95d750e0d2c4dcbf9944a6147232</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a15661f91c1c5635b462c569097268773</anchor>
      <arglist>(DecisionStrategyProto_DomainReductionStrategy value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab65df8c02daf63542fcee35b0a9f7779</anchor>
      <arglist>(const ::std::string &amp;name, DecisionStrategyProto_DomainReductionStrategy *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CpSolverStatus_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8f7f7995f8e9a03c15cdddf39b675702</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>CpSolverStatus_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aef4cfe27470b9d29843e9394cb75f33a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>CpSolverStatus_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a168a8ab6018d96c83fbd0d0ee03e087c</anchor>
      <arglist>(CpSolverStatus value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>CpSolverStatus_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a49458d152506001af5ad6ad1b7c8576e</anchor>
      <arglist>(const ::std::string &amp;name, CpSolverStatus *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_VariableOrder_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a711b59624fbd706f0754647084c665d8</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_VariableOrder_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a63d00708775015b761d79d26958ae008</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_VariableOrder_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aea11eda3bbcc4f79baab267009d28df6</anchor>
      <arglist>(SatParameters_VariableOrder value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_VariableOrder_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a628f11b71a7acbabf2c7eb0a55ebf04e</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_VariableOrder *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_Polarity_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4585806adf77d6f7a56bd21230a31175</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_Polarity_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab019ee0753776b26fed17764e82d23e9</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_Polarity_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aeb3937db56cace9b52fbb3ada9bfea73</anchor>
      <arglist>(SatParameters_Polarity value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_Polarity_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aacbaf337b8a87121b647c838bef22e1b</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_Polarity *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a90d6f173fbfa33e26ff6508013c81ffd</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0405a68dd3f67e20ca8c7b12d45cb870</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a729819ca8e41e5a7c95a32da63d75804</anchor>
      <arglist>(SatParameters_ConflictMinimizationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a184421f59216ca2ef58f282236cf8bc3</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_ConflictMinimizationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_BinaryMinizationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3e37f554c39fbb05faf07674ac550f47</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_BinaryMinizationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac6854e48c578db9f71a0c4a95dc95279</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_BinaryMinizationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af7cc36dac69bb4b7d7d5dacbf37e57ba</anchor>
      <arglist>(SatParameters_BinaryMinizationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_BinaryMinizationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa0292e780dbe4984839ecad4b44fccf0</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_BinaryMinizationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseProtection_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac1aa9d5ea93fbc96a68237c2beda3836</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_ClauseProtection_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afdfdf216dea1b6ca3cb4c816396f7493</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_ClauseProtection_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa46504e5e34f1716ac37b78ddc08b060</anchor>
      <arglist>(SatParameters_ClauseProtection value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseProtection_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a1558fb6c8e007b75889204116c149f78</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_ClauseProtection *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseOrdering_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa6f7c43106217e8a55877110b7d87e7c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_ClauseOrdering_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a38f0d79ca92d2252d62d8db8dfd1556a</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_ClauseOrdering_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a56d5fe6aa184be05f6092ab990f5250e</anchor>
      <arglist>(SatParameters_ClauseOrdering value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_ClauseOrdering_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad674863df7b9117f210c945f2674db58</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_ClauseOrdering *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_RestartAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab199957e5457d8356687f12d67d1aaac</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_RestartAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa2f24a25dc16dd685917069e6bb22b0b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_RestartAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9703a0efa39a7877735205de9a006c0f</anchor>
      <arglist>(SatParameters_RestartAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_RestartAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0960de8f477819a400cbd3a41062b9a2</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_RestartAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatAssumptionOrder_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4104fcd7cb88b2edc4cbc86e6b331cdf</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_MaxSatAssumptionOrder_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a828b06b1d9e9e57276c5092899592cd2</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_MaxSatAssumptionOrder_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3a48b4e764d3598485a64075cee904fa</anchor>
      <arglist>(SatParameters_MaxSatAssumptionOrder value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatAssumptionOrder_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3bc3e149fd0e1959e5805d7ad73ccff2</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_MaxSatAssumptionOrder *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5fcee51ba7784a7c403731301af6e14c</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a52f132562a3089063ffa35dc1c54f21b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a08c2c94217816891bec7180e5f6b50d3</anchor>
      <arglist>(SatParameters_MaxSatStratificationAlgorithm value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>affb8017c363df7be4c369908a6e1f90f</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_MaxSatStratificationAlgorithm *value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_SearchBranching_IsValid</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9018824bcc1b169f32af87ad4faf7561</anchor>
      <arglist>(int value)</arglist>
    </member>
    <member kind="function">
      <type>const ::google::protobuf::EnumDescriptor *</type>
      <name>SatParameters_SearchBranching_descriptor</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a136b498c164dea9e5a9829d1590cec7b</anchor>
      <arglist>()</arglist>
    </member>
    <member kind="function">
      <type>const ::std::string &amp;</type>
      <name>SatParameters_SearchBranching_Name</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aaf4dfaa6a41d60012b210e5587cbbf51</anchor>
      <arglist>(SatParameters_SearchBranching value)</arglist>
    </member>
    <member kind="function">
      <type>bool</type>
      <name>SatParameters_SearchBranching_Parse</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a992a00120a7ac841217f4561576cc354</anchor>
      <arglist>(const ::std::string &amp;name, SatParameters_SearchBranching *value)</arglist>
    </member>
    <member kind="variable">
      <type>std::function&lt; void(Model *)&gt;</type>
      <name>NewFeasibleSolutionObserver</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aacf15d440f0db4cd0a63c8aebe85db6d</anchor>
      <arglist>(const std::function&lt; void(const CpSolverResponse &amp;response)&gt; &amp;observer)</arglist>
    </member>
    <member kind="variable">
      <type>std::function&lt; SatParameters(Model *)&gt;</type>
      <name>NewSatParameters</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a10700832ca6bc420f2931eb707957b0b</anchor>
      <arglist>(const std::string &amp;params)</arglist>
    </member>
    <member kind="variable">
      <type>AllDifferentConstraintProtoDefaultTypeInternal</type>
      <name>_AllDifferentConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad5cadc3f160d3e34ef323536a36578ce</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>AutomatonConstraintProtoDefaultTypeInternal</type>
      <name>_AutomatonConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a89e105e8d30d25c4c680294fe7d572c1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>BoolArgumentProtoDefaultTypeInternal</type>
      <name>_BoolArgumentProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad05e4bcf8c4464c50e1f1b8af2b81ad2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CircuitConstraintProtoDefaultTypeInternal</type>
      <name>_CircuitConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a6a9352c8a15382c9206993a807ca1f97</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CircuitCoveringConstraintProtoDefaultTypeInternal</type>
      <name>_CircuitCoveringConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>adc89524c8aab967f7d4a66bd3ec70bca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>ConstraintProtoDefaultTypeInternal</type>
      <name>_ConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a946e95ccf1a9faf8270238f5c5b301fb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CpModelProtoDefaultTypeInternal</type>
      <name>_CpModelProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ace223c8e846b17ef993566562cec8dda</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CpObjectiveProtoDefaultTypeInternal</type>
      <name>_CpObjectiveProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>acfdc8eaa58fc4cf8b103821df60cd4e9</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CpSolverResponseDefaultTypeInternal</type>
      <name>_CpSolverResponse_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a13b87f99bbea144cc07cdcd2095ab601</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>CumulativeConstraintProtoDefaultTypeInternal</type>
      <name>_CumulativeConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aac6a8bda3dfe9f06ab9e4b5d0273df53</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>DecisionStrategyProtoDefaultTypeInternal</type>
      <name>_DecisionStrategyProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a1d42bd587a5323aaf16295be1dfa1455</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>DecisionStrategyProto_AffineTransformationDefaultTypeInternal</type>
      <name>_DecisionStrategyProto_AffineTransformation_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad0110b5023e714ba7608ca6393a28aee</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>ElementConstraintProtoDefaultTypeInternal</type>
      <name>_ElementConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4ef77bd2a03378993af8582adc081ae6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>IntegerArgumentProtoDefaultTypeInternal</type>
      <name>_IntegerArgumentProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a3dc76ede4b7ff0d2c5bd425c834e1a1b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>IntegerVariableProtoDefaultTypeInternal</type>
      <name>_IntegerVariableProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a44161c9b8ede2f098f009c6980c489a4</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>IntervalConstraintProtoDefaultTypeInternal</type>
      <name>_IntervalConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4215dda19ecaf7d9b3437190df671cbb</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>InverseConstraintProtoDefaultTypeInternal</type>
      <name>_InverseConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4903b3b9596898e507eadb8642d73b7d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>LinearConstraintProtoDefaultTypeInternal</type>
      <name>_LinearConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a35f06e6b931d091b424f42c8db845273</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>NoOverlap2DConstraintProtoDefaultTypeInternal</type>
      <name>_NoOverlap2DConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afc421996f32997364f39272a061499f0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>NoOverlapConstraintProtoDefaultTypeInternal</type>
      <name>_NoOverlapConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a75a5dfa26b4dc21981f4c6cc46ae9c43</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>PartialVariableAssignmentDefaultTypeInternal</type>
      <name>_PartialVariableAssignment_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a5fe88249a924da9eac41aefea5ddabed</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>ReservoirConstraintProtoDefaultTypeInternal</type>
      <name>_ReservoirConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac0865a57214595b3a38ceee49543b4a1</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>RoutesConstraintProtoDefaultTypeInternal</type>
      <name>_RoutesConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae1bf1cf3f7f77485b9d4c7ab4d6894ed</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>TableConstraintProtoDefaultTypeInternal</type>
      <name>_TableConstraintProto_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a1b5b8679bd9fed7c991d05c09cf01466</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a59941f8a574d610fbd0d2766daf437e2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const DecisionStrategyProto_VariableSelectionStrategy</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa936a57453c9681bab32e74a3747c5f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>DecisionStrategyProto_VariableSelectionStrategy_VariableSelectionStrategy_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0fe139f7887fdce2f0d82ba7bfe3b761</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8d89ba785675bf6374b216c6880cf89d</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const DecisionStrategyProto_DomainReductionStrategy</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a2446fab2d79c5ef3d9ab370d8be7519b</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>DecisionStrategyProto_DomainReductionStrategy_DomainReductionStrategy_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a98b9900acdb468cd47a37be6ec6fecce</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const CpSolverStatus</type>
      <name>CpSolverStatus_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a39d6196edcd5c594db5524b4fd1a9cad</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const CpSolverStatus</type>
      <name>CpSolverStatus_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad61de2d59ad12b07b65b1b2497542ea2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>CpSolverStatus_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9309f1a918471faabd064037b40b3a2a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>SatParametersDefaultTypeInternal</type>
      <name>_SatParameters_default_instance_</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae6d7897cec550c4b33117827b971e421</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_VariableOrder</type>
      <name>SatParameters_VariableOrder_VariableOrder_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>afebbdcc35f1ea46b6b36b02942a45718</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_VariableOrder</type>
      <name>SatParameters_VariableOrder_VariableOrder_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa37fa90963dd6767336794ec9ddd88a6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_VariableOrder_VariableOrder_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a0dbcc1f155896d126ee866c6fa7cdbca</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_Polarity</type>
      <name>SatParameters_Polarity_Polarity_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7dcc06ad16f29c763ef71c12e33428d0</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_Polarity</type>
      <name>SatParameters_Polarity_Polarity_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa9e4f6913a334312075d8b06e4a8f481</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_Polarity_Polarity_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa038a595c0924ec0a6b6d1df43a47a92</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ConflictMinimizationAlgorithm</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a797fae7f793822a392093b2c0e0583df</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ConflictMinimizationAlgorithm</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad89401863cafbbb42117a67da51a9c7a</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_ConflictMinimizationAlgorithm_ConflictMinimizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8bb3e1b9fc46859bcb473d877fdf81f7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_BinaryMinizationAlgorithm</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af3993105042b8a18ef4c48af71dbfae6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_BinaryMinizationAlgorithm</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ac3979ff56f4e8e1a3827ffe9a1cfd953</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_BinaryMinizationAlgorithm_BinaryMinizationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a4603f2b46b1da66b7f160b501802a571</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ClauseProtection</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a50a54b74c5a02bf787d5161be8496a65</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ClauseProtection</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a315f1d416996e0e1df6cf7c5f22c4c83</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_ClauseProtection_ClauseProtection_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a177a5f526c9aaf4dde0ae3d973a0a1c6</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ClauseOrdering</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aa6f7a09fe0ece7997430857d3d2b06d7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_ClauseOrdering</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8939c25bb2dba51fc7d410b379ca4b95</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_ClauseOrdering_ClauseOrdering_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a7bd8956745a9e0f194935411ad26a7a7</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_RestartAlgorithm</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ad069012c7b5da25c0b506d93f249ae3c</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_RestartAlgorithm</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae5654991d85d76a2bb57727e534aca69</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_RestartAlgorithm_RestartAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a9fc9eb8a69f68bb8c56f718d2905cccf</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_MaxSatAssumptionOrder</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a8276145f0fea9405a17ea4e15437c370</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_MaxSatAssumptionOrder</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a253ac635cb2d948d9e6f7ddbdf50deb3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_MaxSatAssumptionOrder_MaxSatAssumptionOrder_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ab876374ba4d61ed8a8f5ab1647214f57</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_MaxSatStratificationAlgorithm</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a81ef72928f25bf91f9459e95b30f60a3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_MaxSatStratificationAlgorithm</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>af77a4c44b279a8ed20ba62fe6855f3d3</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_MaxSatStratificationAlgorithm_MaxSatStratificationAlgorithm_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>aeb0e2efc07e4da53cac4c129726d25c2</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_SearchBranching</type>
      <name>SatParameters_SearchBranching_SearchBranching_MIN</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a759737df3763d9079011350ee71b933f</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const SatParameters_SearchBranching</type>
      <name>SatParameters_SearchBranching_SearchBranching_MAX</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>ae811ceae8f8230a59d40b5effad594af</anchor>
      <arglist></arglist>
    </member>
    <member kind="variable">
      <type>const int</type>
      <name>SatParameters_SearchBranching_SearchBranching_ARRAYSIZE</name>
      <anchorfile>namespaceoperations__research_1_1sat.html</anchorfile>
      <anchor>a82110fc37ba023a467574052d75d507b</anchor>
      <arglist></arglist>
    </member>
  </compound>
</tagfile>
